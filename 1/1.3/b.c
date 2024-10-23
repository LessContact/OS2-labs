#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


typedef struct Node {
    int value_int;
    char* ptr_char;
}Node;

void* my_thread(void *arg) {
    struct Node* node = (struct Node*)arg;
    int a = 5;
    printf("value_int value: %d\n", node->value_int);
    printf("ptr_char ptr: %s\n", node->ptr_char);

    sleep(7);
    printf("value stack %d", a);

    pthread_exit(NULL);

}
// cansletion pointer
int main() {
    Node* node;
    node->value_int = 42;
    node->ptr_char = "hello";
    pthread_attr_t attr;

    pthread_attr_init(&attr);

    int err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if(err) {
        fprintf(stderr, "main: pthread_attr_setdetachstate() failed %s\n", strerror(err));
        pthread_attr_destroy(&attr);
        return EXIT_FAILURE;
    }

    pthread_t thread;
    err = pthread_create(&thread, &attr, my_thread, (void *)node);
    if (err) {
        fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    sleep(3);
    pthread_attr_destroy(&attr);

    //free(node);
    return EXIT_SUCCESS;
}

//// gdb