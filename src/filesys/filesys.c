#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#ifdef FILESYS
#include "threads/thread.h"
#include "filesys/cache.h"
#endif

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

#ifdef FILESYS
  /* Initialize cache */
  cache_init ();
  /* Creating read ahead and write behind threads */
  thread_create ("read_aheader", PRI_DEFAULT, read_aheader_func, NULL);
  thread_create ("flusher", PRI_DEFAULT, flusher_func, NULL);
#endif

  if (format) 
    do_format ();
  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
#ifdef FILESYS
  cache_write_behind ();
  cache_destroy ();
  q_destroy ();
#endif
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, enum inode_type type) 
{
  //printf ("filesys_create, name: %s, initial_size %d, inode_type: %d\n", name, initial_size, type);
  block_sector_t inode_sector = 0;
  char *last_name;
  //printf ("last name address: %x\n", &last_name);
  //struct dir *dir = dir_open_path (name, &last_name);
  
  struct dir *dir = dir_open_root ();

  //printf ("last name: %s\n", last_name); 
  struct inode *inode;
  
  if (last_name != NULL)
  {  
    dir_lookup (dir, last_name, &inode);
  }
  if (inode == NULL)
  {
    printf("dfad\n");
  } 
  //printf ("dir %x\n", dir);
  //printf ("enum size: %d\n", sizeof (enum inode_type));
  //bool a = (dir != NULL);
  //bool b = (free_map_allocate (1, &inode_sector));
  //bool c = inode_create (inode_sector, initial_size, 0);//type);
  //bool d = dir_add (dir, last_name, inode_sector);
  //printf ("which is false?: %s %s %s %s\n", a? "t":"f", b?"t":"f",c?"t":"f",d?"t":"f");
  //printf ("type : %d\n", type);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, type)//type);
                  && dir_add (dir, last_name, inode_sector));
  //printf ("success is %s\n", success? "true" : "false");
  if (!success && inode_sector != 0) 
  {  
    //printf ("bbb\n");
    free_map_release (inode_sector, 1);
  }
  dir_close (dir);
  //:printf ("success\n");
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *dummy;
  struct dir *dir = dir_open_path (name, &dummy);
  struct inode *inode = NULL; 

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);
  if (inode == NULL)
      printf ("inode null in filesysopen\n");
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char *dummy;
  struct dir *dir = dir_open_path (name, &dummy);
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  struct dir *root = dir_open_root ();
  /* '.' and '..' for root directory */
  dir_add (root, ".", ROOT_DIR_SECTOR); 
  dir_add (root, "..", ROOT_DIR_SECTOR);
  free_map_close ();
  printf ("done.\n");
}
