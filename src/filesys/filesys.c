#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);
static struct dir *extract_dir (const char *name, char **filename);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  cache_init ();
  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  char *filename = NULL;
  block_sector_t inode_sector = 0;
  struct dir *dir = extract_dir (name, &filename);

  ASSERT (dir != NULL);
  ASSERT (filename != NULL);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)
                  && dir_add (dir, filename, inode_sector));

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  struct thread *cur = thread_current ();
  if (dir != cur->dir)
    dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *filename = NULL;
  struct dir *dir = extract_dir (name, &filename);
  struct inode *inode = NULL;

  if (!dir)
    return NULL;

  ASSERT (filename != NULL);

  dir_lookup (dir, filename, &inode);

  struct thread *cur = thread_current ();
  if (dir != cur->dir)
    dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  if (strlen (name) == 1 && name[0] == '/')
    return false;

  char *filename = NULL;
  struct dir *dir = extract_dir (name, &filename);
  struct thread *cur = thread_current ();

  if (dir == NULL)
    return false;

  bool success = dir != NULL && dir_remove (dir, filename);

  if (dir != cur->dir)
    dir_close (dir);

  return success;
}

bool
filesys_mkdir (const char *name)
{
  char *filename = NULL;
  char entry_name[NAME_MAX + 1];
  struct dir *dir = extract_dir (name, &filename);
  struct thread *cur = thread_current ();

  dir_seek (dir, 0);

  while (dir_readdir (dir, entry_name))
    {
      if (strcmp (filename, entry_name) == 0)
        return false;
    }

  block_sector_t sector;
  free_map_allocate(1, &sector);
  dir_create (sector, 16);

  ASSERT (dir != NULL);

  dir_add (dir, filename, sector);

  if (dir != cur->dir)
    dir_close (dir);

  return true;
}

bool
filesys_chdir (const char *name)
{
  char *filename = NULL;
  struct dir *dir = extract_dir (name, &filename);
  struct thread *cur = thread_current ();
  struct inode *inode;

  ASSERT (cur->dir != NULL);

  if (strcmp (filename, "/") == 0)
    inode = dir_get_inode (dir_open_root ());
  else if (!dir_lookup (dir, filename, &inode))
    return false;

  dir_close (cur->dir);
  cur->dir = dir_open (inode);

  return true;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

static struct dir *
extract_dir (const char *_name, char **filename)
{
  struct thread *cur = thread_current ();
  struct dir *dir;
  struct inode *inode;
  char *token, *save_ptr;
  char *prev_token = NULL;
  size_t name_length = strlen (_name) + 1;
  char *pre_name = malloc (name_length);
  memcpy (pre_name, _name, name_length);

  char *name = pre_name;

  if (cur->dir == NULL || name[0] == '/')
    {
      cur->dir = dir_open_root ();

      if (pre_name[0] == '/')
        name++;
    }

  dir = cur->dir;

  for (token = strtok_r (name, "/", &save_ptr); token != NULL;
       token = strtok_r (NULL, "/", &save_ptr))
  {
    if (prev_token != NULL)
      {
        dir_lookup (dir, prev_token, &inode);

        if (dir != cur->dir)
          dir_close (dir);

        dir = dir_open (inode);

        if (!dir)
          return NULL;
      }

    prev_token = token;
  }

  ASSERT (prev_token != NULL);

  name_length = strlen (prev_token) + 1;
  *filename = malloc (name_length);
  memcpy (*filename, prev_token, name_length);

  ASSERT (dir != NULL);

  free (pre_name);
  return dir;
}
