#include "vm/frame.h"
#include "vm/swap.h"
#include <stdio.h>
#include <list.h>
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

static struct lock lock;
static struct list page_list;

static struct page *pop_victim (void);

void
frame_init (void)
{
  list_init (&page_list);
  lock_init (&lock);
}

bool
frame_load_page (struct page *page)
{
  lock_acquire (&lock);

  void *kpage = palloc_get_page (page->flags);

  if (kpage == NULL)
    {
      swap_out (pop_victim ());
      kpage = palloc_get_page (page->flags);
    }

  page->kaddr = kpage;

  if (page->is_swapped &&
      (page->file == NULL || (page->file != NULL && page->file->mapid <= 0)))
    {
      swap_in (page);
    }
  else if (page->file != NULL)
    {
      int pos = file_tell (page->file);

      file_read (page->file, page->kaddr, PGSIZE);
      file_seek (page->file, pos);

      page->is_swapped = false;
    }

  pagedir_set_dirty (page->owner->pagedir, page->uaddr, false);
  pagedir_set_accessed (page->owner->pagedir, page->uaddr, false);

  page->is_loaded = true;

  if (page->writable)
    list_push_back (&page_list, &page->frame_elem);
  
  lock_release (&lock);

  return true;
}

void
frame_free_page (struct page *page)
{
  if (!page->is_swapped)
    return;

  list_remove (&page->frame_elem);

  if (page->is_loaded)
    swap_free (page);
}

static struct page *
pop_victim (void)
{
  // FIFO
  struct page *page = list_entry (list_pop_front (&page_list), struct page, frame_elem);

  return page;
}

