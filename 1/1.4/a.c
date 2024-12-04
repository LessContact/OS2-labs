#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

void* my_thread(void *arg) {
    while(true) {
        printf("my thread: print text\n");
    }
    return NULL;
}

int main() {
    pthread_t tid;
    int err = pthread_create(&tid, NULL, my_thread, NULL);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    sleep(5);

    err = pthread_cancel(tid);
    if (err) {
        fprintf(stderr, "main: pthread_cancel() failed %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    void* ret_val;
    int err_join = pthread_join(tid, &ret_val);
    if (err_join != 0) {
        fprintf(stderr, "pthread_join error: %s: ", strerror(err_join));
        return EXIT_FAILURE;
    }

    if (ret_val == PTHREAD_CANCELED) {
        printf("Thread was canceled, ret_val:  %d\n", ret_val);
    }

    return EXIT_SUCCESS;
}