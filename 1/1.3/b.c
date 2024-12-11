#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

typedef struct test {
    int value_int;
    char* ptr_char;
}test;

void* my_thread(void *arg) {
    test* test1 = arg;
    printf("value_int value: %d\n", test1->value_int);
    printf("ptr_char ptr: %s\n", test1->ptr_char);
// getc(stdin);
    //free ();
    return NULL;
}

void* my_thread2(void *arg) {
    test* test1 = arg;
    printf("value_int value: %d\n", test1->value_int);
    printf("ptr_char ptr: %s\n", test1->ptr_char);
    // getc(stdin);
    free (test1);
    return NULL;
}

int main() {
    test* test1 = malloc(sizeof(test));
    test1->value_int = 42;
    test1->ptr_char = "hello";

    pthread_attr_t attr;

    test test2;
    test2.value_int = 142;
    test2.ptr_char = "hello from 2";

    pthread_attr_init(&attr);

    int err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if(err) {
        fprintf(stderr, "main: pthread_attr_setdetachstate() failed %s\n", strerror(err));
        pthread_attr_destroy(&attr);
        free(test1);
        return EXIT_FAILURE;
    }

    pthread_t thread;
    err = pthread_create(&thread, &attr, my_thread2, test1);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        free(test1);
        return EXIT_FAILURE;
    }
    pthread_t thread2;
    err = pthread_create(&thread2, &attr, my_thread, &test2);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        free(test1);
        return EXIT_FAILURE;
    }
    pthread_attr_destroy(&attr);

    // sleep(2);

    // free(test1);
    // _exit(1);
    // syscall(SYS_exit, 0);
    // sys_exit(237);
    pthread_exit(NULL);
    // return EXIT_SUCCESS;
}
