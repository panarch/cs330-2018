#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "vm/vm.h"

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
static void syscall_mmap (struct intr_frame *);
static void syscall_munmap (struct intr_frame *);

static void syscall_lock_printf (void);

struct lock file_lock;

static void
syscall_file_lock_acquire (void)
{
  lock_acquire (&file_lock);
  printf ("syscall_file_lock_acquire %s %d\n", thread_current ()->name , thread_current ()->tid);
}

static void
syscall_file_lock_release (void)
{
  lock_release (&file_lock);
  printf ("syscall_file_lock_release %s %d\n", thread_current ()->name , thread_current ()->tid);
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
//  printf ("syscall number : %d\n", *syscall_number);

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

void
syscall_exit_by_status (int exit_status)
{
  struct thread *cur = thread_current ();
  printf ("syscall_exit_by_status, what is thread name ? %s %d\n", cur->name, cur->tid);

  cur->exit_status = exit_status;
  thread_exit();
}

static void
syscall_exit (struct intr_frame *f UNUSED)
{
  printf ("\nsyscall exit here?\n\n");
  int *esp = f->esp;
  int exit_status = *(esp + 1);
 
  printf ("syscall_exit, exit status : %d\n", exit_status);

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

  printf ("syscall_exec, cur thread is %s %d\n", thread_current()->name, thread_current()->tid);

  syscall_file_lock_acquire ();
  printf ("syscall_exec, lock_acquire success %s %d\n", thread_current()-> name, thread_current()->tid);
  printf ("syscall_exec, who hold lock? %s \n", file_lock.holder->name);
  printf ("syscall_exec, what is cmdline? %s\n", cmdline);
  f->eax = process_execute (cmdline);
  printf ("syscall_exec, before lock_release %s  %d\n", thread_current()-> name, thread_current()->tid);
  printf ("syscall_exec, lock_release success %s %d\n", thread_current()-> name, thread_current()->tid); 
  syscall_file_lock_release ();
}

static void
syscall_wait (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  tid_t tid = *(esp + 1);

//  syscall_file_lock_acquire ();
  f->eax = process_wait (tid);
//  syscall_file_lock_release ();
}

static void
syscall_create (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  char *name = (char *)*(esp + 1);
  int32_t initial_size = *(esp + 2);

  if (!name)
  {
    syscall_exit_by_status (-1);
    f->eax = -1;
    return;
  }

  printf ("syscall_create before lock_acquire %s %d\n", thread_current ()->name, thread_current ()->tid);
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

  if (!name)
  {
    f->eax = -1;
    return;
  }

  printf ("syscall_open, cur thread is %s %d\n", thread_current()->name, thread_current()->tid);
  printf ("syscall_open, open file name is %s\n", name);
//  printf ("syscall_open, cur thread list file size : %d\n", list_size (&thread_current ()->files));

  if (file_lock.holder !=NULL)
  {
    printf ("syscall_open, who holds lock? %s %d state : %d\n", file_lock.holder->name, file_lock.holder->tid, file_lock.holder->status);
  }

  syscall_lock_printf ();

  /*
  printf ("syscall_open, then how many threads are waiting lock? %d\n", list_size (&file_lock.semaphore.waiters));
  struct thread *t;
  struct list_elem *e;
  for (e = list_begin (&file_lock.semaphore.waiters);
	   e != list_end (&file_lock.semaphore.waiters);
	   e = list_next (e))
  {
	  t = list_entry (e, struct thread, elem);
	  printf ("syscall_open, waiting threads %s %d\n", t->name, t->tid);
  }
  */

  printf ("syscall_open, before lock_acquire, current thread is %s %d\n", thread_current ()->name, thread_current()->tid);
  syscall_file_lock_acquire ();
  printf ("syscall_open, lock_acquire success %s %d\n", thread_current()-> name, thread_current()->tid);
  printf ("syscall_open, who hold lock? %s %d\n", file_lock.holder->name, file_lock.holder->tid);
  printf ("syscall_open, what is file name? %s\n", name);
  printf ("syscall_open, curr thread is %s %d\n", thread_current ()->name, thread_current ()->tid);
  struct file *file = filesys_open (name);
  printf ("syscall_open, before lock_release %s  %d\n", thread_current()-> name, thread_current()->tid);
  syscall_lock_printf ();
  syscall_file_lock_release ();
  printf ("syscall_open, lock_release success %s %d\n", thread_current()-> name, thread_current()->tid);
  if (!file)
  {
    f->eax = -1;
//	syscall_file_lock_release ();;
    return;
  }

  f->eax = thread_file_add (file);
//  syscall_file_lock_release ();
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
  
  syscall_file_lock_acquire ();
  f->eax = file_length (file);
  syscall_file_lock_release ();
}

static void
syscall_read (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  char *buffer = (char *)*(esp + 2);
  unsigned size = *(esp + 3);

  if (buffer >= PHYS_BASE)
  {
    syscall_exit_by_status (-1);
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
  
  printf ("syscall_read what is cur thread ? %s %d\n", thread_current()->name, thread_current()->tid);

  struct file *file = thread_file_find (fd);

  if (!file)
  {
	printf ("syscall_read file is NULL? %s %d\n", thread_current ()->name, thread_current()->tid);
    f->eax = -1;
    return;
  }

  printf ("syscall_read before lock acquire %s %d\n", thread_current ()->name ,thread_current()->tid);

  if (file_lock.holder !=NULL)
  {
    printf ("syscall_read, who holds lock? %s %d state : %d\n", file_lock.holder->name, file_lock.holder->tid, file_lock.holder->status);
  }

  syscall_file_lock_acquire ();

  printf ("syscall_read, lock acquire success %s %d\n",thread_current()->name, thread_current()->tid);

//  struct file *file = thread_file_find (fd);
  /*
  if (!file)
  {
	f->eax = -1;
	syscall_file_lock_release ();
	return ;
  }
  */

  printf ("syscall_read file pos : %d\n", file->pos);
  f->eax = file_read (file, buffer, size);

  printf ("syscall_read before lock release %s %d\n", thread_current()->name, thread_current()->tid);
  syscall_file_lock_release ();
  printf ("syscall_read after lock release %s %d\n", thread_current ()->name, thread_current()->tid);
}

static void
syscall_write (struct intr_frame *f UNUSED)
{
  int *esp = f->esp;
  int fd = *(esp + 1);
  char *buffer = (char *)*(esp + 2);
  unsigned size = *(esp + 3);

//  thread_printf();

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
  
//  syscall_file_lock_acquire ();
  struct file *file = thread_file_find (fd);

  if (!file)
  {
    f->eax = -1;
//	syscall_file_lock_release ();
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

 
  syscall_file_lock_acquire ();
  struct file *file = thread_file_find (fd);
  file_seek (file, position);
  syscall_file_lock_release ();
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

  syscall_file_lock_acquire ();
  file_close (file);
  syscall_file_lock_release ();
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
syscall_lock_printf (void)
{
  printf ("syscall_lock_printf, then how many threads are waiting lock? %d\n", list_size (&file_lock.semaphore.waiters));
  printf ("syscall_lock_printf, curr thread %s %d\n", thread_current()->name, thread_current ()->tid);
  struct thread *t;
  struct list_elem *e;
  for (e = list_begin (&file_lock.semaphore.waiters);
	   e != list_end (&file_lock.semaphore.waiters);
	   e = list_next (e))
  {
	  t = list_entry (e, struct thread, elem);
	  printf ("syscall_lock_printf, waiting threads %s %d\n", t->name, t->tid);
  }
 
}

