#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>
#include <debug.h>
#include <stdio.h>
#include <stdint.h>
//#include <syscall.h>
#include "filesys/off_t.h"
/* 2018.05.08
 * KimYoonseo
 * EomSungha
 */

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

enum loc_type
{
  LOC_FS,   /* File system */
  LOC_SW,   /* Swap table */
  LOC_PM,   /* Page table */
  LOC_MMAP, /* MMAP */
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
  uint32_t read_bytes;          /* Page's read bytes */
  uint32_t zero_bytes;          /* Page's zero bytes */
  bool writable;                /* Writable */
  /* When swapped out */
  size_t swap_index;            /* Swap disk's index */
  /* Synchronization */
  bool touchable;               /* Is touchable */
};

/* Mmap file */
struct mmap_file
{
  struct list_elem elem;        /* List elem */
  mapid_t mapid;                /* Mapping id */
  void *addr;                   /* Start address of mapping */
  int cnt;                      /* Number of pages allocated */
};

void spt_init (struct hash *spt);
struct spte *spte_lookup (const void *address);
bool fs_load (struct spte *spte);
bool sw_load (struct spte *spte);
#endif
