#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define THREAD_COUNT 5

void *mythread(void *arg) {
    printf("mythread [pid: %d, ppid: %d, tid: %d, pthread_self %lu]: Hello from mythread!\n", getpid(), getppid(), gettid(), pthread_self());
    return NULL;
}

int main() {
    pthread_t tid[THREAD_COUNT];
    int err;

    printf("main [pid: %d, ppid: %d, tid: %d]: Hello from main!\n", getpid(), getppid(), gettid());

    for(int i = 0; i < THREAD_COUNT; ++i) {
        err = pthread_create(&tid[i], NULL, mythread, NULL);
        if (err) {
            fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
            return 1;
        }
    }
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
