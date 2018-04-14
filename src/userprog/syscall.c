#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"


static void syscall_handler (struct intr_frame *);

/*young : total 13 system calls*/
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

static struct file* file_find (struct list files, int fd);




void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
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
//	  syscall_close(f);
	  break;
    default:
      printf ("system call! %d\n", *syscall_number);
      break;
  }

}

static void
syscall_halt (struct intr_frame *f UNUSED)
{
  shutdown_power_off();
}

static void
syscall_exit (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int exit_status = *(esp + 1);

  thread_current()->exit_status = exit_status;
  thread_exit();

  f->eax = exit_status;
}

static void
syscall_exec (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *cmdline = (char *)*(esp + 1);

  f->eax = process_execute (cmdline);
}

static void
syscall_wait (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  tid_t tid = *(esp + 1);

  process_wait (tid);
}

static void
syscall_create (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *name = (char *)*(esp + 1);
  int32_t initial_size = *(esp + 2);

  if (!name)
  {
    f->eax = -1;
    return;
  }

  f->eax = filesys_create (name, initial_size);
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
  return;
}

static void
syscall_open (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *name = (char *)*(esp + 1);

  if (!name)
  {
    f->eax = -1;
    return;
  }

  struct file *file = filesys_open (name);

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
  struct thread *cur;
  struct file *file;

  cur = thread_current();
  file = file_find (cur->files, fd);

  f->eax = file_length (file);

  return;

}

static void
syscall_read (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  char *buffer = (char *)*(esp + 2);
  unsigned size = *(esp + 3);

  struct file *file = thread_file_find (fd);

  if (!file)
  {
    f->eax = -1;
    return;
  }

  f->eax = file_read (file, buffer, size);
}

static void
syscall_write (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  char *buffer = (char *)*(esp + 2);
  unsigned size = *(esp + 3);
  struct thread *cur;
  struct file *file;
  cur = thread_current();

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

  file = file_find (cur->files, fd);

  if (!file)
  {
    f->eax = -1;
    return;
  }

  f->eax = file_write (file, buffer, size);
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

static struct file *
file_find (struct list files, int fd)
{
  struct file *file;
  struct list_elem *e;

  for (e = list_begin (&files) ; e != list_end (&files);
	   e = list_next (e))
  {
	file = list_entry (e, struct file, elem);
	if (file->fd == fd)
	{
	  break;
	}
  }

  return file;
}



