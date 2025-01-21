#ifndef PROXY_H
#define PROXY_H

#include <stdint.h>
#include <signal.h>
#include "../third_party/picohttpparser.h"
#include "../threading/threadpool.h"

#define SERVER_SOCKET_LISTENER_QUEUE_COUNT 128
#define BUFFER_SIZE 8192
#define MAX_METHOD_NAME_LEN 32
#define MAX_URL_NAME_LEN 2048
#define MAX_VERSION_NAME_LEN 16
#define MAX_HEADERS 128

typedef struct _request_t {
    const char *method;
    const char *path;
    struct phr_header headers[MAX_HEADERS];
    size_t methodLen;
    size_t pathLen;
    size_t numHeaders;
    int minorVersion;
} request_t;

typedef struct _response_t {
    int minorVersion;
    int status;
    const char *msg;
    size_t msg_len;
    struct phr_header headers[MAX_HEADERS];
    size_t numHeaders;
} response_t;

void proxy_start(const uint16_t server_port);
void process_request(connection_ctx_t *conn);

#endif
