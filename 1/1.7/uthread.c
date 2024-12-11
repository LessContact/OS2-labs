#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ucontext.h>

#include "uthread.h"

int uthread_startup(void *arg, void *uthread_manager) {
    uthread_struct_t *uthread = (uthread_t)arg;
    uthread_manager_t *manager = (uthread_manager_t*)uthread_manager;
    void *retval = NULL;

    printf("uthread_startup: starting a thread func for the thread %d\n", uthread->uthread_id);
    uthread->thread_func(uthread->arg, manager);
    
    uthread->retval = retval;
    uthread->finished = 1;

    return 0;
}

void *create_stack(off_t size) {
    void* stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    
    return stack;
}

void uthread_sheduler(uthread_manager_t *uthread_manager) {
    if(uthread_manager->uthread_count <= 1) return;
    
    int err;
    ucontext_t *cur_ctx, *next_ctx;
    
    cur_ctx = &(uthread_manager->uthreads[uthread_manager->uthread_cur]->uctx);

    do{
      uthread_manager->uthread_cur = (uthread_manager->uthread_cur + 1) % uthread_manager->uthread_count;
      next_ctx = &(uthread_manager->uthreads[uthread_manager->uthread_cur]->uctx);
    } while(uthread_manager->uthreads[uthread_manager->uthread_cur] == NULL || 
          uthread_manager->uthreads[uthread_manager->uthread_cur]->finished);
    
    if (cur_ctx == next_ctx) return;

    // cur_ctx = &(uthread_manager->uthreads[uthread_manager->uthread_cur]->uctx);
    // uthread_manager->uthread_cur = (uthread_manager->uthread_cur + 1) % uthread_manager->uthread_count;
    // next_ctx = &(uthread_manager->uthreads[uthread_manager->uthread_cur]->uctx);

    err = swapcontext(cur_ctx, next_ctx);
    if (err == -1) {
        perror("sheduler: swapcontext() failed:");
        exit(1);
    }
}

int uthread_create(uthread_t *uthread, uthread_manager_t *uthread_manager, void (*thread_func), void *arg) {
    uthread_t new_uthread;
    int err;

    void* stack = create_stack(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "create_stack() failed\n");
        return -1;
    }
    mprotect(stack, PAGE, PROT_NONE);
    new_uthread = (uthread_struct_t*) (stack + STACK_SIZE - sizeof(uthread_struct_t));

    err = getcontext(&new_uthread->uctx);
    if (err == -1) {
        perror("uthread_create: getcontext() failed:");
        return -1;
    }
    
    new_uthread->uctx.uc_stack.ss_sp = stack;
    new_uthread->uctx.uc_stack.ss_size = STACK_SIZE - sizeof(uthread_t);
    new_uthread->uctx.uc_link = &uthread_manager->uthreads[0]->uctx;

    makecontext(&new_uthread->uctx, (void (*)(void)) uthread_startup, 2, new_uthread, uthread_manager);
    
    new_uthread->uthread_id = uthread_manager->uthread_count + 1;
    new_uthread->thread_func = thread_func;
    new_uthread->arg = arg;
    new_uthread->finished = 0;

    uthread_manager->uthreads[uthread_manager->uthread_count++] = new_uthread;
    printf("uthread_create: creating thread %d\n", new_uthread->uthread_id);

    *uthread = new_uthread;

    return 0;
}

int thread_is_finished(uthread_t utid) {
    return utid->finished;
}

void uthread_manager_shutdown(uthread_manager_t** uthread_manager)
{
  for(unsigned i = 0; i < (*uthread_manager)->uthread_count; ++i)
  {
      munmap((*uthread_manager)->uthreads[i]->uctx.uc_stack.ss_sp, STACK_SIZE);
  }

  free(*uthread_manager);
  *uthread_manager = NULL;
}

void uthread_manager_init(uthread_manager_t** uthread_manager, uthread_t* main_thread)
{
  *uthread_manager = malloc(sizeof(uthread_manager_t));
  (*uthread_manager)->uthread_count = 1;
  (*uthread_manager)->uthreads[0] = main_thread;
  (*uthread_manager)->uthread_cur = 0;
}