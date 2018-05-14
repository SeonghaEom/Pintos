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
  
  /* Page round down to find corresponding page for address */
  spte.addr = pg_round_down (address);
  e = hash_find (thread_current ()->spt, &spte.hash_elem);
  return e != NULL? hash_entry (e, struct spte, hash_elem) : NULL;
}

/* Load page from executable */
bool 
fs_load (struct spte *spte)
{
  struct file *file = spte->file;
  off_t ofs = spte->ofs;
  uint8_t *upage = spte->addr;
  uint32_t page_read_bytes = spte->read_bytes;
  uint32_t page_zero_bytes = spte->zero_bytes;
  bool writable = spte->writable;
  
  /* Get a page of memory. */
  //uint8_t *kpage = palloc_get_page (PAL_USER);
  uint8_t *kpage = frame_alloc (PAL_USER, spte);
  if (kpage == NULL)
  {  
    printf ("kpage is NULL\n");
    return false;
  }
  /* Load this page. */
  if (file_read_at (file, kpage, page_read_bytes, ofs) != (int) page_read_bytes)
    {
      printf ("File load fails\n");
      frame_free (kpage);
      return false; 
    }
  memset (kpage + page_read_bytes, 0, page_zero_bytes);

  /* Add the page to the process's address space. Add mapping in page table */
  if (!install_page (upage, kpage, writable)) 
    {
      printf ("install page fails\n");
      frame_free (kpage);
      return false; 
    }

  /* Set location to physical memory */
  spte->location = LOC_PM;
  printf ("successfully loaded %x\n", spte->addr);
  return true;
}

/* Load page from swap disk */
bool
sw_load (struct spte* spte) 
{
  size_t swap_index = spte->swap_index;
  bool writable = spte->writable;
  uint8_t *upage = spte->addr;
  printf ("swap index : %d\n", (int)spte->swap_index);
  printf ("spte addr : %x\n", spte->addr);
  /* Get a page of memory */ 
  uint8_t *kpage = frame_alloc (PAL_USER, spte);
  if (kpage == NULL)
  {  
    printf ("kpage is NULL\n");
    return false;
  }
  printf ("before swap in\n"); 
  /* Swap in spte */
  swap_in (kpage, swap_index);
  printf ("after swap in\n"); 
  if (!install_page (upage, kpage, writable))
  {

    printf ("install page failed\n");
    frame_free (kpage);
    return false;
  }
  /* Set location to physical memory */
  spte->location = LOC_PM;
  printf ("successfully loaded %x\n", spte->addr);
  return true;
}

