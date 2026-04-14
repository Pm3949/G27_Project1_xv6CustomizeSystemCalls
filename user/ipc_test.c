#include "kernel/types.h"
#include "user/user.h"

/* Maximum length of the IPC message buffer */
#define MSG_BUF_SIZE 64

/* Spin-loop iterations to let the child block on recv() before parent sends */
#define DELAY_ITERATIONS 1000000

int
main(void)
{
  int pid;
  char buf[MSG_BUF_SIZE];

  pid = fork();

  if (pid < 0) {
    printf("ipc_test: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // Child: silently block on recv(), then print result
    if (recv(buf) == 0) {
      printf("Child  : Received -> \"%s\"\n", buf);
    } else {
      printf("Child  : recv() failed\n");
      exit(1);
    }
    exit(0);
  }

  // Parent: give the child time to call recv()
  for (volatile int i = 0; i < DELAY_ITERATIONS; i++)
    ;

  printf("Parent : Sending message to child (PID %d)\n", pid);

  if (send(pid, "Hello from parent!") < 0) {
    printf("Parent : send() failed\n");
    exit(1);
  }

  // Wait for child to finish printing before parent prints again
  wait(0);
  printf("Parent : IPC test passed!\n");
  exit(0);
}