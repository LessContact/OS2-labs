#include "httpcache.h"
#include <stdlib.h>
#include <string.h>
#include <asm-generic/errno.h>

static void free_entry_data(cache_entry_t *entry) {
    data_chunk_t *chunk = entry->data_head;
    while (chunk) {
        data_chunk_t *next = chunk->next;
        free(chunk->data);
        free(chunk);
        chunk = next;
    }
}

static uint32_t hash_url(const char *url) {
    uint32_t hash = 2166136261u;
    while (*url) {
        hash ^= (uint8_t)*url++;
        hash *= 16777619u;
    }
    return hash;
}

// LRU management functions
static void lru_remove(http_cache_t *cache, cache_entry_t *entry) {
    if (entry->lru_prev)
        entry->lru_prev->lru_next = entry->lru_next;
    else
        cache->lru_head = entry->lru_next;

    if (entry->lru_next)
        entry->lru_next->lru_prev = entry->lru_prev;
    else
        cache->lru_tail = entry->lru_prev;

    entry->lru_prev = entry->lru_next = NULL;
}

static void lru_add_head(http_cache_t *cache, cache_entry_t *entry) {
    entry->lru_next = cache->lru_head;
    entry->lru_prev = NULL;

    if (cache->lru_head)
        cache->lru_head->lru_prev = entry;
    cache->lru_head = entry;

    if (!cache->lru_tail)
        cache->lru_tail = entry;
}

static void remove_entry(http_cache_t* cache, cache_entry_t *entry) {
    pthread_mutex_lock(&entry->lock);
    cache_entry_t *to_evict = entry;
    if (to_evict->refcount > 0) {
        pthread_mutex_unlock(&entry->lock);
        return;;
    }

    pthread_mutex_lock(&cache->lru_lock);
    // Remove from LRU list
    lru_remove(cache, to_evict);
    pthread_mutex_unlock(&cache->lru_lock);

    uint32_t bucket_idx = hash_url(to_evict->url) % cache->num_buckets;
    pthread_mutex_lock(&cache->buckets[bucket_idx].lock);

    // Remove from hash bucket
    cache_entry_t **pp = &cache->buckets[bucket_idx].entries;
    while (*pp && *pp != to_evict)
        pp = &(*pp)->next;
    if (*pp)
        *pp = to_evict->next;

    // Update size atomically
    pthread_mutex_lock(&cache->size_lock);
    cache->current_size -= to_evict->total_size;
    pthread_mutex_unlock(&cache->size_lock);

    pthread_mutex_unlock(&cache->buckets[bucket_idx].lock);

    // Free the entry
    pthread_mutex_destroy(&to_evict->lock);
    pthread_cond_destroy(&to_evict->data_ready);
    free_entry_data(to_evict);
    free(to_evict);
}

// dont use this without rewriting it
static void evict_lru_entries(http_cache_t *cache, size_t required_size) {
    while (1) {
        size_t current_size;
        pthread_mutex_lock(&cache->size_lock);
        current_size = cache->current_size;
        pthread_mutex_unlock(&cache->size_lock);

        if (current_size + required_size <= cache->max_size) {
            return;
        }

        pthread_mutex_lock(&cache->lru_lock);
        if (!cache->lru_tail) {
            pthread_mutex_unlock(&cache->lru_lock);
            return;
        }

        cache_entry_t *to_evict = cache->lru_tail;
        if (to_evict->refcount > 0) {
            if (to_evict->lru_prev == NULL) {
                pthread_mutex_unlock(&cache->lru_lock);
                return;
            }
            cache->lru_tail = to_evict->lru_prev;
            cache->lru_tail->lru_next = NULL;
            pthread_mutex_unlock(&cache->lru_lock);
            continue;
        }

        // Remove from LRU list
        lru_remove(cache, to_evict);
        pthread_mutex_unlock(&cache->lru_lock);

        uint32_t bucket_idx = hash_url(to_evict->url) % cache->num_buckets;
        pthread_mutex_lock(&cache->buckets[bucket_idx].lock);

        // Remove from hash bucket
        cache_entry_t **pp = &cache->buckets[bucket_idx].entries;
        while (*pp && *pp != to_evict)
            pp = &(*pp)->next;
        if (*pp)
            *pp = to_evict->next;

        // Update size atomically
        pthread_mutex_lock(&cache->size_lock);
        cache->current_size -= to_evict->total_size;
        pthread_mutex_unlock(&cache->size_lock);

        pthread_mutex_unlock(&cache->buckets[bucket_idx].lock);

        // Free the entry
        pthread_mutex_destroy(&to_evict->lock);
        pthread_cond_destroy(&to_evict->data_ready);
        free_entry_data(to_evict);
        free(to_evict);
    }
}

cache_entry_t* cache_lookup(http_cache_t *cache, const char *url) {

    cache_entry_t *entry = NULL;
    uint32_t bucket_idx = hash_url(url) % cache->num_buckets;

    pthread_mutex_lock(&cache->buckets[bucket_idx].lock);

    for (entry = cache->buckets[bucket_idx].entries; entry != NULL; entry = entry->next) {
        if (strcmp(entry->url, url) == 0) {
            pthread_mutex_lock(&entry->lock);

            // Skip cancelled entries
            if (entry->state == ENTRY_CANCELLED) {
                pthread_mutex_unlock(&entry->lock);
                continue;
            }

            entry->refcount++;
            entry->last_access = time(NULL);

            pthread_mutex_lock(&cache->lru_lock);
            lru_remove(cache, entry);
            lru_add_head(cache, entry);
            pthread_mutex_unlock(&cache->lru_lock);

            pthread_mutex_unlock(&entry->lock);
            break;
        }
    }

    pthread_mutex_unlock(&cache->buckets[bucket_idx].lock);

    return entry;
}

cache_entry_t* cache_insert(http_cache_t *cache, const char *url) {
    // First ensure we have space
    // evict_lru_entries(cache, expected_size);
    //
    // pthread_mutex_lock(&cache->size_lock);
    // if (cache->current_size + expected_size > cache->max_size) {
    //     pthread_mutex_unlock(&cache->size_lock);
    //     log_error("could not free space for new cache entry");
    //     return NULL;
    // }
    // cache->current_size += expected_size;  // Update size immediately if we have space
    // pthread_mutex_unlock(&cache->size_lock);

    cache_entry_t *entry = calloc(1, sizeof(cache_entry_t));
    if (!entry) {
        // pthread_mutex_lock(&cache->size_lock);
        // cache->current_size -= expected_size;  // Rollback on failure as this is less likely
        // pthread_mutex_unlock(&cache->size_lock);
        log_error("could not allocate memory for cache entry");
        return NULL;
    }

    // pthread_mutex_lock(&cache->size_lock);
    // cache->current_size += expected_size;
    // pthread_mutex_unlock(&cache->size_lock);

    strncpy(entry->url, url, MAX_URL_LENGTH - 1);
    // entry->data = malloc(expected_size);
    // if (!entry->data) {
    //     free(entry);
    //     log_error("could not allocate memory for cache entry url");
    //     return NULL;
    // }

    entry->data_head = NULL;
    entry->data_tail = NULL;
    entry->total_size = 0;  // Will grow as data is appended
    entry->state = ENTRY_INCOMPLETE;
    entry->refcount = 1;
    entry->last_access = time(NULL);

    pthread_mutex_init(&entry->lock, NULL);
    pthread_cond_init(&entry->data_ready, NULL);

    uint32_t bucket_idx = hash_url(url) % cache->num_buckets;
    pthread_mutex_lock(&cache->buckets[bucket_idx].lock);
    entry->next = cache->buckets[bucket_idx].entries;
    cache->buckets[bucket_idx].entries = entry;
    pthread_mutex_unlock(&cache->buckets[bucket_idx].lock);

    // Add to LRU list
    pthread_mutex_lock(&cache->lru_lock);
        lru_add_head(cache, entry);
    pthread_mutex_unlock(&cache->lru_lock);

    return entry;
}

// returns 0 on success, -1 on failure
int cache_entry_append_chunk(cache_entry_t *entry, const void *data, size_t size) {
    pthread_mutex_lock(&entry->lock);
    if (entry->state == ENTRY_CANCELLED) {
        pthread_mutex_unlock(&entry->lock);
        log_fatal("cache entry should never be used after cancellation, how did this happen????");
        return -1;
    }

    data_chunk_t *new_chunk = malloc(sizeof(data_chunk_t));
    if (!new_chunk) {
        pthread_mutex_unlock(&entry->lock);
        log_error("could not allocate memory for new data chunk");
        return -1;
    }

    new_chunk->data = malloc(size);
    if (!new_chunk->data) {
        free(new_chunk);
        pthread_mutex_unlock(&entry->lock);
        log_error("could not allocate memory for chunk data");
        return -1;
    }

    memcpy(new_chunk->data, data, size);
    new_chunk->size = size;
    new_chunk->next = NULL;

    if (!entry->data_head) {
        entry->data_head = new_chunk;
        entry->data_tail = new_chunk;
    } else {
        entry->data_tail->next = new_chunk;
        entry->data_tail = new_chunk;
    }

    entry->total_size += size;
    pthread_cond_broadcast(&entry->data_ready);
    pthread_mutex_unlock(&entry->lock);
    return 0;
}

// Function to read from cache entry with waiting
ssize_t cache_entry_read(cache_entry_t *entry, void *buf, ssize_t offset, ssize_t size) {
    pthread_mutex_lock(&entry->lock);

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout); // it is REALTIME because that's what timed wait requires (epoch time)
    timeout.tv_sec += 5; // 5-second timeout

    // todo: this can maybe be an exit point to support multiplexing, though it maybe needs to be in proxy.c and not here
    while (offset >= entry->total_size && entry->state == ENTRY_INCOMPLETE) {
        int rc = pthread_cond_timedwait(&entry->data_ready, &entry->lock, &timeout);
        // Check if entry was cancelled
        if (entry->state == ENTRY_CANCELLED) { // todo maybe shift this after ETIMEDOUT
            pthread_mutex_unlock(&entry->lock);
            log_error("cache entry was cancelled");
            return -1;
        }
        if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&entry->lock);
            log_error("cache read timed out");
            return -1;  // Timeout error
        }
    }

    // Don't read from cancelled entries
    if (entry->state == ENTRY_CANCELLED) {
        pthread_mutex_unlock(&entry->lock);
        return -1;
    }

    // Find the starting chunk and offset within it
    data_chunk_t *chunk = entry->data_head;
    ssize_t chunk_offset = offset;
    while (chunk && chunk_offset >= chunk->size) {
        chunk_offset -= chunk->size;
        chunk = chunk->next;
    }

    // Read data from chunks
    ssize_t bytes_read = 0;
    uint8_t *dest = buf;

    while (chunk && bytes_read < size) {
        ssize_t available_in_chunk = chunk->size - chunk_offset;
        ssize_t to_read = (size - bytes_read < available_in_chunk) ? size - bytes_read : available_in_chunk;

        memcpy(dest + bytes_read, chunk->data + chunk_offset, to_read);
        bytes_read += to_read;
        chunk = chunk->next;
        chunk_offset = 0;  // Reset offset for subsequent chunks
    }

    pthread_mutex_unlock(&entry->lock);
    return bytes_read;
}

void cache_entry_complete(cache_entry_t *entry) {
    if (entry == NULL) {
        log_fatal("cache entry should not be NULL");
        return;
    }
    pthread_mutex_lock(&entry->lock);
    entry->state = ENTRY_COMPLETE;
    pthread_cond_broadcast(&entry->data_ready);
    pthread_mutex_unlock(&entry->lock);
}

void cache_entry_release(cache_entry_t *entry) {
    if (entry == NULL) {
        log_fatal("cache entry should not be NULL");
        return;
    }
    pthread_mutex_lock(&entry->lock);
    entry->refcount--;
    pthread_mutex_unlock(&entry->lock);
}

void cache_entry_cancel(cache_entry_t *entry) {
    if (entry == NULL) {
        log_fatal("cache entry should not be NULL");
        return;
    }
    pthread_mutex_lock(&entry->lock);
    entry->state = ENTRY_CANCELLED;
    pthread_cond_broadcast(&entry->data_ready);  // Wake up any waiting readers
    pthread_mutex_unlock(&entry->lock);
}


http_cache_t* http_cache_init(size_t max_size) {
    http_cache_t *cache = calloc(1, sizeof(http_cache_t));
    if (!cache) return NULL;

    cache->num_buckets = MAX_BUCKETS;
    cache->max_size = max_size ? max_size : DEFAULT_CACHE_SIZE;
    cache->buckets = calloc(cache->num_buckets, sizeof(cache_bucket_t));

    if (!cache->buckets) {
        free(cache);
        return NULL;
    }

    for (size_t i = 0; i < cache->num_buckets; i++) {
        pthread_mutex_init(&cache->buckets[i].lock, NULL);
    }

    pthread_mutex_init(&cache->size_lock, NULL);
    pthread_mutex_init(&cache->lru_lock, NULL);

    return cache;
}

void http_cache_shutdown(http_cache_t **cache_ptr) {
    if (!cache_ptr || !*cache_ptr) {
        return;
    }

    http_cache_t *cache = *cache_ptr;

    // Free all entries in each bucket
    for (size_t i = 0; i < cache->num_buckets; i++) {
        cache_bucket_t *bucket = &cache->buckets[i];

        // Lock the bucket to ensure no new operations can start
        pthread_mutex_lock(&bucket->lock);

        cache_entry_t *entry = bucket->entries;
        while (entry) {
            cache_entry_t *next = entry->next;

            // Destroy synchronization primitives
            pthread_mutex_destroy(&entry->lock);
            pthread_cond_destroy(&entry->data_ready);

            // Free entry data
            free_entry_data(entry);
            free(entry);

            entry = next;
        }

        pthread_mutex_unlock(&bucket->lock);
        pthread_mutex_destroy(&bucket->lock);
    }

    // Destroy remaining synchronization primitives
    pthread_mutex_destroy(&cache->lru_lock);
    pthread_mutex_destroy(&cache->size_lock);

    // Free buckets array and cache structure
    free(cache->buckets);
    free(cache);
    *cache_ptr = NULL;

    log_info("Cache shutdown completed successfully");
}