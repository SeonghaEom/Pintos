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
struct lock frame_lock;
/* Global frame table */
struct list ft;
/* Frame table entry */
struct fte
{
  void *frame;              /* Pointer to Physical frame(kernel vaddr) */
  struct spte *spte;        /* Supplemental page table entry */
  struct thread *thread;    /* Thread who owns this frame */
  struct list_elem elem;    /* List element */
};

void frame_table_init (void);
void *frame_alloc (enum palloc_flags flags, struct spte *spte);
void frame_free (void *frame);
struct fte *find_entry_by_frame(void *);
struct fte *find_fte_by_spte (struct spte *spte);
void remove_all_fte (void);
#endif
