#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void free_handler(void* arg) {
    printf("my thread handler, free() hello world\n");
    char* ptr = (char*) arg;
    free(ptr);
}

void* my_thread(void *arg) {
    char* arr_hello_world = malloc(sizeof(char) * 13);
    if (arr_hello_world == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        pthread_exit(NULL);
    }
    strcpy(arr_hello_world, "hello world\n");

    pthread_cleanup_push(free_handler, arr_hello_world);

    while(true) {
        printf("%s", arr_hello_world);
    }

    pthread_cleanup_pop(1);
    return NULL;
}

int main() {
    pthread_t tid;

    int err = pthread_create(&tid, NULL, my_thread, NULL);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    usleep(100);
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
        printf("Thread was canceled\nretval:  %d\n", (int) ret_val);
    }

    return EXIT_SUCCESS;
}