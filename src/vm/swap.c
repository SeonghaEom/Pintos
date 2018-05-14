#include "threads/synch.h"
#include "devices/block.h"
#include <bitmap.h>
#include "vm/swap.h"
#include "vm/page.h"
#include "vm/frame.h"
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
  size_t slot_max = block_size(swap_block)/8;
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
size_t swap_out (void *frame)
{
  size_t swap_index;
  
  /* Find empty slot */
  lock_acquire (&swap_lock);
  swap_index = bitmap_scan_and_flip (swap_bm, 0, 1, false);
  lock_release (&swap_lock);
  printf ("swap index %d\n", (int)swap_index);
  if (swap_index != BITMAP_ERROR)
  {
    /* Write in swap disk */
    lock_acquire (&swap_lock);
    int i;
    void *buffer = frame;
    for (i = 0; i < 8; i ++)
    {
      block_write (swap_block, swap_index * 8 + i, buffer);
      buffer += BLOCK_SECTOR_SIZE;
    }
    lock_release (&swap_lock);
    find_entry_by_frame (frame)->spte->location = LOC_SW;
  }
  else
  {
    /* No swap slot left */
    printf ("No swap slot left!\n");
    exit (-1);
  }
  return swap_index;
}

/* Swap in 
 * Swap the page of index SWAP_INDEX and map FRAME
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
      block_read (swap_block, swap_index * 8 + i , buffer);
      buffer += BLOCK_SECTOR_SIZE; 
    }
    //lock_release (&swap_lock);
    /* Update swap bitmap */
    //lock_acquire (&swap_lock);
    bitmap_set (swap_bm, swap_index, false); 
    lock_release (&swap_lock);
  }
}

