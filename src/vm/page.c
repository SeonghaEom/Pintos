#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <hash.h>
#include <debug.h>
#include <string.h>
#include <stdio.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "threads/thread.h"

/*
 *  2018.05.08
 *  KimYoonseo
 *  EomSungha
 */

static unsigned page_hash (const struct hash_elem *p_, void *aux);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
                           void *aux UNUSED);

static unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spte *spte = hash_entry (p_, struct spte, hash_elem);
  return hash_bytes (&spte->addr, sizeof spte->addr);
}

static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct spte *a = hash_entry (a_, struct spte, hash_elem);
  const struct spte *b = hash_entry (b_, struct spte, hash_elem);
  
  return a->addr < b->addr;
}

/* Initialize supplement page table */
void spt_init (struct hash *spt)
{
  hash_init (spt, page_hash, page_less, NULL);
}

/* Return the spte containing the given virtual address, 
 * or a null pointer if no such page exists 
 */
struct spte *
spte_lookup (const void *address)
{
  struct spte spte;
  struct hash_elem *e;
  //printf("spte_lookup %p\n", address);
  /* Page round down to find corresponding page for address */
  spte.addr = pg_round_down (address);
  e = hash_find (thread_current ()->spt, &spte.hash_elem);
  if (e == NULL) {
    //printf("Spte lookup failed %p\n", address);
  }
  else
  {
    //printf ("SPte lookup success %p\n", address);
  }
  return e != NULL? hash_entry (e, struct spte, hash_elem) : NULL;
}

/* Load page from executable */
bool 
fs_load (struct spte *spte)
{
  printf ("fs_load : thread %s at %x\n", thread_current ()->argv_name, spte->addr);
  printf ("read_byte: %d, zero_byte: %d\n", spte->read_bytes, spte->zero_bytes);
  struct file *file = spte->file;
  off_t ofs = spte->ofs;
  uint8_t *upage = spte->addr;
  uint32_t page_read_bytes = spte->read_bytes;
  uint32_t page_zero_bytes = spte->zero_bytes;
  bool writable = spte->writable;
  
  /* Get a page of memory. */
  uint8_t *kpage = frame_alloc (PAL_USER, spte);
  if (kpage == NULL)
  { 
    PANIC ("HI");
    //printf ("kpage is NULL\n");
    return false;
  }
  /* Load this page. */
  /* 1. page zero bytes = PGSIZE */
  if (page_zero_bytes == PGSIZE) 
  {
    memset (kpage, 0, PGSIZE);
  }
  /* 2. page zero byte is 0 */
  else if (page_zero_bytes == 0)
  {
    //printf ("fs_load : thread%d try file lock\n", thread_current ()->tid);
    lock_acquire (&file_lock);
    //printf ("fs_load : thread%d a file lock\n", thread_current ()->tid);
    if (file_read_at (file, kpage, page_read_bytes, ofs) != PGSIZE)
    {
      //printf ("fs_load : thread%d r file lock\n", thread_current ()->tid);
      lock_release (&file_lock);
      //printf ("File load failed\n");
      frame_free (kpage);
      return false;
    }
    //printf ("fs_load : thread%d r file lock\n", thread_current ()->tid);
    lock_release (&file_lock);
  }
  /* 3. page zero byte is beween 0 and PGSIZE  */
  else
  {
    //printf ("fs_load : thread%d try file lock\n", thread_current ()->tid);
    lock_acquire (&file_lock);
    //printf ("fs_load : thread%d a file lock\n", thread_current ()->tid);
    if (file_read_at (file, kpage, page_read_bytes, ofs) != (int) page_read_bytes)
    {
      //printf ("fs_load : thread%d r file lock\n", thread_current ()->tid);
      lock_release (&file_lock);
      //printf ("File load fails\n");
      frame_free (kpage);
      return false; 
    }
    //printf ("fs_load : thread%d r file lock\n", thread_current ()->tid);
    lock_release (&file_lock);

    memset (kpage + page_read_bytes, 0, page_zero_bytes);
  }
  /* Add the page to the process's address space. Add mapping in page table */
  /*if (!install_page (upage, kpage, writable)) 
  {
    frame_free (kpage);
    return false; 
  }*/

  /* Set location to physical memory */
  spte->location = LOC_PM;
  pagedir_set_page(thread_current()->pagedir, upage, kpage, writable);
  
  return true;
}

/* Load page from swap disk */
bool
sw_load (struct spte* spte) 
{
  printf ("sw_load : thread %s at %x\n", thread_current ()->argv_name, spte->addr);
  printf ("read_byte: %d, zero_byte: %d\n", spte->read_bytes, spte->zero_bytes);
 
  size_t swap_index = spte->swap_index;
  bool writable = spte->writable;
  uint8_t *upage = spte->addr;
  //printf ("swap index : %d\n", (int)spte->swap_index);
  //printf ("spte addr : %x\n", spte->addr);
  /* Get a page of memory */ 
  uint8_t *kpage = frame_alloc (PAL_USER, spte);
  
  if (kpage == NULL)
  {  
    PANIC ("kpage is NULL\n");
    return false;
  }
  
  /* Swap in spte */
  //printf ("before swap in\n"); 
  swap_in (kpage, swap_index);
  //printf ("after swap in\n"); 
  
  /*if (!install_page (upage, kpage, writable))
  {
    printf ("install page failed\n");
    PANIC("FFFFF\n");
    frame_free (kpage);
    return false;
  }*/
  
  /* Set location to physical memory */
  spte->location = LOC_PM;
  pagedir_set_page(thread_current()->pagedir, upage, kpage, writable);
  //printf ("successfully loaded %x\n", spte->addr);
  return true;
}
