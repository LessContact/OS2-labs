#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdint.h>
#include <poll.h>

#include "../caching/httpcache.h"
#include "../proxy/proxy.h"

#define MAX_WORKER_THREADS 8
#define MAX_CLIENTS_PER_THREAD 64

typedef enum {
    STATE_READ_REQUEST,
    STATE_CONNECT_REMOTE,
    STATE_SEND_REQUEST,
    STATE_READ_RESPONSE_HEADERS,
    STATE_PROCESS_RESPONSE,
    STATE_FORWARD_BODY,
    STATE_CLEANUP
} request_state_t;

typedef struct _con_ctx {
    int sock_fd;
    http_cache_t *cache;
    //int is_forwarded;

    // New fields for state management
    request_state_t state;
    int remote_sock_fd;
    char buffer[BUFFER_SIZE];
    size_t buflen;
    size_t bytes_processed;
    response_t response;
    cache_entry_t *cache_entry;
    size_t content_length;
    size_t total_received;
    int do_cache;
} connection_ctx_t;

typedef struct _worker_data {
    struct pollfd fds[MAX_CLIENTS_PER_THREAD];
    connection_ctx_t *connections[MAX_CLIENTS_PER_THREAD];
    nfds_t nfds;
    _Atomic uint32_t is_shutdown;
    pthread_mutex_t lock;
    pthread_cond_t worker_notify;
} worker_data_t;

typedef struct _threadpool {
    pthread_t worker_threads[MAX_WORKER_THREADS];
    worker_data_t *worker_data;
    // uint32_t is_shutdown;
} threadpool_t;

threadpool_t *threadpool_init(void *(*worker_function)(void *));

void *client_worker_main(void *arg);
// int add_client_to_worker(worker_data_t *worker, int client_fd, http_cache_t *cache);
void threadpool_shutdown(threadpool_t **tp);
int threadpool_add_client(threadpool_t *tp, int client_fd, http_cache_t *cache);

#endif
