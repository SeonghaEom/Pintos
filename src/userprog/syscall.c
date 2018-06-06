#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* PJ2 */
#include "threads/vaddr.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include <string.h>
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/malloc.h"
#ifdef VM
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "filesys/off_t.h"
#include <round.h>
#include "devices/block.h"
#include <bitmap.h>
#include "threads/synch.h"
/* PJ4 */
#endif
#ifdef FILESYS
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "stddef.h"
#endif

static void syscall_handler (struct intr_frame *);
static void valid_address (void *, struct intr_frame *);
static int read_sysnum (void *);
static void read_arguments (void *esp, void **argv, int argc, struct intr_frame *); 
static void halt (void);
static int write (int, const void *, unsigned);
static tid_t exec (const char *cmd_line);
static int wait (tid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size, struct intr_frame * f);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);
#ifdef VM
static mapid_t mmap (int fd, void *addr);
static void munmap (mapid_t mapid);
/* Lock used by allocate_mapid(). */
static mapid_t allocate_mapid (void);
static struct mmap_file* find_mf_by_mapid (mapid_t mapid);
static struct lock mapid_lock;
#endif
#ifdef FILESYS
static bool chdir (const char *dir);
static bool mkdir (const char *dir);
static bool readdir (int fd, char *name);
static bool isdir (int fd);
static int inumber (int fd);
#define READDIR_MAX_LEN 14
#endif

void
syscall_init (void) 
{
  //file_lock = (struct lock *) malloc (sizeof (struct lock));
  //lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&mapid_lock);
}

/* Handles syscall */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* Check that given stack pointer address is valid */
  valid_address (f->esp, f);
  /* sysnum and arguments */
  int sysnum;
  void *argv[3];
  /* Read syscall number and arguuments */
  sysnum = read_sysnum (f->esp);
  
  const char *file;
  void *buffer;
  unsigned size;
  int fd; 
  /* sysnum */
  switch (sysnum)
  {
    /* 0, Halt the operating systems */
    case SYS_HALT:
      halt ();
      break;
    /* 1, Terminate this process */
    case SYS_EXIT:
      read_arguments (f->esp, &argv[0], 1, f);
      int status = (int) argv[0]; 
      exit (status);   
      break;
    /* 2, Start another process */
    case SYS_EXEC:
      read_arguments (f->esp, &argv[0], 1, f);
      char * cmd_line = (char *) argv[0];
      /* Invalid address */
      valid_address (cmd_line, f);
      f->eax = exec (cmd_line);
      break;
    /* 3, Wait for a child process to die */
    case SYS_WAIT:
      read_arguments (f->esp, &argv[0], 1, f);
      int pid = (int) argv[0];
      f->eax = wait (pid);
      break;
    /* 4, Create a file */
    case SYS_CREATE:
      read_arguments (f->esp, &argv[0], 2, f);
      file = (const char *) argv[0];
      unsigned initial_size = (unsigned) argv[1];
      valid_address ((void *) file, f);
      f->eax = create (file, initial_size);
      break;
    /* 5, Delete a file */
    case SYS_REMOVE:
      read_arguments (f->esp, &argv[0], 1, f);
      file = (const char *) argv[0];
      valid_address ((void *) file, f);
      f->eax = remove (file);
      break;
    /* 6, Open a file */
    case SYS_OPEN:
      read_arguments (f->esp, &argv[0], 1, f);
      file = (const char *) argv[0];
      valid_address ((void *) file, f);
      f->eax = open (file);
      break;
    /* 7, Obtain a file's size */
    case SYS_FILESIZE:
      read_arguments (f->esp, &argv[0], 1, f);
      fd = (int) argv[0];
      f->eax = filesize (fd);
      break;
    /* 8, Read from a file */
    case SYS_READ:
      read_arguments (f->esp, &argv[0], 3, f);
      fd = (int) argv[0];
      buffer = (void *) argv[1];
      size = (unsigned) argv[2];
      
      valid_address ((void *) buffer, f);
      f->eax = read (fd, buffer, size, f);
      break;
    /* 9, Write to a file */
    case SYS_WRITE:
      read_arguments (f->esp, &argv[0], 3, f);
      fd = (int) argv[0];
      buffer = (void *) argv[1];
      size = (unsigned) argv[2];
      valid_address ((void *) buffer, f);
      valid_address ((void *) buffer + size, f);
      f->eax = write (fd, buffer, size);
      break;
    /* 10, Change position in a file */
    case SYS_SEEK:
      read_arguments (f->esp, &argv[0], 2, f);
      fd = (int) argv[0];
      unsigned position = (unsigned) argv[1];
      seek (fd, position);
      break;
    /* 11, Report current position in a file */
    case SYS_TELL:
      read_arguments (f->esp, &argv[0], 1, f);
      fd = (int) argv[0];
      f->eax = tell (fd);
      break;
    /* 12, Close a file */
    case SYS_CLOSE:
      read_arguments (f->esp, &argv[0], 1, f);
      fd = (int) argv[0];
      close (fd);
      break;
#ifdef VM
    /* 13, Mmap */
    case SYS_MMAP:
      read_arguments (f->esp, &argv[0], 2, f);
      fd = (int) argv[0];
      void *addr = (void *) argv[1];
      f->eax = mmap (fd, addr);
      break;
    case SYS_MUNMAP:
      read_arguments (f->esp, &argv[0], 1, f);
      mapid_t mapid = (mapid_t) argv[0];
      munmap (mapid);
      break;
#endif
#ifdef FILESYS
    case SYS_CHDIR:
      read_arguments (f->esp, &argv[0], 1, f);
      file = (const char *)argv[0];
      f->eax = chdir (file);
      break;
    case SYS_MKDIR:
      read_arguments (f->esp, &argv[0], 1, f);
      file = (const char *)argv[0];
      f->eax = mkdir (file);
      break;
    case SYS_READDIR:
      read_arguments (f->esp, &argv[0], 2, f);
      fd = (int) argv[0];
      char name[READDIR_MAX_LEN + 1];
      printf ("argv[0] : %s\n", (char *) argv[0]);
      printf ("argv[1] : %s\n", (char *) argv[1]);
      //name = (char *) &argv[1];
      f->eax = readdir (fd, name);
      break;
    case SYS_ISDIR:
      read_arguments (f->esp, &argv[0], 1, f);
      fd = (int) argv[0];
      f->eax = isdir (fd);
      break;
    case SYS_INUMBER:
      read_arguments (f->esp, &argv[0], 1, f);
      fd = (int) argv[0];
      f->eax = inumber (fd);
      break;
#endif
    default:
      printf ("sysnum : default\n");
      break;
  }
}

/* Read syscall number with esp in syscall_handler */
static int
read_sysnum (void *esp)
{
  return *(int *)esp;
}

/* Check the given user-provided pointer is valid and return -1 later */
static void 
valid_address (void *uaddr, struct intr_frame * f)
{
  /* First check given pointer is NULL */
  if (uaddr == NULL) 
  {
    exit (-1);
  }
  /* Non NULL address */
  else 
  {
    /* Check given pointer is user vaddr, point to user address */
    if (is_user_vaddr (uaddr))  
    {
      /* Check given pointer is mapped or unmapped */
      uint32_t *pd = thread_current()->pagedir;
      while (pagedir_get_page (pd, uaddr) == NULL)
      {
        /* Stack growth */

        if (uaddr >= f->esp -32 && uaddr <= (void *) PHYS_BASE)
        {
          void *next_bound = pg_round_down (uaddr);
          if ((uint32_t) next_bound < STACK_LIMIT) 
          {
            exit (-1);
          }

          struct spte *spte = spte_lookup (uaddr);
          /* Spte exist */
          if (spte != NULL) 
          {
            continue;
          }

          spte = (struct spte *) malloc (sizeof (struct spte));
          spte->location = LOC_PM;
          void *kpage = frame_alloc (PAL_USER, spte);
          if (kpage != NULL)
          {
              bool success = install_page (next_bound, kpage, true);
              if (success)
              {
                /* Set spte address */
                spte->addr = next_bound;
                hash_insert (thread_current ()->spt, &spte->hash_elem);
              }
          }
        }
        uaddr = uaddr + PGSIZE;
      }
      return;
    }
    /* Not in user virtual address */
    else 
    {
      exit (-1);
    }
  }
}

/* Read argc number of argumments from esp */
static void
read_arguments (void *esp, void **argv, int argc, struct intr_frame * f)
{
  int count = 0;
  /* To reach first arugments */
  esp += 4;
  while (count != argc)
  {
    memcpy (&argv[count], esp, 4);
    valid_address (esp, f);
    esp += 4;
    count++;
  }
}

/* Terminate Pintos */
static void
halt (void)
{
  shutdown_power_off ();
}

/* Terminate the current user program, returning status to the kernel */
void
exit (int status)
{
  /* exit_sema exists */
  thread_current ()->exit_status = status;
  printf("%s: exit(%d)\n", thread_current ()->argv_name, status);
  thread_exit ();
}

/* waits for pid(child) and retrieve the pid(child)'s exit status */
static int
wait (tid_t pid)
{
  return process_wait (pid);
} 

/* Runs the executable whose name is given in cmd_line */
static tid_t
exec (const char *cmd_line)
{
  return process_execute (cmd_line);
}

/* Create */
static bool
create (const char *file, unsigned initial_size)
{
  //lock_acquire (&file_lock);
  bool result = filesys_create (file, (int32_t) initial_size, INODE_FILE);
  //lock_release (&file_lock);
  return result;
}

/* Write */
static int
write (int fd, const void *buffer, unsigned size)
{
  /* fd==1 reserved from standard output */
  if (fd == 1) 
  {
    int rest = (int) size;
    while (rest >= 300)
    {
      putbuf (buffer, 300);
      buffer += 300;
      rest -= 300;
    }
    putbuf (buffer, rest);
    rest = 0;

    return size;
  }
  else if (fd == 0)
  {
    exit (-1);
  }
  else
  {
    struct filedescriptor *filedes = find_file (fd);

    if (filedes == NULL)
    {
      exit (-1);
    }
    else
    {
      thread_yield();
      struct file *f = filedes->file;
      //lock_acquire (&file_lock);
      int result = (int) file_write (f, buffer, (off_t) size);
      //lock_release (&file_lock);
      return result;
    }
  }
}

/* Remove */
static bool
remove (const char *file)
{ 
  //printf ("remove : thread%d a file lock\n", thread_current ()->tid);
  //lock_acquire (&file_lock);
  //printf ("remove : thread%d a file lock\n", thread_current ()->tid);
  bool result = filesys_remove (file);
  //printf ("remove : thread%d r file lock\n", thread_current ()->tid);
  //lock_release (&file_lock);

  return result;
}

/* Open */
static int
open (const char *file)
{    
  //lock_acquire (&file_lock);
  struct file *open_file = filesys_open(file);
  //lock_release (&file_lock);
 
  /* file open fail */
  if (open_file == NULL)
  {
    return -1;
  }
  /* file open success */
  else 
  {
    int new_fd = thread_current ()->next_fd;
    thread_current ()->next_fd++;
    /* create new filedescriptor */
    struct filedescriptor *filedes =
      (struct filedescriptor*) malloc (sizeof (struct filedescriptor));
    filedes->file = open_file;
    filedes->fd = new_fd;
    filedes->filename = file;
    /* push it open_files or open_dirs */
    list_push_back (&thread_current ()->open_files, &filedes->elem);
    if (!strcmp ( file, thread_current()->argv_name))
    {
      file_deny_write(open_file); 
    }
    return new_fd;
  }
}

/* Filesize */
static int
filesize (int fd)
{
  struct filedescriptor *filedes = find_file (fd);
  if (filedes == NULL)
  {
    exit (-1);
  }
  else 
  {
    struct file *f = find_file (fd)->file;
    int length = file_length (f);
    return length;
  }
}

/* Read */
static int
read (int fd, void *buffer, unsigned size, struct intr_frame * i_f)
{
  if (fd < 0)
  {
    exit (-1);
  }
  /* fd == 0 */
  else if (fd == 0) 
  {
    int i;
    for (i = 0; i < (int) size; i++)
    {
      input_getc ();
    }
    return size;
  }
  else if (fd == 1)
  {
    struct filedescriptor *filedes = find_file (fd);
    free (filedes);
    exit (-1);
  }
  else
  {
    struct filedescriptor *filedes = find_file (fd);
    if (filedes == NULL)
    { 
      exit (-1);
    }
    else 
    {
      struct file *f = find_file (fd)->file;

#ifdef VM
      int down = (int) buffer / PGSIZE;
      int up = ((int) buffer + (int) size) / PGSIZE;
      int alloc_num = up - down + 1;
      int i;
      for (i = 0; i < alloc_num; i++)
      {
        void *addr = buffer + PGSIZE * i;


        struct spte *spte = spte_lookup (addr); 
        if (spte != NULL)
        {
          spte->touchable = false;

          switch (spte->location)
          {
            case LOC_FS:
              if (!fs_load (spte))
              {
                exit (-1);
              }
              break;
            case LOC_MMAP:
              if (!fs_load (spte))
              {
                exit (-1);
              }
              break;
            case LOC_SW:
              if (!sw_load (spte))
              {
                exit (-1);
              }
              break;
            default:
              break;
          }
        }
        else 
        {
          if ((uint32_t)i_f->esp -32 <= (uint32_t)addr &&
            addr <= PHYS_BASE)
          {
            /* When stack growth happens, new page address would be this */
            void *next_bound = pg_round_down (addr);
            /* Stack limit */
            if ((uint32_t) next_bound < STACK_LIMIT) 
            {
              exit (-1);
            }
            /* Allocate new spte */
            struct spte *spte = (struct spte *) malloc (sizeof (struct spte));
            void *kpage = frame_alloc (PAL_USER, spte);
            if (kpage != NULL)
            {
              bool success = install_page (next_bound, kpage, true);
              if (success)
              {
                off_t len = file_length (f);
                /* Set spte information */
 
                if (i<alloc_num-1)
                {
                  spte->read_bytes = PGSIZE;
                  spte->zero_bytes = 0;
                }
                else if (i == alloc_num-1)
                {
                  spte->read_bytes = len - PGSIZE * i;
                  spte->zero_bytes = PGSIZE - spte->read_bytes;
                }
                spte->ofs = PGSIZE * i;
                spte->addr = next_bound;
                spte->location = LOC_PM;
                spte->writable = true;
                spte->touchable = false;
                hash_insert (thread_current ()->spt, &spte->hash_elem);
              }
              else 
              {
                frame_free (kpage);
                PANIC ("AA");
                exit (-1);
              }
            } 
            else
            {
              PANIC ("kpage nulln");
              exit (-1);
            }
          }
          
          else
          {
            
            exit (-1);
            printf ("a\n");
          }
        }
      } 
      
#endif
      //lock_acquire (&file_lock);
      int result = (int) file_read (f, buffer, size);
      //lock_release (&file_lock);
#ifdef VM
      /* Make all spte touchable here */
      for (i = 0; i < alloc_num; i++)
      {
        void *addr = buffer + PGSIZE * i;
        struct spte *spte = spte_lookup (addr);
        spte->touchable = true;
      }
#endif
      
      return result;
    }
  }
}

/* Seek */
static void
seek (int fd , unsigned position)
{
  struct filedescriptor *filedes = find_file (fd);
  if (filedes == NULL)
    exit (-1);
  else 
  {
    struct file *f = filedes->file;
    //lock_acquire (&file_lock);
    file_seek (f, (off_t) position);
    //lock_release (&file_lock);
  }
}

/* Tell */
static unsigned 
tell (int fd)
{
  struct filedescriptor *filedes = find_file (fd);
  if (filedes == NULL)
    exit (-1);
  else 
  {
    struct file *f = find_file (fd)->file;
    //lock_acquire (&file_lock);
    unsigned result = (unsigned) file_tell (f);
    //lock_release (&file_lock);

    return result;
  }
}

static void
close (int fd)
{
  if (fd == 1)
  {
    exit(-1);
    return;
  }

  struct filedescriptor *filedes = find_file (fd);
  if (filedes == NULL)
  {
    free (filedes);
    exit (-1);
  }
  else 
  { 
    struct file *f = find_file(fd)->file;
    list_remove (&filedes->elem);
    //lock_acquire (&file_lock);
    file_close (f);
    //lock_release (&file_lock);
    free (filedes);
  }
}

/* close all files in open files in current thread */
void
close_all_files (void)
{
  struct list *open_files = &thread_current ()->open_files;
  struct list_elem *e; 
  while (!list_empty (open_files))
  {
    e = list_pop_front (open_files);
    struct filedescriptor *filedes =
      list_entry (e, struct filedescriptor, elem);
    struct file *f = filedes->file;
    list_remove (&filedes->elem);
    file_close (f);
    free (filedes);
  }
}

#ifdef VM

/* Remove all mmap files from memory */
void 
remove_all_mfs (void)
{
  struct list *mmap_files = &thread_current ()->mmap_files;
  struct list_elem *e;
  while (!list_empty (mmap_files))
  {
    e = list_front (mmap_files);
    struct mmap_file *mf =
      list_entry (e, struct mmap_file, elem);
    munmap (mf->mapid);
  }
}

/* Mmap with fd and user virtual address */
static mapid_t
mmap (int fd, void *addr)
{
  /* Fd is invalid, std_in, std_out is also invalid */
  if (fd <= 1)
  { 
    //printf ("fd is below 1\n");
    return -1;
  }

  /* Check for validity of addr such as
   * is it page-aligned?, null? is in user address space? */
  if (!is_user_vaddr(addr) || addr == NULL || (int) addr % (int) PGSIZE != 0)
  {
    return -1;
  }

  struct file *file = find_file (fd)->file;
  
  /* Check if the file length is availabe to map */
  off_t len = file_length (file);
  int cnt = DIV_ROUND_UP (len, PGSIZE);

  int i = 0;
  for (; i<cnt; i++)
  {
    /* Check for validity of addr, is it overlapped with other mapping?,
     * or is it in code or stack segment? */
    struct spte *spte = spte_lookup (addr + PGSIZE * i);
    if (spte != NULL)
    {
      return -1;
    }
  }
   
  //lock_acquire (&file_lock);

  struct file *newfile = file_reopen (file);
  /* Push the new file as new filedescriptor in open files and close it later by close_all_files */
  struct filedescriptor *filedes = (struct filedescriptor *) malloc (sizeof (struct filedescriptor));
  filedes->fd = fd;
  filedes->file = newfile;
  list_push_back (&thread_current ()->open_files, &filedes->elem);
  //lock_release (&file_lock);
   

  /* Allocating new spte for each page while memory mapping */
  for (i = 0; i < cnt; i++)
  {
    struct spte *spte = (struct spte *) malloc (sizeof (struct spte));
    spte->addr = addr + PGSIZE * i;
    spte->location = LOC_MMAP;
    spte->file = newfile;
    spte->ofs = PGSIZE * i;
    spte->writable = true;
    if (i<cnt-1)
    {
      spte->read_bytes = PGSIZE;
      spte->zero_bytes = 0;
    }
    else if (i == cnt-1)
    {
      spte->read_bytes = len - PGSIZE * i;
      spte->zero_bytes = PGSIZE - spte->read_bytes;
    }
    hash_insert (thread_current ()->spt, &spte->hash_elem);
  }
  /* Allocating new mmap file for memory mapping */
  struct mmap_file *mf = (struct mmap_file *) malloc (sizeof (struct mmap_file));
  mapid_t mapid = allocate_mapid ();
  mf->mapid = mapid;
  mf->addr = addr;
  mf->cnt = cnt;
  
  /* Adding mmap file to current thread's mmap file list */
  list_push_back (&thread_current ()->mmap_files, &mf->elem);
 
  return mapid;
}

static mapid_t
allocate_mapid (void)
{
  static mapid_t next_id = 1;
  mapid_t mapid;

  lock_acquire (&mapid_lock);
  mapid = next_id++;
  lock_release (&mapid_lock);
  
  return mapid;
}

/* Munmap after loaded into physical memory or swapped into swap disk */
static void
munmap (mapid_t mapid)
{
  struct mmap_file *mf = find_mf_by_mapid (mapid);
  int i;
  for (i = 0; i < mf->cnt; i++)
  {
    struct spte *spte = spte_lookup (mf->addr + PGSIZE * i);
    /* Page is loaded in physical memory */
    if (spte->location == LOC_PM)
    {
      /* If the given page is dirty, then write it to filesys */
      if (pagedir_is_dirty (thread_current ()->pagedir, spte->addr))
      {
        lock_acquire (&frame_lock);
        struct file *file = spte->file;
        void *addr = spte->addr;
        off_t size = spte->read_bytes;
        off_t ofs = spte->ofs;
        file_write_at (file, addr, size, ofs);
        lock_release (&frame_lock);
      }
      /* Remove it from frame table */
      struct fte *fte = find_fte_by_spte (spte);
      list_remove (&fte->elem);
      /* Remove mapping from user virtual to kernel virtual (physical) */
      pagedir_clear_page (thread_current ()->pagedir, spte->addr);
    }
    /* Page is already evicted into swap disk */
    else if (spte->location == LOC_SW)
    {
      /* Write to block only if the page is dirty */
      if (pagedir_is_dirty (thread_current ()->pagedir, spte->addr))
      {
        size_t swap_index = spte->swap_index;
        off_t ofs = spte->ofs;
        if (swap_index != BITMAP_ERROR)
        {
          lock_acquire (&swap_lock);
          struct file *file = spte->file;
          void *buffer = (void *) malloc (BLOCK_SECTOR_SIZE);
          int i=0;
          for (; i<8; i++)
          {
            block_read (swap_block, swap_index + i, buffer);
            file_write_at (file, buffer, BLOCK_SECTOR_SIZE, ofs + BLOCK_SECTOR_SIZE * i);
            buffer += BLOCK_SECTOR_SIZE;
          }
          /* Update swap sector bitmap */
          bitmap_set_multiple (swap_bm, swap_index, 8, false);
          lock_release (&swap_lock);
        }
      }
    } 
    else if (spte->location == LOC_MMAP)
    {
      //printf ("LOC_MMAP!\n");
    }
    /* Remove spte from spt and free its resource for every page */
    hash_delete (thread_current ()->spt, &spte->hash_elem);
    free (spte);
  }
  /* Remove mmap file from mmap list and free its resource */
  list_remove (&mf->elem);
  free (mf);
}

/* Find mmap file pointer by mapid */
static struct mmap_file*
find_mf_by_mapid (mapid_t mapid)
{
  struct list_elem *e;
  struct list* mmap_files = &thread_current ()->mmap_files;
  struct mmap_file *mmap_file;
  
  /* Mmap_files is not empty */
  if (list_size (mmap_files) != 0)
  {
    /* Find mmap file by mapid */
    for (e = list_begin (mmap_files); e != list_end (mmap_files);
        e = list_next (e))
    {
      mmap_file = list_entry (e, struct mmap_file, elem);
      if (mmap_file->mapid == mapid)
      {
        return mmap_file;
      }
    }
  }
  /* There is no such mmap file with mapid MAPID */
  return NULL;
}

#endif

#ifdef FILESYS
/* Change the current directory to DIR */
static bool chdir (const char *dir)
{
  //absolute or relative?
  //dir_open_root
  //dir_lookup
  //check if it exists
  //
  char *dir_copy;
  char *save_ptr;
  strlcpy (dir_copy, dir, strlen(dir)+1);
  //absolute directory`
  if (strcmp (dir_copy, "/") == 0) // dir_copy first letter and "/"
  {
    struct dir *directory = dir_open_root ();
    char *ret_ptr = strtok_r (dir_copy, "/", &save_ptr);
    struct inode **inode;
    while (ret_ptr)
    {
      /* If the directory has subdirectory or file name which is ret_ptr return true */
      if (dir_lookup (directory, ret_ptr, inode))
      {
        /* Change the directory based on the inode */
        directory = dir_open (*inode);
      }
      else
      {
        /* There is subdirectory or file name but does not exist in directory */
        return false;
      }
      ret_ptr = strtok_r (NULL, "/", &save_ptr);
    }
  }
  //Relative directory
  /////////thread_current ()->cur_dir = directory;
  return true;
}

/* Create a new directory DIR */
static bool mkdir (const char *dir)
{
  //absolute or relative?
  char *dir_copy;
  char *save_ptr;
  strlcpy (dir_copy, dir, strlen(dir)+1);
  //absolute directory`
  if (strcmp (dir_copy, "/") == 0) //TODO dir_copy first letter and "/" not for sure
  {
    struct dir *directory = dir_open_root ();
    char *ret_ptr = strtok_r (dir_copy, "/", &save_ptr);
    struct inode **inode;
    while (ret_ptr)
    {
      /* If the directory has subdirectory or file name which is ret_ptr return true */
      if (dir_lookup (directory, ret_ptr, inode))
      {
        /* Change the directory based on the inode */
        directory = dir_open (*inode);
      }
      else
      {
        /* There is subdirectory or file name but does not exist in directory
         * possible or new directory?*/
        ret_ptr = strtok_r (NULL, "/", &save_ptr);
        if (ret_ptr == NULL)
        {
          //dir_create (directory->inode->sector, 0);
        }
        else
        {
          return false;
        }
      }
      ret_ptr = strtok_r (NULL, "/", &save_ptr);
    }
  }
  return true;
}

/* Read dir_entry from FD and stores file name in NAME */
static bool readdir (int fd, char *name)
{
  struct filedescriptor *filedes = find_dir (fd);
  // open file , open directory  따로 관리!?!!
  struct dir *dir = filedes->dir;
  char buf[NAME_MAX + 1];
  name = buf;
  return dir_readdir (dir, name);
}

/* True if FD presents directory, false if ordinary file */
static bool isdir (int fd)
{
  //root    에서부터 찾기
 struct filedescriptor *filedes = find_file (fd);

  if (filedes == NULL)
  {
    return true;
  }
  else
  {
    return false;
  }
}

/* Returns sector number of inode related with FD */
static int inumber (int fd)
{
 struct filedescriptor *filedes = find_file (fd);

 if (filedes == NULL)
 {
  exit (-1);
 }
 else
 {
  struct file *f = filedes->file;
  int inumber = inode_get_inumber (file_get_inode (f));
  return inumber;
 }
}
#endif
