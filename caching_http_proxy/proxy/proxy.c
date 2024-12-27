#include "proxy.h"

#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <curl/curl.h>

#include "../threading/threadpool.h"
#include "../cache/linkedlist.h"

#define SERVER_SOCKET_LISTENER_QUEUE_COUNT 128
#define BUFFER_SIZE 4096
#define MAX_METHOD_NAME_LEN 32
#define MAX_URL_NAME_LEN 512
#define MAX_VERSION_NAME_LEN 32

// Called each time any transfer is done over TCP.
static size_t proxy_curl_write_callback(char *ptr, size_t size /* = 1 always */, size_t nmemb, void *userdata) {
    const size_t total_size = size * nmemb;
    if (total_size == 0) return total_size;
    // This function may be called with zero bytes data if the transferred file is empty.

    node_t *cache_entry = (node_t *) userdata;
    if (!cache_entry) {
        printf("%s: userdata is invalid!", __func__);
        return 0; // will abort overall transaction because (size * nmemb) != 0.
    }

    http_cache_node_notify_subscribers(cache_entry, ptr, total_size);
    return total_size;
}

void process_request(connection_ctx_t *conn) {
    int sock_fd = conn->sock_fd;
    http_cache_t *cache = conn->cache;

    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    // ssize_t bytes_read = read(sock_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        if (bytes_read < 0) {
            fprintf(stderr, "recv() error on fd %d: %s\n", sock_fd, strerror(errno));
        }
        // close(sock_fd);
        // conn->sock_fd = -1; // Mark as closed
        return;
    }

    buffer[bytes_read] = '\0';
    char method[MAX_METHOD_NAME_LEN]  = {0};
    char url[MAX_URL_NAME_LEN]    = {0};
    char version[MAX_VERSION_NAME_LEN] = {0};

    if (sscanf(buffer, "%31s %511s %31s", method, url, version) != 3) {
        fprintf(stderr, "Failed to parse request line on fd %d\n", sock_fd);
        fprintf(stderr, "%s", buffer);
        close(sock_fd);
        conn->sock_fd = -1; // Mark as closed
        return;
    }

    // Only GET/HEAD are accepted
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        fprintf(stderr, "Unsupported method: %s from %s\n", method, url);
        close(sock_fd);
        conn->sock_fd = -1; // Mark as closed
        return;
    }

    // Accept HTTP/1.0 or HTTP/1.1, ignoring keep-alive
    if (strcmp(version, "HTTP/1.0") != 0 && strcmp(version, "HTTP/1.1") != 0) {
        fprintf(stderr, "Unsupported HTTP version: %s\n", version);
        close(sock_fd);
        conn->sock_fd = -1; // Mark as closed
        return;
    }

    printf("Process request from fd %d: %s %s %s\n", sock_fd, method, url, version);

    node_t *cached_node = http_cache_find_node(cache, url);
    if (cached_node) {
        if (http_cache_node_add_subscriber(cached_node, sock_fd) != 0) {
            fprintf(stderr, "Failed to add subscriber\n");
            // close(sock_fd);
            // conn->sock_fd = -1;
        }
        return;
    }

    if (http_cache_find_serialized_and_send(url, sock_fd) == 0) {
        // close(sock_fd);
        // conn->sock_fd = -1;
        return;
    }

    cached_node = http_cache_create_node(cache, url, sock_fd);
    if (!cached_node) {
        fprintf(stderr, "Failed to create cache node\n");
        // close(sock_fd);
        // conn->sock_fd = -1;
        return;
    }

    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "curl_easy_init() failed\n");
        // close(sock_fd);
        // conn->sock_fd = -1;
        return;
    }
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, proxy_curl_write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, cached_node);

    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() error: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl_handle);
    http_cache_node_destroy(cache, url);
}

void proxy_start(const uint16_t server_port) {
    printf("Started proxy on: %u\n", server_port);

    threadpool_t *tp_client = NULL;
    http_cache_t *cache = NULL;
    int32_t server_sockfd = 0;
    struct sockaddr_in server_addr = {};
    struct sockaddr_in client_addr = {};
    constexpr socklen_t client_addr_len = sizeof(client_addr);

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        printf("socket() fail\n");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        fprintf(stderr, "bind() failed: %s\n", strerror(errno));
        close(server_sockfd);
        return;
    }

    if (listen(server_sockfd, SERVER_SOCKET_LISTENER_QUEUE_COUNT) != 0) {
        fprintf(stderr, "listen() failed: %s\n", strerror(errno));
        close(server_sockfd);
        return;
    }

    tp_client = threadpool_init();
    if (!tp_client) {
        printf("threadpool_init()\n");
        goto end;
    }

    cache = http_cache_init();
    if (!cache) {
        printf("http_cache_init()");
        goto end;
    }

    // Accept loop: assign each client to a worker
    // int round_robin_index = 0;
    while (1) {
        const int client_fd = accept(server_sockfd, (struct sockaddr *) &client_addr,  (socklen_t *) &client_addr_len);
        if (client_fd < 0) {
            fprintf(stderr, "accept() error: %s\n", strerror(errno));
            continue;
        }

        if (threadpool_add_client(tp_client, client_fd, cache) < 0) {
            fprintf(stderr, "All workers are full; closing client fd %d.\n", client_fd);
            close(client_fd);
        }
        // Simple round-robin selection (could be any balancing strategy)
        // if (add_client_to_worker(&tp_client->worker_data[round_robin_index], client_fd, cache) < 0) {
        //     // Worker is full, try the next
        //     fprintf(stderr, "Worker %d is full. Closing client %d\n", round_robin_index, client_fd);
        //     close(client_fd);
        // }
        // round_robin_index = (round_robin_index + 1) % MAX_WORKER_THREADS;
    }

end:
    threadpool_shutdown(&tp_client);
    http_cache_shutdown(&cache);
    if (server_sockfd >= 0) {
        close(server_sockfd);
    }
}
