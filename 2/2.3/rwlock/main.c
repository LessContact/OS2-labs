#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>

#include "storage.h"

#define STORAGE_SIZE 100000

#define CPU_Asc 1
#define CPU_Desc 2
#define CPU_Eq 3
#define CPU_Swap1 4
#define CPU_Swap2 5
#define CPU_Swap3 6

typedef struct stats {
    size_t asc_iters;
    size_t desc_iters;
    size_t eq_iters;

    size_t swaps1;
    size_t swaps2;
    size_t swaps3;

    size_t asc_string_count;
    size_t desc_string_count;
    size_t eq_string_count;
} stats_t;

stats_t stats = { 0 };

void set_cpu(int n) {
    int err;
    cpu_set_t cpuset;
    pthread_t tid = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(n, &cpuset);

    err = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
    if (err) {
        printf("set_cpu: pthread_setaffinity failed for cpu %d\n", n);
        return;
    }

    printf("set_cpu: set cpu %d\n", n);
}

void print_stats() {
    printf("iters: %zu | %zu | %zu ||| swaps: %zu | %zu | %zu ||| seq strings found: %zu | %zu | %zu\n",
        stats.asc_iters, stats.desc_iters, stats.eq_iters, stats.swaps1,
        stats.swaps2, stats.swaps3, stats.asc_string_count,
        stats.desc_string_count, stats.eq_string_count);
}

void count_asc(Storage* storage) {
    set_cpu(CPU_Asc);
    while (1) {
        Node* cur = storage->first;
        Node* next;
        Node* tmp;

        while (cur != NULL) {
            pthread_mutex_lock(&cur->sync);
            tmp = cur;

            next = cur->next;
            if (next == NULL) {
                pthread_mutex_unlock(&cur->sync);
                break;
            }
            pthread_mutex_lock(&next->sync);

            if (strlen(cur->value) < strlen(next->value)) stats.asc_string_count++;

            cur = cur->next;

            pthread_mutex_unlock(&cur->sync);
            pthread_mutex_unlock(&tmp->sync);
        }
        stats.asc_iters++;
    }
}

void count_desc(Storage* storage) {
    set_cpu(CPU_Desc);
    while (1) {
        Node* cur = storage->first;
        Node* next;
        Node* tmp;

        while (cur != NULL) {
            pthread_mutex_lock(&cur->sync);
            tmp = cur;

            next = cur->next;
            if (next == NULL) {
                pthread_mutex_unlock(&cur->sync);
                break;
            }
            pthread_mutex_lock(&next->sync);

            if (strlen(cur->value) > strlen(next->value)) stats.desc_string_count++;

            cur = cur->next;

            pthread_mutex_unlock(&cur->sync);
            pthread_mutex_unlock(&tmp->sync);
        }
        stats.desc_iters++;
    }
}

void count_eq(Storage* storage) {
    set_cpu(CPU_Eq);
    while (1) {
        Node* cur = storage->first;
        Node* next;
        Node* tmp;

        while (cur != NULL) {
            pthread_mutex_lock(&cur->sync);
            tmp = cur;

            next = cur->next;
            if (next == NULL) {
                pthread_mutex_unlock(&cur->sync);
                break;
            }
            pthread_mutex_lock(&next->sync);

            if (strlen(cur->value) == strlen(next->value)) stats.eq_string_count++;

            cur = cur->next;

            pthread_mutex_unlock(&cur->sync);
            pthread_mutex_unlock(&tmp->sync);
        }
        stats.eq_iters++;
    }
}

void swapper1(Storage* storage) {
    set_cpu(CPU_Swap1);
    while (1) {
        Node* prev = storage->first;
        Node* cur = NULL;
        Node* next = NULL;
        Node* tmp = NULL;
        if (prev == NULL) {
            continue;
        }
        while (1) {
            if (prev == NULL) {
                break;
            }
            pthread_mutex_lock(&prev->sync);

            if (rand() % 3 == 0) {

                if (prev->next == NULL) {
                    pthread_mutex_unlock(&prev->sync);
                    break;
                }
                cur = prev->next;

                pthread_mutex_lock(&cur->sync);
                if (cur->next == NULL) {
                    pthread_mutex_unlock(&prev->sync);
                    pthread_mutex_unlock(&cur->sync);
                    break;
                }
                next = cur->next;

                pthread_mutex_lock(&next->sync);

                prev->next = next;
                cur->next = next->next;
                next->next = cur;
                stats.swaps1++;

                pthread_mutex_unlock(&next->sync);
                pthread_mutex_unlock(&cur->sync);
            }
            tmp = prev;
            prev = prev->next;
            pthread_mutex_unlock(&tmp->sync);
        }
    }
}

void swapper2(Storage* storage) {
    set_cpu(CPU_Swap2);
    while (1) {
        Node* prev = storage->first;
        Node* cur = NULL;
        Node* next = NULL;
        Node* tmp = NULL;
        if (prev == NULL) {
            continue;
        }
        while (1) {
            if (prev == NULL) {
                break;
            }
            pthread_mutex_lock(&prev->sync);

            if (rand() % 3 == 0) {

                if (prev->next == NULL) {
                    pthread_mutex_unlock(&prev->sync);
                    break;
                }
                cur = prev->next;

                pthread_mutex_lock(&cur->sync);
                if (cur->next == NULL) {
                    pthread_mutex_unlock(&prev->sync);
                    pthread_mutex_unlock(&cur->sync);
                    break;
                }
                next = cur->next;

                pthread_mutex_lock(&next->sync);

                prev->next = next;
                cur->next = next->next;
                next->next = cur;
                stats.swaps2++;

                pthread_mutex_unlock(&next->sync);
                pthread_mutex_unlock(&cur->sync);
            }
            tmp = prev;
            prev = prev->next;
            pthread_mutex_unlock(&tmp->sync);
        }
    }
}

void swapper3(Storage* storage) {
    set_cpu(CPU_Swap3);
    while (1) {
        Node* prev = storage->first;
        Node* cur = NULL;
        Node* next = NULL;
        Node* tmp = NULL;
        if (prev == NULL) {
            continue;
        }
        while (1) {
            if (prev == NULL) {
                break;
            }
            pthread_mutex_lock(&prev->sync);

            if (rand() % 3 == 0) {

                if (prev->next == NULL) {
                    pthread_mutex_unlock(&prev->sync);
                    break;
                }
                cur = prev->next;

                pthread_mutex_lock(&cur->sync);
                if (cur->next == NULL) {
                    pthread_mutex_unlock(&prev->sync);
                    pthread_mutex_unlock(&cur->sync);
                    break;
                }
                next = cur->next;

                pthread_mutex_lock(&next->sync);

                prev->next = next;
                cur->next = next->next;
                next->next = cur;
                stats.swaps3++;

                pthread_mutex_unlock(&next->sync);
                pthread_mutex_unlock(&cur->sync);
            }
            tmp = prev;
            prev = prev->next;
            pthread_mutex_unlock(&tmp->sync);
        }
    }
}

int main(int argc, char **argv) {
    Storage storage = init_storage(STORAGE_SIZE);

    pthread_t asc_thread, desc_thread, eq_thread;

    int err = pthread_create(&asc_thread, NULL, count_asc, &storage);
    if (err) {
        printf("pthread_create: pthread_create failed %d\n", err);
        return -1;
    }
    err = pthread_create(&desc_thread, NULL, count_desc, &storage);
    if (err) {
        printf("pthread_create: pthread_create failed %d\n", err);
        return -1;
    }
    err = pthread_create(&eq_thread, NULL, count_eq, &storage);
    if (err) {
        printf("pthread_create: pthread_create failed %d\n", err);
        return -1;
    }

    pthread_t swapper1_thread, swaper2_thread, swaper3_thread;
    err = pthread_create(&swapper1_thread, NULL, swapper1, &storage);
    if (err) {
        printf("pthread_create: pthread_create failed %d\n", err);
        return -1;
    }
    err = pthread_create(&swaper2_thread, NULL, swapper2, &storage);
    if (err) {
        printf("pthread_create: pthread_create failed %d\n", err);
        return -1;
    }
    err = pthread_create(&swaper3_thread, NULL, swapper3, &storage);
    if (err) {
        printf("pthread_create: pthread_create failed %d\n", err);
        return -1;
    }

    while (1) {
        sleep(1);
        print_stats();
    }

    return 0;
}

