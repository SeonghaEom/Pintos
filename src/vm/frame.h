<<<<<<< HEAD
#indef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include <stdbool.h>
#include <stdint.h>
#include <list.h>

struct lock frame_table_lock;

struct list frame_table;

struct frame_entry {
  void *frame;
  struct supple_page_entry *spte;
  struct thread *thread;
  struct list_elem elem;
};

void frame_table_init (void);
void frame_add_to_table (void *frame, struct supple_page_entry *spte);
void *frame_alloc (enum palloc_flags flags, struct supple_page_entry *spte);
void frame_free (void *frame);
void *frame_evict (enum palloc_flags flags);

#endif / vm/frame.h */
=======
/*
 *  2018.05.06
 *  frame.h
 *
 *  To implement PJ3-1 Frame table
 */
>>>>>>> a251b1b068aac0fa0f69016c00c936ed4c08efc2
