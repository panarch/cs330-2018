#include "vm/swap.h"
#include <bitmap.h>
#include <stdio.h>
#include "devices/block.h"
#include "threads/vaddr.h"

static struct bitmap *used_map;

void swap_init (void)
{
  size_t size = 0;
  struct block *block = block_get_role (BLOCK_SWAP);

  if (block != NULL)
    size = block_size (block) * BLOCK_SECTOR_SIZE / PGSIZE;

  used_map = bitmap_create (size);
}

void
swap_in (void)
{
}

void
swap_out (void)
{
}

