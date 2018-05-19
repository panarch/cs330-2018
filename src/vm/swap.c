#include "vm/swap.h"
#include <bitmap.h>
#include <stdio.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

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
//  printf ("swap_in before acquire, thread name : %s\n", thread_current ()->name);
  lock_acquire (&lock);
//  printf ("swap_in after acquire, thread name : %s\n", thread_current ()->name);

  int i;

  for (i = 0; i < BLOCK_SECTORS_PER_PAGE; i++)
    {
      block_sector_t sector = page->swap_idx * BLOCK_SECTORS_PER_PAGE + i;
      void *buffer = page->kaddr + BLOCK_SECTOR_SIZE * i;
	  
//	  printf ("swap in, for loop before block_read here %d\n", i);

      block_read (block, sector, buffer);

//	  printf ("swap in, for loop after block_read here %d\n", i);


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

  if (page->file != NULL && page->file->mapid > 0) // mmap file
    {
      int pos = file_tell (page->file);

      file_write (page->file, page->kaddr, PGSIZE);
      file_seek (page->file, pos);
    }
  else
    {
      size_t swap_idx = bitmap_scan_and_flip (used_map, 0, 1, false);

      int i;

      for (i = 0; i < BLOCK_SECTORS_PER_PAGE; i++)
        {
          block_sector_t sector = swap_idx * BLOCK_SECTORS_PER_PAGE + i;
          const void *buffer = page->kaddr + BLOCK_SECTOR_SIZE * i;

          block_write (block, sector, buffer);
        }

      page->swap_idx = swap_idx;
    }

  pagedir_clear_page (page->owner->pagedir, page->uaddr);
  palloc_free_page (page->kaddr);

  page->kaddr = NULL;
  page->is_loaded = false;
  page->is_swapped = true;

  lock_release (&lock);
}

void
swap_free (struct page *page)
{
  lock_acquire (&lock);

  bitmap_reset (used_map, page->swap_idx);
  page->is_swapped = false;

  lock_release (&lock);
}

