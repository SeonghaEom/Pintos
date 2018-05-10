#include "vm/page.h"
#include <hash.h>
#include <debug.h>
#include "threads/synch.h"
/*
 *  2018.05.08
 *  KimYoonseo
 *  EomSungha
 */
static unsigned page_hash (const struct hash_elem *p_, void *aux);
static unsigned page_less (const struct hash_elem *a_, const struct hash_elem *b_,
                           void *aux UNUSED);

static unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spte *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct spte *a = hash_entry (a_, struct spte, hash_elem);
  const struct spte *b = hash_entry (b_, struct spte, hash_elem);
  
  return a->addr < b->addr;
}

/* Initialize supplement page table */
void spt_init (void)
{
  hash_init (&spt, page_hash, page_less, NULL);
  lock_init (&spt_lock);
}

/* Return the spte containing the given virtual address, 
 * or a null pointer if no such page exists 
 */
struct spte *
spte_lookup (const void *addr)
{
  struct spte spte;
  struct hash_elem *e;

  spte.addr = addr;
  e = hash_find (&spte, &spte.hash_elem);
  return e != NULL? hash_entry (e, struct spte, hash_elem) : NULL;
}

/* */
bool 
spte_load (struct *spte)
{
  struct file *file = spte->file;
  off_t ofs = spte->ofs;
  uint8_t *upage = spte->addr;
  uint32_t page_read_bytes = spte->read_bytes;
  uint32_t page_write_bytes = spte->write_bytes;
  bool writable = spte->writable;
  
  /* Get a page of memory. */
  //uint8_t *kpage = palloc_get_page (PAL_USER);
  uint8_t *kpage = frame_alloc (PAL_USER, spte);
  if (kpage == NULL)
    return false;

  /* Load this page. */
  if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
    {
      palloc_free_page (kpage);
      return false; 
    }
  memset (kpage + page_read_bytes, 0, page_zero_bytes);

  /* Add the page to the process's address space. Add mapping in page table */
  if (!install_page (upage, kpage, writable)) 
    {
      palloc_free_page (kpage);
      return false; 
    }

  /* Set location to page table */
  spte->location = 2;
  return true;
}
