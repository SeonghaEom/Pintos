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
static void exit (int status);

static int wait (tid_t pid);


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
      
      int status = (int) argv[0]; 
      
      exit (status);   
      break;
    /* 2, Start another process */
    case SYS_EXEC:
      read_arguments (f->esp, &argv[0]. 1);
      char * cmd_line = (char *) argv[0];

      exec (cmd_line);
      break;
    /* 3, Wait for a child process to die */
    case SYS_WAIT:
      read_arguments (f->esp, &argv[0], 1);

      int pid = (int) argv[0];
      
      wait(pid);
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
      read_arguments (f->esp, &argv[0], 3);
      
      int fd = (int)argv[0];
      void *buffer = (void *)argv[1];
      unsigned size = (unsigned)argv[2];
      
      process_write (fd, buffer, size);
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
  /* sema_up exit_sema */
  struct semaphore exit_sema = thread_current ()->process->exit_sema;
  thread_current ()->process->exit_status = status;
  sema_up (&exit_sema);
  /* Kill all child */
  
  thread_exit ();
}


/* waits for pid(child) and retrieve the pid(child)'s exit status */
static int
wait (tid_t pid)
{
  return process_wait(pid);
} 

static pid_t
exec ( const char *cmd_line);
{
  return process_execute (cmd_line);
}
