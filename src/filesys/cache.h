#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stddef.h>
#include <stdbool.h>

void cache_init (void);
void cache_destory (void);
struct cache_entry *cache_find (block_sector_t);
void *cache_read (block_sector_t);

#endif /* filesys/cache.h */
