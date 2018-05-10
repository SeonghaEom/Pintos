#ifdef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <debug.h>
#include "threads/synch.h"
#include <stdbool.h>
#include <stdint.h>
#include "vm/frame.h"

/* 2018.05.10
 * EomSungha
 * KimYoonseo
 */

/* Swap table */
struct list swap_table;
/* Swap table lock */
struct lock swap_lock;

/* Swap table slot */
struct swap_slot
{
 struct thread *t;           /* Thread who owns this page */
 void *addr;                 /* Virtual user address */
 struct *frame_table_entry;  /* Frame in swap disk */
 struct list_elem elem;      /* List element */
};

void swap_table_init (void); /* Swap table init */
void swap_out (struct frame_table_entry *frame_table_entry);    /* Swap out */
void swap_in (struct frame_table_entry *frame_table_entry);     /* Swap in */

struct frame_table_entry *
swap_find_victim ();           /* Evict which swap slot to evcit
                               LRU or second chance */
