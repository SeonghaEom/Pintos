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

static void syscall_handler (struct intr_frame *);
static bool valid_address (void *);
static int read_sysnum (void *);
static void read_arguments (void *esp, void **argv, int argc); 
static void halt (void);
static int write (int, const void *, unsigned);
static void exit (int status);
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

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Handles syscall */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("Syscall!\n");
  /* Check that given stack pointer address is valid */
  valid_address (f->esp);
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
  printf ("sysnum : %d\n", sysnum);
  switch (sysnum)
  {
    /* 0, Halt the operating systems */
    case SYS_HALT:
      halt ();
      break;
    /* 1, Terminate this process */
    case SYS_EXIT:
      read_arguments (f->esp, &argv[0], 1);
      int status = (int) argv[0]; 
      
      exit (status);   
      break;
    /* 2, Start another process */
    case SYS_EXEC:
      read_arguments (f->esp, &argv[0], 1);
      char * cmd_line = (char *) argv[0];
      /* Invalid address */
      if (!valid_address (cmd_line))
      {
        exit (-1);
      }
      exec (cmd_line);
      break;
    /* 3, Wait for a child process to die */
    case SYS_WAIT:
      read_arguments (f->esp, &argv[0], 1);
      int pid = (int) argv[0];
      
      wait (pid);
      break;
    /* 4, Create a file */
    case SYS_CREATE:
      read_arguments (f->esp, &argv[0], 2);
      file = (const char *) argv[0];
      unsigned initial_size = (unsigned) argv[1];
      if (!valid_address ((void *) file))
      {
        exit (-1);
      }
      create (file, initial_size);
      break;
    /* 5, Delete a file */
    case SYS_REMOVE:
      read_arguments (f->esp, &argv[0], 1);
      file = (const char *) argv[0];
      if (!valid_address ((void *) file))
      {
        exit (-1);
      }
      remove (file);
      break;
    /* 6, Open a file */
    case SYS_OPEN:
      read_arguments (f->esp, &argv[0], 1);
      file = (const char *) argv[0];
      if (!valid_address ((void *) file))
      {
        exit (-1);
      }
      open (file);
      break;
    /* 7, Obtain a file's size */
    case SYS_FILESIZE:
      read_arguments (f->esp, &argv[0], 1);
      fd = (int) argv[0];
      filesize (fd);
      break;
    /* 8, Read from a file */
    case SYS_READ:
      read_arguments (f->esp, &argv[0], 3);
      fd = (int) argv[0];
      buffer = (void *) argv[1];
      size = (unsigned) argv[2];
      if (!valid_address ((void *) buffer))
      {
        exit (-1);
      }
      read (fd, buffer, size);
      break;
    /* 9, Write to a file */
    case SYS_WRITE:
      read_arguments (f->esp, &argv[0], 3);
      fd = (int) argv[0];
      buffer = (void *) argv[1];
      size = (unsigned) argv[2];
      if (!valid_address ((void *) buffer))
      {
        exit (-1);
      }
      write (fd, buffer, size);
      break;
    /* 10, Change position in a file */
    case SYS_SEEK:
      read_arguments (f->esp, &argv[0], 2);
      fd = (int) argv[0];
      unsigned position = (unsigned) argv[1];
      seek (fd, position);
      break;
    /* 11, Report current position in a file */
    case SYS_TELL:
      read_arguments (f->esp, &argv[0], 1);
      fd = (int) argv[0];
      tell (fd);
      break;
    /* 12, Close a file */
    case SYS_CLOSE:
      read_arguments (f->esp, &argv[0], 1);
      fd = (int) argv[0];
      close (fd);
      break;
    default:
      printf ("sysnum : default\n");
      break;
  }
  
  //thread_exit ();
}

/* Read syscall number with esp in syscall_handler */
static int
read_sysnum (void *esp)
{
  //printf ("esp : %p\n", esp);
  //printf ("value in esp : %d\n", *((int *)esp));
  return *(int *)esp;
}

/* Check the given user-provided pointer is valid and return -1  later */
static bool 
valid_address (void *uaddr)
{
  /* First check given pointer is NULL */
  if (uaddr == NULL) 
  {
    return 0;
  }
  /* Non NULL address */
  else 
  {
    /* Check given pointer is user vaddr, point to user address */
    if (is_user_vaddr (uaddr))  
    {
      /* Check given pointer is mapped or unmapped */
      uint32_t *pd = thread_current()->pagedir;
      if (pagedir_get_page (pd, uaddr) == NULL)
      {
        return 0;
      }
      return 1;
    }
    /* Not in user virtual address */
    else 
    {
      return 0;
    }
  }
}

/* Read argc number of argumments from esp */
static void
read_arguments (void *esp, void **argv, int argc)
{
  int count = 0;
  /* To reach first arugments */
  esp += 4;
  while (count != argc)
  {
    //printf ("argv[count] : %p\n", &argv[count]);
    //printf ("esp : %p\n", esp);
    memcpy (&argv[count], esp, 4);
    //printf ("%d th : %d\n", count, (int) argv[count]);
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
static void
exit (int status)
{
  /* sema_up exit_sema. */
  /* exit_sema doesn't exist */
  if (thread_current ()->exit_sema == NULL || thread_current ()->exit_sema == -858993460)
  {
    thread_current ()->exit_status = status;
    thread_exit ();
  }
  /* exit_sema exists */
  else 
  {
    struct semaphore *exit_sema = thread_current ()->exit_sema; 
    sema_up (exit_sema);
    
    thread_current ()->exit_status = status; 
    thread_exit ();
  }
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
  return filesys_create (file, (int32_t) initial_size);   
}

/* Write */
static int
write (int fd, const void *buffer, unsigned size)
{
  /* fd==1 reserved from standard output */
  if (fd == 1) 
  {
    putbuf (buffer, size);
    return (int)size;
  }
  return -1;
}

/* Remove */
static bool
remove (const char *file)
{
  return filesys_remove (file);
}

/* Open */
static int
open (const char *file)
{
  return filesys_open (file);  
}

/* Filesize */
static int
filesize (int fd UNUSED)
{
  /* TODO */
  return 1;
}

/* Read */
static int
read (int fd UNUSED, void *buffer UNUSED, unsigned size UNUSED)
{
  /* TODO */
  return 1;
}

/* Seek */
static void
seek (int fd UNUSED, unsigned position UNUSED)
{
  /* TODO */
  return;
}

static unsigned 
tell (int fd UNUSED)
{
  /* TODO */
  return (unsigned) 0;
}

static void
close (int fd UNUSED)
{
  /* TODO */
  return;
}

