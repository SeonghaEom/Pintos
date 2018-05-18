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
#endif

static void syscall_handler (struct intr_frame *);
static void valid_address (void *, struct intr_frame *);
static int read_sysnum (void *);
static void read_arguments (void *esp, void **argv, int argc, struct intr_frame *); 
static void halt (void);
static int write (int, const void *, unsigned);
//static void exit (int status);
static tid_t exec (const char *cmd_line);
static int wait (tid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);
//static void close_all_files (void);

void
syscall_init (void) 
{
  //file_lock = (struct lock *) malloc (sizeof (struct lock));
  lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Handles syscall */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("syscall_hanlder\n");
  /* Check that given stack pointer address is valid */
  valid_address (f->esp, f);
  /* sysnum and arguments */
  int sysnum;
  void *argv[3];
  /* Read syscall number and arguuments */
  sysnum = read_sysnum (f->esp);
  //hex_dump ((int) f->esp, f->esp, 50, true);
  
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
      //printf ("SYS_READ\n");
      read_arguments (f->esp, &argv[0], 3, f);
      fd = (int) argv[0];
      buffer = (void *) argv[1];
      size = (unsigned) argv[2];
      //valid_address (&fd);
      //printf ("buffer : %x\n", buffer);
      //printf ("size : %d\n", size);
 
      valid_address ((void *) buffer, f);
      f->eax = read (fd, buffer, size);
      break;
    /* 9, Write to a file */
    case SYS_WRITE:
      //printf ("SYS_WRITE\n");
      read_arguments (f->esp, &argv[0], 3, f);
      fd = (int) argv[0];
      buffer = (void *) argv[1];
      size = (unsigned) argv[2];
      //valid_address (&fd);
      //printf ("buffer : %x\n", buffer);
      //printf ("size : %d\n", size);
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
    //if (true)
    {
      /* Check given pointer is mapped or unmapped */
      uint32_t *pd = thread_current()->pagedir;
      while (pagedir_get_page (pd, uaddr) == NULL)
      {
        //printf("uaddr %x\n", uaddr);
        /* Stack growth */

        if (uaddr >= f->esp -32 && uaddr <= (int) PHYS_BASE)
        {
          void *next_bound = pg_round_down (uaddr);
          //printf ("next bound %x \n stack limit %x\n", next_bound, STACK_LIMIT);
          if ((uint32_t) next_bound < STACK_LIMIT) 
          {
            //printf ("next bound exceed growth limit\n");
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
                //printf ("thread : %s\n", thread_current ()->name);
                //printf ("uaddr : %x\n", uaddr);
                //printf ("addr : %x\n", next_bound);
                //printf ("syscall stack growth\n");
              }

            /*
            else 
            { 
              printf("dfd\n");
              frame_free (kpage);
              exit (-1);
            }
            */
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
  //printf("thread %d, %s: exit(%d)\n", thread_current ()->tid, thread_current ()->argv_name, status);
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
  lock_acquire (&file_lock);
  //printf ("create : thread%d a file lock\n", thread_current ()->tid);
  bool result =  filesys_create (file, (int32_t) initial_size);
  //printf ("create : thread%d r file lock\n", thread_current ()->tid);
  lock_release (&file_lock);
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
      struct file *f = find_file (fd)->file;
      lock_acquire (&file_lock);
      //printf ("write : thread%d a file lock\n", thread_current ()->tid);
      int result = (int) file_write (f, buffer, (off_t) size);
      //printf ("write : thread%d r file lock\n", thread_current ()->tid);
      lock_release (&file_lock);
      return result;
    }
  }
}

/* Remove */
static bool
remove (const char *file)
{ 
  lock_acquire (&file_lock);
  //printf ("remove : thread%d a file lock\n", thread_current ()->tid);
  bool result = filesys_remove (file);
  //printf ("remove : thread%d r file lock\n", thread_current ()->tid);
  lock_release (&file_lock);

  return result;
}

/* Open */
static int
open (const char *file)
{    
  lock_acquire (&file_lock);
  //printf ("open : thread%d a file lock\n", thread_current ()->tid);
  struct file *open_file = filesys_open(file);
  //printf ("open : thread%d r file lock\n", thread_current ()->tid);
  lock_release (&file_lock);
 
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
    /* push it open_files */
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
read (int fd, void *buffer, unsigned size)
{
  thread_yield ();
  //printf("1\n");
  if (fd < 0)
  {
    //printf("a\n");
    exit (-1);
  }
  /* fd == 0 */
  else if (fd == 0) 
  {
    //printf("b\n");
    int i;
    for (i = 0; i < (int) size; i++)
    {
      input_getc ();
    }
    return size;
  }
  else if (fd == 1)
  {
    //printf("c\n");
    struct filedescriptor *filedes = find_file (fd);
    free (filedes);
    exit (-1);
  }
  else
  {
    //printf("d\n");
    struct filedescriptor *filedes = find_file (fd);
    if (filedes == NULL)
    { 
      //printf("e\n");
      exit (-1);
    }
    else 
    {
      //printf("f\n");
      struct file *f = find_file (fd)->file;
      lock_acquire (&file_lock);
      //printf ("read : thread%d a file lock\n", thread_current ()->tid);
      int result = (int) file_read (f, buffer, size);
      //printf ("read : thread%d r file lock\n", thread_current ()->tid);
      lock_release (&file_lock);
      //printf("g\n");
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
    lock_acquire (&file_lock);
    //printf ("seek : thread%d a file lock\n", thread_current ()->tid);
    file_seek (f, (off_t) position);
    //printf ("seek : thread%d r file lock\n", thread_current ()->tid);
    lock_release (&file_lock);
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
    lock_acquire (&file_lock);
    //printf ("tell : thread%d a file lock\n", thread_current ()->tid);
    unsigned result = (unsigned) file_tell (f);
    //printf ("tell : thread%d r file lock\n", thread_current ()->tid);
    lock_release (&file_lock);

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
    lock_acquire (&file_lock);
    //printf ("close : thread%d a file lock\n", thread_current ()->tid);
    file_close (f);
    //printf ("close : thread%d r file lock\n", thread_current ()->tid);
    lock_release (&file_lock);
    free (filedes);
  }
}

/* close all files in open files in current thread */
void
close_all_files (void)
{
  struct list *open_files = &thread_current ()->open_files;
  struct list_elem *e; 
  int i = 0;
  while (!list_empty (open_files))
  {
    e = list_pop_front (open_files);
    struct filedescriptor *filedes =
      list_entry (e, struct filedescriptor, elem);
    struct file *f = filedes->file;
    list_remove (&filedes->elem);
    file_close (f);
    free (filedes);
    i++;
  }
}

