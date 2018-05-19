#include "threads/synch.h"
#include "devices/block.h"
#include <bitmap.h>
#include "vm/swap.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/syscall.h"
#include "threads/thread.h"
#include <stdio.h>

/* 2018.05.10
 * EomSungha
 * KimYoonseo
 */

/* Initialize the swap table */
void swap_table_init (void)
{
  /* Find swap disk */
  swap_block = block_get_role (BLOCK_SWAP);
  /* No device has been assigned as swap block */
  if (swap_block == NULL)
  {
    printf ("No device has been assigned as swap block\n");
    return;
  } 
  size_t slot_max = block_size(swap_block);
  printf ("slot_max: %d\n", slot_max);
  /* Initialize swap bitmap */  
  swap_bm = bitmap_create (slot_max);
  if (swap_bm == NULL)
  {
    printf ("Bitmap creation fails\n");
    return;
  }
  lock_init (&swap_lock);
  printf ("Swap table successfully initialized\n");
  return;
}

/* Swap out
 * Swap out the physical frame and return SWAP_INDEX
 */
size_t swap_out (struct fte *fte)
{
  //printf ("swap out : thread%d\n", thread_current ()->tid);
  size_t swap_index;
  void *frame = fte->frame;
  
  /* Find empty slot */
  lock_acquire (&swap_lock);
  swap_index = bitmap_scan_and_flip (swap_bm, 0, 8, false);
  //lock_release (&swap_lock);
  
  //printf ("swap outed index %d\n", (int)swap_index);
  if (swap_index != BITMAP_ERROR)
  {
    /* Write in swap disk */
    //lock_acquire (&swap_lock);
    int i;
    void *buffer = frame;
    for (i = 0; i < 8; i ++)
    {
      //lock_acquire (&swap_lock);
      block_write (swap_block, swap_index + i, buffer);
      //lock_release (&swap_lock);
      buffer += BLOCK_SECTOR_SIZE;
    }
    lock_release (&swap_lock);
    fte->spte->location = LOC_SW;
  }
  else
  {
    /* No swap slot left */
    //printf ("No swap slot left!\n");
    //lock_release (&swap_lock);
    exit (-1);
  }
  return swap_index;
}

/* Swap in 
 * Swap the page of index SWAP_INDEX with given FRAME
 * and do mapping
 */
void swap_in (void *frame, size_t swap_index)
{
  if (swap_index != BITMAP_ERROR)
  {
    /* Read from swap disk */
    lock_acquire (&swap_lock);
    int i;
    void *buffer = frame;
    for (i = 0; i < 8; i++)
    {
      block_read (swap_block, swap_index + i, buffer);
      buffer += BLOCK_SECTOR_SIZE; 
    }
    /* Update swap bitmap */
    bitmap_set_multiple (swap_bm, swap_index, 8, false); 
    lock_release (&swap_lock);
    //printf ("current swap slot num : %d\n", bitmap_count (swap_bm, 0,
    //      bitmap_size (swap_bm), true));
  }
}

