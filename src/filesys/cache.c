#include "filesys/cache.h"
#include <debug.h>
#include <list.h>
#include <string.h>
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "devices/block.h"

/* Structure for buffer cache */
static struct list cache;
/* List element for saved victim */
static struct list_elem *saved_victim;

static struct cache_entry *cache_alloc (block_sector_t sector);
static struct cache_entry *cache_evict (void);

/* Structure for cache entry */
struct cache_entry
{
  struct list_elem elem;        /* List elem */
  block_sector_t sector;        /* Sector number of disk location */
  bool dirty;                   /* Dirty bit */
  bool valid;                   /* Valid bit */
  int read_cnt;                 /* Read count */
  int write_cnt;                /* Write count */
  int pending_cnt;              /* Pending count for read/write requests */
  // struct lock lock;
  uint32_t *data;               /* Actual data that are cached */
};

/* Cache initialization */
void cache_init (void)
{
  list_init (&cache);
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
  free (&cache);
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
  ce->dirty = false;
  ce->valid = true;
  ce->read_cnt = 0;
  ce->write_cnt = 0;
  ce->pending_cnt = 0;

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


