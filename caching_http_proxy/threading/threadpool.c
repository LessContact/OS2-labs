#include "threadpool.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../proxy/proxy.h"
#include "../third_party/log.h"

void *client_worker_main(void *arg) {
    worker_data_t *worker = (worker_data_t *)arg;

    while (1) {
        pthread_mutex_lock(&worker->lock);

        // If we have no clients, wait for new ones or for a shutdown
        while (worker->nfds == 0 && !worker->is_shutdown) {
            pthread_cond_wait(&worker->worker_notify, &worker->lock);
        }

        // If shutting down
        if (worker->is_shutdown) {
            pthread_mutex_unlock(&worker->lock);
            break;
        }

        // Make a local copy of poll fds & connections
        struct pollfd fds_local[MAX_CLIENTS_PER_THREAD];
        connection_ctx_t *conn_local[MAX_CLIENTS_PER_THREAD];
        nfds_t nfds_local = worker->nfds;

        memcpy(fds_local, worker->fds, nfds_local * sizeof(struct pollfd));
        memcpy(conn_local, worker->connections, nfds_local * sizeof(connection_ctx_t *));

        pthread_mutex_unlock(&worker->lock);

        int ret = poll(fds_local, nfds_local, 10);
        if (ret < 0) {
            log_error("poll() error: %s\n", strerror(errno));
            continue;
        }
        else if (ret == 0) {
            // Timeout, no events
            continue;
        }

        // Process each readable socket
        for (int i = 0; i < nfds_local; i++) {
            if (fds_local[i].revents & (POLLERR | POLLNVAL)) {
                if (fds_local[i].revents & POLLERR) {
                    log_error("POLLERR error");
                    // Mark the socket as closed
                    close(conn_local[i]->sock_fd);
                }
                if (fds_local[i].revents & POLLNVAL) log_error("POLLNVAL error");
                conn_local[i]->sock_fd = -1;
            } else if (fds_local[i].revents & POLLIN) {
                process_request(conn_local[i]);
            }
            else if (fds_local[i].revents & POLLHUP) {
                /*
                this is like this beacuse:
                POLLHUP
                Hang up (only returned in revents; ignored in events).  Note that when reading from a channel such as a pipe or  a  stream
                socket,  this  event merely indicates that the peer closed its end of the channel.  Subsequent reads from the channel will
                return 0 (end of file) only after all outstanding data in the channel has been consumed.
                */
                // Mark as closed
                close(conn_local[i]->sock_fd);
                conn_local[i]->sock_fd = -1;
            }
        }

        // Clean up closed connections
        pthread_mutex_lock(&worker->lock);
        memcpy(worker->fds, fds_local, nfds_local * sizeof(struct pollfd));
        memcpy(worker->connections, conn_local, nfds_local * sizeof(connection_ctx_t *));

        for (int i = 0; i < worker->nfds; i++) {
            // If sock_fd < 0, connection was closed while processing
            if (worker->connections[i]->sock_fd < 0) {
                free(worker->connections[i]);
                // Shift the array left by one element to fill the gap
                memmove(worker->fds + i, worker->fds + i + 1,(worker->nfds - 1 - i) * sizeof(worker->fds[0]));
                memmove(worker->connections + i, worker->connections + i + 1, (worker->nfds - 1 - i) * sizeof(worker->connections[0]));
                worker->nfds--;
                i--; // re-check same index after shift
            }
        }
        pthread_mutex_unlock(&worker->lock);
    }

    // free resources
    pthread_mutex_lock(&worker->lock);

    // Free all remaining connections
    for (int i = 0; i < worker->nfds; i++) {
        if (worker->connections[i] != NULL) {
            // Close socket if it's still open
            if (worker->connections[i]->sock_fd >= 0) {
                close(worker->connections[i]->sock_fd);
            }
            // Free the connection context
            free(worker->connections[i]);
            worker->connections[i] = NULL;
        }
    }
    // Reset the number of file descriptors
    worker->nfds = 0;
    // Clear the arrays
    // memset(worker->fds, 0, sizeof(struct pollfd) * MAX_CLIENTS_PER_THREAD);
    // memset(worker->connections, 0, sizeof(connection_ctx_t*) * MAX_CLIENTS_PER_THREAD);
    pthread_mutex_unlock(&worker->lock);
    return NULL;
}

static worker_data_t *pick_worker(threadpool_t *tp) {
    static int last_worker; // Keeps track of the last worker picked

    int start_index = (last_worker + 1) % MAX_WORKER_THREADS; // Rotate starting point
    int best_worker = -1;
    uint32_t min_load = UINT_MAX;

    for (int i = 0; i < MAX_WORKER_THREADS; i++) {
        int current_index = (start_index + i) % MAX_WORKER_THREADS;
        worker_data_t *w = &tp->worker_data[current_index];

        pthread_mutex_lock(&w->lock);
        if (w->nfds < min_load) {
            min_load = w->nfds;
            best_worker = current_index;
        }
        pthread_mutex_unlock(&w->lock);
    }

    // Update the last picked worker
    last_worker = best_worker;

    return &tp->worker_data[best_worker];
}


static int add_client_to_worker(worker_data_t *worker, int client_fd, http_cache_t *cache) {
    pthread_mutex_lock(&worker->lock);

    if (worker->nfds >= MAX_CLIENTS_PER_THREAD) {
        pthread_mutex_unlock(&worker->lock);
        return -1; // This worker is at capacity
    }

    ulong idx = worker->nfds++;
    worker->fds[idx].fd = client_fd;
    worker->fds[idx].events = POLLIN;
    worker->fds[idx].revents = 0;

    connection_ctx_t *conn = calloc(1, sizeof(connection_ctx_t));
    if (!conn) {
        // If allocation fails, revert nfds and return -1
        worker->fds[idx].fd = -1;
        worker->nfds--;
        pthread_mutex_unlock(&worker->lock);
        return -1;
    }
    conn->sock_fd = client_fd;
    conn->cache = cache;

    worker->connections[idx] = conn;

    // Signal the worker in case it was waiting for new connections
    pthread_cond_signal(&worker->worker_notify);
    pthread_mutex_unlock(&worker->lock);

    return 0;
}

int threadpool_add_client(threadpool_t *tp, int client_fd, http_cache_t *cache) {
    if (!tp) {
        return -1;
    }

    worker_data_t *worker = pick_worker(tp);
    if (!worker) {
        return -1;
    }

    int ret = add_client_to_worker(worker, client_fd, cache);

    return ret;
}

threadpool_t *threadpool_init(void *(*worker_function)(void *)) {
    if (!worker_function) {
        log_fatal("Worker function cannot be NULL\n");
        return NULL;
    }

    threadpool_t *tp = (threadpool_t *) calloc(1, sizeof(threadpool_t));
    if (!tp) {
        log_fatal("no mem");
        return NULL;
    }

    tp->worker_data = calloc(MAX_WORKER_THREADS, sizeof(worker_data_t));

    if (!tp->worker_data) {
        log_fatal("Allocation error\n");
        free(tp);
        return NULL;
    }

    for (uint32_t i = 0; i < MAX_WORKER_THREADS; i++) {
        pthread_mutex_init(&tp->worker_data[i].lock, NULL);
        pthread_cond_init(&tp->worker_data[i].worker_notify, NULL);
        tp->worker_data[i].nfds = 0;
        tp->worker_data[i].is_shutdown = 0;
        if (pthread_create(&tp->worker_threads[i], NULL, worker_function, &tp->worker_data[i]) != 0) {
            log_fatal("Failed to create worker thread %d\n", i);
            perror("pthread_create");
            free(tp->worker_data);
            free(tp);
            return NULL;
        }
    }

    return tp;
}

void threadpool_shutdown(threadpool_t **tp) {
    if (!tp || !*tp) return;
    for (uint32_t i = 0; i < MAX_WORKER_THREADS; i++) {
        (*tp)->worker_data[i].is_shutdown = 1;
        pthread_cond_signal(&(*tp)->worker_data[i].worker_notify);
    }

    for (uint32_t i = 0; i < MAX_WORKER_THREADS; ++i) {
        pthread_join((*tp)->worker_threads[i], NULL);
        pthread_cond_destroy(&(*tp)->worker_data[i].worker_notify);
        pthread_mutex_destroy(&(*tp)->worker_data[i].lock);
    }

    free((*tp)->worker_data);

    free(*tp);
    *tp = NULL;

    log_info("threadpool shutdown successfully");
}
