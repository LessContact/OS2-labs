#ifndef HTTP_CACHE_H
#define HTTP_CACHE_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include "../third_party/log.h"


#define MAX_URL_LENGTH 2048
#define DEFAULT_CACHE_SIZE (100 * 1024 * 1024) // 100MB default cache size
#define MAX_BUCKETS 1024

typedef volatile enum _state_t {
    ENTRY_INCOMPLETE = 0,
    ENTRY_COMPLETE = 1,
    ENTRY_CANCELLED = 2
} entry_state_t;

typedef struct data_chunk {
    uint8_t *data;
    ssize_t size;
    struct data_chunk *next;
} data_chunk_t;

// LRU list node
typedef struct cache_entry {
    char url[MAX_URL_LENGTH];
    data_chunk_t *data_head;    // Head of data chunks list
    data_chunk_t *data_tail;    // Tail for fast appending
    size_t total_size;
    // size_t current_size;
    time_t last_access;
    entry_state_t state;
    uint32_t refcount;

    // Synchronization
    pthread_mutex_t lock;          // Protects entry data and condition
    pthread_cond_t data_ready;     // Signals when new data is available

    // Hash table links
    struct cache_entry *next;      // Next in hash bucket

    // LRU list links
    struct cache_entry *lru_prev;
    struct cache_entry *lru_next;
} cache_entry_t;

typedef struct cache_bucket {
    cache_entry_t *entries;
    pthread_mutex_t lock;
} cache_bucket_t;

typedef struct http_cache {
    cache_bucket_t *buckets;
    size_t num_buckets;
    size_t current_size;
    size_t max_size;

    // LRU list management
    cache_entry_t *lru_head;      // Most recently used
    cache_entry_t *lru_tail;      // Least recently used
    pthread_mutex_t lru_lock;     // Protects LRU list modifications

    pthread_mutex_t size_lock; // Protects current_size
} http_cache_t;

http_cache_t* http_cache_init(size_t max_size);
void http_cache_shutdown(http_cache_t **cache);
cache_entry_t* cache_lookup(http_cache_t *cache, const char *url);
cache_entry_t* cache_insert(http_cache_t *cache, const char *url);
ssize_t cache_entry_read(cache_entry_t *entry, void *buf, ssize_t offset, ssize_t size);
int cache_entry_append_chunk(cache_entry_t *entry, const void *data, size_t size);
void cache_entry_complete(cache_entry_t *entry);
void cache_entry_release(cache_entry_t *entry);
void cache_entry_cancel(cache_entry_t *entry);

#endif // HTTP_CACHE_H