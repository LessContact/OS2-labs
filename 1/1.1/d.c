#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <threads.h>

static int global_value = 10;

void* my_thread(void *arg) {
    const int const_local = 5;
    static int static_local = 10;
    int local = 15;

    printf("my_thread [pid: %d, ppid: %d, tid: %d, pthread_self %lu]: Hello from my_thread!\n", getpid(), getppid(), gettid(), (unsigned long)pthread_self());

    printf("my_thread: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value);

    sleep(5);

    static_local += 1;
    local += 1;
    global_value += 1;

    printf("my_thread: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value);
    printf("my_thread: local: %d, static_local: %d, const_local: %d, global_value: %d\n", local, static_local, const_local, global_value);


    // getc(stdin);

    return NULL;
}

int main() {
    printf("main [pid: %d, ppid: %d, tid: %d]: Hello from main!\n", getpid(), getppid(), gettid());

    const int const_local = 5;
    static int static_local = 10;
    int local = 15;
    printf("main: address local: %p, static_local: %p, const_local: %p, global_value: %p\n", &local, &static_local, &const_local, &global_value);

    pthread_t threads[5];
    int err;

    for (int i = 0; i < 5; i++) {
        err = pthread_create(&threads[i], NULL, my_thread, NULL);
        if (err) {
            fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
            return 1;
        }
    }

    sleep(15);


    // for (int i = 0; i < 5; i++) {
    //     void* ret_val;
    //     err = pthread_cancel(threads[i]);
    //     if (err) {
    //         fprintf(stderr, "main: pthread_cancel() failed %s\n", strerror(err));
    //         return 1;
    //     }
    // }



    for (int i = 0; i < 5; i++) {
        void* ret_val;
        err = pthread_join(threads[i], &ret_val);
        if (err) {
            fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
            return 1;
        }
    }


    return EXIT_SUCCESS;
}

// пункт e)  watch -d -n1 cat /proc/pid/maps

// пункт f) strace ./thread