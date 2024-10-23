#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char **argv) {
    pid_t pid = fork();
    if (pid==0) {
        printf("This is the Child process and pid is: %d\n",getpid());
        exit(0);
    } else if (pid > 0) {
        printf("This is the Parent process and pid is: %d\n",getpid());
    } else {
        printf("Error while forking\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}