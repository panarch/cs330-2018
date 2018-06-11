#include "devices/block.h"

void cache_init (void);
void cache_read (struct block *, block_sector_t, void *);
void cache_write (struct block *, block_sector_t, const void *);
void cache_flush (block_sector_t);
void cache_flush_all (void);

