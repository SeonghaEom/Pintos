#include "filesys/cache.h"
#include <debug.h>
#include <list.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "devices/block.h"

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
#define WRITE_BEHIND_PERIOD 1000000000000000000

/* Structure for cache entry */
struct cache_entry
{
  struct list_elem elem;        /* List elem pushed in cache */
  block_sector_t sector;        /* Sector number of disk location */
  bool dirty;                   /* Dirty bit */
  bool valid;                   /* Valid bit */
  int read_cnt;                 /* Read count */
  int write_cnt;                /* Write count */
  int pending_cnt;              /* Pending count for read/write requests */
  // struct lock lock;
  uint32_t *data;               /* Actual data that are cached */
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
    return ce;
  }
 }
 return NULL;
}

/* Try to read SECTOR from cache or block read if there is none */
void *cache_read (block_sector_t sector)
{
  struct cache_entry *ce = cache_find (sector);

  /* There is no corresponding block for the sector
   * and create new cache_entry */
  if (ce == NULL)
  {
    ce = cache_alloc (sector);
  }
  ce->read_cnt++;

  return ce->data;
}


/* Set dirty with sector number SECTOR */
bool cache_set_dirty (block_sector_t sector)
{
  struct cache_entry *ce = cache_find (sector);

  if (ce == NULL)
  {
    return false;
  }
  ce->dirty = true;
  return true;
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
    ce->data = (void *) malloc (BLOCK_SECTOR_SIZE);
  }
  /* Ce initialization */
  ce->sector = sector;
  ce->dirty = false;
  ce->valid = true;
  ce->read_cnt = 0;
  ce->write_cnt = 0;
  ce->pending_cnt = 0;
  /* Block read in cache data */
  block_read (fs_device, ce->sector, ce->data);
  return ce;
}

/* Evict one cache entry */
static struct cache_entry *
cache_evict (void)
{
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
    cache_read (qe->sector);
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
  //printf ("cache_write_behind\n");
  struct list_elem *e;
  
  if (list_empty (&cache))
  {
    return;
  }

  for (e = list_front (&cache); e != list_end (&cache);
       e = list_next (e))
  {
    struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
    if (ce->dirty) 
    {
      block_write (fs_device, ce->sector, ce->data);
    }
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
