#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    int value_int;
    char* ptr_char;
}Node;

void* my_thread(void *arg) {
    struct Node* node = (struct Node*)arg;

    printf("value_int value: %d\n", node->value_int);
    printf("ptr_char ptr: %s\n", node->ptr_char);

    return NULL;
}

int main() {
    struct Node node;
    node.value_int = 42;
    node.ptr_char = "hello";

    pthread_t tid;

    printf("hello ");
    int err = pthread_create(&tid, NULL, my_thread, (void *)&node);
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
