#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);


//static void sys_write (int *ret, int fd, 



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *p = f->esp;
  int fd = *(p+1);
  char *buffer=*(p+2);
  unsigned size = *(p+3);

//  putbuf(buffer,size);



  if(*p==SYS_WRITE){
//	printf("Sys write ?\n");
	putbuf(buffer,size);
	f->eax=(int)size;
  }
  else if(*p==SYS_EXIT)
  {
	printf("Executed SYS_EXIT?\n");
  thread_exit ();
  }
  else if(*p==SYS_OPEN){
//	printf("Executed SYS_OPEN\n");
  }
  else
  {
//	printf("what is going on?\n");
  }

 // printf("int p : %d\n",*p);
 // printf ("system call!\n");
}
