#include <stdio.h>
#include "storage.h"

#include <stdlib.h>

pthread_mutex_t create_lock() {
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    return lock;
}

void generate_rand_len_string(char *str, size_t max_size) {
    size_t len = rand() % max_size;
    for (size_t i = 0; i < len; ++i) {
        str[i] = 'a';
    }
    str[len] = '\0';
}

Storage init_storage(const size_t size) {
    Storage storage = { 0 };

    for (size_t i = 0; i < size; ++i) {
        Node *new = calloc(1, sizeof(*new));
        if (!new) {
            perror("malloc()");
            abort();
        }

        new->sync = create_lock();
        new->next = storage.first;
        generate_rand_len_string(new->value, 100);

        storage.first = new;
    }

    return storage;
}