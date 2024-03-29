#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"

static void syscall_handler (struct intr_frame *);

static void syscall_halt (struct intr_frame *);
static void syscall_exit (struct intr_frame *);
static void syscall_exec (struct intr_frame *);
static void syscall_wait (struct intr_frame *);
static void syscall_create (struct intr_frame *);
static void syscall_remove (struct intr_frame *);
static void syscall_open (struct intr_frame *);
static void syscall_filesize (struct intr_frame *);
static void syscall_read (struct intr_frame *);
static void syscall_write (struct intr_frame *);
static void syscall_seek (struct intr_frame *);
static void syscall_tell (struct intr_frame *);
static void syscall_close (struct intr_frame *);
static void syscall_mmap (struct intr_frame *);
static void syscall_munmap (struct intr_frame *);
static void syscall_chdir (struct intr_frame *);
static void syscall_mkdir (struct intr_frame *);
static void syscall_readdir (struct intr_frame *);
static void syscall_isdir (struct intr_frame *);
static void syscall_inumber (struct intr_frame *);

struct lock file_lock;

static void
syscall_file_lock_acquire (void)
{
  lock_acquire (&file_lock);
}

static void
syscall_file_lock_release (void)
{
  lock_release (&file_lock);
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init (&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  thread_set_esp (f->esp);

  int *syscall_number = f->esp;

  switch (*syscall_number)
  {
    case SYS_HALT:
      syscall_halt(f);
      break;
    case SYS_EXIT:
      syscall_exit(f);
      break;
    case SYS_EXEC:
      syscall_exec(f);
      break;
    case SYS_WAIT:
      syscall_wait(f);
      break;
    case SYS_CREATE:
      syscall_create(f);
      break;
    case SYS_REMOVE:
      syscall_remove(f);
      break;
    case SYS_OPEN:
      syscall_open(f);
      break;
    case SYS_FILESIZE:
      syscall_filesize(f);
      break;
    case SYS_READ:
      syscall_read(f);
      break;
    case SYS_WRITE:
      syscall_write(f);
      break;
    case SYS_SEEK:
      syscall_seek(f);
      break;
    case SYS_TELL:
      syscall_tell(f);
      break;
    case SYS_CLOSE:
      syscall_close(f);
      break;
    case SYS_MMAP:
      syscall_mmap(f);
      break;
    case SYS_MUNMAP:
      syscall_munmap(f);
      break;
    case SYS_CHDIR:
      syscall_chdir(f);
      break;
    case SYS_MKDIR:
      syscall_mkdir(f);
      break;
    case SYS_READDIR:
      syscall_readdir(f);
      break;
    case SYS_ISDIR:
      syscall_isdir(f);
      break;
    case SYS_INUMBER:
      syscall_inumber(f);
      break;
    default:
      ASSERT (false);
      break;
  }

}

static void
syscall_halt (struct intr_frame *f UNUSED)
{
  shutdown_power_off();
}

void
syscall_exit_by_status (int exit_status)
{
  struct thread *cur = thread_current ();

  cur->exit_status = exit_status;
  thread_exit();
}

static void
syscall_exit (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int exit_status = *(esp + 1);
  if (exit_status >= PHYS_BASE)
  {
    syscall_exit_by_status (-1);
    return;
  }

  syscall_exit_by_status (exit_status);
}

static void
syscall_exec (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *cmdline = (char *)*(esp + 1);

  syscall_file_lock_acquire ();
  f->eax = process_execute (cmdline);
  syscall_file_lock_release ();
}

static void
syscall_wait (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  tid_t tid = *(esp + 1);

  f->eax = process_wait (tid);
}

static void
syscall_create (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *name = (char *)*(esp + 1);
  int32_t initial_size = *(esp + 2);

  if (!name || strlen (name) == 0)
  {
    syscall_exit_by_status (-1);
    f->eax = -1;
    return;
  }

  syscall_file_lock_acquire ();
  f->eax = filesys_create (name, initial_size);
  syscall_file_lock_release ();
}

static void
syscall_remove (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *name = (char *)*(esp +1);

  if (!name)
  {
    f->eax = -1;
    return;
  }

  f->eax = filesys_remove (name);
}

static void
syscall_open (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *name = (char *)*(esp + 1);

  if (!name || strlen (name) == 0)
  {
    f->eax = -1;
    return;
  }

  syscall_file_lock_acquire ();
  struct file *file = filesys_open (name);
  syscall_file_lock_release ();

  if (!file)
  {
    f->eax = -1;
    return;
  }

  f->eax = thread_file_add (file);
}

static void
syscall_filesize (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  struct file *file = thread_file_find (fd);

  if (!file)
  {
    f->eax = -1;
    return;
  }

  f->eax = file_length (file);
}

static void
syscall_read (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  char *buffer = (char *)*(esp + 2);
  unsigned size = *(esp + 3);

  struct file *file = thread_file_find (fd);

  if (buffer >= PHYS_BASE)
  {
    syscall_exit_by_status (-1);
    f->eax = -1;
    return;
  }

  if (!file)
  {
    f->eax = -1;
    return;
  }

  if (fd == 0)
  {
    unsigned _size = size;
    while (_size--)
    {
      *buffer = input_getc();
      buffer++;
    }

    f->eax = size;
    return;
  }

  // check stack_growth (exception.c check_stack_growth function)
  if (f->esp - (void *) buffer > 32)
    syscall_exit_by_status (-1);

  vm_pin_pages (buffer, size);

  syscall_file_lock_acquire ();
  f->eax = file_read (file, buffer, size);
  syscall_file_lock_release ();

  vm_unpin_pages (buffer, size);
}

static void
syscall_write (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  char *buffer = (char *)*(esp + 2);
  unsigned size = *(esp + 3);

  if (fd == 1)
  {
    putbuf(buffer, size);
    f->eax = (int)size;
    return;
  }
  else if (fd == 0){
    f->eax = -1;
    return;
  }

  struct file *file = thread_file_find (fd);

  if (!file)
  {
    f->eax = -1;
    return;
  }

  syscall_file_lock_acquire ();
  f->eax = file_write (file, buffer, size);
  syscall_file_lock_release ();
}

static void
syscall_seek (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  unsigned position = *(esp + 2);

  struct file *file = thread_file_find (fd);

  file_seek (file, position);
}

static void
syscall_tell (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);

  struct file *file = thread_file_find (fd);

  f->eax = file_tell (file);
}

static void
syscall_close (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);

  struct file *file = thread_file_find (fd);

  thread_file_remove (file);
  file_close (file);
}

static void
syscall_mmap (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  void *upage = *(esp + 2);

  struct file *file = thread_file_find (fd);

  if (file == NULL)
    {
      syscall_exit_by_status (-1);
      return;
    }

  syscall_file_lock_acquire ();
  int mapid = vm_mmap (upage, file);
  syscall_file_lock_release ();

  f->eax = mapid;
}

static void
syscall_munmap (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int mapid = *(esp + 1);

  syscall_file_lock_acquire ();
  vm_munmap (mapid);
  syscall_file_lock_release ();
}

static void
syscall_chdir (struct intr_frame *f)
{
  int *esp = f->esp;
  char *dirname = (char *)*(esp + 1);

  syscall_file_lock_acquire ();

  bool success = filesys_chdir (dirname);

  syscall_file_lock_release ();

  f->eax = success;
}

static void
syscall_mkdir (struct intr_frame *f)
{
  int *esp = f->esp;
  char *dirname = (char *)*(esp + 1);
  bool success = false;

  if (!dirname || strlen (dirname) == 0)
  {
    f->eax = false;
    return;
  }

  syscall_file_lock_acquire ();

  success = filesys_mkdir (dirname);

  syscall_file_lock_release ();

  f->eax = success;
}

static void
syscall_readdir (struct intr_frame *f)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  char *name = (char *)*(esp + 2);

  struct file *file = thread_file_find (fd);

  if (file == NULL)
    {
      syscall_exit_by_status (-1);
      return;
    }

  struct dir *dir = dir_open (file_get_inode (file));
  dir_seek (dir, file_tell (file));

  if (dir == NULL)
    {
      f->eax = false;
      return;
    }

  bool success = dir_readdir (dir, name);

  if (!success)
    {
      f->eax = false;
      return;
    }

  file_seek (file, dir_tell (dir));
  free (dir);

  f->eax = true;
}

static void
syscall_isdir (struct intr_frame *f)
{
  int *esp = f->esp;
  int fd = *(esp + 1);

  struct file *file = thread_file_find (fd);

  if (file == NULL)
    {
      syscall_exit_by_status (-1);
      return;
    }

  f->eax = file_isdir (file);
}

static void
syscall_inumber (struct intr_frame *f)
{
  int *esp = f->esp;
  int fd = *(esp + 1);

  struct file *file = thread_file_find (fd);

  if (file == NULL)
    {
      syscall_exit_by_status (-1);
      return;
    }

  f->eax = file_inumber (file);
}
