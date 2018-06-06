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
//#define DIRECT_BLOCK 124
//#define INDIRECT_BLOCK 1
//#define DOUBLY_INDIRECT_BLOCK 1
//#define INDEX_BLOCK 128

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock extension_lock;         /* Extension lock */
    //struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
/*
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
}
*/

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
inode_create (block_sector_t sector, off_t length)
{
  //printf ("thread%d, inode_create, sector : %d\n", thread_current ()->tid, sector);
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  struct index_disk *disk_index = NULL;
  if (sector == 227)
  {
    //printf ("inode_create sector%d, length %d\n", sector, length);
  }
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  ASSERT (sizeof *disk_index == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    size_t sectors = bytes_to_sectors (length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    
    /* Calculate number of needed sector */
    size_t sector_needed = sectors;
    if (sectors > DIRECT_BLOCK) 
    {
      sector_needed++;
    }
    if (sectors > DIRECT_BLOCK + INDEX_BLOCK)
    {
      sector_needed += DIV_ROUND_UP(sectors - DIRECT_BLOCK - INDEX_BLOCK, INDEX_BLOCK);
    }
    if (sector == 227)
    {
      //printf ("sectors: %d, sector_needed: %d\n", sectors, sector_needed);
    }
    /* Available space in file disk or not */
    if (free_map_left () >= sector_needed)
    {
      static char zeros[BLOCK_SECTOR_SIZE];

      size_t sector_remained = sectors;
      /* Number of block in direct indexing */
      size_t direct_cnt = DIRECT_BLOCK > sector_remained?
        sector_remained : DIRECT_BLOCK;
      size_t i;
      /* Allocate for direct block */
      for (i = 0; i < direct_cnt; i++)
      {
        free_map_allocate (1, &disk_inode->direct[i]);
        cache_write_at (disk_inode->direct[i], zeros, BLOCK_SECTOR_SIZE, 0); 
        //printf ("allocate direct block at  %d\n", disk_inode->direct[i]);
        sector_remained--;
      }
      
      /* Indirect block needed? */
      if (sector_remained > 0)
      {
        /* Allocate index_disk for indirect index block */
        struct index_disk *indirect_index = calloc (1, sizeof *disk_index);
        if (indirect_index != NULL)
        {
          /* Write indirect index */
          free_map_allocate (1, &disk_inode->indirect[0]);
          //printf ("allocate indirect index block at %d\n", disk_inode->indirect[0]);
          /* Number of block in indirect indexing */
          size_t indirect_cnt = INDEX_BLOCK > sector_remained?
            sector_remained : INDEX_BLOCK;
          //printf ("indirect_cnt : %d\n", indirect_cnt);
          size_t i;
          /* Allocate for indirect block */
          for (i = 0; i < indirect_cnt; i++)
          {
            free_map_allocate (1, &indirect_index->index[i]);
            cache_write_at (indirect_index->index[i], zeros, BLOCK_SECTOR_SIZE, 0);
            //printf ("allocate indirect block at %d\n", indirect_index->index[i]);
            sector_remained--;
          }
          cache_write_at (disk_inode->indirect[0],
              indirect_index, BLOCK_SECTOR_SIZE, 0);
          
          for (i = 0; i < indirect_cnt; i++)
          {
            //printf ("indirect_index->index[%d] = %d\n", i, indirect_index->index[i]);
          }
          free (indirect_index);

          /* Doubly indirect block needed? */
          if (sector_remained > 0)
          {
            //printf ("doubly indirect block needed?\n");
            /* Allocate index_disk for doubly indirect index block */
            struct index_disk *doubly_indirect_index = calloc (1, sizeof *disk_index);
            if (doubly_indirect_index != NULL)
            {
              /* Write doubly indirect index */
              free_map_allocate (1, &disk_inode->doubly_indirect[0]);
              size_t k = 0;
              /* */
              //printf ("sector_remained : %d\n", sector_remained);
              //printf ("doubly indirect sector : %d\n", disk_inode->doubly_indirect[0]);
              while (sector_remained > 0)
              {
                struct index_disk *doubly_indirect_indirect_index =
                  calloc (1, sizeof *disk_index);
                if (doubly_indirect_indirect_index != NULL)        
                {
                  /* Write doubly indirect indirect index */
                  free_map_allocate (1, &doubly_indirect_index->index[k]);
                  //printf ("doubly indirect indirect sector: %d\n", doubly_indirect_index->index[k]);
                  size_t doubly_indirect_cnt = INDEX_BLOCK > sector_remained? 
                    sector_remained : INDEX_BLOCK;
                  size_t i;
                  /* Allocate for doubly indirect block */
                  for (i = 0; i < doubly_indirect_cnt; i++)
                  {
                    free_map_allocate (1, &doubly_indirect_indirect_index->index[i]);
                    //printf ("doubly indirect indirect index sector: %d\n", 
                    //    doubly_indirect_indirect_index->index[i]);
                    cache_write_at (doubly_indirect_indirect_index->index[i],
                        zeros, BLOCK_SECTOR_SIZE, 0);
                    sector_remained--;
                  }
                  //printf ("doubly indirect indirect index saved in %d\n", doubly_indirect_index->index[k]);
                  cache_write_at (doubly_indirect_index->index[k],
                      doubly_indirect_indirect_index, BLOCK_SECTOR_SIZE, 0);
                  free (doubly_indirect_indirect_index);
                  k++;
                }
              }
              cache_write_at (disk_inode->doubly_indirect[0],
                  doubly_indirect_index, BLOCK_SECTOR_SIZE, 0);
              free (doubly_indirect_index);
            }
          }
        }
      }
      cache_write_at (sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
      free (disk_inode);
      success = true;
    }

    /* 
    if (free_map_allocate (sectors, &disk_inode->start)) 
    {
          
      //bounce = cache_read (sector);
      //cache_read_ahead (sector + 1);
      //memcpy (bounce, disk_inode, BLOCK_SECTOR_SIZE);
      //cache_set_dirty (sector); 
      cache_write_at (sector, disk_inode, BLOCK_SECTOR_SIZE, 0, false);
      //block_write (fs_device, sector, disk_inode);
      if (sectors > 0) 
      {
        static char zeros[BLOCK_SECTOR_SIZE];
        size_t i;
              
        for (i = 0; i < sectors; i++)
        {
                
          //bounce = cache_read (disk_inode->start + i);
          //cache_read_ahead (disk_inode->start + i + 1);
          //memcpy (bounce, zeros, BLOCK_SECTOR_SIZE); 
          cache_write_at (disk_inode->start + i, zeros, BLOCK_SECTOR_SIZE, 0, true);
          //block_write (fs_device, disk_inode->start + i, zeros);
        }
      }
      success = true; 
    } 
    free (disk_inode);
    */
  }
  //printf ("inode create success? %s\n", success? "true" : "false");
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  //printf ("thread%d inode_open, sector :%d\n", thread_current ()->tid, sector);
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

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init (&inode->extension_lock);  
  //block_read (fs_device, inode->sector, &inode->data);
  /*
  bounce = cache_read (sector);
  cache_read_ahead (sector + 1);
  memcpy (&inode->data, bounce, BLOCK_SECTOR_SIZE); */
  
  //cache_read_at (&inode->data, sector, BLOCK_SECTOR_SIZE, 0, false);
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
  //printf ("aaa\n");
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  
  /* Save inode's block to disk */
  //cache_save_inode (inode->sector);
  cache_write_behind ();
  

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          cache_close_inode (inode->sector);
          /*
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
          */
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
  //printf ("inode sector : %d, size : %d, offset : %d\n", inode->sector, size, offset);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = cache_byte_to_sector (inode->sector, offset);
      if (sector_idx == -1)
      {
        //printf ("sector_idx is -1...\n");
        //printf ("inode_sector : %d, size : %d, offset: %d\n", inode->sector, size, offset);
        break;
        //return bytes_read;
      }
      //printf ("need to read sector %d\n");
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      bool read_ahead_needed = false;
      block_sector_t next_sector_idx = 0;
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
        //printf ("read ahead needed?\n");
        next_sector_idx = cache_byte_to_sector (inode->sector, offset + chunk_size);
        read_ahead_needed = true;
      }

      /*
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
           Read full sector directly into caller's buffer. 
          block_read (fs_device, sector_idx, buffer + bytes_read);
          buffer + bytes_read = cache_read (sector_idx);
          memcpy (buffer _ + bytes_read, 
        }
      else 
        {
           Read sector into bounce buffer, then partially copy
             into caller's buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      */
      
      //printf ("In inode_read_at, sector_idx: %d\n", sector_idx);
      cache_read_at (buffer + bytes_read, sector_idx, chunk_size, sector_ofs, 
          next_sector_idx, read_ahead_needed);
      /*
      bounce = cache_read (sector_idx);
      cache_read_ahead (sector_idx + 1);
      memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      */

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce);

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

  if (inode->deny_write_cnt)
    return 0;

  //cache_inode_extend (inode->sector, size, offset);
  inode_extend (inode, size + offset); 
  
  while (size > 0) 
    {
      //cache_inode_extend (inode->sector, size, offset);
      
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = cache_byte_to_sector (inode->sector, offset);
      if (sector_idx == 1835353407)
      {
        printf ("inode_write_at inode->sector: %d, offset: %d, sector_idx: %d\n", inode->sector, offset, sector_idx);
      }
      //printf ("sector_idx: %d\n", sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      /*
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          // Write full sector directly to disk. 
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          // We need a bounce buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

           If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. 
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }
      */

      cache_write_at (sector_idx, buffer + bytes_written , chunk_size, sector_ofs);

      /*
      bounce = cache_read (sector_idx);
      cache_read_ahead (sector_idx + 1);
      memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      cache_set_dirty (sector_idx);
      */
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  //free (bounce);

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
  //return inode->data.length;
}

void
inode_extend (struct inode *inode, size_t new_pos)
{
  lock_acquire (&inode->extension_lock);
  cache_inode_extend (inode->sector, new_pos);
  lock_release (&inode->extension_lock);
}

