#ifndef MYTHREAD_CREATE_H
#define MYTHREAD_CREATE_H
#include <sys/types.h>

enum {
    EXC_CREATE_CLONE = 1,
    PAGE = 4096,
    SIZE_STACK = PAGE * 8,
    SUCCESSFUL = 0
};


typedef void*(*start_routine_t)(void*);

typedef struct _my_thread {
    int id;
    start_routine_t start_routine;
    void* arg;
    void* ret_val;
    void* stack;
    volatile bool joined;
    volatile bool exited;
} _my_thread_struct_t;

typedef _my_thread_struct_t* _my_thread;

void* create_stack(off_t size, int thread_num);
int my_thread_start_up(void* arg);
int my_thread_join(_my_thread *my_tid, void** ret_val);
int my_thread_create(_my_thread* thread, start_routine_t start_routine, void *arg);
void* my_thread(void *arg);

#endif //MYTHREAD_CREATE_H
