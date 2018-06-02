#include "filesys/cache.h"

void
cache_init (void)
{
}

void
cache_read (struct block *block, block_sector_t sector, void *buffer)
{
  block_read (block, sector, buffer);
}

void
cache_write (struct block *block, block_sector_t sector, const void *buffer)
{
  block_write (block, sector, buffer);
}

