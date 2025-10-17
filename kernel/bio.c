// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define HASH(x) ((x) % NBUCKET)

struct bucket {
  struct spinlock lock;
  struct buf head; // Dummy head for linked list
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct bucket buckets[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  static char name[NBUCKET][16];
  for (int i = 0; i < NBUCKET; i++) {
    snprintf(name[i], sizeof(name[i]), "bcache_bucket%d", i);
    initlock(&bcache.buckets[i].lock, name[i]);
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // Initialize each hash bucket's linked list of buffers
  for (int i = 0; i < NBUF; i++) {
    b = &bcache.buf[i];
    b->dev = 0;
    b->blockno = 0;
    b->valid = 0;
    b->refcnt = 0;
    b->timestamp = 0;
    initsleeplock(&b->lock, "buffer");

    int idx = HASH(i);
    // Insert buffer into the corresponding hash bucket's linked list
    b->next = bcache.buckets[idx].head.next;
    b->prev = &bcache.buckets[idx].head;
    bcache.buckets[idx].head.next->prev = b;
    bcache.buckets[idx].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);
  int idx = HASH(blockno);
  acquire(&bcache.buckets[idx].lock);

  // Is the block already cached?
  for (b = bcache.buckets[idx].head.next; b != &bcache.buckets[idx].head; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.buckets[idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.buckets[idx].head.prev; b != &bcache.buckets[idx].head; b = b->prev) {
    if(b->refcnt == 0) {
      // Remove b from its current position in the list
      b->next->prev = b->prev;
      b->prev->next = b->next;
      // Insert b at the head of the list
      b->next = bcache.buckets[idx].head.next;
      b->prev = &bcache.buckets[idx].head;
      bcache.buckets[idx].head.next->prev = b;
      bcache.buckets[idx].head.next = b;

      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.buckets[idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[idx].lock);

  // If no buffers are free in this bucket
  // Borrow a buffer from other buckets (simple approach)
  for (int i = 0; i < NBUCKET; i++) {
    if (i == idx) continue; // Skip the original bucket
    if (holding(&bcache.buckets[i].lock)) continue; // Skip if the lock is held
    acquire(&bcache.buckets[i].lock);
    for (b = bcache.buckets[i].head.prev; b != &bcache.buckets[i].head; b = b->prev) {
      if(b->refcnt == 0) {
        // Remove b from its current position in the list
        b->next->prev = b->prev;
        b->prev->next = b->next;
        // Insert b at the head of the original bucket's list
        b->next = bcache.buckets[idx].head.next;
        b->prev = &bcache.buckets[idx].head;
        bcache.buckets[idx].head.next->prev = b;
        bcache.buckets[idx].head.next = b;
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.buckets[i].lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // acquire(&bcache.lock);
  int idx = HASH(b->blockno);
  acquire(&bcache.buckets[idx].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[idx].head.next;
    b->prev = &bcache.buckets[idx].head;
    bcache.buckets[idx].head.next->prev = b;
    bcache.buckets[idx].head.next = b;
  }

  release(&bcache.buckets[idx].lock);
}

void
bpin(struct buf *b) {
  int idx = HASH(b->blockno);
  acquire(&bcache.buckets[idx].lock);
  b->refcnt++;
  release(&bcache.buckets[idx].lock);
}

void
bunpin(struct buf *b) {
  int idx = HASH(b->blockno);
  acquire(&bcache.buckets[idx].lock);
  b->refcnt--;
  release(&bcache.buckets[idx].lock);
}


