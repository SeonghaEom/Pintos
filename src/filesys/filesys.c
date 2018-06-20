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
  //thread_create ("read_aheader", PRI_DEFAULT, read_aheader_func, NULL);
  //thread_create ("flusher", PRI_DEFAULT, flusher_func, NULL);
#endif

  if (format) 
  {
    do_format ();
  }
  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();


#ifdef FILESYS
  cache_destroy ();
  
  //cache_write_behind ();
  //q_destroy ();
#endif
  //free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, enum inode_type type) 
{
  //printf ("[filesys_create], name: %s, initial_size %d, inode_type: %d\n", name, initial_size, type);
  block_sector_t inode_sector = 0;
  char *last_name = NULL;
  struct inode *inode = NULL;
  struct dir *dir = dir_open_path (name, &last_name);
  
  if (dir == NULL) 
  {
    if (last_name != NULL)
    {
      printf ("dir is NULL but last name is not null!!\n");
    }
    return false;
  }
  
  if (last_name == NULL)
  { 
    dir_close (dir);
    return false;
  }
  
  /* Last name not null 
   * So check dir has last_name already */
  dir_lookup (dir, last_name, &inode); 

  /* Found the inode with last name, thus create failed */
  if (inode != NULL)
  {
    dir_close (dir);
    free (last_name);
    return false;
  }

  /* Allocate one sector for inode and create inode */
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, type)
                  && dir_add (dir, last_name, inode_sector));
  
  if (!success && inode_sector != 0) 
  {  
    free_map_release (inode_sector, 1);
  }
  
  dir_close (dir);
  free (last_name);
  
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
  //printf ("[filesys open], name: %s\n", name);
  char *last_name = NULL;
  struct inode *inode = NULL;
  struct dir *dir = dir_open_path (name, &last_name);
  
  /* Dir is null */ 
  if (dir == NULL)
  {
    return NULL;
  }

  /* Last name is null */
  if (last_name == NULL)
  { 
    dir_close (dir);
    /* 전체 name이 "/"인지 아닌지 구분하기 위해 */
    if (strlen (name) != 1)
    {
      return NULL;
    }
    return file_open (inode_open (ROOT_DIR_SECTOR));
  }
  
  dir_lookup (dir, last_name, &inode);
  dir_close (dir);

  if (inode == NULL)
  {
    free (last_name);
    return NULL;
  }
  
  free (last_name); 

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //printf ("[filesys remove], name: %s\n", name);
  char *last_name = NULL;
  struct dir *dir = dir_open_path (name, &last_name);
  bool success = true;
  /* Dir is null */
  if (dir == NULL)
  { 
    printf ("dir is NULL\n");
    return false;
    success = false;
  }
  /* Last name is null */
  if (last_name == NULL)
  {
    dir_close (dir);
    return false;
  }

  success = success && dir_remove (dir, last_name);
  
  dir_close (dir); 
  free (last_name);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 7))
    PANIC ("root directory creation failed");
  
  struct dir *root = dir_open_root ();
  /* '.' and '..' for root directory */
  dir_add (root, ".", ROOT_DIR_SECTOR); 
  dir_add (root, "..", ROOT_DIR_SECTOR);
  
  dir_close (root);
  
  free_map_close ();
  printf ("done.\n");
}
