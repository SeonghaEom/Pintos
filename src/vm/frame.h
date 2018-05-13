#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdbool.h>
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"

/*
 * 2018.05.05
 * KimYoonseo
 * EomSungha
 */
/* Frame table lock */
struct lock frame_table_lock;
/* Global frame table */
struct list frame_table;
/* Frame table entry */
struct frame_table_entry
{
  void *frame;              /* Physical frame pointer */
  struct spte *spte;        /* Supplement page entry */
  struct thread *thread;    /* Thread who uses this frame */
  struct list_elem elem;    /* List element */
  bool rbit;                 /* Reference bit for eviction */
};

void frame_table_init (void);
void frame_add_to_table (void *frame, struct spte *spte);
void *frame_alloc (enum palloc_flags flags, struct spte *spte);
void frame_free (void *frame);
void *frame_evict (enum palloc_flags flags);
struct frame_table_entry *find_entry_by_frame(void *);

#endif
