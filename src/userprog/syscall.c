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

static void syscall_handler (struct intr_frame *);
static void valid_address (void *);
static int read_sysnum (void *);
static void read_arguments (void *esp, void **argv, int argc); 
static void halt (void);
static int write (int, const void *, unsigned);
static void exit (int status, struct intr_frame *f);

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
      
      int status = (int)argv[0]; 
       
      exit (status, f);   
      break;
    /* 2, Start another process */
    case SYS_EXEC:
      break;
    /* 3, Wait for a child process to die */
    case SYS_WAIT:
      break;
    /* 4, Create a file */
    case SYS_CREATE:
      break;
    /* 5, Delete a file */
    case SYS_REMOVE:
      break;
    /* 6, Open a file */
    case SYS_OPEN:
      break;
    /* 7, Obtain a file's size */
    case SYS_FILESIZE:
      break;
    /* 8, Read from a file */
    case SYS_READ:
      break;
    /* 9, Write to a file */
    case SYS_WRITE:
      //printf ("WRITE\n");
      read_arguments (f->esp, &argv[0], 3);
      
      int fd = (int)argv[0];
      void *buffer = (void *)argv[1];
      unsigned size = (unsigned)argv[2];
      
      write (fd, buffer, size);
      break;
    /* 10, Change position in a file */
    case SYS_SEEK:
      break;
    /* 11, Report current position in a file */
    case SYS_TELL:
      break;
    /* 12, Close a file */
    case SYS_CLOSE:
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
/* Check the given user-provided pointer is valid */
static void 
valid_address (void *uaddr)
{
  /* First check given pointer is NULL */
  if (uaddr == NULL) 
  {
    thread_exit ();
  }
  /* Non NULL address */
  else 
  {
    /* Check given pointer is user vaddr */
    if (is_user_vaddr (uaddr))  
    {
      /* Check given pointer is mapped or unmapped */
      uint32_t *pd = thread_current()->pagedir;
      if (pagedir_get_page (pd, uaddr) == NULL)
      {
        thread_exit ();
      }
      return;
    }
    /* Not in user virtual address */
    else 
    {
      thread_exit ();
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
exit (int status, struct intr_frame *f)
{
  /* Save status in eax */
  f->eax = status;
  thread_exit ();
}

/* Write size bytes from buffer to the open fild fd */
static int
write (int fd, const void *buffer, unsigned size)
{
  /* TODO */
  /* Write to consol */
  if (fd == 1)
  {
    putbuf (buffer, size);
  }

  return 1;
}
