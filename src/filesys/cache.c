#include "filesys/cache.h"
#include <bitmap.h>
#include <string.h>
#include <stdio.h>
#include "devices/timer.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"

#define CACHE_MAX 64

static void read_ahead_async (void *aux UNUSED);
static void flush_all_async (void *aux UNUSED);

static char cache_entries[CACHE_MAX * BLOCK_SECTOR_SIZE];
static block_sector_t cache_sectors[CACHE_MAX];

static struct lock lock;
static struct bitmap *used_map;
static struct bitmap *dirty_map;

static size_t pop_victim (void);
static size_t find_cache_idx (block_sector_t sector);

static block_sector_t read_ahead_sector;
static size_t victim_idx;

void
cache_init (void)
{
  used_map = bitmap_create (CACHE_MAX);
  dirty_map = bitmap_create (CACHE_MAX);

  victim_idx = 0;

  lock_init (&lock);

  thread_create ("read_ahead_async", PRI_DEFAULT, read_ahead_async, NULL);
  thread_create ("flush_all_async", PRI_DEFAULT, flush_all_async, NULL);
}

static size_t
find_and_cache_read (block_sector_t sector)
{
  size_t cache_idx = find_cache_idx (sector);
  if (cache_idx == CACHE_MAX)
    {
      cache_idx = bitmap_scan_and_flip (used_map, 0, 1, false);

      if (cache_idx == BITMAP_ERROR)
        cache_idx = pop_victim ();
      else
        ASSERT (!bitmap_test (dirty_map, cache_idx));

      block_read (fs_device, sector, &cache_entries[cache_idx * BLOCK_SECTOR_SIZE]);
      cache_sectors[cache_idx] = sector;
    }

  return cache_idx;
}

void
cache_read (struct block *block, block_sector_t sector, void *buffer)
{
  // block_read (block, sector, buffer);
  ASSERT (block == fs_device);

  lock_acquire (&lock);

  size_t cache_idx = find_and_cache_read (sector);

  ASSERT (cache_idx < CACHE_MAX);

  memcpy (buffer, &cache_entries[cache_idx * BLOCK_SECTOR_SIZE], BLOCK_SECTOR_SIZE);

  lock_release (&lock);

  read_ahead_sector = sector + 1;
}

void
cache_write (struct block *block, block_sector_t sector, const void *buffer)
{
  // block_write (block, sector, buffer);
  ASSERT (block == fs_device);
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

void
cache_flush (block_sector_t sector)
{
  size_t cache_idx = find_cache_idx (sector);

  if (cache_idx < CACHE_MAX)
    {
      victim_idx = cache_idx;
      pop_victim ();
    }
}

void
cache_flush_all ()
{
  lock_acquire (&lock);

  read_ahead_sector = CACHE_MAX;

  int i;

  for (i = 0; i < CACHE_MAX; i++)
    pop_victim ();

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

static void
read_ahead_async (void *aux UNUSED)
{
  block_sector_t sector;

  while (true)
    {
      sector = read_ahead_sector;

      if (sector == CACHE_MAX)
        timer_msleep (10);

      lock_acquire (&lock);

      find_and_cache_read (sector);
      read_ahead_sector = CACHE_MAX;

      lock_release (&lock);
    }
}

static void
flush_all_async (void *aux UNUSED)
{
  while (true)
    {
      timer_msleep (10000);

      cache_flush_all ();
    }
}
