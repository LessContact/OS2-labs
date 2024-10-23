#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

// #define THREAD_COUNT 5

static int global_value = 10;

void* my_thread(void *arg) {
    const int const_local = 5;
    static int static_local = 10;
    int local = 15;

    printf("my_thread [pid: %d, ppid: %d, tid: %d, pthread_self %lu]: Hello from my_thread!\n", getpid(), getppid(), gettid(), (unsigned long)pthread_self());

    printf("my_thread: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value );

    sleep(5);

    static_local = 11;
    local = 16;
    global_value = 11;
    printf("my_thread: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value );
    printf("my_thread: local: %d, static_local: %d, const_local: %d, global_value: %d\n", local, static_local, const_local, global_value );

    return NULL;
}

void* my_thread2(void *arg) {
    const int const_local = 5;
    static int static_local = 10;
    int local = 15;

    printf("my_thread2 [pid: %d, ppid: %d, tid: %d, pthread_self %lu]: Hello from my_thread!\n", getpid(), getppid(), gettid(), (unsigned long)pthread_self());

    printf("my_thread2: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value );

    sleep(5);

    static_local = 20;
    local = 20;
    global_value = 20;
    printf("my_thread2: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value );
    printf("my_thread2: local: %d, static_local: %d, const_local: %d, global_value: %d\n", local, static_local, const_local, global_value );

    return NULL;
}

int main() {
    printf("main [pid: %d, ppid: %d, tid: %d]: Hello from main!\n", getpid(), getppid(), gettid());

    const int const_local = 5;
    static int static_local = 10;
    int local = 15;
    printf("main: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value );

    pthread_t tid1;
    pthread_t tid2;
    int err;
        err = pthread_create(&tid1, NULL, my_thread, NULL);
        if (err) {
            fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
            return EXIT_FAILURE;
        }
    err = pthread_create(&tid2, NULL, my_thread2, NULL);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    void* ret_val;
    err = pthread_join(tid1, &ret_val);
    if (err) {
        fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
        return 1;
    }

    void* ret_val2;
        err = pthread_join(tid2, &ret_val2);
        if (err) {
            fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
            return 1;
        }

    return EXIT_SUCCESS;
}

// пункт e)  watch -d -n1 cat /proc/pid/maps

// пункт f) strace ./thread