#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

// pthread_self стал randomый так как освобоэдается

void* my_thread() {
    printf("my_thread [%d %d %d %lu]: Hello from my_thread!\n", getpid(), getppid(), gettid(), (unsigned long)pthread_self());
    // sleep(15);
    getc(stdin);
    // return NULL;
}
pthread_attr_t attr;

int main() {
    pthread_t tid;
    int err;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int ret_val;
    int threads = 0;
    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
    while(1) {
        err = pthread_create(&tid, &attr, my_thread, NULL);

        if (err) {
            fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
            return EXIT_FAILURE;
        }
        threads++;
        printf("%d\n", threads);
    }

    return EXIT_SUCCESS;
}