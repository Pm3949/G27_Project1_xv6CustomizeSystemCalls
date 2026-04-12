#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"



int
main(void)
{
    int pid = fork();

    if(pid < 0){
        printf("Fork failed\n");
        exit(1);
    }


    if(pid == 0){
        // In the child process
        // Note: getpid() and getppid() are called here
        printf("Child: My PID is %d, My Parent PID is %d\n", getpid(), getppid());
    } else {
        // In the parent process
        // wait(0) handles the required pointer argument
        wait(0);
        printf("Parent: My PID is %d\n", getpid());
    }


    exit(0); // Exit requires an integer status

}