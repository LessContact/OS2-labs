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

#define COUNT_ITERATION 10000000

int counter = 0;

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
    set_cpu(2);
    mutex_t* m = (mutex_t*) args;
    assert(m != NULL);

    for(int i = 0; i < COUNT_ITERATION; ++i) {
        mutex_lock(m);
        ++counter;
        mutex_unlock(m);
    }

    return NULL;
}

void* thread2(void* args) {
    set_cpu(1);
    mutex_t* m = (mutex_t*) args;
    assert(m != NULL);

    for(int i = 0; i < COUNT_ITERATION; ++i) {
        mutex_lock(m);
        ++counter;
        mutex_unlock(m);
    }
    return NULL;
}

int main() {
    mutex_t m;

    mutex_init(&m);

    pthread_t tid1, tid2;
    int err = pthread_create(&tid1, NULL, thread1, &m);
	if (err) {
        printf("main: pthread_create(): thread_1 failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    err = pthread_create(&tid2, NULL, thread2, &m);
	if (err) {
		printf("main: pthread_create(): thread_2 failed: %s\n", strerror(err));
        pthread_join(tid1, NULL);
		return EXIT_FAILURE;
	}

    int err1 = pthread_join(tid1, NULL);
    if (err1) {
        printf("main: pthread_join() failed: %s\n", strerror(err1));
        return EXIT_FAILURE;
    }
    err1 = pthread_join(tid2, NULL);
    if (err1) {
        printf("main: pthread_join() failed: %s\n", strerror(err1));
        return EXIT_FAILURE;
    }

    printf("counter %d", counter);
    return EXIT_SUCCESS;
}