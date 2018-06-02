#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stddef.h>
#include <stdbool.h>
#include "devices/block.h"
#include "filesys/off_t.h"

void cache_init (void);
void cache_destroy (void);
struct cache_entry *cache_find (block_sector_t);
struct cache_entry *cache_get_block (block_sector_t);
void read_aheader_func (void);
void cache_read_ahead (block_sector_t);
void flusher_func (void);
void cache_write_behind (void);
void q_destroy (void);
off_t cache_read_at (void*, block_sector_t, off_t size, off_t offset, bool ahead);
off_t cache_write_at (block_sector_t, void*, off_t size, off_t offset, bool ahead);
void cache_close_inode (block_sector_t);
off_t cache_inode_length (block_sector_t);
#endif /* filesys/cache.h */
