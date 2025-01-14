#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdint.h>
#include <poll.h>

#include "../caching/linkedlist.h"

#define MAX_WORKER_THREADS 8
#define MAX_CLIENTS_PER_THREAD 64

typedef struct _con_ctx {
    int sock_fd;
    http_cache_t *cache;
    //int in_forwarded;
} connection_ctx_t;

typedef struct _worker_data {
    struct pollfd fds[MAX_CLIENTS_PER_THREAD];
    connection_ctx_t *connections[MAX_CLIENTS_PER_THREAD];
    nfds_t nfds;
    uint32_t is_shutdown;
    pthread_mutex_t lock;
    pthread_cond_t worker_notify;
} worker_data_t;

typedef struct _threadpool {
    pthread_t worker_threads[MAX_WORKER_THREADS];
    worker_data_t *worker_data;
    // uint32_t is_shutdown;
} threadpool_t;

threadpool_t *threadpool_init();

// int add_client_to_worker(worker_data_t *worker, int client_fd, http_cache_t *cache);
void threadpool_shutdown(threadpool_t **tp);
int threadpool_add_client(threadpool_t *tp, int client_fd, http_cache_t *cache);

#endif
