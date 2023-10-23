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

// 使用哈希桶代替链表
struct {
  struct spinlock lock[NBUCKETS];
  // struct spinlock biglock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKETS];
  struct spinlock hashlock;
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.hashlock, "bcache_glob");
  
  for (int cnt = 0; cnt < NBUCKETS; cnt++) {
    initlock(&bcache.lock[cnt], "bcache");  

    // Create linked list of buffers
    bcache.head[cnt].prev = &bcache.head[cnt];
    bcache.head[cnt].next = &bcache.head[cnt];
  }
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
      int num = (b - bcache.buf) % NBUCKETS;
      b->next = bcache.head[num].next;
      b->prev = &bcache.head[num];
      initsleeplock(&b->lock, "buffer");
      bcache.head[num].next->prev = b;
      bcache.head[num].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // acquire(&bcache.biglock);

  int start = blockno % NBUCKETS;  
  acquire(&bcache.lock[start]);

  for(b = bcache.head[start].next; b != &bcache.head[start]; b = b->next) {
    // Is the block already cached?

    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[start]);
      // release(&bcache.biglock);
      acquiresleep(&b->lock);
      return b;
    }
  }


  for (b = bcache.head[start].next; b != &bcache.head[start]; b = b->next) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[start]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  release(&bcache.lock[start]);
  acquire(&bcache.hashlock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (int i = start + 1; i < NBUCKETS + start; i++) {
    int num = i % NBUCKETS;
    acquire(&bcache.lock[num]);

    for(b = bcache.head[num].prev; b != &bcache.head[num]; b = b->prev) {
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->next->prev = b->prev;
        b->prev->next = b->next;

        acquire(&bcache.lock[start]);
        b->next = bcache.head[start].next;
        b->prev = &bcache.head[start];
        bcache.head[start].next->prev = b;
        bcache.head[start].next = b;
          
        release(&bcache.lock[num]);
        release(&bcache.lock[start]);
        release(&bcache.hashlock);
        // release(&bcache.biglock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[num]);
  }
  release(&bcache.hashlock);
  // release(&bcache.biglock);
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

  int num = b->blockno % NBUCKETS;
  acquire(&bcache.lock[num]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[num].next;
    b->prev = &bcache.head[num];
    bcache.head[num].next->prev = b;
    bcache.head[num].next = b;
  }
  release(&bcache.lock[num]);
}

void
bpin(struct buf *b) {
  int num = b->blockno % NBUCKETS;
  acquire(&bcache.lock[num]);
  b->refcnt++;
  release(&bcache.lock[num]);
}

void
bunpin(struct buf *b) {
  int num = b->blockno % NBUCKETS;
  acquire(&bcache.lock[num]);
  b->refcnt--;
  release(&bcache.lock[num]);
}


