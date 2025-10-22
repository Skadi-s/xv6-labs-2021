#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
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
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  // The system call takes three arguments
  // The first argument is the starting virtual address of the pages to be checked.
  // The second argument is the number of pages to be checked.
  // The third argument is the address of a user-space array that will be filled with the
  // accessed bits of the pages. Each bit in the array corresponds to a page, with
  // the least significant bit of the first byte representing the first page.
  uint64 start_va;
  int page_num;
  uint64 result_va;

  if(argaddr(0, &start_va) < 0)
    return -1;
  if(argint(1, &page_num) < 0)
    return -1;
  if(argaddr(2, &result_va) < 0)
    return -1;
  // check page_num validity
  if(page_num < 0 || page_num > 64)
    return -1;
  
  struct proc *p = myproc();
  if (p == 0)
    return -1;
  if (pgaccess(start_va, page_num, result_va) < 0)
    return -1;
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
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

uint64
sys_trace(void)
{
    int mask;
    if (argint(0, &mask) < 0) {
      return -1;
    }
    struct proc *p = myproc();
    acquire(&p->lock);
    p->tracemask = mask;
    release(&p->lock);
    return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr; // user pointer to struct sysinfo
  struct sysinfo info;
  struct proc *p = myproc();
  if(argaddr(0, &addr) < 0) {
    return -1;
  }
  info.freemem = kfreemem();
  info.nproc = proccount();
  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }
  return 0;
}

uint64
sys_connect(void)
{
  return 0;
}
