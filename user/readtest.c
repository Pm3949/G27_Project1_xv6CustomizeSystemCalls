#include "kernel/types.h"
#include "user/user.h"

int main() {
    printf("Initial read count: %d\n", readCount());

    char buf[10];
    // Perform a few read operations from stdin (fd 0)
    // Note: read(0, buf, 0) still triggers the syscall even if it reads 0 bytes
    read(0, buf, 0);
    read(0, buf, 0);
    read(0, buf, 0);

    printf("Read count after 3 reads: %d\n", readCount());

    exit(0);
}