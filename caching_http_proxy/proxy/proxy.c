#include "proxy.h"

#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include <curl/curl.h>
#include "../third_party/log.h"

#include "../threading/threadpool.h"
#include "../caching/linkedlist.h"

static void disconnect(int sock) {
    shutdown(sock, SHUT_RDWR);
    close(sock);
}

static struct phr_header *findHeader(struct phr_header *headers, size_t numHeaders, const char *name) {
    size_t len = strlen(name);
    for (size_t i = 0; i < numHeaders; i++) {
        if (headers[i].name_len == len && (strncasecmp(headers[i].name, name, len) == 0)) {
            return &headers[i];
        }
    }
    return NULL;
}

// takes a string url and port number to return an open socket
// connected to the specified host
// the socket need to be closed manually afterwards
int resolve_and_connect(const char *url, int port) {
    struct addrinfo hints, *res, *rp;
    int sockfd = -1;

    // Set up hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    // Resolve URL
    char port_str[6];
    snprintf(port_str, 6, "%d", port);
    int ret;
    if ((ret = getaddrinfo(url, port_str, &hints, &res)) != 0) {
        log_error("failed to resolve host. getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    // Create socket and connect
    for(rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            log_error("failed to create socket: %s\n", strerror(errno));
            continue;
        }

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == -1) {
            close(sockfd);
            log_error("failed to connect to host. connect: %s\n", strerror(errno));
            continue;
        }
        break;
    }

    freeaddrinfo(res); // Free the linked-list

    if (rp == NULL) {
        log_error("Could not connect to any address\n");
        return -1;
    }

    return sockfd;
}

void process_request(connection_ctx_t *conn) {
    int client_sock_fd = conn->sock_fd;
    http_cache_t *cache = conn->cache;
    int pret;
    ssize_t rret;
    size_t buflen = 0, prevbuflen = 0;
    char buffer[BUFFER_SIZE] = {0};
    request_t request;

    while (1) {
        /* read the request */
        while ((rret = recv(client_sock_fd, buffer + buflen, sizeof(buffer) - buflen, 0)) == -1 && errno == EINTR)
            {}
        if (rret == -1) {
            log_error("request recv error: %s", strerror(errno));
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
        }
        if (rret == 0) {
            log_warn("client socket closed");
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
        }

        prevbuflen = buflen;
        buflen += rret;

        /* parse the request */
        request.numHeaders = sizeof(request.headers) / sizeof(request.headers[0]);
        pret = phr_parse_request(
            buffer, buflen, &request.method, &request.methodLen, &request.path, &request.pathLen,
            &request.minorVersion, request.headers, &request.numHeaders, prevbuflen
        );

        if (pret > 0) {
            break;
        }
        else if (pret == -1) {
            log_error("Error parsing request with picoparser");
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
        }

        /* request is incomplete, continue the loop */
        assert(pret == -2);
        if (buflen == sizeof(buffer)) {
            log_error("Request is too long");
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
        }
    }
    // request is now received from client and parsed ==================================================================

    // Only GET/HEAD are accepted
    if (strncmp(request.method, "GET", 3) != 0 && strncmp(request.method, "HEAD", 4) != 0) {
        // todo possibly forward unsupported requests without any work
        log_warn("Unsupported method: %s from %s\n", request.method, request.path);
        disconnect(client_sock_fd);
        conn->sock_fd = -1; // Mark as closed
        return;
    }

    // Accept HTTP/1.0 or HTTP/1.1, ignoring keep-alive
    if (request.minorVersion != 0 || request.minorVersion != 1) {
        log_warn("Unsupported HTTP version: %s\n", request.minorVersion);
        disconnect(client_sock_fd);
        conn->sock_fd = -1; // Mark as closed
        return;
    }

    struct phr_header *host_header = findHeader(request.headers, request.numHeaders, "Host");
    char *hostname = calloc(1, host_header->value_len + 1);
    if (!hostname) {
        log_fatal("Failed to allocate memory for hostName");
        disconnect(client_sock_fd);
        conn->sock_fd = -1;
        return;
    }
    memcpy(hostname, host_header->value, host_header->value_len);
    hostname[host_header->value_len] = '\0';

    log_info("Process request from fd %d: %s\n", client_sock_fd, hostname);

    int remote_sock_fd;
    if ((remote_sock_fd = resolve_and_connect(hostname, 80)) == -1) {
        disconnect(client_sock_fd);
        conn->sock_fd = -1;
        return;
    }
    // PASS SEND the request from the client to remote =================================================================
    size_t bytes_read = buflen;
    ssize_t total_sent_bytes = 0;
    while (total_sent_bytes != bytes_read) {
        ssize_t sent_bytes = send(remote_sock_fd, buffer + total_sent_bytes, bytes_read - total_sent_bytes, MSG_NOSIGNAL);
        if (sent_bytes == -1) {
            if (errno == EINTR) continue;
            log_error("failed to pass request \"%s\" with: %s\n", buffer, strerror(errno));
            disconnect(remote_sock_fd);
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
            return;
        } else if (sent_bytes == 0) {
            log_error("??? something is seriously? wrong? with \"%s\" or the site \"%s\" : %s\n", buffer, hostname, strerror(errno));
            disconnect(remote_sock_fd);
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
            return;
        }
        total_sent_bytes += sent_bytes;
    }
    // request has been sent to remote, now we need to get the response from remote and pass to client
    // PASS RESPONSE ===================================================================================================
    size_t bytes_recieved = 0, total_bytes_recieved = 0;
    response_t response;
    size_t header_len;
    char *headerend_pos = 0;
    do {
        if (total_bytes_recieved == BUFFER_SIZE) {
            log_error("headers section is too long, consider increasing buffer_size\n");
            disconnect(client_sock_fd);
            disconnect(remote_sock_fd);
            conn->sock_fd = -1;
            return;
        }

        bytes_recieved = recv(client_sock_fd, buffer + total_bytes_recieved, BUFFER_SIZE - total_bytes_recieved, 0);
        if (bytes_recieved <= 0) {
            if (bytes_recieved == -1) log_error("receive error from %s: %s", hostname, strerror(errno));
            if (bytes_recieved == 0) log_error("receive error: server %s disconnected", hostname);
            disconnect(client_sock_fd);
            disconnect(remote_sock_fd);
            conn->sock_fd = -1;
            return;
        }

        total_bytes_recieved += bytes_recieved;
        buffer[total_bytes_recieved] = '\0';
    } while ((headerend_pos = strstr(buffer, "\r\n\r\n")) == NULL);
    headerend_pos += 4; // advance for "\r\n\r\n"
    headerend_pos = headerend_pos - buffer; //=--=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=

    response.numHeaders = sizeof(response.headers) / sizeof(response.headers[0]);
    err = phr_parse_response(
        buffer, header_len, &response.minorVersion, &response.status, &response.msg,
        &response.msgLen, response.headers, &response.numHeaders, 0
    );

    //-----------------
    ssize_t bytes_received;
    while (1) {
        // Receive data from the remote server
        bytes_received = recv(remote_sock_fd, buffer, sizeof(buffer), 0);

        if (bytes_received == -1) {
            if (errno == EINTR) continue;
            log_error("Failed to receive data from remote server \"%s\": %s\n", url, strerror(errno));
            break;
        } else if (bytes_received == 0) {
            // Connection closed by the remote server
            log_info("Remote server \"%s\" closed the connection\n", url);
            break;
        }

        // Forward the received data to the client
        ssize_t total_sent = 0;
        while (total_sent < bytes_received) {
            ssize_t sent_bytes = send(client_sock_fd, buffer + total_sent, bytes_received - total_sent, MSG_NOSIGNAL);

            if (sent_bytes == -1) {
                if (errno == EINTR) continue;
                else {
                    log_error("Failed to send data to client: %s\n", strerror(errno));
                    return;
                }
            } else if (sent_bytes == 0) {
                // Connection closed unexpectedly
                log_error("Client closed the connection unexpectedly\n");
                return;
            }
            total_sent += sent_bytes;
        }
    }
}

void proxy_start(const uint16_t server_port) {
    log_info("Started proxy on: %u\n", server_port);

    threadpool_t *tp_client = NULL;
    http_cache_t *cache = NULL;
    int32_t server_sockfd = 0;
    struct sockaddr_in server_addr = {};
    struct sockaddr_in client_addr = {};
    constexpr socklen_t client_addr_len = sizeof(client_addr);

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        log_fatal("socket() fail\n");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        log_fatal("bind() failed: %s\n", strerror(errno));
        close(server_sockfd);
        return;
    }

    if (listen(server_sockfd, SERVER_SOCKET_LISTENER_QUEUE_COUNT) != 0) {
        log_fatal("listen() failed: %s\n", strerror(errno));
        close(server_sockfd);
        return;
    }

    tp_client = threadpool_init(client_worker_main);
    if (!tp_client) {
        log_fatal("threadpool_init()\n");
        goto end;
    }

    cache = http_cache_init();
    if (!cache) {
        log_fatal("http_cache_init()");
        goto end;
    }

    // Accept loop: assign each client to a worker
    while (1) {
        const int client_fd = accept(server_sockfd, (struct sockaddr *) &client_addr,  (socklen_t *) &client_addr_len);
        if (client_fd < 0) {
            log_error("accept() error: %s\n", strerror(errno));
            continue;
        }

        if (threadpool_add_client(tp_client, client_fd, cache) < 0) {
            log_warn("All workers are full; closing client fd %d.\n", client_fd);
            close(client_fd);
        }
    }

end:
    threadpool_shutdown(&tp_client);
    http_cache_shutdown(&cache);
    if (server_sockfd >= 0) {
        close(server_sockfd);
    }
}
