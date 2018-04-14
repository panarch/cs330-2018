#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
static void syscall_exit (struct intr_frame *);
static void syscall_exec (struct intr_frame *);
static void syscall_wait (struct intr_frame *);
static void syscall_create (struct intr_frame *);
static void syscall_open (struct intr_frame *);
static void syscall_write (struct intr_frame *);

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
    case SYS_OPEN:
      syscall_open(f);
      break;
    case SYS_WRITE:
      syscall_write(f);
      break;
    default:
      printf ("system call! %d\n", *syscall_number);
      break;
  }

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
  unsigned initial_size = *(esp + 3);

  f->eax = filesys_create (name, initial_size);
}

static void
syscall_open (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *name = (char *)*(esp + 1);

  struct file *file = filesys_open (name);

  if (!file)
  {
    f->eax = -1;
    return;
  }

  f->eax = thread_file_open (file);
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
  }
}
