#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/frame.h"
#include <stdbool.h>
#include <stdint.h>
#include <list.h>
/*
 *  2018.05.05
 *  KimYoonseo
 *  EomSungha
 */
static struct frame_table_entry *find_entry_by_frame (void *frame);

/* Initialize the frame table */
void frame_table_init (void)
{
  lock_init (&frame_table_lock);
  list_init (&frame_table);
}

/* Add one frame to the frame table */
void frame_add_to_table (void *frame, struct spte *spte)
{
  /* Initialize frame table entry */
  struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));
  fte->frame = frame;
  fte->spte = spte;
  fte->thread = thread_current();
  fte->rbit = 0;
  
  /* Push it in frame table */
  lock_acquire (&frame_table_lock);
  list_push_back (&frame_table, &fte->elem);
  lock_release (&frame_table_lock);
}

/* Allocate each frame by palloc_get_page */
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
    frame_add_to_table (frame, spte);
    pagedir_set_page (thread_current ()->pagedir, spte->addr, frame, spte->writable);
  }
  /* Frame table is full, need to evict frame with eviction policy */
  else
  {
    lock_acquire (&frame_table_lock);
    frame_evict (flag);
    lock_release (&frame_table_lock);
    frame_add_to_table (frame, spte);
  }
}
  return frame;
}

/* Free frame and remove corresponding frame table entry in frame table */
void frame_free (void *frame)
{
  /* First, find frame table entry by frame(physical frame pointer */
  struct frame_table_entry *fte = find_entry_by_frame (frame);
  /* Remove frame table entry in frame table */
  lock_acquire (&frame_table_lock);
  list_remove (&fte->elem);
  lock_acquire (&frame_table_lock);
  /* Free frame */
  free (frame);
  /* Free frame table entry */
  free (fte);
}

/* Find the victom in frame table by second chance algorithm
 * and swapt out */
void frame_evict (enum palloc_flags flag)
{
  /* Find the victim in frame table by second chance algorithm */
  struct frame_table_entry *victim;
  struct list_elem *i;

  for ( i = list_begin (&frame_table); i != list_end (&frame_table);
        i = list_next (i))
  {
    struct frame_table_entry *fte = list_entry (i, struct frame_table_entry, elem);
    if (fte->rbit && pagedir_is_dirty (thread_current ()->pagedir, fte->spte->addr))
    {
      victim = fte;
      fte->rbit = 0;
      break;
    }
    else if (pagedir_is_dirty (thread_current ()->pagedir, fte->spte->addr))
    {
      fte->rbit = 1;
    }
  }
  /* Case where all frames have 0 reference bit, FIFO */
  if (victim == NULL)
  {
    victim = list_front (&frame_table);
  }
  /* Swap out */
  victim->spte->swap_index = swap_out (victim->frame);
  /* Free frame */
  pagedir_clear_page (thread_current ()->pagedir, fte->spte->addr);
  frame_free (victim->frame);
}

/* Find frame table entry in frame table by frame */
static struct frame_table_entry *
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
