#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void syscall_write (struct intr_frame *);
static void syscall_exit (struct intr_frame *);

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
    case SYS_WRITE:
      syscall_write(f);
      break;
    case SYS_EXIT:
      syscall_exit(f);
      break;
    default:
      printf ("system call!\n");
      break;
  }

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

static void
syscall_exit (struct intr_frame *f UNUSED)
{
  thread_exit();
  f->eax = -1;
}
