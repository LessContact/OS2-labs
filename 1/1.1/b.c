#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define THREAD_COUNT 5


static int global_value = 10;

void *mythread(void *arg) {
    const int const_local = 5;
    static int static_local = 10;
    int local = 15;

    printf("my_thread [pid: %d, ppid: %d, tid: %d, pthread_self %lu]: Hello from my_thread!\n", getpid(), getppid(), gettid(), (unsigned long)pthread_self());
    printf("my_thread: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value );
    return NULL;
}

int main() {
    pthread_t tid[THREAD_COUNT];
    int err;

    printf("main [pid: %d, ppid: %d, tid: %d]: Hello from main!\n", getpid(), getppid(), gettid());

    for(int i = 0; i < THREAD_COUNT; ++i) {
        err = pthread_create(&tid[i], NULL, mythread, NULL);
        // printf("%lu %d\n", tid[i], i);
        if (err) {
            fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
            return 1;
        }
    }

    sleep(20);
    void* ret_val;
    for(int i = 0; i < THREAD_COUNT; ++i) {
        err = pthread_join(tid[i], &ret_val);
        if (err) {
            fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
            return 1;
        }
    }
    return 0;
}
