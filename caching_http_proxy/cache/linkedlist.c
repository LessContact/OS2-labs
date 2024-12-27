#include "linkedlist.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

static void url_to_filename(char *url_filename) {
    const size_t url_len = strlen(url_filename);
    if (!url_filename || url_len == 0) return;

    for (size_t i = 0, j = 0; url_filename[i] != '\0' && j < url_len; ++i) {
        if (isalnum(url_filename[i]) || url_filename[i] == '.' || url_filename[i] == '_') {
            url_filename[j++] = url_filename[i];
        } else {
            url_filename[j++] = '_';
        }
    }
}

http_cache_t *http_cache_init() {
    http_cache_t *cache = (http_cache_t *) calloc(1, sizeof(http_cache_t));
    if (!cache) {
        printf("%s: calloc()\n", __func__);
        return NULL;
    }

    if (pthread_mutex_init(&cache->queue_mutex, NULL) != 0) {
        printf("%s: pthread_mutex_init()\n", __func__);
        free(cache);
        return NULL;
    }

    return cache;
}

node_t *http_cache_create_node(http_cache_t *cache, const char *url, const int32_t client_sockfd) {
    if (!cache || !url || client_sockfd < 0) {
        printf("%s: cache or url or client_sockfd is invalid!", __func__);
        return NULL;
    }

    node_t *node = (node_t *) calloc(1, sizeof(node_t));
    if (!node) {
        printf("%s: calloc()\n", __func__);
        return NULL;
    }

    if (cache->tail) cache->tail->next = node;
    else cache->head = node;
    cache->tail = node;

    if (pthread_mutex_init(&node->node_mutex, NULL) != 0) {
        printf("%s: pthread_mutex_init()\n");
        free(node);
        return NULL;
    }

    subscriber_t *new_sub = (subscriber_t *) calloc(1, sizeof(subscriber_t));
    if (!new_sub) {
        printf("%s: calloc()\n", __func__);
        pthread_mutex_destroy(&node->node_mutex);
        free(node);
        return NULL;
    }

    new_sub->client_sockfd = client_sockfd;
    node->tail = new_sub;
    node->head = new_sub;

    const size_t url_len = strlen(url) + 1;
    node->url = (char *) calloc(1, url_len * sizeof(char));
    if (!node->url) {
        printf("%s: calloc()\n", __func__);
        pthread_mutex_destroy(&node->node_mutex);
        free(new_sub);
        free(node);
        return NULL;
    }
    memcpy(node->url, url, url_len * sizeof(char));
    node->url[url_len - 1] = '\0';

    node->url_filename = (char *) calloc(1, url_len * sizeof(char));
    if (!node->url_filename) {
        printf("%s: calloc()\n", __func__);
        pthread_mutex_destroy(&node->node_mutex);
        free(new_sub);
        free(node->url);
        free(node);
        return NULL;
    }

    memcpy(node->url_filename, node->url, url_len * sizeof(char));
    node->url_filename[url_len - 1] = '\0';
    url_to_filename(node->url_filename);

    FILE *file = fopen(node->url_filename, "w+");
    if (file) {
        fclose(file);
    } else {
        printf("%s: failed to create file for: %s\n", __func__, node->url);
    }

    printf("%s: created node for url: %s, url_fname: %s\n", __func__, node->url, node->url_filename);
    return node;
}

node_t *http_cache_find_node(http_cache_t *cache, const char *url) {
    if (!cache || !url) {
        printf("%s: cache or url is invalid!", __func__);
        return NULL;
    }

    pthread_mutex_lock(&cache->queue_mutex);
    node_t *cached_node = cache->head;
    while (cached_node) {
        if (strcmp(url, cached_node->url) == 0) {
            pthread_mutex_unlock(&cache->queue_mutex);
            return cached_node;
        }
        cached_node = cached_node->next;
    }

    pthread_mutex_unlock(&cache->queue_mutex);
    return NULL;
}

int32_t http_cache_node_add_subscriber(node_t *node, const int32_t client_sockfd) {
    if (!node || client_sockfd < 0) {
        printf("%s: node or client_sockfd is invalid!\n", __func__);
        return -1;
    }

    pthread_mutex_lock(&node->node_mutex);

    // 1. Add subscriber.
    subscriber_t *new_sub = (subscriber_t *) calloc(1, sizeof(subscriber_t));
    if (!new_sub) {
        pthread_mutex_unlock(&node->node_mutex);
        printf("%s: calloc()\n", __func__);
        return -1;
    }

    new_sub->client_sockfd = client_sockfd;
    if (node->tail) node->tail->next = new_sub;
    else node->head = new_sub;
    node->tail = new_sub;

    // 2. Fetch data from disk.
    FILE *file = fopen(node->url_filename, "r");
    if (file) {
        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_read = 0;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            ssize_t overall_bytes_sent = 0;
            //        printf("sending %ld bytes to sock: %d\n", bytes_read, client_sockfd);
            while (overall_bytes_sent != bytes_read) {
                const ssize_t bytes_left_to_send = bytes_read - overall_bytes_sent;
                const ssize_t current_bytes_sent = send(client_sockfd, (char *) buffer + overall_bytes_sent,
                                                        bytes_left_to_send, 0);
                if (current_bytes_sent < 0) {
                    printf("%s: send()\n", __func__);
                    break;
                }
                overall_bytes_sent += current_bytes_sent;
            }
        }
        fclose(file);
    } else {
        printf("%s: failed to fetch data from disk for %s.\n", __func__, node->url);
    }

    printf("%s: added subscriber to: %s\n", __func__, node->url);
    pthread_mutex_unlock(&node->node_mutex);

    return 0;
}

void http_cache_node_notify_subscribers(node_t *node, const void *data, const size_t data_size) {
    if (!node || !data || data_size == 0) {
        printf("%s: node or data or data_size is invalid!\n", __func__);
        return;
    }

    pthread_mutex_lock(&node->node_mutex);

    // 1. Sending new chunk of data to all subs.
    subscriber_t *tmp = node->head;
    while (tmp) {
        ssize_t overall_bytes_sent = 0;
        while (overall_bytes_sent != data_size) {
            const ssize_t bytes_left_to_send = data_size - overall_bytes_sent;
            const ssize_t current_bytes_sent = send(tmp->client_sockfd, (char *) data + overall_bytes_sent,
                                                    bytes_left_to_send, MSG_NOSIGNAL);
            if (current_bytes_sent < 0) {
                printf("%s: send()\n", __func__);
                break;
            }
            overall_bytes_sent += current_bytes_sent;
        }

        tmp = tmp->next;
    }

    // 2. Store new chunk of data to disk.
    FILE *file = fopen(node->url_filename, "a+");
    if (file) {
        size_t overall_bytes_written = 0;
        while (overall_bytes_written != data_size) {
            const size_t bytes_left_to_write = data_size - overall_bytes_written;
            const size_t current_bytes_written = fwrite((char *) data + overall_bytes_written, 1, bytes_left_to_write,
                                                        file);
            if (current_bytes_written < 1 && ferror(file)) {
                printf("%s: fwrite() error\n", __func__);
                break;
            }
            overall_bytes_written += current_bytes_written;
        }
        fclose(file);
    } else {
        printf("%s: failed to open file for url: %s\n", __func__, node->url);
    }

    pthread_mutex_unlock(&node->node_mutex);
}

void http_cache_node_destroy(http_cache_t *cache, const char *url) {
    if (!cache || !url) {
        printf("%s: cache or url is invalid!\n", __func__);
        return;
    }

    pthread_mutex_lock(&cache->queue_mutex);

    node_t *prev = NULL;
    node_t *current = cache->head;
    while (current) {
        if (strcmp(current->url, url) == 0) {
            subscriber_t *sub = current->head;
            while (sub) {
                close(sub->client_sockfd);

                subscriber_t *tmp = sub;
                sub = sub->next;

                free(tmp);
            }

            // Remove the node from the cache
            if (prev) prev->next = current->next; // Bypass the current node
            else cache->head = current->next; // Update head if it's the first node

            if (cache->tail == current) cache->tail = prev;

            pthread_mutex_destroy(&current->node_mutex);
            free(current->url);
            free(current->url_filename);
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&cache->queue_mutex);
}

int32_t http_cache_find_serialized_and_send(const char *url, const int32_t client_sockfd) {
    if (!url || client_sockfd < 0) {
        printf("%s: url or client_sockfd is invalid!\n", __func__);
        return -1;
    }

    const size_t url_len = strlen(url) + 1;
    char *url_filename = (char *) calloc(1, url_len * sizeof(char));
    if (!url_filename) {
        printf("%s: calloc()\n", __func__);
        return -1;
    }

    memcpy(url_filename, url, url_len * sizeof(char));
    url_filename[url_len - 1] = '\0';
    url_to_filename(url_filename);

    FILE *file = fopen(url_filename, "r");
    if (!file) {
        free(url_filename);
        return -1;
    }

    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        ssize_t overall_bytes_sent = 0;
        while (overall_bytes_sent != bytes_read) {
            const ssize_t bytes_left_to_send = bytes_read - overall_bytes_sent;
            const ssize_t current_bytes_sent = send(client_sockfd, (char *) buffer + overall_bytes_sent,
                                                    bytes_left_to_send, MSG_NOSIGNAL);
            if (current_bytes_sent < 0) {
                printf("%s: send()\n", __func__);
                break;
            }
            overall_bytes_sent += current_bytes_sent;
        }
    }

    close(client_sockfd);
    fclose(file);
    free(url_filename);
    return 0;
}

void http_cache_shutdown(http_cache_t **cache) {
    if (!cache || !*cache) return;

    while ((*cache)->head) {
        node_t *tmp = (*cache)->head;
        (*cache)->head = (*cache)->head->next;

        // in case clients aren't closed, we close them.
        while (tmp->head) {
            subscriber_t *sub_tmp = (tmp)->head;
            tmp->head = tmp->head->next;

            close(sub_tmp->client_sockfd);
            free(sub_tmp);
        }

        pthread_mutex_destroy(&tmp->node_mutex);
        free(tmp->url);
        free(tmp->url_filename);

        free(tmp);
    }
    pthread_mutex_destroy(&(*cache)->queue_mutex);

    free(*cache);
    *cache = NULL;
}
