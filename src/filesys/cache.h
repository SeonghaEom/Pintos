#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stddef.h>
#include <stdbool.h>

void cache_init (void);
void cache_destroy (void);
struct cache_entry *cache_find (block_sector_t);
void *cache_read (block_sector_t);
void read_aheader_func (void);
void cache_read_ahead (block_sector_t);
void flusher_func (void);
void cache_write_behind (void);
void q_destroy (void);

#endif /* filesys/cache.h */
