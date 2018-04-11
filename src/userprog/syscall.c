#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
/* PJ2 */
#include "threads/vaddr.h"
#include "pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static void valid_address (void *);
static int read_sysnum (void *);
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Handles syscall */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* Check that given stack pointer address is valid */
  valid_address (f->esp);
  /* Read syscall number and arguuments */
  int sysnum = read_sysnum (f->esp);
  //read_arguments (f->esp);
  
  hex_dump ((int) f->esp, f->esp, 50, true);
  /* sysnum */
  switch (sysnum)
  {
    /* Halt the operating systems */
    case SYS_HALT:
      halt ();
      break;
    /* Terminate this process */
    case SYS_EXIT:
      break;
    /* Start another process */
    case SYS_EXEC:
      break;
    /* Wait for a child process to die */
    case SYS_WAIT:
      break;
    /* Create a file */
    case SYS_CREATE:
      break;
    /* Delete a file */
    case SYS_REMOVE:
      break;
    /* Open a file */
    case SYS_OPEN:
      break;
    /* Obtain a file's size */
    case SYS_FILESIZE:
      break;
    /* Read from a file */
    case SYS_READ:
      break;
    /* Write to a file */
    case SYS_WRITE:
      break;
    /* Change position in a file */
    case SYS_SEEK:
      break;
    /* Report current position in a file */
    case SYS_TELL:
      break;
    /* Close a file */
    case SYS_CLOSE:
      break;
    default:
      printf ("sysnum : default\n");
      break;
  }
  
  printf ("system call!\n");
  thread_exit ();
}

/* Read syscall number with esp in syscall_handler */
static int
read_sysnum (void *esp)
{
  printf ("esp : %p\n", esp);
  printf ("value in esp : %d\n", *((int *)esp));
  return 1;
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

static void
halt (void)
{

}

