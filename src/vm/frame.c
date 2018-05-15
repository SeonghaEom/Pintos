#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <stdbool.h>
#include <stdint.h>
#include <list.h>
/*
 *  2018.05.05
 *  KimYoonseo
 *  EomSungha
 */
/* Saved victim for second change algorithm */
static struct list_elem *saved_victim;
/* Initialize the frame table */
void frame_table_init (void)
{
  lock_init (&ft_lock);
  list_init (&ft);
}

/* Add one frame to the frame table */
void frame_add_to_table (void *frame, struct spte *spte)
{
  /* Initialize frame table entry */
  struct fte *fte = malloc (sizeof (struct fte));
  fte->frame = frame;
  fte->spte = spte;
  fte->thread = thread_current();
  
  /* We will lock the ft lock outside when we call this function */
  list_push_back (&ft, &fte->elem);
  /* Add mapping to current thread's page table */
  //pagedir_set_page (thread_current ()->pagedir, spte->addr, frame, spte->writable);
}

/* Allocate one frame by palloc_get_page */
void *frame_alloc (enum palloc_flags flag, struct spte *spte)
{
  /* Check PAL_USER flag */
  if ((flag & PAL_USER) == 0)
  {
    return NULL;
  }
  void *frame = palloc_get_page (flag);
  /* Unused frame exist */
  if (frame != NULL)
  {
    printf ("unused frame exist....\n");
    lock_acquire (&ft_lock);
    frame_add_to_table (frame, spte);
    lock_release (&ft_lock);
    
    printf("frame table size %d\n", list_size (&ft));
    return frame;
  }
  /* Frame table is full, need to evict frame with eviction policy */
  else
  {
    printf ("unused frame doesn't exist, need eviction.... \n");
    lock_acquire (&ft_lock);
    void *evicted_frame = frame_evict (flag);
    //printf ("eviction success\n");
    // New spte is mapped with evited frame
    struct fte *fte = find_entry_by_frame (evicted_frame);
    printf ("evicted fte's spte addr : %x\n", fte->spte->addr); 
    fte->spte = spte;
    fte->thread = thread_current ();
    //frame_add_to_table (evicted_frame, spte);
    /* Add mapping to current thread's page table */
    //pagedir_set_page (thread_current ()->pagedir, spte->addr, frame, spte->writable);
    lock_release (&ft_lock);
    printf("frame table size %d\n", list_size(&frame_table));
    return evicted_frame;
  }
}

/* Free frame and remove corresponding frame table entry in frame table */
void frame_free (void *frame)
{
  /* First, find frame table entry by frame(physical frame pointer */
  struct frame_table_entry *fte = find_entry_by_frame (frame);
  /* Remove frame table entry in frame table */
  //printf("lock holder : %s\n", (&frame_table_lock)->holder->name);
  //lock_acquire (&frame_table_lock);
  list_remove (&fte->elem);
  //lock_release (&frame_table_lock);
  /* Free frame */
  //printf("frame %x\n", frame);
  //printf ("frame table entry %x\n", fte);
  
  free (frame);
  /* Free frame table entry */
  free (fte);
}

/* Find the victom in frame table by second chance algorithm
 * and swapt out and return the victim's frame pointer to allocate new */
void *frame_evict (enum palloc_flags flag)
{
  printf ("frame evict!\n");
  /* Find the victim in frame table by second chance algorithm */
  struct list_elem *i;
  struct fte *victim;

  /* Resume saved victim to i */
  if (saved_victim == NULL)
  {
    i = list_begin (&ft);
  }
  else
  {
    i = saved_victim;
  }
  
  /* Search for referenced file to evict */
  while (true) 
  {
    struct fte *fte = list_entry (i, struct fte, elem);
    if (!pagedir_is_accessed (fte->thread->pagedir, fte->spte->addr))
    {
      pagedir_set_accessed (fte->thread->pagedir, fte->spte->addr, true);
      victim = fte;
      saved_victim = list_next (i);
      break;
    }
    else 
    {
      pagedir_set_accessed (fte->thread->pagedir, fte->spte->addr, false);
    }
    /* Iteration */
    i = list_next (i);
    if (i == list_end (&ft))
    {
      printf ("circular search 하는중, evited frame 찾다가 처음으로 넘어감\n");
      i = list_begin (&ft);
    }
  }
  /* Case where all frames have 0 reference bit, FIFO
  if (victim == NULL)
  {
    printf ("victim is NULL, should use FIFO now\n");
    victim = list_entry (list_front (&frame_table), struct frame_table_entry, elem);
    saved_victim = list_next (i);
  }
  */
  /* TODO TODO*/ 
  /* Should we need swap out? */
  if (pagedir_is_dirty (thread_current ()->pagedir, victim->spte->addr))
  {
    /* Write in SW */
    printf ("Evicted frame changed, should save in swap disk\n");
    victim->spte->swap_index = swap_out (victim->frame);
    pagedir_clear_page (thread_current ()->pagedir, victim->spte->addr);
    pagedir_set_dirty (thread_current ()->pagedir, victim->spte->addr, false);
    victim->spte->location = LOC_SW;
    //free (victim->spte);
  }
  
  else
  {
    printf ("Evicted frame does not changed\n");
    pagedir_clear_page (thread_current ()->pagedir, victim->spte->addr);
    victim->spte->location = LOC_FS;
    /* Write in FS */
    /*
    file_write_at (victim->spte->file, victim->frame, PGSIZE, victim->spte->ofs);
    pagedir_clear_page (thread_current ()->pagedir, victim->spte->addr);
    free (victim->spte);
    */
  }
  //frame_free (victim->frame);
  printf ("frame evict fin\n");
  return victim->frame;
}

/* Find frame table entry in frame table by frame */
struct frame_table_entry *
find_entry_by_frame (void *frame)
{
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
  {
    struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
    if (fte->frame == frame)
    {
      return fte;
    }
  }
  return NULL;
}
