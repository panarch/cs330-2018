#include "vm/frame.h"
#include "vm/swap.h"
#include <stdio.h>
#include <list.h>
#include "threads/palloc.h"

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

  if (page->is_swapped)
    {
      swap_in (page);
    }

  page->is_loaded = true;

  if (page->writable)
    list_push_back (&page_list, &page->frame_elem);
  
  lock_release (&lock);

  return true;
}

static struct page *
pop_victim (void)
{
  // FIFO
  struct page *page = list_entry (list_pop_front (&page_list), struct page, frame_elem);

  return page;
}

