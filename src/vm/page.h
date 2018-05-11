#ifdef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>
/*  2018.05.08 
 *  KimYoonseo
 *  EomSungha
 */
enum loc_type
{
  LOC_FS;   /* File System */
  LOC_SW;   /* Swap table */
  LOC_PM;   /* Page table */
}

/* Supplement page table entry */
struct spte
{
  struct hash_elem hash_elem;   /* Hash table element */
  void *addr;                   /* User virtual page address */
  loc_type location;            /* Location */
  /* In file system */
  struct file *file;            /* File */
  off_t ofs;                    /* Offset */
  uint32_t read_bytes;          /* Load_segment's read_bytes */
  uint32_t zero_bytes;          /* Load_segment's zero_bytes */    
  bool writable;                /* Writable */
  /* When swapped out */
  uint32_t swap_index;          /* Swap disk's index*/
};

void spt_init (struct hash *spt);
struct spte *spte_lookup (const void *addr);
bool fs_load (struct *spte); 

#endif
