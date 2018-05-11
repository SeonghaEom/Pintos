#ifdef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <debug.h>
#include <bitmap.h>
#include "devices/block.h"
#include "threads/synch.h"
#include <stdio.h>
/* 2018.05.10
 * EomSungha
 * KimYoonseo
 */

/* Swap bitmap */
struct bitmap *swap_bm;
/* Swap table lock */
struct lock swap_lock;
/* Swap block */
struct block *swap_block;

void swap_table_init (void);
size_t swap_out (void *frame);
void swap_in (void *frame, size_t swap_index);

#endif
