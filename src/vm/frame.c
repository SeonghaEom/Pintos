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
  if (frame)
  {
    frame_add_to_table (frame, spte);
  }
  /* Frame table is full, need to evict frame with eviction policy */
  else
  {
    while (!frame)
    {
      lock_acquire (&frame_table_lock);
      frame = frame_evict (flag);
      lock_release (&frame_table_lock);
    }
    frame_add_to_table (frame, spte);
  }
  return frame;
}

/* Free frame and remove corresponding frame table entry in frame table */
void frame_free (void *frame)
{
  /* First, find frame table entry by frame(physical frame pointer */
  struct frame_table_entry *fte = find_entry_by_frame (frame);
  /* Remove frame table entry in frame table */
  list_remove (&fte->elem);
  /* Free frame */
  free (frame);
  /* Free frame table entry */
  free (fte);
}

/* Find the victom in frame table by our POLICY */
void *frame_evict (enum palloc_flags flag)
{
  struct list_elem *e = list_begin (&frame_table);
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
