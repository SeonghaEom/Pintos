#ifdef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <debug.h>
/*  2018.05.08 
 *  KimYoonseo
 *  EomSungha
 */
/* Supplement page table entry */
struct spte
{
  struct hash_elem hash_elem;   /* Hash table element */
  void *addr;                   /* Virtual address */
  struct thread *t;             /* Thread who owns this page */
  uint32_t length;              /* Data's length */
};

struct hash spt;

struct lock spt_lock;

void spt_init (void);



#endif
