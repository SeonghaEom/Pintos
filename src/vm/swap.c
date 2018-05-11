#include "threads/synch.h"
#include "devices/block.h"
#include <bitmap.h>
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
  slot_max = swap_block->size/8;
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

  if (swap_index != BITMAP_ERROR)
  {
    /* Write in swap disk */
    block_write (swap_block, 8, frame);
  }
  else
  {
    printf ("No swap slot left!\n");
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
    block_read (swap_block,i 8, frame); 
    /* Update swap bitmap */
    lock_acquire (&swap_lock);
    bitmap_set (swap_bm, swap_index, true); 
    lock_release (&swap_lock);
  }
}

