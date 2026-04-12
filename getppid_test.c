#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
    int pid = fork();

    if(pid < 0){
        printf(1, "Fork failed\n");
        exit();
    }

    if(pid == 0){
        // In the child process
        printf(1, "Child: My PID is %d, My Parent PID is %d\n", getpid(), getppid());
    } else {
        // In the parent process
        wait();
        printf(1, "Parent: My PID is %d\n", getpid());
    }

    exit();
}