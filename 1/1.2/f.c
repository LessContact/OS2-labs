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
    return NULL;
}
pthread_attr_t attr;

void signal_callback_handler(int sig_num) {
    printf("Caught signal %d\n", sig_num);
    pthread_attr_destroy(&attr);
    exit(sig_num);
}

int main() {
    pthread_t tid;
    int err;

    signal(SIGINT, signal_callback_handler);
    signal(SIGSEGV, signal_callback_handler);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int ret_val;

    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
    while(1) {
        err = pthread_create(&tid, &attr, my_thread, NULL);

        if (err) {
            fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
            return EXIT_FAILURE;
        }

        // err = pthread_join(tid, &ret_val);
        // if (err) {
        //     fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
        //     free(ret_val);
        //     return EXIT_FAILURE;
        // }
    }

    return EXIT_SUCCESS;
}