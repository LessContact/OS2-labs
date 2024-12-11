#define _GNU_SOURCE
#include "mythread_create.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <memory.h>
#include <signal.h>


void* create_stack(off_t size, int thread_num) {
    int stack_fd;
    void* stack;
    char stack_file[128];

    snprintf(stack_file, sizeof(stack_file), "stack-%d", thread_num);

    stack_fd = open(stack_file, O_RDWR | O_CREAT, 0660);
    ftruncate(stack_fd, 0);
    ftruncate(stack_fd, size);

    stack = mmap(NULL, size, /*PROT_READ|PROT_WRITE*/PROT_NONE, MAP_SHARED, stack_fd, 0);
    close(stack_fd);

    return stack;
}

int my_thread_start_up(void* arg) {
    _my_thread_struct_t* my_thread = (_my_thread_struct_t*)arg;
    my_thread->ret_val = my_thread->start_routine(my_thread->arg);
    my_thread->exited = true;

    while(!my_thread->joined)
        sleep(1);

    return SUCCESSFUL;
}

int my_thread_join(_my_thread *my_tid, void** ret_val) {
    _my_thread_struct_t** my_thread = my_tid;
    void* unmapptr = (*my_thread)->stack;
    while(!(*my_thread)->exited)
        sleep(1);

    *ret_val = (*my_thread)->ret_val;

    (*my_thread)->joined = true;
    int err = munmap(unmapptr, SIZE_STACK);
    if(err) {
        perror("munmap failed");
        fprintf(stderr, "Error code: %d\n", errno);
    }

    return SUCCESSFUL;
}

int my_thread_create(_my_thread* thread, start_routine_t start_routine, void *arg) {
    static int thread_num = 0;

    void* child_stack = create_stack(SIZE_STACK, ++thread_num);
    mprotect(child_stack + PAGE, SIZE_STACK - PAGE, PROT_READ | PROT_WRITE);
    memset(child_stack + PAGE, 0x7f, SIZE_STACK - PAGE);
    if(errno){
        printf("create_stack() failed: %s\n", strerror(errno));
        return -1;
    }
    printf("stack begins at %p, %p\n", child_stack, child_stack + PAGE);

    _my_thread_struct_t* my_thread = (child_stack + SIZE_STACK - sizeof(_my_thread_struct_t)); //pointer to thread stack
    my_thread->id = thread_num;
    my_thread->start_routine = start_routine;
    my_thread->arg = arg;
    my_thread->joined = false;
    my_thread->exited = false;
    my_thread->ret_val = NULL;
    my_thread->stack = child_stack;

    child_stack = (void*) my_thread;

    int child_pid = clone(my_thread_start_up, child_stack,
                          CLONE_VM | CLONE_FILES | CLONE_THREAD | CLONE_SIGHAND | CLONE_FS | CLONE_SYSVSEM/* |
                          CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID*/, (void *) my_thread);
    if(child_pid == -1) {
        printf("clone failed %s\n", strerror(errno));
        return EXC_CREATE_CLONE;
    }
    *thread = my_thread;

    return SUCCESSFUL;
}


void* my_thread(void *arg) {
    usleep(rand() % 1000);
    //sleep(1);
    // char *str = (char *) arg;
    int number = (int)arg;
    long long numb = 123123123213;
    printf("my thread [%d %d %d]: %i, %lld\n", getpid(), getppid(), gettid(), number, numb);
    // sleep(1000);
    return NULL;
}
