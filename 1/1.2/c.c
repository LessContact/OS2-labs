#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

void* my_thread() {
    char* value = malloc(sizeof(char)*12);
    if (value == NULL) {
        perror("my_thread: malloc failed");
        pthread_exit(NULL);
    }

    strcpy(value, "hello world");
    pthread_exit(value);
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
        free(ret_val);
        return EXIT_FAILURE;
    }

    printf("ret_val_hello_world = %s\n", (char*)ret_val);
    free(ret_val);
    return EXIT_SUCCESS;
}