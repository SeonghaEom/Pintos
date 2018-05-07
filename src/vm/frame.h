#ifdef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include <stdbool.h>
#include <stdint.h>
#include <list.h>
/*
 *  2018.05.05
 *  KimYoonseo
 *  EomSungha
 */
/* Frame table lock */
struct lock frame_table_lock;
/* Global frame table */
struct list frame_table;
/* Frame table entry */
struct frame_table_entry {
  void *frame;                      /* Physical frame pointer */
  struct s_page_table_entry *spte;  /* Supplement page entry */      
  struct thread *thread;            /* Thread who uses this frame */
  struct list_elem elem;            /* List element */
};

void frame_table_init (void);
void frame_add_to_table (void *frame, struct s_page_table_entry *spte);
void *frame_alloc (enum palloc_flags flags, struct s_page_table_entry *spte);
void frame_free (void *frame);
void *frame_evict (enum palloc_flags flags);

#endif
