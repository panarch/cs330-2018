#include "vm/frame.h"
#include "vm/swap.h"
#include <list.h>
#include "threads/palloc.h"

static struct list page_list;

void
frame_init (void)
{
  list_init (&page_list);
}

bool
frame_load_page (struct page *page)
{
  void *kpage = palloc_get_page (page->flags);

  if (kpage == NULL)
    return false;

  page->kaddr = kpage;
  page->is_loaded = true;
  
  return true;
}

