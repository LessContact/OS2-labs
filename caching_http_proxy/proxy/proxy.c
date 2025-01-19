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
#include "../caching/httpcache.h"

static pthread_mutex_t searchCreateMutex;

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

long long get_content_len(struct phr_header *headers, size_t numHeaders) {
    struct phr_header *contLenHeader = findHeader(headers, numHeaders, "Content-Length");
    if (!contLenHeader) return -1;

    // char contentLength[contLenHeader->value_len + 1];
    // memcpy(contentLength, contLenHeader->value, contLenHeader->value_len);
    // contentLength[contLenHeader->value_len] = '\0';

    char *endptr;
    long long ret = strtoll(contLenHeader->value, &endptr, 10);
    if (*endptr != '\r') {
        log_error("Invalid content length: %.*s", contLenHeader->value_len, contLenHeader->value);
        return -1;
    }
    if (errno == ERANGE || errno == EINVAL) {
        log_error("an overflow or underflow occurred in content length, or content-length is EINVAL, returning -1");
        return -1;
    }
    if (ret < 0) return 0;
    return ret;
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
        log_error("failed to resolve host. getaddrinfo: %s", gai_strerror(ret));
        return -1;
    }

    // Create socket and connect
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            log_error("failed to create socket: %s", strerror(errno));
            continue;
        }

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == -1) {
            close(sockfd);
            log_error("failed to connect to host. connect: %s", strerror(errno));
            continue;
        }
        break;
    }

    freeaddrinfo(res); // Free the linked-list

    if (rp == NULL) {
        log_error("Could not connect to any address");
        return -1;
    }

    return sockfd;
}

// sends up to buflen bytes from buf to socket sockfd
// on error or socket disconnect shuts down the socket and closes it
ssize_t send_buffer(int sockfd, const void *buffer, size_t buflen) {
    ssize_t total_sent_bytes = 0;
    while (total_sent_bytes != buflen) {
        ssize_t sent_bytes = send(sockfd, buffer + total_sent_bytes, buflen - total_sent_bytes, MSG_NOSIGNAL);
        if (sent_bytes == -1) {
            if (errno == EINTR) continue;
            log_error("failed to pass request with: %s", strerror(errno));
            disconnect(sockfd);
            return -1;
        } else if (sent_bytes == 0) {
            log_error("??? something is seriously? wrong? with: %s", strerror(errno));
            disconnect(sockfd);
            return -1;
        }
        total_sent_bytes += sent_bytes;
    }
    return total_sent_bytes;
}

// returns 0 on success, -1 on error
int handle_cached_request(cache_entry_t *entry, int client_fd) {
    size_t offset = 0;
    char buffer[8192];
    size_t bytes_read;

    while ((bytes_read = cache_entry_read(entry, buffer, offset, sizeof(buffer))) > 0) {
        if (send_buffer(client_fd, buffer, bytes_read) < 0) {
            log_error("could not send cached data to client");
            return -1;
        }
        offset += bytes_read;
    }
    return 0;
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
        while ((rret = recv(client_sock_fd, buffer + buflen, sizeof(buffer) - buflen, 0)) == -1 && errno == EINTR) {
        }
        if (rret == -1) {
            log_error("request recv error: %s", strerror(errno));
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
            return;
        }
        if (rret == 0) {
            log_warn("client socket closed");
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
            return;
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
        } else if (pret == -1) {
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
    // log_debug("request received and parsed: %s", buffer);
    // request is now received from client and parsed ==================================================================

    // Only GET are accepted
    if (strncmp(request.method, "GET", 3) != 0 ) {//&& strncmp(request.method, "HEAD", 4) != 0) {
        // todo possibly forward unsupported requests without any work
        log_warn("Unsupported method: %.*s from %.*s", request.methodLen, request.method, request.pathLen,
                 request.path);
        disconnect(client_sock_fd);
        conn->sock_fd = -1; // Mark as closed
        return;
    }

    // Accept HTTP/1.0 or HTTP/1.1, ignoring keep-alive
    if (request.minorVersion != 0 && request.minorVersion != 1) {
        log_warn("Unsupported HTTP version: HTTP/1.%d", request.minorVersion);
        disconnect(client_sock_fd);
        conn->sock_fd = -1; // Mark as closed
        return;
    }

    struct phr_header *host_header = findHeader(request.headers, request.numHeaders, "Host");
    char hostname[1024];
    // if (!hostname) { // i just couldn't be bothered to make sure this is freed in all branches and VLAs are bad, right?
    //     log_fatal("Failed to allocate memory for hostName");
    //     disconnect(client_sock_fd);
    //     conn->sock_fd = -1;
    //     return;
    // }

    memcpy(hostname, host_header->value, host_header->value_len);
    hostname[host_header->value_len] = '\0';

    log_debug("Process request from fd %d: %s", client_sock_fd, hostname);

    // caching =-=-=-=-=-=-=-=-=-=-=-=-=-=
    char url[2048];
    if (host_header->value_len + request.pathLen > 2048) {
        log_error("full url too long ");
        disconnect(client_sock_fd);
        conn->sock_fd = -1;
        return;
    }
    memcpy(url, request.path, request.pathLen);
    url[request.pathLen] = '\0';
    // strcat(url, request.path);

    pthread_mutex_lock(&searchCreateMutex);

    cache_entry_t *entry = cache_lookup(cache, url);
    if (entry) {
        pthread_mutex_unlock(&searchCreateMutex);

        handle_cached_request(entry, client_sock_fd);
        cache_entry_release(cache, entry);

        //todo send cached response
    } else {
        // If no cache entry was found ============================================================================

        int remote_sock_fd;
        if ((remote_sock_fd = resolve_and_connect(hostname, 80)) == -1) {
            pthread_mutex_unlock(&searchCreateMutex);
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
            return;
        }
        log_debug("connected to %s:%d", hostname, remote_sock_fd);
        // PASS SEND request from client to remote =========================================================================

        size_t bytes_sent = send_buffer(remote_sock_fd, buffer, buflen);
        if (bytes_sent == -1) {
            pthread_mutex_unlock(&searchCreateMutex);
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
            return;
        }
        log_debug("buffer sent to remote");
        // request has been sent to remote, now we need to get the response from remote and pass to client
        // PASS RESPONSE ===================================================================================================

        ssize_t bytes_recieved = 0, total_bytes_recieved = 0;
        response_t response;
        ssize_t header_len;
        char *headerend_pos = 0;
        do {
            if (total_bytes_recieved >= BUFFER_SIZE) {
                log_error("headers section is too long from %s, consider increasing buffer_size", hostname);
                pthread_mutex_unlock(&searchCreateMutex);
                disconnect(client_sock_fd);
                disconnect(remote_sock_fd);
                conn->sock_fd = -1;
                return;
            }

            bytes_recieved = recv(remote_sock_fd, buffer + total_bytes_recieved, BUFFER_SIZE - total_bytes_recieved, 0);
            // log_debug("bytes recieved : %s", buffer);
            if (bytes_recieved <= 0) {
                pthread_mutex_unlock(&searchCreateMutex);
                if (bytes_recieved == -1)
                    log_error("receive error from %s: %s", hostname, strerror(errno));
                if (bytes_recieved == 0)
                    log_error("receive error: server %s disconnected", hostname);
                disconnect(client_sock_fd);
                disconnect(remote_sock_fd);
                conn->sock_fd = -1;
                return;
            }

            total_bytes_recieved += bytes_recieved;
            buffer[total_bytes_recieved] = '\0';
        } while ((headerend_pos = strstr(buffer, "\r\n\r\n")) == NULL);
        headerend_pos += 4; // advance for "\r\n\r\n"
        header_len = headerend_pos - buffer;
        log_debug("found end of response headers");

        response.numHeaders = sizeof(response.headers) / sizeof(response.headers[0]);
        int err;
        err = phr_parse_response(
            buffer, header_len, &response.minorVersion, &response.status, &response.msg,
            &response.msg_len, response.headers, &response.numHeaders, 0
        );
        if (err < 0) {
            log_error("failed to parse response with picoparser from %s: %s", hostname, strerror(errno));
            pthread_mutex_unlock(&searchCreateMutex);
            disconnect(client_sock_fd);
            disconnect(remote_sock_fd);
            conn->sock_fd = -1;
            return;
        }
        // log_debug("response parsed from %s", buffer);
        int do_cache = 1;
        if (response.status != 200) {
            // todo for cache
            do_cache = 0;
            pthread_mutex_unlock(&searchCreateMutex);
        }

        ssize_t content_len = get_content_len(response.headers, response.numHeaders);

        // this is a hack really -=-=-=-=-=-
        if (content_len <= 0) {
            do_cache = 0;
            pthread_mutex_unlock(&searchCreateMutex);
        }

        entry = NULL;

        if (do_cache) {
            entry = cache_insert(cache, url, header_len + content_len);
            if (!entry) {
                log_error("failed to create cache entry");
                pthread_mutex_unlock(&searchCreateMutex);
                disconnect(client_sock_fd);
                disconnect(remote_sock_fd);
                conn->sock_fd = -1;
                return;
            }
        }
        // if (entry)
        if (do_cache)
            pthread_mutex_unlock(&searchCreateMutex); // cache entry now created

        log_debug("content-length is %d", content_len);

        if (do_cache)
            cache_entry_append(entry, buffer, total_bytes_recieved);

        // pass the received response header and maybe part of response body
        bytes_sent = send_buffer(client_sock_fd, buffer, total_bytes_recieved);
        if (bytes_sent == -1) {
            disconnect(client_sock_fd);
            conn->sock_fd = -1;
            return;
        }
        log_debug("first part of response forwarded to client");

        log_debug("starting recv from remote send to client loop");
        if (content_len != -1) {
            ssize_t remaining = header_len + content_len - total_bytes_recieved;
            while (remaining > 0) {
                bytes_recieved = recv(remote_sock_fd, buffer, BUFFER_SIZE, 0);
                if (bytes_recieved <= 0) {
                    if (do_cache) {
                        cache_entry_release(cache, entry);
                    }
                    if (bytes_recieved == -1)
                        log_error("recv: %s", strerror(errno));
                    if (bytes_recieved == 0)
                        log_error("recv: server disconnected");
                    disconnect(client_sock_fd);
                    disconnect(remote_sock_fd);
                    conn->sock_fd = -1;
                    return;
                }

                remaining -= bytes_recieved;

                if (do_cache) {
                    cache_entry_append(entry, buffer, bytes_recieved);
                }

                bytes_sent = send_buffer(client_sock_fd, buffer, bytes_recieved);
                if (bytes_sent == -1) {
                    if (do_cache) {
                        cache_entry_release(cache, entry);
                    }
                    disconnect(remote_sock_fd);
                    conn->sock_fd = -1;
                    return;
                }
            }
            if (do_cache) {
                cache_entry_complete(entry);
                cache_entry_release(cache, entry);
            }
            disconnect(client_sock_fd);
            disconnect(remote_sock_fd);
            conn->sock_fd = -1;
            log_debug("client %s finished successfully", hostname);
            return;
        } else {
            while (1) {
                bytes_recieved = recv(remote_sock_fd, buffer, BUFFER_SIZE, 0);
                if (bytes_recieved <= 0) {
                    if (bytes_recieved == -1)
                        log_error("recv: %s", strerror(errno));
                    if (bytes_recieved == 0) {
                        log_info("recv: server %s disconnected as per http 1.0 standard", hostname);
                        if (do_cache) {
                            cache_entry_complete(entry);
                        }
                    }
                    cache_entry_release(cache, entry);
                    disconnect(client_sock_fd);
                    disconnect(remote_sock_fd);
                    conn->sock_fd = -1;
                    return;
                }

                bytes_sent = send_buffer(client_sock_fd, buffer, bytes_recieved);
                if (bytes_sent == -1) {
                    if (do_cache) {
                        cache_entry_release(cache, entry);
                    }
                    disconnect(remote_sock_fd);
                    conn->sock_fd = -1;
                    return;
                }
            }
        }
    }
}

void proxy_start(const uint16_t server_port) {
    log_set_level(LOG_INFO);

    log_info("Started proxy on: %u", server_port);

    threadpool_t *tp_client = NULL;
    http_cache_t *cache = NULL;
    int32_t server_sockfd = 0;
    struct sockaddr_in server_addr = {};
    struct sockaddr_in client_addr = {};
    constexpr socklen_t client_addr_len = sizeof(client_addr);

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        log_fatal("socket() fail");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        log_fatal("bind() failed: %s", strerror(errno));
        close(server_sockfd);
        return;
    }

    if (listen(server_sockfd, SERVER_SOCKET_LISTENER_QUEUE_COUNT) != 0) {
        log_fatal("listen() failed: %s", strerror(errno));
        close(server_sockfd);
        return;
    }

    tp_client = threadpool_init(client_worker_main);
    if (!tp_client) {
        log_fatal("threadpool_init()");
        goto end;
    }

    cache = http_cache_init(1000 * 1024 * 1024); //1000 MB cache max size
    if (!cache) {
        log_fatal("http_cache_init()");
        goto end;
    }

    // Accept loop: assign each client to a worker
    while (1) {
        const int client_fd = accept(server_sockfd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
        if (client_fd < 0) {
            log_error("accept() error: %s", strerror(errno));
            continue;
        }

        if (threadpool_add_client(tp_client, client_fd, cache) < 0) {
            log_warn("All workers are full; closing client fd %d.", client_fd);
            close(client_fd);
        }
        log_debug("added client fd %d", client_fd);
    }

end:
    threadpool_shutdown(&tp_client);
    http_cache_shutdown(&cache);
    if (server_sockfd >= 0) {
        close(server_sockfd);
    }
}
