#ifdef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>
/*  2018.05.08 
 *  KimYoonseo
 *  EomSungha
 */
/* Supplement page table */
struct hash spt;
/* Supplement page table lock */
struct lock spt_lock;
/* Supplement page table entry */
struct spte
{
  struct hash_elem hash_elem;   /* Hash table element */
  void *addr;                   /* Virtual user address */
  struct thraed *t;             /* Thread who owns this page */
  struct file *file;            /* File */
  off_t ofs;                    /* Offset */
  uint32_t read_bytes;          /* Load_segment's read_bytes */
  uint32_t zero_bytes;          /* Load_segment's zero_bytes */    
  bool writable;                /* Writable */
  uint8_t location;             /* Location, 0(executable), 1(swap disk), 2(page table) */
 
  uint32_t pte;                 /* Page table entry */
}

void spt_init (void);
struct spte *spte_lookup (const void *addr);
bool spte_load (struct *spte); 

#endif
