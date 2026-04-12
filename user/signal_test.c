#include "kernel/types.h"
#include "user/user.h"

int main() {
    int pid = fork();

    if(pid == 0) {
        // Child: Poll until signal_pending is 1
        printf("Child: Waiting for signal...\n");
        while(sigcheck() == 0) {
            // Just wait
        }
        printf("Child: Signal received! Exiting...\n");
        exit(0);
    } else {
        // Parent: Wait a bit, then send signal
        for(volatile int i = 0; i < 5000000; i++); 

        printf("Parent: Sending signal to child (PID %d)\n", pid);
        sigsend(pid);
        
        wait(0);
        printf("Parent: Child has finished.\n");
        exit(0);
    }
}