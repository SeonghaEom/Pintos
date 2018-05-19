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
static void frame_add_to_table (void *frame, struct spte *spte);
static void *frame_evict (enum palloc_flags flag);

/* Initialize the frame table */
void frame_table_init (void)
{
  lock_init (&frame_lock);
  list_init (&ft);
}

/* Add one frame to the frame table */
static void frame_add_to_table (void *frame, struct spte *spte)
{
  /* Initialize frame table entry */
  struct fte *fte = malloc (sizeof (struct fte));
  fte->frame = frame;
  fte->spte = spte;
  fte->thread = thread_current();
  
  lock_acquire (&frame_lock);
  spte->location = LOC_PM;
  list_push_back (&ft, &fte->elem);
  lock_release (&frame_lock);
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
    //printf ("unused frame exists\n");
    frame_add_to_table (frame, spte);
    //printf("frame table size %d\n", list_size (&ft));
    return frame;
  }
  /* Frame table is full, need to evict frame with eviction policy */
  else
  {
    //printf ("unused frame doesn't exist, need eviction.... \n");
    lock_acquire (&frame_lock);
    void *evicted_frame = frame_evict (flag);
    // New spte is mapped with evited frame
    struct fte *fte = find_entry_by_frame (evicted_frame);
    //printf ("evicted fte's spte addr : %x\n", fte->spte->addr); 
    fte->spte = spte;
    fte->thread = thread_current ();
    //frame_add_to_table (evicted_frame, spte);
    /* Add mapping to current thread's page table */
    spte->location = LOC_PM;
    lock_release (&frame_lock);
    //printf("frame table size %d\n", list_size(&ft));
    return evicted_frame;
  }
}

/* Free frame and remove corresponding frame table entry in frame table */
void frame_free (void *frame)
{
  lock_acquire (&frame_lock);
  /* First, find frame table entry by frame(physical frame pointer */
  struct fte *fte = find_entry_by_frame (frame);
  /* Remove frame table entry in frame table */
  list_remove (&fte->elem);
  
  free (frame);
  /* Free frame table entry */
  free (fte);
  lock_release (&frame_lock);
}

/* Find the victom in frame table by second chance algorithm
 * and swapt out and return the victim's frame pointer to allocate new */
static void *frame_evict (enum palloc_flags flag)
{
  //printf ("frame evict : thread%d\n", thread_current ()->tid);
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
  
  victim = list_entry(i, struct fte, elem);
  /* Can we swap out? */
  while (!victim->spte->touchable)
  {
    i = list_next (i);
    if  (i == list_end (&ft))
    {
      i = list_begin (&ft);  
    }
    victim = list_entry (i, struct fte, elem);
  }
  /* Saving for next victim */
  i = list_next(i);
  if (i == list_end (&ft))
  {
    i = list_begin (&ft);
  }
  saved_victim = i;
  
  /* Write in SW */
  victim->spte->swap_index = swap_out (victim);
  //victim->spte->location = LOC_SW;
  pagedir_clear_page (victim->thread->pagedir, victim->spte->addr);
  
  /*else
  {
    //PANIC("Frame evict to FS %p, %p\n", victim->spte->addr, victim->frame);
    printf ("Evicted frame does not changed\n");
    pagedir_clear_page (thread_current ()->pagedir, victim->spte->addr);
    victim->spte->location = LOC_FS;
    /* Write in FS 
    file_write_at (victim->spte->file, victim->frame, PGSIZE, victim->spte->ofs);
    pagedir_clear_page (thread_current ()->pagedir, victim->spte->addr);
    free (victim->spte);
    
  }
  frame_free (victim->frame);
  printf ("frame evict fin\n");*/
  
  return victim->frame;
}

/* Find frame table entry in frame table by frame */
struct fte *
find_entry_by_frame (void *frame)
{
  struct list_elem *e;

  for (e = list_begin (&ft); e != list_end (&ft);
       e = list_next (e))
  {
    struct fte *fte = list_entry (e, struct fte, elem);
    if (fte->frame == frame)
    {
      return fte;
    }
  }
  return NULL;
}
