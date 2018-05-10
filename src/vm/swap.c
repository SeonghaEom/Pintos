#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "vm/frame.h"
#include <stdint.h>
#include <list.h>
/*
 * 2018.05.10
 * EomSungha
 * KimYoonseo
 */

/* Initialize the swap table */
void swap_table_init (void)
{
  lock_init (&swap_lock);
  list_init (&swap_table);
}

/* Swap out one frame and add to swap table */
void swap_out (struct frame_table_entry *frame_table_entry)
{
  struct swap_slot *swap_slot = malloc (sizeof (struct swap_slot));
  swap_slot->t = thread_current ();
  swap_slot->addr = frame_table_entry->spte->addr;
  swap_slot->frame_table_entry = frame_table_entry;
  
  lock_acquire (&swap_lock);
  list_push_back (&swap_table, swap_slot->elem);
  lock_release (&swap_lock);
}

/* Swap in one frame and remove from swap table */
void swap_in (struct swap_slot *swap_slot)
{
  frame_alloc(PAL_USER);
  add to frame;
  update spt;
  pagedir_modify;
  list_remove (&swap_slot->elem);
}

/* Find one victim frame from frame table */
struct frame_table_entry *
swap_find_victim ()
{

}
