#include "filesys/cache.h"
#include <bitmap.h>
#include <string.h>
#include <stdio.h>
#include "threads/synch.h"
#include "filesys/filesys.h"

#define CACHE_MAX 64

static char cache_entries[CACHE_MAX * BLOCK_SECTOR_SIZE];
static block_sector_t cache_sectors[CACHE_MAX];

static struct lock lock;
static struct bitmap *used_map;
static struct bitmap *dirty_map;

static size_t pop_victim (void);
static size_t find_cache_idx (block_sector_t sector);

static size_t victim_idx;

void
cache_init (void)
{
  used_map = bitmap_create (CACHE_MAX);
  dirty_map = bitmap_create (CACHE_MAX);

  victim_idx = 0;

  lock_init (&lock);
}

void
cache_read (struct block *block, block_sector_t sector, void *buffer)
{
  lock_acquire (&lock);

  size_t cache_idx = find_cache_idx (sector);
  if (cache_idx == CACHE_MAX)
    {
      cache_idx = bitmap_scan_and_flip (used_map, 0, 1, false);

      if (cache_idx == BITMAP_ERROR)
        cache_idx = pop_victim ();
      else
        ASSERT (!bitmap_test (dirty_map, cache_idx));

      block_read (block, sector, &cache_entries[cache_idx * BLOCK_SECTOR_SIZE]);
      cache_sectors[cache_idx] = sector;
    }

  ASSERT (cache_idx < CACHE_MAX);

  memcpy (buffer, &cache_entries[cache_idx * BLOCK_SECTOR_SIZE], BLOCK_SECTOR_SIZE);

  lock_release (&lock);
}

void
cache_write (struct block *block, block_sector_t sector, const void *buffer)
{
  ASSERT (block != NULL);

  lock_acquire (&lock);

  size_t cache_idx = find_cache_idx (sector);
  if (cache_idx == CACHE_MAX)
    {
      cache_idx = bitmap_scan_and_flip (used_map, 0, 1, false);

      if (cache_idx == BITMAP_ERROR)
        cache_idx = pop_victim ();

      cache_sectors[cache_idx] = sector;

      ASSERT (!bitmap_test (dirty_map, cache_idx));
    }

  ASSERT (cache_idx < CACHE_MAX);

  bitmap_mark (dirty_map, cache_idx);
  memcpy (&cache_entries[cache_idx * BLOCK_SECTOR_SIZE], buffer, BLOCK_SECTOR_SIZE);

  lock_release (&lock);
}

static size_t
find_cache_idx (block_sector_t sector)
{
  size_t cache_idx;

  for (cache_idx = 0; cache_idx < CACHE_MAX; cache_idx++)
    {
      if (bitmap_test (used_map, cache_idx) && cache_sectors[cache_idx] == sector)
        break;
    }

  return cache_idx;
}

static void
incr_victim_idx (void)
{
  victim_idx++;

  if (victim_idx >= CACHE_MAX)
    victim_idx = 0;
}

static size_t
pop_victim ()
{
  ASSERT (bitmap_test (used_map, victim_idx) == true);

  bool is_dirty = bitmap_test (dirty_map, victim_idx);
  if (is_dirty)
    {
      block_sector_t sector = cache_sectors[victim_idx];
      block_write (fs_device, sector, &cache_entries[victim_idx * BLOCK_SECTOR_SIZE]);
    }

  bitmap_reset (dirty_map, victim_idx);

  size_t cache_idx = victim_idx;

  incr_victim_idx ();

  return cache_idx;
}

