# xv6-riscv: Custom System Call Extensions

This project is the result of five developers digging into the xv6-riscv kernel to implement features that, frankly, make the OS feel a lot more "real." xv6 is a fantastic teaching tool because it’s minimal, but that minimalism means you’re often missing the basic plumbing required for complex applications—things like process hierarchy, inter-process communication, or even basic system-wide telemetry.

We’ve divided the work into five core modules, each handled by a different team member. Below is the technical breakdown of how we modified the kernel, the logic we followed, and how to verify the work.

---

## Member 1: Process Management (`getppid`)

### 1. Core Logic
The goal was simple: allow a child process to identify its creator. In xv6, every `struct proc` maintains a pointer to its parent. The core logic involves safely accessing this pointer and returning the `pid` associated with that parent process.

### 2. Files Modified
* **`kernel/syscall.h`**: Defined the syscall number `SYS_getppid`.
* **`kernel/syscall.c`**: Added the function pointer to the syscall dispatch table.
* **`kernel/sysproc.c`**: Implemented the kernel-side handler `sys_getppid()`.
* **`user/user.h`**: Added the user-level prototype.
* **`user/usys.pl`**: Added the entry to generate the assembly wrapper.

### 3. Kernel Analysis & Understanding
To implement this, I had to live in **`kernel/proc.h`**. Understanding the lifecycle of a process in xv6 is key. When `fork()` is called, the kernel sets `np->parent = p`. This means the relationship is already baked into the process structure. My main takeaway was how xv6 handles the "Init" process—since it has no parent, I had to ensure the code wouldn't try to dereference a null pointer if `getppid` was called by the first process.

### 4. Implementation Details
The implementation of `sys_getppid()` is straightforward but requires using `myproc()` to get the current process's context. 
```c
uint64 sys_getppid(void) {
  struct proc *p = myproc();
  if (p->parent)
    return p->parent->pid;
  return 0; // Or handle init accordingly
}
```
It’s an $O(1)$ operation that provides essential process-tree information.

### 5. Testing and Execution
Verified via `getppid_test`. It forks a child, and both the parent and child print their PIDs. The child then calls `getppid()` to verify the number matches the parent’s printed PID.

---

## Member 2: IPC Communication (`send` and `recv`)

### 1. Core Logic
I implemented a 64-byte mailbox system for synchronous message passing. The logic follows a "Direct Addressing" model: a sender specifies a destination PID, and the receiver blocks until a message is available in its own mailbox.

### 2. Files Modified
* **`kernel/proc.h`**: Added `char mailbox[64]`, `int has_msg`, and `struct spinlock msg_lock` to the `proc` struct.
* **`kernel/proc.c`**: Modified `allocproc` to initialize the mailbox lock and clear the message flag.
* **`kernel/sysproc.c`**: Wrote the logic for `sys_send` and `sys_recv`.

### 3. Kernel Analysis & Understanding
The biggest hurdle here was understanding **Virtual Memory boundaries**. I analyzed `kernel/vm.c` to see how the kernel moves data between user and kernel space. I realized I couldn't just copy a pointer; I had to use `copyin()` to pull the message from the sender's user space into a kernel buffer, and `copyout()` to push it into the receiver’s user space. I also studied `sleep()` and `wakeup()` to handle the blocking nature of the receiver.

### 4. Implementation Details
* **`send(pid, msg)`**: Scans the process table for the target PID. If found, it acquires that process's `msg_lock`, copies the 64-byte payload into the target's `mailbox` field, sets `has_msg = 1`, and calls `wakeup()`.
* **`recv(buf)`**: Checks the `has_msg` flag. If it's zero, the process sleeps on its own mailbox address. Once woken, it copies the data to the provided user buffer and clears the flag.

### 5. Testing and Execution
Verified via `ipc_test`. The test involves a parent sending a string to a child. The child remains in a blocked state until the parent finishes its "work" and sends the message, proving the sleep/wakeup mechanism is working.

---

## Member 3: Mutex Locks (`ulock_acquire` & `ulock_release`)

### 1. Core Logic
Standard xv6 doesn't provide a way for user programs to sleep while waiting for a lock—they usually have to spin. I implemented kernel-level mutexes that allow a process to yield the CPU while waiting for a resource.

### 2. Files Modified
* **`kernel/proc.h`**: Added a lock state table to the kernel.
* **`kernel/sysproc.c`**: Implemented `sys_ulock_acquire` and `sys_ulock_release`.
* **`kernel/defs.h`**: Added global definitions for the lock management functions.

### 3. Kernel Analysis & Understanding
I analyzed **`kernel/spinlock.c`** and **`kernel/sleeplock.c`**. I learned that while spinlocks are great for the kernel (where tasks are short), they are terrible for user space because they prevent the scheduler from doing its job effectively. By moving the "waiting" logic into the kernel, I could use the scheduler to put a process in the `SLEEPING` state and wake it only when the lock holder releases it.

### 4. Implementation Details
I maintained an array of lock structures in the kernel. Each has a `locked` integer.
* **Acquire**: If `locked` is 1, the process calls `sleep()` using the lock's address as the "wait channel."
* **Release**: Sets `locked` to 0 and calls `wakeup()` on that specific channel. This prevents "thundering herd" problems where every sleeping process wakes up for no reason.

### 5. Testing and Execution
Tested with `lock_test`. Multiple processes attempt to increment a shared value. Without the syscalls, the final value is inconsistent due to race conditions. With `ulock_acquire`, the result is consistently correct.

---

## Member 4: Signals (`sigsend` & `sigcheck`)

### 1. Core Logic
Implementing a full POSIX-style signal handler is an enormous task, so I implemented a lightweight "Signal Pending" system. It allows processes to send notifications to each other that can be polled and cleared.

### 2. Files Modified
* **`kernel/proc.h`**: Added an `int signal_pending` flag to the process structure.
* **`kernel/sysproc.c`**: Implemented `sys_sigsend` and `sys_sigcheck`.

### 3. Kernel Analysis & Understanding
I spent time in **`kernel/trap.c`** looking at how xv6 handles external interrupts. I originally wanted signals to be asynchronous, but after analyzing how the kernel manages the program counter during a trap, I decided a "synchronous polling" model was much safer for the stability of the system. I learned how to iterate through the `proc` array safely using the `pid` to find a specific target.

### 4. Implementation Details
* **`sigsend(pid)`**: Finds the process in the process table and sets its `signal_pending` flag to 1. 
* **`sigcheck()`**: A process calls this to see if any signals are waiting. It returns the current state of the flag and resets it to 0 immediately to acknowledge receipt.

### 5. Testing and Execution
Verified with `signal_test`. A child process runs a long-standing loop. The parent sends a signal after a delay. The child, checking `sigcheck()` in its loop, detects the signal and exits gracefully.

---

## Member 5: System Monitoring (`readCount`)

### 1. Core Logic
I implemented a global telemetry feature to track system-wide activity. Specifically, I created a counter that tracks every successful call to the `read()` system call made by any process since boot.

### 2. Files Modified
* **`kernel/syscall.c`**: Added a global `read_count` variable and modified the `syscall()` dispatcher.
* **`kernel/sysproc.c`**: Implemented `sys_readCount` to expose the counter to user space.

### 3. Kernel Analysis & Understanding
The "aha!" moment for me was analyzing the **System Call Dispatcher** in `kernel/syscall.c`. I realized that every single system call, regardless of its type, eventually funnels through the `syscall(void)` function. This is the perfect bottleneck for monitoring. I also had to account for multi-core concurrency; if two CPUs handle a `read()` at once, the counter needs protection.

### 4. Implementation Details
I added a global integer in `syscall.c`. Inside the `syscall()` function, I added a simple check:
```c
void syscall(void) {
  int num = p->trapframe->a7;
  if (num == SYS_read) {
      read_count++;
  }
  // ... rest of the dispatcher
}
```
The `readCount()` syscall simply returns the current value of this global variable.

### 5. Testing and Execution
Tested with `readtest`. I ran the command once to get a baseline, performed several `cat` and `ls` commands (which trigger `read`), and ran `readtest` again to verify the counter accurately reflected the activity.

---

## Technical Summary Table

| Syscall | Purpose | Primary Location |
| :--- | :--- | :--- |
| `getppid` | Process Identity | `kernel/sysproc.c` |
| `send`/`recv` | Blocking IPC | `kernel/proc.h` |
| `ulock_*` | Synchronization | `kernel/sysproc.c` |
| `sig*` | Event Signaling | `kernel/proc.h` |
| `readCount` | System Metrics | `kernel/syscall.c` |

## Build Instructions
1.  Run `make clean` to ensure a fresh environment.
2.  Run `make qemu` to compile and launch the kernel.
3.  Once the shell appears, execute any of the test files (e.g., `ipc_test`, `lock_test`, `getppid_test`, `signal_test`, `read_test`).
