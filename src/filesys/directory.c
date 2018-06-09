#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "devices/block.h"
#include "threads/thread.h"

#define DELIM "/"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  //printf ("dir create sector: %d, entry cnt: %d\n", sector, entry_cnt);
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), INODE_DIR);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  // TODO inode가 디렉토리를 나타내는지 확인은 안해줘도 되는가? 
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = inode->pos;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  //printf ("sector: %d, name: %s\n", dir->inode->sector, name);
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
  {
    //printf ("name: %s, e.name: %s\n", name, e.name);
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;



  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
  {
    //printf ("lookup success\n");
    *inode = inode_open (e.inode_sector);
  }
  else
  {
    //printf ("lookup failed\n");
    *inode = NULL;
  }

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  //printf ("dir add\n");
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
  {  
    //printf ("dir_add success : %s\n", success? "true" : "false");
    return false;
  }
  
  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;
  //printf ("name is not used\n");
  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
  {
    if (!e.in_use)
    {
      break;
    }
  }
  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = (inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e);

 done:
  //printf ("dir_add success : %s\n", success? "true" : "false");
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  //printf ("dir_removed, %s\n", name);
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  if (inode->sector == thread_current ()->dir_sector)
  {
    //printf ("We are trying to remove CWD\n");
    thread_current ()->dir_removed = true;
  }
  
  /* Try to check that directory is empty or not.
   * We can only remove empty directory */
  if (inode->type == INODE_DIR)
  {
    //printf ("lets remove directory inode\n");
    /* Open new inode and use it to open directory */
    struct dir *r_dir = dir_open (inode_open (inode->sector));
    char buf[NAME_MAX + 1];  
    char *name = buf;
    /* Directory has file other than '.', '..' */
    if (dir_readdir (r_dir, name))
    {
      //printf ("dir is not empty\n");
      //inode_close (inode);
      dir_close (r_dir);
      return false;
    }
    /* Directory is empty, we can remove it */
    else
    {
      dir_close (r_dir);
      //printf ("dir is empty, can remove\n");
      //inode_close (inode);
      //return false;
    }
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
  {
    printf ("Try to inode write the erasing directory entry failed\n");
    goto done;
  }
  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  //printf ("dir readdir!\n");
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      //printf ("dir->pos : %d\n", dir->pos);
      dir->pos += sizeof e;
      dir->inode->pos += sizeof e;
      /* We don't read ".", ".." on readdir */
      if (strcmp (".", e.name) == 0 || strcmp ("..", e.name) == 0)
      {
        continue;
      }
      /* We found in use entry */
      if (e.in_use)
        {
          /* Copy it to given NAME and return true */
          strlcpy (name, e.name, NAME_MAX + 1);
          //printf ("dir readdir : %s\n", e.name);
          //printf ("name : %s\n", name);
          return true;
        } 
    }
  return false;
}

/* Open and return directory that the file is in and store in LAST_TOKEN*/
struct dir *
dir_open_path (const char *file, char **last_token)
{
  //printf ("dir open path, file: %s\n", file);
 
  char *file_copy = (char *) malloc (strlen (file) + 1);
  char *save_ptr;
  //printf ("file: %s\n", file);
  strlcpy (file_copy, file, strlen (file) + 1);
  //printf ("file_copy: %s\n", file_copy);
  struct inode *inode;
  char *current_token = strtok_r (file_copy, DELIM, &save_ptr);
  //printf ("save ptr: %s\n", *save_ptr);
  //printf ("current_token: %s\n", current_token);
  char *next_token = strtok_r (NULL, DELIM , &save_ptr);
  //printf ("next_token: %s\n", next_token);
  /*
  printf ("address of file copy: %x\n", file_copy);
  printf ("address of current token: %x\n", current_token);
  printf ("address of next token: %x\n", next_token);
  printf ("file copy: %s\n", file_copy);
  printf ("current token: %s\n", current_token);
  printf ("next token: %s\n", next_token);
  */
  struct dir *directory;
  
  /* File copy is null */
  if (file_copy == NULL)
  {
    //printf ("file copy is null\n");
    /* TODO last_token 지정하는 것은? */
    directory = dir_open_root ();
    return directory;
  }
  /* File copy is empty maybe. */
  else if (current_token == NULL)
  {
    //printf ("file copy is maybe empty\n");
    /* TODO last_token 지정? */
    free (file_copy);
    directory = dir_open_root ();
    return directory;
  }
  /* This is the case for absolute path */
  else if ((char) *file == '/')
  {
    //printf ("thread%d: %d\n", thread_current ()->tid, thread_current ()->dir_sector);
    directory = dir_open_root ();
  }
  /* THis is the case for relative path */
  else 
  {
    //printf ("Relative path\n");
    //printf ("thread_current cur dir %x\n", thread_current ()->cur_dir);
    /* Can we start with relative path?
     * We need to check that cwd is removed or not */
    if (thread_current ()->dir_removed)
    {
      //printf ("thread current CWD is removed\n");
      free (file_copy);
      return NULL;
    }
    /* Open CWD */
    directory = dir_open (inode_open (thread_current ()->dir_sector)); //thread_current ()->cur_dir;
    //printf ("current_token: %s\n", current_token);
    //printf ("next_token: %s\n", next_token);
  } 
  //printf ("Start parsing...\n");
  /* While next token is not null, should explore directory deeply */
  while (next_token)
  {
    //printf ("dir_lookup, current_token %s\n", current_token);
    if (dir_lookup (directory, current_token, &inode))
    {
      /* Directory, let's move into */
      if (inode->type == INODE_DIR)
      {
        dir_close (directory);
        directory = dir_open (inode);
      }
      /* The current token should be directory but is file */
      else if (inode->type == INODE_FILE)
      {
        //printf ("dir_open_path: %s should be directory not file\n", current_token);
        dir_close (directory);
        free (file_copy);
        return NULL;
      }
      else
      {
        PANIC ("Found inode type should be INODE DIR or INODE FILE");
      }
    }
    /* Current token is not in directory */
    else
    {
      //printf ("dir_open_path: %s is not in directory\n", current_token);
      dir_close (directory);
      free (file_copy);
      return NULL;
    }
    //printf ("adfdafdafa\n");
    // Advance
    current_token = next_token;
    next_token = strtok_r (NULL, "/", &save_ptr);
  }
  //printf ("almost ends in dir open path\n");
  //printf ("current_token: %s\n", current_token);
  /* Finally get here, need to store last token */
  *last_token = (char *) malloc (strlen (current_token) + 1);
  strlcpy (*last_token, current_token, strlen (current_token) + 1);
  free (file_copy);
  return directory;
}

