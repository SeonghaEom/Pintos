#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, enum inode_type type)
{
  //printf ("inode_create %d, length %d, type %d\n", sector, length, type);
  struct inode_disk *inode_id = NULL;
  struct index_disk *index_id = NULL;
  bool success = false;
  
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *inode_id == BLOCK_SECTOR_SIZE);
  ASSERT (sizeof *index_id == BLOCK_SECTOR_SIZE);
  
  inode_id = calloc (1, sizeof *inode_id);
  if (inode_id != NULL)
  {
    size_t sectors = bytes_to_sectors (length);
    //printf ("sectors: %d\n", sectors);
    inode_id->length = length;
    inode_id->magic = INODE_MAGIC;
    inode_id->type = type;
    /* Calculate number of needed sector for inode */
    size_t sector_needed = sectors;
    if (sectors > DIRECT_BLOCK) 
    {
      sector_needed++;
    }
    if (sectors > DIRECT_BLOCK + INDEX_BLOCK)
    {
      sector_needed += DIV_ROUND_UP(sectors - DIRECT_BLOCK - INDEX_BLOCK, INDEX_BLOCK);
    }
    
    /* Available space in file disk or not */
    if (free_map_left () >= sector_needed)
    {
      //printf ("available space in disk\n");
      /* zeros */
      static char zeros[BLOCK_SECTOR_SIZE];
      /* sector remained for data part of this inode */
      size_t sector_remained = sectors;
      /* Number of block in direct indexing */
      size_t direct_cnt = DIRECT_BLOCK > sector_remained? sector_remained : DIRECT_BLOCK;
      
      /* Allocate for direct block */
      size_t i;
      for (i = 0; i < direct_cnt; i++)
      {
        //printf ("Direct block %d\n", i);
        free_map_allocate (1, &inode_id->direct[i]);
        cache_write_at (inode_id->direct[i], zeros, BLOCK_SECTOR_SIZE, 0); 
        sector_remained--;
      }
      
      /* Indirect block needed? */
      if (sector_remained > 0)
      {
        /* Allocate index_disk for single indirect index sector */
        struct index_disk *si_id = calloc (1, sizeof *index_id);
        if (si_id != NULL)
        {
          /* Write indirect index */
          free_map_allocate (1, &inode_id->indirect[0]);
          /* Number of block in indirect indexing */
          size_t indirect_cnt = INDEX_BLOCK > sector_remained? sector_remained : INDEX_BLOCK;

          /* Allocate for indirect block */
          size_t i;
          for (i = 0; i < indirect_cnt; i++)
          {
            free_map_allocate (1, &si_id->index[i]);
            cache_write_at (si_id->index[i], zeros, BLOCK_SECTOR_SIZE, 0);
            sector_remained--;
          }
          /* Write single indirect index block in inode_id->indirect[0] */
          cache_write_at (inode_id->indirect[0], si_id, BLOCK_SECTOR_SIZE, 0);
          free (si_id);

          /* Doubly indirect block needed? */
          if (sector_remained > 0)
          {
            /* Allocate index_disk for doubly indirect index block */
            struct index_disk *di_id = calloc (1, sizeof *index_id);
            
            if (di_id != NULL)
            {
              /* Allocate  doubly indirect index */
              free_map_allocate (1, &inode_id->doubly_indirect[0]);
              /* Index of doubly indirect index */
              size_t k = 0;
              
              while (sector_remained > 0)
              {
                struct index_disk *dii_id = calloc (1, sizeof *index_id);
                if (dii_id != NULL)        
                {
                  free_map_allocate (1, &di_id->index[k]);
                  /* Number of block in current doubly indirect indirect index */ 
                  size_t doubly_indirect_cnt = INDEX_BLOCK > sector_remained? 
                    sector_remained : INDEX_BLOCK;
                  
                  /* Allocate for doubly indirect block */
                  size_t i;
                  for (i = 0; i < doubly_indirect_cnt; i++)
                  {
                    free_map_allocate (1, &dii_id->index[i]);
                    cache_write_at (dii_id->index[i], zeros, BLOCK_SECTOR_SIZE, 0);
                    sector_remained--;
                  }
                  /* Write doubly indirect indirect index block */
                  cache_write_at (di_id->index[k], dii_id, BLOCK_SECTOR_SIZE, 0);
                  free (dii_id);
                  k++;
                }
              }
              /* Write doubly indirect index block */
              cache_write_at (inode_id->doubly_indirect[0], di_id, BLOCK_SECTOR_SIZE, 0);
              free (di_id);
            }
          }
        }
      }
      cache_write_at (sector, inode_id, BLOCK_SECTOR_SIZE, 0);
      free (inode_id);
      //printf ("TLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\n");
      success = true;
    }
  }
  /* Return inode creat success or not */
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }
  /* No opened inode for given sector, should open it */ 
  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;


  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  struct inode_disk *inode_id = cache_get_data (sector);
  inode->type = inode_id->type;
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  if (inode->type == INODE_DIR)
  {
    inode->pos = 0;
  }
  lock_init (&inode->extension_lock);

  
  
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  
  /* Save inode's block to disk */
  cache_write_behind ();
  
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          /* Deallocate blocks in inode here */
          cache_close_inode (inode->sector);
        }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = cache_byte_to_sector (inode->sector, offset);
      /* Abnormal offset */
      if (sector_idx == -1)
      {
        //printf ("Strange offset in inode_read_at\n");
        //printf ("Inode length: %d, offset: %d, size: %d\n", inode_length (inode), offset, size);
        break;
      }
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      /* Used for read ahead */
      bool read_ahead_needed = false;
      block_sector_t next_sector_idx;
      
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      /* Next sector should be read ahead asynchronously? */
      if (size - chunk_size > 0 && inode_left - chunk_size > 0)
      {
        next_sector_idx = cache_byte_to_sector (inode->sector, offset + chunk_size);
        read_ahead_needed = true;
      }
      
      cache_read_at (buffer + bytes_read, sector_idx, chunk_size, sector_ofs, 
          next_sector_idx, read_ahead_needed);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  //printf ("inode in sector %d, length %d, size %d, offset %d\n", inode->sector, inode_length(inode),  size, offset);
  if (inode->deny_write_cnt)
  {
    //printf ("inode_write_at sector%d inode deny write cnt is %d\n", inode->sector, inode->deny_write_cnt);
    return 0;
  }

  /* First check that inode extension needed */
  inode_extend (inode, size + offset); 
  
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = cache_byte_to_sector (inode->sector, offset);
      
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_write_at (sector_idx, buffer + bytes_written , chunk_size, sector_ofs);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return cache_inode_length (inode->sector);
}

void
inode_extend (struct inode *inode, size_t new_pos)
{
  lock_acquire (&inode->extension_lock);
  cache_inode_extend (inode->sector, new_pos);
  lock_release (&inode->extension_lock);
}

