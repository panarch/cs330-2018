/* Generates about 1 MB of random data that is then divided into
   16 chunks.  A separate subprocess sorts each chunk; the
   subprocesses run in parallel.  Then we merge the chunks and
   verify that the result is what it should be. */

#include "tests/vm/parallel-merge.h"
#include <stdio.h>
#include <syscall.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

 #define CHUNK_SIZE (128 * 1024)
//#define CHUNK_SIZE (500)
#define CHUNK_CNT 8                             /* Number of chunks. */
#define DATA_SIZE (CHUNK_CNT * CHUNK_SIZE)      /* Buffer size. */

unsigned char buf1[DATA_SIZE], buf2[DATA_SIZE];
size_t histogram[256];

/* Initialize buf1 with random data,
   then count the number of instances of each value within it. */
static void
init (void) 
{
  struct arc4 arc4;
  size_t i;

  msg ("init");

  arc4_init (&arc4, "foobar", 6);
  arc4_crypt (&arc4, buf1, sizeof buf1);
  for (i = 0; i < sizeof buf1; i++)
    histogram[buf1[i]]++;
}

/* Sort each chunk of buf1 using SUBPROCESS,
   which is expected to return EXIT_STATUS. */
static void
sort_chunks (const char *subprocess, int exit_status)
{
  pid_t children[CHUNK_CNT];
  size_t i;

  for (i = 0; i < CHUNK_CNT; i++) 
    {
      char fn[128];
      char cmd[128];
      int handle;

      msg ("sort chunk %zu", i);

      /* Write this chunk to a file. */
      snprintf (fn, sizeof fn, "buf%zu", i);
      create (fn, CHUNK_SIZE);
      quiet = true;
      CHECK ((handle = open (fn)) > 1, "open \"%s\"", fn);

	  printf ("sort-chunk %d before write %s %d\n", i,fn, handle);

      write (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE);
	  printf ("sort-chunk %d after write %s %d\n", i,fn, handle);

	  printf ("sort-chunk %d before close %s %d\n", i, fn, handle);
      close (handle);
	  printf ("sort-chunk %d after close %s %d\n", i, fn, handle);

      /* Sort with subprocess. */
      snprintf (cmd, sizeof cmd, "%s %s", subprocess, fn);
      
	  printf ("sort-chunk %d before exec fn : %s cmd : %s \n", i, fn, cmd);

      CHECK ((children[i] = exec (cmd)) != -1, "exec \"%s\"", cmd);
	  printf ("sort-chunk %d after  exec fn : %s cmd : %s children : %d\n", i, fn, cmd, children[i]);
      quiet = false;

	//  printf ("sort-chunk after exec %d ----------------------------------------\n", i);
    }

  printf ("\n\nsort-chunks first loop over\n\n");

  int j;
  for (j=0; j < CHUNK_CNT; j++)
  {
    printf ("children[i] : %d\n", children[j]);
  }


  for (i = 0; i < CHUNK_CNT; i++) 
    {
      char fn[128];
      int handle;

	  printf ("sort-chunk %d before wait children[%d] : %d\n", i, i, children[i]);

      CHECK (wait (children[i]) == exit_status, "wait for child %zu", i);

	  printf ("sort-chunk %d after wait children[%d] : %d\n", i, i, children[i]);
      /* Read chunk back from file. */
      quiet = true;
      snprintf (fn, sizeof fn, "buf%zu", i);
	  printf ("sort-chunk %d what is fn? %s\n", i, fn);

	  printf ("sort-chunk before open %d\n",i);
      CHECK ((handle = open (fn)) > 1, "open \"%s\"", fn);
      printf ("sort-chunk %d after open , handle is %d\n",i, handle);

	  printf ("sort-chunk %d before read\n",i);

      read (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE);

	  printf ("sort-chunk %d after read\n",i);
      close (handle);
      quiet = false;
    }
}

/* Merge the sorted chunks in buf1 into a fully sorted buf2. */
static void
merge (void) 
{
  unsigned char *mp[CHUNK_CNT];
  size_t mp_left;
  unsigned char *op;
  size_t i;

  msg ("merge");

  /* Initialize merge pointers. */
  mp_left = CHUNK_CNT;
  for (i = 0; i < CHUNK_CNT; i++)
    mp[i] = buf1 + CHUNK_SIZE * i;

  /* Merge. */
  op = buf2;
  while (mp_left > 0) 
    {
      /* Find smallest value. */
      size_t min = 0;
      for (i = 1; i < mp_left; i++)
        if (*mp[i] < *mp[min])
          min = i;

      /* Append value to buf2. */
      *op++ = *mp[min];

      /* Advance merge pointer.
         Delete this chunk from the set if it's emptied. */
      if ((++mp[min] - buf1) % CHUNK_SIZE == 0) 
        mp[min] = mp[--mp_left];
    }
}

static void
verify (void) 
{
  size_t buf_idx;
  size_t hist_idx;

  msg ("verify");

  buf_idx = 0;
  for (hist_idx = 0; hist_idx < sizeof histogram / sizeof *histogram;
       hist_idx++)
    {
      while (histogram[hist_idx]-- > 0) 
        {
          if (buf2[buf_idx] != hist_idx)
            fail ("bad value %d in byte %zu", buf2[buf_idx], buf_idx);
          buf_idx++;
        } 
    }

  msg ("success, buf_idx=%'zu", buf_idx);
}

void
parallel_merge (const char *child_name, int exit_status)
{
  init ();
  sort_chunks (child_name, exit_status);
  merge ();
  verify ();
}
