#include "kernel/types.h"
#include "user/user.h"

int main() {
    int pid = fork();
    char buf[64];

    if(pid == 0) {
        // Child: Wait for a message
        printf("Child: Waiting for message...\n");
        if(recv(buf) == 0) {
            printf("Child received: %s\n", buf);
        } else {
            printf("Child: Error receiving message.\n");
        }
        exit(0);
    } else {
        // Parent: Simple delay loop instead of sleep()
        // This gives the child time to start up and call recv()
        for(volatile int i = 0; i < 1000000; i++) {
            // Just spinning to create a delay
        }

        printf("Parent: Sending message to PID %d\n", pid);
        if(send(pid, "Hello from your parent!") < 0) {
            printf("Parent: Send failed (maybe child wasn't ready?)\n");
        }
        
        wait(0);
        exit(0);
    }
}