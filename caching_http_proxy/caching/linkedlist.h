#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdint.h>
#include <pthread.h>

typedef struct _subscriber {
    struct _subscriber *next;
    int32_t client_sockfd;
} subscriber_t;

typedef struct _node {
    pthread_mutex_t node_mutex;
    struct _node *next;
    char *url;
    char *url_filename;
    subscriber_t *tail;
    subscriber_t *head;
} node_t;

typedef struct {
    pthread_mutex_t queue_mutex;
    node_t *tail;
    node_t *head;
} http_cache_t;

http_cache_t *http_cache_init();

void http_cache_shutdown(http_cache_t **cache);

node_t *http_cache_find_node(http_cache_t *cache, const char *url);

node_t *http_cache_create_node(http_cache_t *cache, const char *url, const int32_t client_sockfd);

void http_cache_node_destroy(http_cache_t *cache, const char *url);

int32_t http_cache_node_add_subscriber(node_t *node, const int32_t client_sockfd);

void http_cache_node_notify_subscribers(node_t *node, const void *data, const size_t data_size);

int32_t http_cache_find_serialized_and_send(const char *url, const int32_t client_sockfd);

#endif
