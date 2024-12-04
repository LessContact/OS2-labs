#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

int flag = 0;
void cancel() {
    flag = 1;
}

void testcancel() {
    if (flag) {
        pthread_exit(0);
    }
}

void* my_thread(void *arg) {
    int old_state;
    int count_inter = 0;
    // int err = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_state);
    printf("old state %d\n", old_state);
    // if (err) {
    //     fprintf(stderr, "main: pthread_setcancelstate() failed %s\n", strerror(err));
    //     pthread_exit(NULL);
    // }//todo: почему асинхронный cancel не по умолчанпию


    while(true) {
        ++count_inter;
        // pthread_testcancel();
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

    sleep(1);

    err = pthread_cancel(tid);
    if (err) {
        fprintf(stderr, "main: pthread_cancel() failed %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    void* ret_val;
    int err_join = pthread_join(tid, &ret_val);
    if (err_join) {
        fprintf(stderr, "pthread_join error: %s: ", strerror(err_join));
        return EXIT_FAILURE;
    }

    if (ret_val == PTHREAD_CANCELED) {
        printf("Thread was canceled, ret_val:  %d\n", (int) ret_val);
    }

    return EXIT_SUCCESS;
}