#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>
#include <stdio.h>
#include "filesys/off_t.h"
/* 2018.05.08
 * KimYoonseo
 * EomSungha
 */
enum loc_type
{
  LOC_FS,   /* File system */
  LOC_SW,   /* Swap table */
  LOC_PM,   /* Page table */
};

/* Supplement page table entry */
struct spte
{
  struct hash_elem hash_elem;   /* Hash table element */
  void *addr;                   /* User virtual page address */
  enum loc_type location;       /* Location */
  /* In file system */
  struct file *file;            /* File*/
  off_t ofs;                    /* Offset */
  uint32_t read_bytes;          /* Load segment's read bytes */
  uint32_t zero_bytes;          /* Load segment's zero bytes */
  bool writable;                /* Writable */
  /* When swapped out */
  uint32_t swap_index;          /* Swap disk's index */
};

void spt_init (struct hash *spt);
struct spte *spte_lookup (const void *address);
bool fs_load (struct spte *spte);
bool sw_load (struct spte *spte);
#endif
