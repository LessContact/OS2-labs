#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct test {
    int value_int;
    char* ptr_char;
}test;

void* my_thread(void *arg) {
    test* test = (struct test*)arg;
    printf("value_int value: %d\n", test->value_int);
    printf("ptr_char ptr: %s\n", test->ptr_char);

    return NULL;
}

int main() {
    struct test test;
    test.value_int = 42;
    test.ptr_char = "hello";

    pthread_t tid;

    printf("hello ");
    int err = pthread_create(&tid, NULL, my_thread, (void *)&test);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }
    sleep(2);
    void* ret_val;
    err = pthread_join(tid, &ret_val);
    if (err) {
        fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
