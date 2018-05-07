<<<<<<< HEAD
#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include <stdbool.h>
#include <stdint.h>
#include <list.h>


void frame_table_init (void);
void frame_add_to_table (void *frame, struct supple_page_entry *spte)

void frame_table_init (void)
{
  lock_init (&frame_table_lock);
  list_init (&frame_table);
}

void frame_add_to_table (void *frame, struct supple_page_entry *spte)
{
  struct frame_entry *fte = malloc (sizeof (struct frame_entry));
  fte-> frame = frame;
  fte-> spte = spte;
  fte-> thread = thread_current();
  lock_acquire (&frame_table_lock);
  list_push_back (&frame_table, &fte->elem);
  lock_release (&frame_table_lock);
}

void* frame_alloc (enum palloc_flags flag, struct supple_page_entry *spte)
{
  if ( (flags & PAL_USER ) == 0 )
  {
    return NULL;
  }
  void *frame = palloc_get_page (flag);
  if (frame)
  {
    frame_add_to_table (frame, spte);
  }
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

void frame_evict (enum palloc_flags flag)
{
  struct list_elem *e = list_begin (&frame_table);
}
=======
/*
 *  2018.05.06
 *  frame.c
 *
 *  To implement PJ3-1 Frame table
 */
>>>>>>> a251b1b068aac0fa0f69016c00c936ed4c08efc2
