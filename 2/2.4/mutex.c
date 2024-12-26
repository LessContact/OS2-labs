#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <sys/syscall.h>
#include "mutex.h"

#include <linux/futex.h>

#define COUNT_ITERATION 100000

int counter = 0;
mutex_t m;

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

void mutex_init(mutex_t* m){
    m->lock = 1;
}

static int futex(int* uaddr, int futex_op, int val, const struct timespec* timeout,
                         int* uaddr2, int val3) {
    return syscall(SYS_futex, uaddr, futex_op,
                   val, timeout, uaddr2, val3);
}

void mutex_lock(mutex_t* m) {
    while (1) {
        int expected = STATUS_UNLOCK;
        if (atomic_compare_exchange_strong(&m->lock, &expected, STATUS_LOCK))
            break;

        int err = futex(&m->lock, FUTEX_WAIT, STATUS_LOCK, NULL, NULL, 0);
        if (err == -1 && errno != EAGAIN) {
            fprintf(stderr, "futex FUTEX_WAIT failed %s\n", strerror(errno));
            abort();
        }
    }
}

void mutex_unlock(mutex_t *m) {
    int expected = STATUS_LOCK;
    if(atomic_compare_exchange_strong(&m->lock, &expected, STATUS_UNLOCK)) {
        int err = futex(&m->lock, FUTEX_WAKE, STATUS_UNLOCK, NULL, NULL, 0);
        if(err == -1 && errno != EAGAIN) {
            fprintf(stderr, "futex FUTEX_WAKE failed %s\n", strerror(errno));
            abort();
        }
    }
}

void* thread1(void* args) {
    // set_cpu(2);
    mutex_t* m = (mutex_t*) args;
    assert(m != NULL);

    for(int i = 0; i < COUNT_ITERATION; ++i) {
        mutex_lock(m);
        ++counter;
        mutex_unlock(m);
    }

    return NULL;
}

// void* thread2(void* args) {
//     set_cpu(1);
//     mutex_t* m = (mutex_t*) args;
//     assert(m != NULL);
//
//     for(int i = 0; i < COUNT_ITERATION; ++i) {
//         mutex_lock(m);
//         ++counter;
//         mutex_unlock(m);
//     }
//     return NULL;
// }

int main() {

    mutex_init(&m);

    int n = 100;
    pthread_t threads[n];
    int thread_args[n];

    for (int i = 0; i < n; ++i) {
        thread_args[i] = i; // Номер потока
        int err = pthread_create(&threads[i], NULL, thread1, &m);
        if (err) {
            printf("main: pthread_create() failed for thread %d: %s\n", i, strerror(err));
            return 1;
        }
    }

    for (int i = 0; i < n; ++i) {
        int err = pthread_join(threads[i], NULL);
        if (err) {
            printf("main: pthread_join() failed for thread %d: %s\n", i, strerror(err));
            return 1;
        }
    }

    printf("counter %d", counter);
    return EXIT_SUCCESS;
}