#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stddef.h>
#include <stdbool.h>
#include "devices/block.h"
#include "filesys/off_t.h"

struct lock c_lock;

void cache_init (void);
void cache_destroy (void);
//void read_aheader_func (void);
//void flusher_func (void);
void cache_write_behind (void);
//void q_destroy (void);
off_t cache_read_at (void*, block_sector_t sector, off_t size,
    off_t offset, block_sector_t next_sector, bool ahead);
off_t cache_write_at (block_sector_t, void*, off_t size, off_t offset);
void cache_close_inode (block_sector_t);
off_t cache_inode_length (block_sector_t);
block_sector_t cache_byte_to_sector (block_sector_t, off_t);
void cache_inode_extend (block_sector_t, off_t);
enum inode_type cache_get_type (block_sector_t sector);
#endif /* filesys/cache.h */
