#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
//Code snippet for implementation of getppid()
uint64
sys_getppid(void){
  struct proc *currproc = myproc();

  if(currproc->parent == 0){
    return 0;
  }

  return currproc->parent->pid;
}


extern struct proc proc[NPROC];

// Send Message
uint64
sys_send(void)
{
  int dest_pid;
  uint64 msg_addr;
  struct proc *p;
  char buf[64];

  argint(0, &dest_pid);
  argaddr(1, &msg_addr);

  if(copyin(myproc()->pagetable, buf, msg_addr, 64) < 0)
    return -1;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock); // Acquire process lock to check PID and State
    if(p->pid == dest_pid && p->state != UNUSED){
      
      acquire(&p->msg_lock); // Acquire mailbox lock
      if(p->has_msg){
        release(&p->msg_lock);
        release(&p->lock);
        return -1; 
      }

      memmove(p->mailbox, buf, 64);
      p->has_msg = 1;
      
      // CRITICAL CHANGE START 
      release(&p->msg_lock); // Release mailbox lock
      release(&p->lock);     // Release process lock BEFORE wakeup
      
      wakeup(&p->mailbox);   // Now it is safe to wake up the receiver
      // CRITICAL CHANGE END
      
      return 0;
    }
    release(&p->lock);
  }
  return -1; 
}

//Receive Message
uint64
sys_recv(void)
{
  uint64 msg_addr;
  struct proc *p = myproc();

  // Fetch argument
  argaddr(0, &msg_addr);

  acquire(&p->msg_lock);
  while(p->has_msg == 0){
    // If the process is being killed, stop waiting
    if(p->killed){
      release(&p->msg_lock);
      return -1;
    }
    sleep(&p->mailbox, &p->msg_lock);
  }

  if(copyout(p->pagetable, msg_addr, p->mailbox, 64) < 0){
    release(&p->msg_lock);
    return -1;
  }

  p->has_msg = 0; 
  release(&p->msg_lock);
  
  return 0;
}

// Send a signal to a specific PID
uint64
sys_sigsend(void)
{
  int pid;
  struct proc *p;

  argint(0, &pid);

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED){
      p->signal_pending = 1; // Set the signal flag
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Check if the current process has a signal
uint64
sys_sigcheck(void)
{
  struct proc *p = myproc();
  int pending;

  acquire(&p->lock);
  pending = p->signal_pending;
  p->signal_pending = 0; // Clear the signal once read
  release(&p->lock);

  return pending;
}

struct {
  struct spinlock lock;
  int state[10];  // 0 = available, 1 = held
} user_lock_mgr;

// 1. Acquire User Lock
uint64
sys_ulock_acquire(void)
{
  int id;
  argint(0, &id);

  if(id < 0 || id >= 10) return -1;

  acquire(&user_lock_mgr.lock);
  
  // While the lock is held by someone else, sleep
  while(user_lock_mgr.state[id] == 1){
    sleep(&user_lock_mgr.state[id], &user_lock_mgr.lock);
  }

  user_lock_mgr.state[id] = 1; // Take the lock
  release(&user_lock_mgr.lock);
  
  return 0;
}

// 2. Release User Lock
uint64
sys_ulock_release(void)
{
  int id;
  argint(0, &id);

  if(id < 0 || id >= 10) return -1;

  acquire(&user_lock_mgr.lock);
  user_lock_mgr.state[id] = 0; // Free the lock
  
  wakeup(&user_lock_mgr.state[id]); // Wake up anyone waiting for THIS lock
  
  release(&user_lock_mgr.lock);
  return 0;
}