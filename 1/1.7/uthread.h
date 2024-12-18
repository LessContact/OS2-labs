#ifndef UTHREAD_H
#define UTHREAD_H

#include <ucontext.h>

#define PAGE 4096
#define STACK_SIZE (PAGE * 8)
#define MAX_THREADS 10

typedef struct {
    int              uthread_id;
    void             (*thread_func)(void*, void*);
    void             *arg;
    void             *retval;

    volatile int     is_finished;
    ucontext_t       uctx;
} uthread_struct_t;

typedef uthread_struct_t *uthread_t;

typedef struct {
    uthread_t uthreads[MAX_THREADS];
    int uthread_count;
    int uthread_cur;
} uthread_manager_t;


int uthread_create(uthread_t *uthread, uthread_manager_t *uthread_manager, void (*thread_func), void *arg);
void uschedule(uthread_manager_t *uthread_manager);
int is_thread_finished(uthread_t utid);
void uthread_manager_init(uthread_manager_t** uthread_manager, uthread_t* main_thread);
void uthread_manager_destroy(uthread_manager_t** uthread_manager);

#endif