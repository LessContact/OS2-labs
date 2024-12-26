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
#include <immintrin.h>
#include "speen.h"

#define COUNT_ITERATION 100000

int counter = 0;
spinlock_t s;

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

void spinlock_init(spinlock_t* s){
    s->lock = STATUS_UNLOCK;
}

void spinlock_lock(spinlock_t* s) {
  	int expected = STATUS_UNLOCK;
  	while(!atomic_compare_exchange_weak(&s->lock, &expected, STATUS_LOCK)){
  		// __asm ("pause");
  		_mm_pause();
  		expected = STATUS_UNLOCK;
    }
}

void spinlock_unlock(spinlock_t *s) {
    int expected = STATUS_LOCK;
    atomic_compare_exchange_strong(&s->lock, &expected, STATUS_UNLOCK);
}

void* thread1(void* args) {
    // set_cpu(2);
    spinlock_t* s = (spinlock_t*) args;
    assert(s != NULL);

    for(int i = 0; i < COUNT_ITERATION; ++i) {
        spinlock_lock(s);
        ++counter;
        spinlock_unlock(s);
    }
    return NULL;
}

// void* thread2(void* args) {
//     set_cpu(1);
//     spinlock_t* s = (spinlock_t*) args;
//     assert(s != NULL);
//
//     for(int i = 0; i < COUNT_ITERATION; ++i) {
//         spinlock_lock(s);
//         ++counter;
//         spinlock_unlock(s);
//     }
//     return NULL;
// }

int main() {

    spinlock_init(&s);
	int n = 100;
	pthread_t threads[n];
	int thread_args[n];

	for (int i = 0; i < n; ++i) {
		thread_args[i] = i; // Номер потока
		int err = pthread_create(&threads[i], NULL, thread1, &s);
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


 //    pthread_t tid1, tid2;
 //    int err = pthread_create(&tid1, NULL, thread1, &s);
	// if (err) {
 //        printf("main: pthread_create(): thread_1 failed: %s\n", strerror(err));
 //        return EXIT_FAILURE;
 //    }
 //
 //    err = pthread_create(&tid2, NULL, thread2, &s);
	// if (err) {
	// 	printf("main: pthread_create(): thread_2 failed: %s\n", strerror(err));
 //        pthread_join(tid1, NULL);
	// 	return EXIT_FAILURE;
	// }
 //
 //    int err1 = pthread_join(tid1, NULL);
	// if (err1) {
	// 	printf("main: pthread_join() failed: %s\n", strerror(err));
	// 	return EXIT_FAILURE;
	// }
 //    err1 = pthread_join(tid2, NULL);
	// if (err1) {
	// 	printf("main: pthread_join() failed: %s\n", strerror(err));
	// 	return EXIT_FAILURE;
	// }

    printf("counter %d", counter);
    return EXIT_SUCCESS;
}