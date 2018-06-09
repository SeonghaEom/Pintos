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
    /* Set main thread's current directory */
    thread_current ()->dir_sector = ROOT_DIR_SECTOR; //dir_open_root ();
    thread_current ()->dir_removed = false;
  }

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
#ifdef FILESYS
  lock_acquire (&c_lock);
  cache_write_behind ();
  lock_release (&c_lock);
  cache_destroy ();

  //q_destroy ();
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

  //printf ("dir %x\n", dir);
  //printf ("enum size: %d\n", sizeof (enum inode_type));
  //bool a = (dir != NULL);
  //bool b = (free_map_allocate (1, &inode_sector));
  //bool c = inode_create (inode_sector, initial_size, 0);//type);
  //bool d = dir_add (dir, last_name, inode_sector);
  //printf ("which is false?: %s %s %s %s\n", a? "t":"f", b?"t":"f",c?"t":"f",d?"t":"f");
  //printf ("type : %d\n", type);
  //printf ("last name: %s\n", last_name);

  /* Allocate one sector for inode and create inode */
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, type)
                  && dir_add (dir, last_name, inode_sector));
  
  //printf ("inode_sector: %d\n", inode_sector);
  //printf ("success is %s\n", success? "true" : "false");
  if (!success && inode_sector != 0) 
  {  
    free_map_release (inode_sector, 1);
  }
  
  // TODO free what and when?
 
  //printf ("dir_close the %x\n", dir);
  
  //free (dir);
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
  //printf ("filesys open, name: %s\n", name);
  
  char *last_name = NULL;
  struct inode *inode = NULL;
  struct dir *dir = dir_open_path (name, &last_name);
  //printf ("last_name1: %s\n", last_name);
  //printf ("dir sector is %d\n", dir_get_inode (dir)->sector);
  //printf ("2\n");
  /* Dir is null */ 
  if (dir == NULL)
  {
    //printf ("dir is null\n");
    return NULL;
  }

  //printf ("last_name : %s\n", last_name);
  /* Last name is null */
  if (last_name == NULL)
  { 
    dir_close (dir);
    /* 전체 name이 "/"인지 아닌지 구분하기 위해 */
    if (strlen (name) != 1)
    {
      //printf ("last name is null and name is not \"/\"\n");
      return NULL;
    }
    //printf ("last name is null\n");
    printf ("open root\n");
    return file_open (inode_open (ROOT_DIR_SECTOR));
  }
  
  //printf ("filesys open2, name is %s\n", last_name);
  //printf ("dir->inode->sector: %d\n", dir_get_inode(dir)->sector);
  //printf ("A\n"); 
  dir_lookup (dir, last_name, &inode);
  //printf ("B\n");
  dir_close (dir);
  //printf ("C\n");
  //printf ("last_name2: %s\n", last_name);
  free (last_name); 
  /* If inode is null, there is no file with last_name */ 
  if (inode == NULL)
  {
    return NULL;
  }
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //printf ("filesys remove, name: %s\n", name);
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
    //printf ("last name is null\n");
    dir_close (dir);
    return false;
  }

  success = success && dir_remove (dir, last_name);
  
  dir_close (dir); 
  free (last_name);
  //printf ("filesys_remove success? %s\n", success? "true" : "false");
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
