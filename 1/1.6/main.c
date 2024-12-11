#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mythread_create.h"
int main() {
     _my_thread my_tid;
    void* ret_val;

    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
    // while(true) {
    for (int i = 0; i < 100; i++) {
        int ret = my_thread_create(&my_tid, my_thread, i);
        if(ret == -1) {
            printf("my_thread_create error!\n");
        }

    }
    // }
    // sleep(5);
    my_thread_join(&my_tid, &ret_val);
    return EXIT_SUCCESS;
}