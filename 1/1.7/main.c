#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "uthread.h"

#define THREADS 5

// test scheme: [t1 - main thread, t2, t3 - returns, t4, t5]

void threadFunc(void *arg, uthread_manager_t *uthread_manager) {
    int threadIndex = (int)arg;

    for (int i = 0; i < 3; ++i) {
        printf("thread[%d]: hi from iteration = %d\n", threadIndex, i);
        sleep(1);

        if(threadIndex == 2) {
            printf("thread[%d]: terminate requested..\n", threadIndex);
            return;
        }

        uschedule(uthread_manager);
    }
}

int main() {
    assert(THREADS < MAX_THREADS);
    int err;

    uthread_t mythreads[THREADS];
    uthread_t main_thread;

    uthread_manager_t* uthread_manager = NULL;
    uthread_manager_init(&uthread_manager, &main_thread);

    mythreads[0] = main_thread;
    printf("main [%d]\n", getpid());

    for(int i = 1; i< THREADS; ++i)
    {
        err = uthread_create(&mythreads[i], uthread_manager, threadFunc, i + 1);
        if (err == -1) fprintf(stderr, "uthread_create() failed\n%d", i);
    }

    while (1) {
        int count = 0;
        for (int i = 1; i < THREADS; ++i) {
            if (is_thread_finished(mythreads[i])) ++count;
        }
        if (count == THREADS - 1) break;

        uschedule(uthread_manager);
    }

    uthread_manager_destroy(&uthread_manager);

    return 0;
}
