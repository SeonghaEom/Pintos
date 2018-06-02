#include "filesys/cache.h"
#include <stdio.h>
#include <debug.h>
#include <list.h>
#include <string.h>
#include <round.h>
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"

/* Structure for buffer cache */
static struct list cache;
static struct list queue;
static struct lock q_lock;
static struct condition q_not_empty;
/* List element for saved victim */
static struct list_elem *saved_victim;

static struct cache_entry *cache_alloc (block_sector_t sector);
static struct cache_entry *cache_evict (void);

/* Write behind period (ticks) */
#define WRITE_BEHIND_PERIOD 10000000000000

/* Lock state */
enum lock_state
{
  LOCK_FREE,
  LOCK_READ,
  LOCK_WRITE,
};

/* Structure for cache entry */
struct cache_entry
{
  struct list_elem elem;        /* List elem pushed in cache */
  block_sector_t sector;        /* Sector number of disk location */
  bool dirty;                   /* Dirty bit */
  bool valid;                   /* Valid bit */
  int read_cnt;                 /* Reader count */
  int write_cnt;                /* Writer count */
  int pending_cnt;              /* Pending count for read/write requests */
  struct lock lock;             /* Read-write lock */
  enum lock_state state;        /* State of lock */
  struct condition r_end;       /* Signaled when read ends */
  struct condition w_end;       /* Signaled when write ends */
  uint8_t *data;               /* Actual data that are cached */
  bool touchable;               /* Can evict this cache entry */
};

struct q_entry
{
  struct list_elem elem;        /* List elem pushed in queue */
  block_sector_t sector;        /* Sector number of block location */
};

/* Cache initialization */
void cache_init (void)
{
  list_init (&cache);
  list_init (&queue);
  lock_init (&q_lock);
  cond_init (&q_not_empty);
}

/* Cache destruction */
void cache_destroy (void)
{
  struct list_elem *e;
  struct cache_entry *ce;
  while (!list_empty (&cache))
  {
    e = list_pop_front (&cache);
    ce = list_entry (e, struct cache_entry, elem);
    free (ce->data);
    free (ce);
  }
}

/* Find cache entry pointer with block sector number and return NULL if none*/
struct cache_entry *
cache_find (block_sector_t sector)
{
 struct list_elem *e;

 for (e = list_begin (&cache); e != list_end (&cache); e = list_next (e))
 {
  struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
  if (ce->sector == sector)
  {
    ce->touchable = false;
    return ce;
  }
 }
 return NULL;
}

/* Try to get data from SECTOR in  cache or block read if there is none */
struct cache_entry *cache_get_block (block_sector_t sector)
{
  //printf ("thread%d, cache_get_block sector : %d\n", thread_current ()->tid, sector);
  struct cache_entry *ce = cache_find (sector);

  /* There is no corresponding block for the sector
   * and create new cache_entry */
  if (ce == NULL)
  {
    //printf ("cache miss! sector %d\n", sector);
    //printf ("cache entry is NULL for sector : %d\n", sector);
    ce = cache_alloc (sector);
  }
  else 
  {
    //printf ("cache hit! sector %d\n", sector);
    //printf ("cache entry is not NULL for sector : %d\n", sector);
  }
  //ce->read_cnt++;

  return ce;
}

/* Allocate one cache entry */
static struct cache_entry *
cache_alloc (block_sector_t sector)
{
  struct cache_entry *ce;
  
  /* If list size is 64 evict */
  if (list_size (&cache) == 64)
  {
    ce = cache_evict ();
    memset (ce->data, 0xcc, BLOCK_SECTOR_SIZE);
  }
  /* Allocate new cache entry */
  else
  {
    ce = (struct cache_entry *) malloc (sizeof (struct cache_entry));
    ce->touchable = false;
    lock_init (&ce->lock);
    cond_init (&ce->w_end);
    cond_init (&ce->r_end);
    ce->data = (void *) malloc (BLOCK_SECTOR_SIZE);
    list_push_back (&cache, &ce->elem);
  }
  /* Ce initialization */
  ce->sector = sector;
  ce->dirty = false;
  ce->valid = true;
  ce->read_cnt = 0;
  ce->write_cnt = 0;
  ce->pending_cnt = 0;
  ce->state = LOCK_FREE;

  /* Block read in cache data */
  block_read (fs_device, ce->sector, ce->data);
  
  return ce;
}

/* Evict one cache entry */
static struct cache_entry *
cache_evict (void)
{
  //printf ("cache evict!\n");
  /* FIFO */
  struct list_elem *e;
  struct cache_entry *victim;
  
  /* Find the victim */
  if (saved_victim == NULL)
  {
    e = list_begin (&cache);
  }
  else 
  {
    e = saved_victim;
  }

  victim = list_entry (e, struct cache_entry, elem);
  
  while (!victim->touchable)
  {
    e = list_next (e);
    if (e == list_end (&cache))
    {
      e = list_begin (&cache);
    }
    victim = list_entry (e, struct cache_entry, elem);
  }
  victim->touchable = false;
  /* Should we wrtie on disk? */
  if (victim->dirty)
  {
    /* Block write */
    block_write (fs_device, victim->sector, victim->data);
  }
  /* Update saved_victim */
  e = list_next (e);
  if (e == list_end (&cache))
  {
    e = list_begin (&cache);
  }
  saved_victim = e;

  return victim;
}

/* Read aheader function
 * This function will used in kernel thread read_aheader */
void read_aheader_func (void)
{
  while (true)
  {
    lock_acquire (&q_lock);
    while (list_empty (&queue))
    { 
      cond_wait (&q_not_empty, &q_lock);
    }
    struct list_elem *e = list_pop_front (&queue);
    struct q_entry *qe = list_entry (e, struct q_entry, elem);
    
    struct cache_entry *ce = cache_get_block (qe->sector);
    ce->touchable = true;
    
    lock_release (&q_lock);
  }
}

/* Cache read ahead 
 * Push read ahead request in queue */
void cache_read_ahead (block_sector_t sector)
{
  lock_acquire (&q_lock);
  struct q_entry *qe = (struct q_entry *) malloc (sizeof (struct q_entry));
  qe->sector = sector;
  list_push_back (&queue, &qe->elem);
  cond_signal (&q_not_empty, &q_lock);
  lock_release (&q_lock);
}

/* Flusher function 
 * This function will used in kernel thread flusher */
void flusher_func (void)
{
  while (true)
  {
    timer_sleep (WRITE_BEHIND_PERIOD);
    cache_write_behind ();
  }
}

/* Cache write behind
 * Flush all dirty cache slots */
void cache_write_behind (void)
{
  //printf ("----------------cache_write_behind--------------\n");
  struct list_elem *e;
  
  if (list_empty (&cache))
  {
    return;
  }

  for (e = list_front (&cache); e != list_end (&cache);
       e = list_next (e))
  {
    struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
    /* For sync with flusher */
    ce->touchable = false;
    if (ce->dirty) 
    {
      block_write (fs_device, ce->sector, ce->data);
      ce->dirty = false;
    }
    ce->touchable = true;
  }
}

/* Queue destruction */
void q_destroy (void)
{
  struct list_elem *e;
  struct q_entry *qe;
  while (!list_empty (&queue))
  {
    e = list_pop_front (&queue);
    qe = list_entry (e, struct q_entry, elem);
    free (qe);
  }
}

/* Cache read at from sector plus offset to dst by size */
off_t
cache_read_at (void *dst, block_sector_t sector, off_t size, off_t offset,
    block_sector_t next_sector, bool ahead)
{
  //printf ("thread%d, cache READ, sector: %d, size: %d, offset: %d\n", thread_current()->tid, sector, size, offset);
  struct cache_entry *ce = cache_get_block (sector); 
  
  lock_acquire (&ce->lock);
  /* When cache entry is free, acquire the lock */
  if (ce->state == LOCK_FREE)
  {
    ce->read_cnt = 1;
    ce->state = LOCK_READ;
  }
  /* Wait for signal from condition w_exist 
   * when it is not free */
  while (ce->state == LOCK_WRITE)
  {
    cond_wait (&ce->w_end, &ce->lock);
  }
  /* Start reading */
  /* Accessing cache entry and write to buffer */
  
  if (ahead)
    cache_read_ahead (next_sector);
  memcpy (dst, ce->data + offset, size);
  
  /* End reading 
   * When there are other threads accessing the cache entry,
   * just pass */
  if (--ce->read_cnt == 0)
  {
    /* If there are no waiters in r_end, free the lock */
    if (list_empty(&(&ce->r_end)->waiters))
    {
      ce->state = LOCK_FREE;
      //printf ("sector%d lock release early in cache_READ_at by thread%d\n", sector, thread_current()->tid);
      lock_release (&ce->lock);
      ce->touchable = true;
      return;
    }
    /* If there are waiters in r_end, change state */
    else
    {
      ce->state = LOCK_WRITE;
      cond_signal (&ce->r_end, &ce->lock);
    }
  }
  //printf ("read_cnt %d, sector%d lock release late in cache_READ_at by thread%d\n", ce->read_cnt,  sector, thread_current()->tid);
  lock_release (&ce->lock);
}

/* Cache write from src to sector plus offset by size*/
off_t
cache_write_at (block_sector_t sector, void *src, off_t size, off_t offset)
{
  //printf ("thread%d, cache WRITE at sector: %d\n", thread_current()->tid, sector);
  struct cache_entry *ce = cache_get_block (sector);

  lock_acquire (&ce->lock);
  //printf ("lock held in cache_WRITE_at , lock state = %d\n", ce->state);
  /* If the lock is free change the state */
  if (ce->state == LOCK_FREE)
  {
    ce->state = LOCK_WRITE;
  }
  /* Wait for signal from condition r_exist 
   * if the lock is not free */
  while (ce->state == LOCK_READ)
  {
    //printf ("afadfa\n");
    cond_wait (&ce->r_end, &ce->lock);
  }
  /* Accessing cache entry and write to it */
  //if (ahead)
  //  cache_read_ahead (sector + 1);
  
  memcpy (ce->data + offset, src, size);
  ce->dirty = true;
  /* If there are actually no waiters in w_end
   * free the lock */
  if (list_empty (&(&ce->w_end)->waiters))
  {
    ce->state = LOCK_FREE;
    lock_release (&ce->lock);
    ce->touchable = true;
    return;
  }
  /* If there are waiters in w_end, 
   * change the state to read */
  else
  {
    ce->state = LOCK_READ;
    //printf ("write end waiter empty? %s\n", list_empty (&(&ce->w_end)->waiters) ? "true" : "false");
    ce->read_cnt = list_size (&(&ce->w_end)->waiters);
    cond_broadcast (&ce->w_end, &ce->lock);
  }
  lock_release (&ce->lock);
}

/* Inode in given sector is closed, so need to release corresponding free map */
void cache_close_inode (block_sector_t sector)
{
  struct cache_entry *ce = cache_get_block (sector);
  struct inode_disk *id = ce->data;
  off_t sector_remained = DIV_ROUND_UP (id->length, BLOCK_SECTOR_SIZE);
  
  /* Release inode_disk */
  free_map_release (sector, 1);
  /* Release direct index blocks */
  off_t release_cnt = sector_remained < DIRECT_BLOCK ? sector_remained : DIRECT_BLOCK;
  int i;
  for (i=0; i != release_cnt; i++)
  {
    free_map_release (id->direct[i], 1);
    sector_remained--;
  }
  /* Release indirect index blocks */
  if (sector_remained > 0)
  {
    release_cnt = sector_remained < INDEX_BLOCK ? sector_remained : INDEX_BLOCK;
    struct cache_entry *ce2 = cache_get_block (id->indirect[0]);
    struct index_disk *indirect_index = ce2->data;
    for (i=0; i != release_cnt; i++)
    {
      free_map_release (indirect_index->index[i], 1);
      sector_remained--;
    }
    free_map_release (id->indirect[0],1);
    list_remove (&ce2->elem);
  }
  /* Release doubly indirect index blocks */
  if (sector_remained > 0)
  {
    struct cache_entry *ce3 = cache_get_block (id->doubly_indirect[0]);
    struct index_disk *doubly_indirect_index = ce3->data;
    int k = 0;
    while (sector_remained > 0)
    {
      /* Release doubly indirect index block's indirect index blocks */
      struct cache_entry *ce4 = cache_get_block (doubly_indirect_index->index[k]);
      struct index_disk *doubly_indirect_indirect_index = ce4->data;
      size_t doubly_indirect_cnt = INDEX_BLOCK > sector_remained ?
        sector_remained : INDEX_BLOCK;
      int j;
      for (j = 0; j < doubly_indirect_cnt; j++)
      {
        free_map_release (doubly_indirect_indirect_index->index[j], 1);
        sector_remained--;
      }
      free_map_release (doubly_indirect_index->index[k], 1);
      k++;
      list_remove (&ce4->elem);     
    }
    free_map_release (id->doubly_indirect[0], 1);
    list_remove (&ce3->elem);
  }
  list_remove (&ce->elem);
}

/* Returns the length, in bytes, of inode's datain given SECTOR */
off_t cache_inode_length (block_sector_t sector)
{
  struct cache_entry *ce = cache_get_block (sector);
  struct inode_disk *id = ce->data;
  return id->length;
}

/* byte to sector with given inode in SECTOR */
block_sector_t 
cache_byte_to_sector (block_sector_t sector, off_t offset)
{
  /* Accesing inode disk */
  struct cache_entry *ce = cache_get_block (sector);
  struct inode_disk *id = ce->data;
  if (offset < id->length)
  {
    size_t sector_index = offset/BLOCK_SECTOR_SIZE;  
    //printf ("sector_index :%d\n", sector_index);
    if (sector_index < DIRECT_BLOCK)
    {
      //printf ("id->direct[sector_index] = %d\n",id->direct[sector_index]);
      return id->direct[sector_index];
    } 
    else if  (sector_index < DIRECT_BLOCK + INDEX_BLOCK)
    {
      /* Accessing indirect block */
      struct cache_entry *ce2 = cache_get_block (id->indirect[0]);
      struct index_disk *id2 = ce2->data;
      //printf ("id2->index[sector_index - DIRECT_BLOCK] = %d\n", id2->index[sector_index-DIRECT_BLOCK]);
      return id2->index[sector_index - DIRECT_BLOCK];
    }
    else 
    {
      /* Accessing doubly indirect block */
      struct cache_entry *ce3 = cache_get_block (id->doubly_indirect[0]);
      struct index_disk *id3 = ce3->data;
      size_t doubly_indirect_index = 
        DIV_ROUND_UP (sector_index - DIRECT_BLOCK - INDEX_BLOCK, INDEX_BLOCK);
    
      /* Acessing doubly indirect indirect block */
      struct cache_entry *ce4 = cache_get_block (id3->index[doubly_indirect_index]);
      struct index_disk *id4 = ce4->data;
      return id4->index[sector_index - DIRECT_BLOCK -
        INDEX_BLOCK * doubly_indirect_index];
    }
  }
  else
  {
    return -1;
  }
}   


