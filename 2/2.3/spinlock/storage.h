#ifndef STORAGE_H
#define STORAGE_H

#include <pthread.h>

typedef struct _Node {
    char value[100];
    struct _Node* next;
    pthread_spinlock_t sync;
} Node;

typedef struct _Storage {
    Node *first;
} Storage;

/* not thread-safe */
Storage init_storage(size_t size);
void generate_rand_len_string(char *str, size_t max_size);

#endif // !STORAGE_H