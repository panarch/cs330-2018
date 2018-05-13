#include "vm/swap.h"
#include <bitmap.h>
#include <stdio.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#define BLOCK_SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct lock lock;
static struct bitmap *used_map;
static struct block *block;

void
swap_init (void)
{
  size_t size = 0;
  block = block_get_role (BLOCK_SWAP);

  if (block != NULL)
    size = block_size (block) / BLOCK_SECTORS_PER_PAGE;

  used_map = bitmap_create (size);
  lock_init (&lock);
}

void
swap_in (struct page *page)
{
  lock_acquire (&lock);

  int i;

  for (i = 0; i < BLOCK_SECTORS_PER_PAGE; i++)
    {
      block_sector_t sector = page->swap_idx * BLOCK_SECTORS_PER_PAGE + i;
      void *buffer = page->kaddr + BLOCK_SECTOR_SIZE * i;

      block_read (block, sector, buffer);
    }

  bitmap_reset (used_map, page->swap_idx);

  page->swap_idx = -1;
  page->is_swapped = false;

  lock_release (&lock);
}

void
swap_out (struct page *page)
{
  lock_acquire (&lock);

  size_t swap_idx = bitmap_scan_and_flip (used_map, 0, 1, false);

  int i;

  for (i = 0; i < BLOCK_SECTORS_PER_PAGE; i++)
    {
      block_sector_t sector = swap_idx * BLOCK_SECTORS_PER_PAGE + i;
      const void *buffer = page->kaddr + BLOCK_SECTOR_SIZE * i;

      block_write (block, sector, buffer);
    }

  pagedir_clear_page (page->owner->pagedir, page->uaddr);
  palloc_free_page (page->kaddr);

  page->swap_idx = swap_idx;
  page->kaddr = NULL;
  page->is_loaded = false;
  page->is_swapped = true;

  lock_release (&lock);
}

