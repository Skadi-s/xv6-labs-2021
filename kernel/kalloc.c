// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int ref_count[(PHYSTOP - KERNBASE)/PGSIZE];
} pageref_count;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pageref_count.lock, "pageref_count");
  memset(pageref_count.ref_count, 0, sizeof(pageref_count.ref_count));
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&pageref_count.lock);
  int index = ((uint64)pa - KERNBASE)/PGSIZE;
  if(pageref_count.ref_count[index] > 1){
    pageref_count.ref_count[index]--;
    release(&pageref_count.lock);
    return;
  }
  if(pageref_count.ref_count[index] == 1){
    pageref_count.ref_count[index] = 0;
  } else if(pageref_count.ref_count[index] == 0) {
    // ok
  } else {
    panic("kfree: reference count is negative");
  }
  release(&pageref_count.lock);
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    acquire(&pageref_count.lock);
    int index = ((uint64)r - KERNBASE)/PGSIZE;
    if(pageref_count.ref_count[index] != 0)
      panic("kalloc: reference count is not zero");
    pageref_count.ref_count[index] = 1; // first reference
    release(&pageref_count.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Return the reference count for the page of physical memory
// pointed at by pa. Returns -1 if pa is not a valid page address.
int
krefget(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("krefget");
  acquire(&pageref_count.lock);
  int index = ((uint64)pa - KERNBASE)/PGSIZE;
  if(pageref_count.ref_count[index] <= 0){
    release(&pageref_count.lock);
    return -1;
  }
  int ref_count = pageref_count.ref_count[index];
  release(&pageref_count.lock);
  return ref_count;
}

// Increment the reference count for the page of physical memory
// pointed at by pa.
void
krefinc(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("krefinc");
  acquire(&pageref_count.lock);
  int index = ((uint64)pa - KERNBASE)/PGSIZE;
  if(pageref_count.ref_count[index] <= 0)
    panic("krefinc: reference count is not positive");
  pageref_count.ref_count[index]++;
  release(&pageref_count.lock);
}

// Decrement the reference count for the page of physical memory
// pointed at by pa. If the reference count reaches zero, free the page.
void
krefdec(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("krefdec");
  acquire(&pageref_count.lock);
  int index = ((uint64)pa - KERNBASE)/PGSIZE;
  if(pageref_count.ref_count[index] > 1){
    pageref_count.ref_count[index]--;
    release(&pageref_count.lock);
    return;
  }
  if(pageref_count.ref_count[index] == 1){
    pageref_count.ref_count[index] = 0;
    release(&pageref_count.lock);
    kfree(pa);
    return;
  }
  release(&pageref_count.lock);
  panic("krefdec: reference count is already zero");
}
