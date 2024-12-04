#define _GNU_SOURCE
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
//// signal.SIGKILL и signal.SIGSTOP не могут быть заблокированы

//// Ctrl + \ получаем сигнал SIGQUIT

//// Ctrl + C получаем сигнал INT

#define COUNT_THREAD 3

#define handle_error_en(en, msg) do { errno = en; perror(msg); exit(EXIT_FAILURE); } while(0)

void signal_handler(int sig_num) {
  printf("Caught signal %d, tid %d\n", sig_num, gettid());
}

void* block_all_signal() {
    printf("started block_all_signal %d\n", gettid());
    sigset_t set_all_signal;

    int err = sigfillset(&set_all_signal);
    if(err)
        handle_error_en(err, "sigfillset");

    err = pthread_sigmask(SIG_BLOCK, &set_all_signal, NULL);
    if (err)
        handle_error_en(err, "pthread_sigmask");

    sleep(10);
    printf("block_all returned\n");
    return NULL;
}

void* non_block_int_sig() {
    printf("started non_block_int_sig %d\n", gettid());
    sigset_t set_all_sig, set_int_sig;
    int err;

    err = sigfillset(&set_all_sig);
    if (err)
        handle_error_en(err, "sigfillset");
    err = sigemptyset(&set_int_sig);
    if (err)
        handle_error_en(err, "sigemptyset");

    err = pthread_sigmask(SIG_BLOCK, &set_all_sig, NULL);
    if (err)
        handle_error_en(err, "pthread_sigmask");

    err = sigaddset(&set_int_sig, SIGINT);
    if (err)
        handle_error_en(err, "sigfillset");

    err = pthread_sigmask(SIG_UNBLOCK, &set_int_sig, &set_all_sig);
    if (err)
        handle_error_en(err, "pthread_sigmask");
    // signal(SIGINT, signal_handler);
    struct sigaction action;
    action.sa_handler = signal_handler;
    sigaction(SIGINT, &action, NULL);
    sleep(120);
    return NULL;
}

void* non_block_quit_sig() {
    printf("started non_block_quit_sig %d\n", gettid());
    sigset_t mask;
    int sig;

    int ret = sigfillset(&mask);
    if (ret)
        handle_error_en(ret, "sigfillset");

    ret = pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if (ret)
        handle_error_en(ret, "pthread_sigmask");

    sigaddset(&mask, SIGQUIT);

    // signal(SIGQUIT, signal_handler);

    ret = sigwait(&mask, &sig);
    if(ret) {
         handle_error_en(ret, "main: sigwait() failed: %s\n");
    }

    printf("receive sig %d\n", sig);
    sleep(3);
    printf("block_quit returns\n");
    return NULL;
}

int main() {
    pthread_t tid[COUNT_THREAD];
	int err;
    void* ret_val;
    printf("%d\n", getpid());

    err = pthread_create(&tid[0], NULL, block_all_signal, NULL);
    if (err) {
        fprintf(stderr, "main: block_all_signal() failed: %s\n", strerror(err));
         return 1;
    }

    sleep(3);
    pthread_kill(tid[0], SIGINT);
    printf("sent sigint to block all\n");

    err = pthread_create(&tid[1], NULL, non_block_int_sig, NULL);
    if (err) {
        fprintf(stderr, "main: non_block_int_sig() failed: %s\n", strerror(err));
        return 1;
    }
    sleep(3);
    // pthread_kill(tid[1], SIGINT);
    // getc
    printf("sent sigint to handler\n");

    err = pthread_create(&tid[2], NULL, non_block_quit_sig, NULL);
    if (err) {
        fprintf(stderr, "main: non_block_quit_sig() failed: %s\n", strerror(err));
        return 1;
    }

    sleep(3);
    // pthread_kill(tid[2], SIGQUIT);
    while(1){}
    printf("sent sigquit to sigwait\n");
    // void* ret_val;
    for(int i = 0; i < COUNT_THREAD-1; ++i) {
        err = pthread_join(tid[i], &ret_val);
        if (err) {
            fprintf(stderr, "main: pthread_join() failed %s\n", strerror(err));
            return 1;
        }
    }
    printf("return main thread\n");
    sleep(3);
    return EXIT_SUCCESS;
}