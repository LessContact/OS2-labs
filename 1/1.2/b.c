#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

void* my_thread() {
    int* value = malloc(sizeof(int));
    if (value == NULL) {
        perror("my_thread: malloc failed");
        pthread_exit(NULL);
    }

    *value = 42;
    // pthread_exit(value);
    return value;
}

int main() {
    pthread_t tid;
    int err;

    err = pthread_create(&tid, NULL, my_thread, NULL);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    void* ret_val;
    err = pthread_join(tid, &ret_val);
    if (err) {
        fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    int* ret_val_int = ret_val;
    printf("ret_value %d\n", *ret_val_int);

    free(ret_val_int);

    return EXIT_SUCCESS;
}