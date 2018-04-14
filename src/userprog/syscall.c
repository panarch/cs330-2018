#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);


//static void sys_write (int *ret, int fd, 

static int syscall_execute (const char *cmd_line);
static int syscall_create (const char *file, unsigned initial_size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *p =f->esp;
//  printf("(check) pointer address : %p\n", p);
  int fd = *(p+1);
  int return_value;
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
//	printf("Executed SYS_EXIT?\n");
  thread_exit ();
  }
  else if(*p==SYS_OPEN){
	printf("Executed SYS_OPEN\n");
  }
  else if(*p==SYS_EXEC){
	printf("(check) SYS_EXEC work?\n");
	f-> eax = syscall_execute (*(p+1)); // *(p+1) seems to mean its file name
//	exec (
  }
  else if(*p==SYS_WAIT){
	printf("(check) SYS_WAIT work?\n");
//	process_wait(int pid);
  }
  else if(*p==SYS_HALT){
	printf("(check) SYS_HALT work?\n");
	shutdown_power_off();
  }
  else if(*p==SYS_CREATE){
	printf("(check) SYS_CREATE work?\n");
	f-> eax = syscall_create (*(p+1), *(p+2));
  }
  else if(*p==SYS_REMOVE){

  }
  else
  {
	printf("what is going on?\n");
  }

 // printf("int p : %d\n",*p);
 // printf ("system call!\n");
}





static int syscall_execute(const char* file_name){

  int return_value;
  return_value = process_execute (file_name);


  return return_value;

}

static int syscall_create (const char *file, unsigned initial_size){
  return filesys_open (file, initial_size);
}




