#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);


//static void sys_write (int *ret, int fd, 

static int syscall_execute (const char *cmd_line);
static int syscall_create (const char *file, unsigned initial_size);
static int syscall_write (int fd, const char * buffer, unsigned size);
static int syscall_filesize (int fd);

struct file_structure {
  struct file* file_ptr;
// how about this? :  struct file files;
  struct list_elem elem;
  int fd;
}
  





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
//	syscall_write(*(p+1),*(p+2),*(p+3));
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
  else if(*p == SYS_SEEK){


  }
  else if(*p==SYS_FILESIZE){
	syscall_filesize (*(p+1));
	file_length ( list_search( &thread_current()->files, *(p+1))->ptr);

  }
  else
  {
	printf("what is going on?\n");
  }

 // printf("int p : %d\n",*p);
 // printf ("system call!\n");
}





static int syscall_write(int fd, const char *buffer, unsigned size){

  struct file *f;
  int return_value;
  return_value = -1;

  /* stdout */
  if (fd == 1){
	putbuf (buffer, size);
  }
  else if (fd == 0){
	// stdin
	return -1;
  }

  if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size)){
	syscall_exit(-1);
  }

//  file_write


  return -1;





}


static int syscall_execute(const char* file_name){

  int return_value;
  return_value = process_execute (file_name);


  return return_value;

}

static int syscall_create (const char *file, unsigned initial_size){
  return filesys_open (file, initial_size);
}


static int32_t syscall_filesize (int fd){
  struct thread *cur;
  struct file *file;
  file = NULL;
  cur = thread_current();

  file = file_find (cur, fd)->file_ptr;

  if(file == NULL){
	return -1;
  }

  return file_length (file); 
}

static int syscall_seek (int fd, unsigned position){





}













struct file_structure* file_find (struct thread *t, int fd){

  struct file_structure *f;
  f = NULL;
  struct list_elem *e;
  t->files;
  for ( e = list_begin (&t->file_list); e != list_end (&t->file_list); e = list_next (e)){
	f = list_entry (e, sturct file_structure, elem);
	if (f->fd == fd){
	  break;
	}
  }

  return f;
}
	 





















