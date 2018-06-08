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

static struct cache_entry *cache_find (block_sector_t);
static struct cache_entry *cache_get_block (block_sector_t);
static void cache_read_ahead (block_sector_t sector);

static struct cache_entry *cache_alloc (block_sector_t sector);
static struct cache_entry *cache_evict (void);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Write behind period (ticks) */
#define WRITE_BEHIND_PERIOD 1000000000000

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
  //bool valid;                   /* Valid bit */
  //int read_cnt;                 /* Reader count */
  //int write_cnt;                /* Writer count */
  //int pending_cnt;              /* Pending count for read/write requests */
  //struct lock lock;             /* Read-write lock */
  //enum lock_state state;        /* State of lock */
  //struct condition r_end;       /* Signaled when read ends */
  //struct condition w_end;       /* Signaled when write ends */
  uint8_t *data;               /* Actual data that are cached */
  int index;
  int use_cnt;
  //bool touchable;               /* Can evict this cache entry */
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
  lock_init (&c_lock);
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
static struct cache_entry *
cache_find (block_sector_t sector)
{
  //printf ("cache find, sector: %d\n", sector);
  struct list_elem *e;

  for (e = list_begin (&cache); e != list_end (&cache); e = list_next (e))
  {
    struct cache_entry *ce = list_entry (e, struct cache_entry, elem);
    if (ce->sector == sector)
    {
      //ce->touchable = false;
      return ce;
    }
  }
 return NULL;
}

/* Try to get data from SECTOR in  cache or block read if there is none */
static struct cache_entry *cache_get_block (block_sector_t sector)
{
  lock_acquire (&c_lock);
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
  ce->use_cnt++;
  //ce->read_cnt++;
  lock_release (&c_lock);
  return ce;
}

/* Allocate one cache entry */
static struct cache_entry *
cache_alloc (block_sector_t sector)
{
  //printf ("cache alloc: %d\n", sector);
  struct cache_entry *ce;
  
  /* If list size is 64 evict */
  if (list_size (&cache) == 1000)
  {
    //printf ("caache size = 64, needs evict\n");
    ce = cache_evict ();
    //printf ("evicted cache entry's sector: %d\n", ce->sector);
    /* 
    if (ce == NULL)
    {
      return NULL;
    }*/
    memset (ce->data, 0xcc, BLOCK_SECTOR_SIZE);
  }
  /* Allocate new cache entry */
  else
  {
    //printf ("cache size < 64, needs allocation\n");
    ce = (struct cache_entry *) malloc (sizeof (struct cache_entry));
    //ce->touchable = false;
    //lock_init (&ce->lock);
    //cond_init (&ce->w_end);
    //cond_init (&ce->r_end);
    ce->data = (void *) malloc (BLOCK_SECTOR_SIZE);
    ce->index = list_size (&cache);
    ce->use_cnt = 0;
    //printf ("ce index: %d\n", list_size (&cache));
    list_push_back (&cache, &ce->elem);
    
  }
  /* Ce initialization */
  ce->sector = sector;
  ce->dirty = false;
  //ce->valid = true;
  //ce->read_cnt = 0;
  //ce->write_cnt = 0;
  //ce->pending_cnt = 0;
  //ce->state = LOCK_FREE;

  /* Block read in cache data */
  block_read (fs_device, ce->sector, ce->data);
  ce->dirty = true;
  
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
  //printf ("victim's index is :%d\n", victim->index);
  while (victim->sector == 0 || victim->use_cnt != 0)
  {
    //printf ("Can't evict this cache entry\n");
    e = list_next (e);
    if (e == list_end (&cache))
    {
      e = list_begin (&cache);
    }
    victim = list_entry (e, struct cache_entry, elem);
  }
  /* 
  while (!victim->touchable)
  {
    e = list_next (e);
    if (e == list_end (&cache))
    {
      e = list_begin (&cache);
    }
    victim = list_entry (e, struct cache_entry, elem);
  }*/
  //victim->touchable = false;
  
  /* Should we wrtie on disk? */
  if (victim->dirty)
  {
    /* Block write */
    block_write (fs_device, victim->sector, victim->data);
    victim->dirty = false;
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
    //printf ("read ahead function !!!!!!!!!!!!!!!!!!\n");
    struct list_elem *e = list_pop_front (&queue);
    struct q_entry *qe = list_entry (e, struct q_entry, elem);
    
    struct cache_entry *ce = cache_get_block (qe->sector);
    ce->use_cnt--; 
    lock_release (&q_lock);
  }
}

/* Cache read ahead 
 * Push read ahead request in queue */
static void cache_read_ahead (block_sector_t sector)
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
    //ce->touchable = false;
    if (ce->dirty) 
    {
      //ce->touchable = false;
      block_write (fs_device, ce->sector, ce->data);
      ce->dirty = false;
    }
    //ce->touchable = true;
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

  //lock_acquire (&ce->lock);
  /* When cache entry is free, acquire the lock */
  /* 
  if (ce->state == LOCK_FREE)
  {
    ce->read_cnt = 1;
    ce->state = LOCK_READ;
  }*/
  

  /* Wait for signal from condition w_exist 
   * when it is not free */
  /*
  while (ce->state == LOCK_WRITE)
  {
    cond_wait (&ce->w_end, &ce->lock);
  }*/
  /* Start reading */
  /* Accessing cache entry and write to buffer */
  
  if (ahead)
    cache_read_ahead (next_sector);
  memcpy (dst, ce->data + offset, size);
  
  ce->use_cnt--; 
  /* End reading 
   * When there are other threads accessing the cache entry,
   * just pass */
  /*  
  if (--ce->read_cnt == 0)
  {
    /* If there are no waiters in r_end, free the lock 
    if (list_empty(&(&ce->r_end)->waiters))
    {
      ce->state = LOCK_FREE;
      //printf ("sector%d lock release early in cache_READ_at by thread%d\n", sector, thread_current()->tid);
      lock_release (&ce->lock);
      ce->touchable = true;
      return;
    }
    /* If there are waiters in r_end, change state 
    else
    {
      ce->state = LOCK_WRITE;
      cond_signal (&ce->r_end, &ce->lock);
    }
  }*/
  //printf ("read_cnt %d, sector%d lock release late in cache_READ_at by thread%d\n", ce->read_cnt,  sector, thread_current()->tid);
  //lock_release (&ce->lock);
  return size;
}

/* Cache write from src to sector plus offset by size*/
off_t
cache_write_at (block_sector_t sector, void *src, off_t size, off_t offset)
{
  //printf ("thread%d, cache WRITE at sector: %d, size: %d, offset: %d\n", thread_current()->tid, sector, size, offset);
  struct cache_entry *ce = cache_get_block (sector);

  //lock_acquire (&ce->lock);
  //printf ("lock held in cache_WRITE_at , lock state = %d\n", ce->state);
  /* If the lock is free change the state */
  /*
  if (ce->state == LOCK_FREE)
  {
    ce->state = LOCK_WRITE;
  }
  */
  /* Wait for signal from condition r_exist 
   * if the lock is not free */
  /*
  while (ce->state == LOCK_READ)
  {
    //printf ("afadfa\n");
    cond_wait (&ce->r_end, &ce->lock);
  }
  */
  /* Accessing cache entry and write to it */
  //if (ahead)
  //  cache_read_ahead (sector + 1);
  
  memcpy (ce->data + offset, src, size);
  ce->dirty = true;
  ce->use_cnt--;
  /* If there are actually no waiters in w_end
   * free the lock */
  /*
  if (list_empty (&(&ce->w_end)->waiters))
  {
    ce->state = LOCK_FREE;
    lock_release (&ce->lock);
    ce->touchable = true;
    return;
  }*/
  /* If there are waiters in w_end, 
   * change the state to read */
  /*
  else
  {
    ce->state = LOCK_READ;
    //printf ("write end waiter empty? %s\n", list_empty (&(&ce->w_end)->waiters) ? "true" : "false");
    ce->read_cnt = list_size (&(&ce->w_end)->waiters);
    cond_broadcast (&ce->w_end, &ce->lock);
  }
  lock_release (&ce->lock);*/
  return size;
}

/* Close the Inode in given sector,
 * so need to release corresponding sectors in free map */
void cache_close_inode (block_sector_t sector)
{
  struct cache_entry *inode_ce = cache_get_block (sector);
  struct inode_disk *inode_id = inode_ce->data;
  off_t sector_remained = DIV_ROUND_UP (inode_id->length, BLOCK_SECTOR_SIZE);
  
  /* Release inode_disk */
  if (inode_ce->dirty)
  {
    printf ("a\n");
    block_write (fs_device, sector, inode_id);
  }
  //free_map_release (sector, 1);
  //list_remove (&inode_ce->elem);
  
  /* Release direct blocks */
  off_t release_cnt = sector_remained < DIRECT_BLOCK ? sector_remained : DIRECT_BLOCK;
  int i;
  for (i = 0; i < release_cnt; i++)
  {
    free_map_release (inode_id->direct[i], 1);
    sector_remained--;
  }
 
  /* Release indirect index blocks */
  if (sector_remained > 0)
  {
    release_cnt = sector_remained < INDEX_BLOCK ? sector_remained : INDEX_BLOCK;
    struct cache_entry *si_ce = cache_get_block (inode_id->indirect[0]);
    struct index_disk *si_id = si_ce->data;
    int j;
    for (j = 0; j < release_cnt; j++)
    {
      free_map_release (si_id->index[j], 1);
      sector_remained--;
    }
    
    if (si_ce->dirty)
    {
      printf ("b\n");
      block_write (fs_device, si_ce->sector, si_id);
    }
    free_map_release (inode_id->indirect[0], 1);
    list_remove (&si_ce->elem);
    free (si_ce->data);
    free (si_ce);
  }

  /* Release doubly indirect index blocks */
  if (sector_remained > 0)
  {
    struct cache_entry *di_ce = cache_get_block (inode_id->doubly_indirect[0]);
    struct index_disk *di_id = di_ce->data;
    
    int k = 0;
    while (sector_remained > 0)
    {
      /* Release doubly indirect index block's indirect index blocks */
      struct cache_entry *dii_ce = cache_get_block (di_id->index[k]);
      struct index_disk *dii_id = dii_ce->data;
      size_t di_cnt = INDEX_BLOCK > sector_remained ?
        sector_remained : INDEX_BLOCK;
      int l;
      for (l = 0; l < di_cnt; l++)
      {
        free_map_release (dii_id->index[l], 1);
        sector_remained--;
      }
      free_map_release (di_id->index[k], 1);
      k++;
      list_remove (&dii_ce->elem);
      free (dii_ce->data);
      free (di_ce);
    }
    free_map_release (inode_id->doubly_indirect[0], 1);
    list_remove (&di_ce->elem);
    free (di_ce->data);
    free (di_ce);
  }
  free_map_release (sector, 1);
  list_remove (&inode_ce->elem);
  free (inode_ce->data);
  free (inode_ce);
}

/* Returns the length, in bytes, of inode's datain given SECTOR */
off_t cache_inode_length (block_sector_t sector)
{
  struct cache_entry *inode_ce = cache_get_block (sector);
  struct inode_disk *inode_id = inode_ce->data;
  inode_ce->use_cnt--;
  return inode_id->length;
}

/* Translate byte(offset) to sector with given inode in SECTOR */
block_sector_t
cache_byte_to_sector (block_sector_t sector, off_t offset)
{
  /* Accesing inode disk */
  struct cache_entry *inode_ce = cache_get_block (sector);
  struct inode_disk *inode_id = inode_ce->data;
  
  if (offset < inode_id->length)
  {
    /* Block index in data part of inode,
     * function should return sector which has this indexed data */
    size_t sector_index = offset / BLOCK_SECTOR_SIZE;  
    /* Direct block */ 
    if (sector_index < DIRECT_BLOCK)
    {
      block_sector_t result = inode_id->direct[sector_index];
      inode_ce->use_cnt--;
      return result;
    } 
    /* Indirect block */
    else if  (sector_index < DIRECT_BLOCK + INDEX_BLOCK)
    {
      struct cache_entry *si_ce = cache_get_block (inode_id->indirect[0]);
      struct index_disk *si_id = si_ce->data;
      block_sector_t result = si_id->index[sector_index - DIRECT_BLOCK];
      inode_ce->use_cnt--;
      si_ce->use_cnt--;
      return result;
    }
    /* Dbouly indiriect block */
    else 
    {
      struct cache_entry *di_ce = cache_get_block (inode_id->doubly_indirect[0]);
      struct index_disk *di_id = di_ce->data;
      size_t di_index = (sector_index - DIRECT_BLOCK - INDEX_BLOCK) / INDEX_BLOCK;
      
      /* Acessing doubly indirect indirect block */
      struct cache_entry *dii_ce = cache_get_block (di_id->index[di_index]);
      struct index_disk *dii_id = dii_ce->data;
      
      block_sector_t result = dii_id->index[sector_index -DIRECT_BLOCK - INDEX_BLOCK -
        INDEX_BLOCK * (di_index)];
      inode_ce->use_cnt--;
      di_ce->use_cnt--;
      dii_ce->use_cnt--;
      return result;
    }
  }
  else
  {
    return -1;
  }
}   

/* Extend inode with given sector to length be a new_pos */
void cache_inode_extend (block_sector_t sector, off_t new_pos)
{
  //printf ("cache inode extend, sector: %d, new_pos: %d\n", sector, new_pos);
  
  /* First find inode cache entry and inode inode disk */
  struct cache_entry *inode_ce = cache_get_block (sector);
  struct inode_disk *inode_id = inode_ce->data;
  /* Current sector length and needed sector length */
  size_t current_length = DIV_ROUND_UP (inode_id->length, BLOCK_SECTOR_SIZE);
  size_t needed_length = DIV_ROUND_UP (new_pos, BLOCK_SECTOR_SIZE);
  //printf ("inode in sector %d current_length %d, needed_length %d\n", sector, current_length, needed_length);
  int ext_cnt = needed_length - current_length;
  //printf ("ext_cnt: %d\n", ext_cnt);
  if (ext_cnt > 0)
  {
    //printf ("I'm here!\n");
    /* Zeros */
    static char zeros[BLOCK_SECTOR_SIZE];
    /* Extend ext_cnt blocks */
    int i;
    for (i = 0; i < ext_cnt; i++)
    {
      /* Next block is direct block */
      if (current_length < DIRECT_BLOCK)
      {
        free_map_allocate (1, &inode_id->direct[current_length]);
        inode_ce->dirty = true;
        //printf ("new direct block sector: %d\n", inode_id->direct[current_length]);
        cache_write_at (inode_id->direct[current_length], zeros, BLOCK_SECTOR_SIZE, 0);
      }
      /* Next block is indirect block */
      else if (current_length < DIRECT_BLOCK + INDEX_BLOCK)
      {
        /* Need to allocate single indirect index disk block */
        if (current_length == DIRECT_BLOCK)
        {
          struct index_disk *si_id = calloc (1, sizeof *si_id);
          if (si_id != NULL)
          {
            free_map_allocate (1, &inode_id->indirect[0]);
            inode_ce->dirty = true;
            cache_write_at (inode_id->indirect[0], si_id, BLOCK_SECTOR_SIZE, 0);
          }
        }
        struct cache_entry *si_ce = cache_get_block (inode_id->indirect[0]);
        struct index_disk *si_id = si_ce->data;
        free_map_allocate (1, &si_id->index[current_length - DIRECT_BLOCK]);
        cache_write_at (si_id->index[current_length - DIRECT_BLOCK], zeros, BLOCK_SECTOR_SIZE, 0);
        si_ce->use_cnt--;
      }
      /* Next block is doubly indirect block */
      else
      {
        /* Need to allocate doubly indirect index disk block */
        if (current_length == DIRECT_BLOCK + INDEX_BLOCK)
        {
          struct index_disk *di_id = calloc (1, sizeof *di_id);
          if (di_id != NULL)
          {
            free_map_allocate (1, &inode_id->doubly_indirect[0]);
            cache_write_at (inode_id->doubly_indirect[0], di_id, BLOCK_SECTOR_SIZE, 0);
          }
        }
        /* Determine we need to allocate doubly indirect indirect index disk block */ 
        size_t index = (current_length - DIRECT_BLOCK - INDEX_BLOCK) / INDEX_BLOCK;
        size_t remainder = (current_length - DIRECT_BLOCK - INDEX_BLOCK) % INDEX_BLOCK;
        struct cache_entry *di_ce = cache_get_block (inode_id->doubly_indirect[0]);
        struct index_disk *di_id  = di_ce->data;
        
        /* Doubly indirect indirect index disk block is needed */
        if (remainder == 0)
        {
          struct index_disk *dii_id  = calloc (1, sizeof *dii_id);
          if (dii_id != NULL)
          {
            free_map_allocate (1, &di_id->index[index]);
            cache_write_at (di_id->index[index], dii_id, BLOCK_SECTOR_SIZE, 0);
          }
        }
        
        struct cache_entry *dii_ce = cache_get_block (di_id->index[index]);
        struct index_disk *dii_id = dii_ce->data;
        free_map_allocate (1, &dii_id->index[current_length - DIRECT_BLOCK - INDEX_BLOCK * index]);
        cache_write_at (dii_id->index[current_length - DIRECT_BLOCK - INDEX_BLOCK *index],
            zeros, BLOCK_SECTOR_SIZE, 0);
        di_ce->use_cnt--;
        dii_ce->use_cnt--;
      }   
      current_length++; 
    }
  }
  /* Update inode length */
  if (new_pos > inode_id->length)
  {
    //printf ("new position: %d\n", new_pos);
    inode_id->length = new_pos;
  }
}

struct inode_disk * cache_get_data (block_sector_t sector)
{
  struct cache_entry *ce = cache_get_block (sector);
  struct inode_disk *id = ce->data;
  ce->use_cnt--;
  return id;
}
