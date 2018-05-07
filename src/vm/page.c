#include "vm/page.h"
#include <hash.h>
#include <debug.h>
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


