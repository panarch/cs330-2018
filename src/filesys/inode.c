#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_FILE_MAGIC 0x494e4f44
#define INODE_DIR_MAGIC 0x494d4f34

#define TOTAL_SECTORS 127
#define DIRECT_SECTORS 123
#define INDIRECT_SECTORS 3

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                                         /* File size in bytes. */
    unsigned magic;                                       /* Magic number. */
    block_sector_t sectors[DIRECT_SECTORS];               /* Sector -> blob */
    block_sector_t indirect_sectors[INDIRECT_SECTORS];    /* Indirect_inode_disk -> sector -> blob */
  };

struct indirect_inode_disk
  {
    unsigned magic;
    block_sector_t sectors[TOTAL_SECTORS];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

static bool
fill_inode_disk_sector (struct inode *inode, off_t idx, bool fill_before)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  struct inode_disk *disk_inode = &inode->data;

  off_t direct_max_idx = DIRECT_SECTORS;
  off_t indirect_max_idx = direct_max_idx + INDIRECT_SECTORS * TOTAL_SECTORS;

  off_t indirect_idx = (idx - direct_max_idx) / TOTAL_SECTORS;
  off_t indirect_sector_idx = (idx - direct_max_idx) % TOTAL_SECTORS;

  if (idx < direct_max_idx)
    {
      if (disk_inode->sectors[idx] > 0)
        return false;

      free_map_allocate (1, &disk_inode->sectors[idx]);
      cache_write (fs_device, disk_inode->sectors[idx], zeros);
      cache_write (fs_device, inode->sector, disk_inode);
    }
  else if (idx < indirect_max_idx)
    {
      struct indirect_inode_disk *indirect_disk_inode;
      indirect_disk_inode = calloc (1, sizeof *indirect_disk_inode);

      if (disk_inode->indirect_sectors[indirect_idx] == 0)
        {
          indirect_disk_inode->magic = INODE_FILE_MAGIC;

          free_map_allocate (1, &disk_inode->indirect_sectors[indirect_idx]);
          cache_write (fs_device, disk_inode->indirect_sectors[indirect_idx], indirect_disk_inode);
        }

      cache_read (fs_device, disk_inode->indirect_sectors[indirect_idx], indirect_disk_inode);

      ASSERT (indirect_disk_inode->magic == INODE_FILE_MAGIC);

      if (indirect_disk_inode->sectors[indirect_sector_idx] == 0)
        {
          free_map_allocate (1, &indirect_disk_inode->sectors[indirect_sector_idx]);
          cache_write (fs_device, indirect_disk_inode->sectors[indirect_sector_idx], zeros);
          cache_write (fs_device, disk_inode->indirect_sectors[indirect_idx], indirect_disk_inode);
          free (indirect_disk_inode);
        }
      else
        {
          free (indirect_disk_inode);
          return false;
        }
    }

  while (fill_before && idx-- > 0)
  {
    if (!fill_inode_disk_sector (inode, idx, false))
      break;
  }

  return true;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (struct inode *inode, off_t pos, size_t size, bool is_write)
{
  ASSERT (inode != NULL);

  if (!is_write && pos >= inode->data.length)
    return -1;

  off_t direct_max_idx = DIRECT_SECTORS;
  off_t indirect_max_idx = direct_max_idx + INDIRECT_SECTORS * TOTAL_SECTORS;
  block_sector_t sector;
  struct indirect_inode_disk *indirect_disk_inode = NULL;

  off_t idx = pos / BLOCK_SECTOR_SIZE;
  size_t oft = pos % BLOCK_SECTOR_SIZE;

  if (is_write && pos >= inode->data.length)
    {
      size_t max_size = BLOCK_SECTOR_SIZE - oft;
      inode->data.length = pos + (size < max_size ? size : max_size);

      ASSERT (idx < DIRECT_SECTORS + TOTAL_SECTORS * INDIRECT_SECTORS);

      fill_inode_disk_sector (inode, idx, true);

      cache_write (fs_device, inode->sector, &inode->data);
    }

  if (idx < direct_max_idx)
    sector = inode->data.sectors[idx];
  else if (idx < indirect_max_idx)
    {
      indirect_disk_inode = calloc (1, sizeof *indirect_disk_inode);

      idx -= direct_max_idx;
      sector = inode->data.indirect_sectors[idx / TOTAL_SECTORS];

      cache_read (fs_device, sector, indirect_disk_inode);

      ASSERT (indirect_disk_inode->magic == INODE_FILE_MAGIC);

      if (idx >= TOTAL_SECTORS)
        idx -= TOTAL_SECTORS;

      sector = indirect_disk_inode->sectors[idx];

      free (indirect_disk_inode);
    }
  else
    sector = -1;

  return sector;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  ASSERT (length < (DIRECT_SECTORS + INDIRECT_SECTORS * TOTAL_SECTORS) * BLOCK_SECTOR_SIZE);

  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = is_dir ? INODE_DIR_MAGIC : INODE_FILE_MAGIC;

      static char zeros[BLOCK_SECTOR_SIZE];
      size_t i, j;
      size_t min_direct_sectors = DIRECT_SECTORS < sectors ? DIRECT_SECTORS : sectors;

      for (i = 0; i < min_direct_sectors; i++)
        {
          free_map_allocate (1, &disk_inode->sectors[i]);
          cache_write (fs_device, disk_inode->sectors[i], zeros);
        }

      if (sectors > DIRECT_SECTORS)
        {
          sectors -= DIRECT_SECTORS;

          size_t min_indirect_sectors = sectors / TOTAL_SECTORS + 1;

          for (i = 0; i < min_indirect_sectors; i++)
            {
              struct indirect_inode_disk *indirect_disk_inode = calloc (1, sizeof *indirect_disk_inode);
              indirect_disk_inode->magic = INODE_FILE_MAGIC;

              free_map_allocate (1, &disk_inode->indirect_sectors[i]);

              size_t remain_sectors = sectors - TOTAL_SECTORS * i;
              if (remain_sectors > TOTAL_SECTORS)
                remain_sectors = TOTAL_SECTORS;

              for (j = 0; j < remain_sectors; j++)
                {
                  free_map_allocate (1, &indirect_disk_inode->sectors[j]);
                  cache_write (fs_device, indirect_disk_inode->sectors[j], zeros);
                }

              cache_write (fs_device, disk_inode->indirect_sectors[i], indirect_disk_inode);
              free (indirect_disk_inode);
            }
        }

      block_write (fs_device, sector, disk_inode);
      success = true;

      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  cache_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          // cache_flush (inode->sector);
          free_map_release (inode->sector, 1);
          size_t i;
          size_t cnt = bytes_to_sectors (inode->data.length);
          for (i = 0; i < cnt; i++)
            {
              // cache_flush (inode->data.sectors[i]);
              free_map_release (inode->data.sectors[i], 1);
            }
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, size, false);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          cache_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          cache_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, size, true);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          cache_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            cache_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cache_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

static bool
inode_disk_isdir (const struct inode_disk *disk_inode)
{
  return disk_inode->magic == INODE_DIR_MAGIC;
}

bool
inode_isdir (const struct inode *inode)
{
  return inode_disk_isdir (&inode->data);
}
