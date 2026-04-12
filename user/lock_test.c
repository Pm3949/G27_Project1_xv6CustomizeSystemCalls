#include "kernel/types.h"
#include "user/user.h"

int main() {
    int lock_id = 1; // We choose lock #1
    
    printf("Testing Locks...\n");

    if(fork() == 0) {
        ulock_acquire(lock_id);
        printf("Child has the lock. Doing heavy work...\n");
        for(volatile int i = 0; i < 20000000; i++);
        printf("Child is releasing lock.\n");
        ulock_release(lock_id);
        exit(0);
    } else {
        // Parent waits a tiny bit then tries to grab the same lock
        for(volatile int i = 0; i < 1000000; i++); 
        
        printf("Parent trying to acquire lock...\n");
        ulock_acquire(lock_id);
        printf("Parent finally got the lock!\n");
        ulock_release(lock_id);
        
        wait(0);
        exit(0);
    }
}