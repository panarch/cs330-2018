/* Reads a 128 kB file into static data and "sorts" the bytes in
   it, using counting sort, a single-pass algorithm.  The sorted
   data is written back to the same file in-place. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"


const char *test_name = "child-sort";

unsigned char buf[128 * 1024];
size_t histogram[256];

int
main (int argc UNUSED, char *argv[]) 
{
  int handle;
  unsigned char *p;
  size_t size;
  size_t i;

  printf ("child-sort main start, argv[1] is %s\n", argv[1]);


  quiet = true;

  printf ("child-sort %s, open start\n", argv[1]);

  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);

  printf ("child-sort %s, open end\n", argv[1]);

  printf ("child-sort %s, before read\n", argv[1]);

  size = read (handle, buf, sizeof buf);

  printf ("child-sort %s, after read\n", argv[1]);

  for (i = 0; i < size; i++)
    histogram[buf[i]]++;
  p = buf;
  for (i = 0; i < sizeof histogram / sizeof *histogram; i++) 
    {
      size_t j = histogram[i];
      while (j-- > 0)
        *p++ = i;
    }

  printf ("child-sort %s, before seek\n", argv[1]);
  seek (handle, 0);
  printf ("child-sort %s, after seek\n", argv[1]);

  printf ("child-sort %s, before write\n", argv[1]);
  write (handle, buf, size);
  printf ("child-sort %s, after write \n", argv[1]);

  printf ("child-sort %s, before close \n", argv[1]);
  close (handle);
  printf ("child-sort %s, after close \n", argv[1]);

  printf ("child-sort before exit, %s\n", argv[1]);
  return 123;
}
