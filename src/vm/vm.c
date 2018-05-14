#include "vm/vm.h"
#include <stdio.h>
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"

static unsigned vm_hash_hash_func (const struct hash_elem *a, void *aux);
static bool vm_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void vm_hash_free_func (struct hash_elem *a, void *aux);

static bool install_page (void *upage, void *kpage, bool writable);

void
vm_init (struct hash *vm)
{
  hash_init (vm, vm_hash_hash_func, vm_hash_less_func, 0);
}

void
vm_destroy (struct hash *vm)
{
  hash_destroy (vm, vm_hash_free_func);
}

bool
vm_get_and_install_page (enum palloc_flags flags, void *upage, bool writable)
{
  struct page *page = vm_get_page_instant (flags, upage, writable);
  if (page == NULL)
    return false;

  return vm_install_page (page);
}

static struct page *
vm_find_page (void *upage)
{
  struct page key_page;
  key_page.uaddr = pg_round_down (upage);

  struct thread *cur = thread_current();
  struct hash_elem *elem = hash_find (&cur->vm, &key_page.vm_elem);

  if (elem == NULL)
    return NULL;

  return hash_entry (elem, struct page, vm_elem);
}

bool
vm_has_page (void *upage)
{
  return vm_find_page (upage) != NULL;
}

struct page *
vm_get_page_instant (enum palloc_flags flags, void *upage, bool writable)
{
  struct page *page = vm_find_page (upage);
  void *uaddr = pg_round_down (upage);

  if (page == NULL)
    {
      page = (struct page *) malloc (sizeof (struct page));
      page->writable = writable;
      page->is_swapped = false;

      page->flags = flags;
      page->uaddr = uaddr;

      page->owner = thread_current ();
    }
  else
    {
      // printf ("PAGE IS NOT NULL \n"); and is_swapped == true
    }

  if (!frame_load_page (page))
    {
      free (page);
      return NULL;
    }

  hash_insert (&page->owner->vm, &page->vm_elem);

  return page;
}

bool
vm_install_page (struct page *page)
{
  return install_page (page->uaddr, page->kaddr, page->writable);
}

/*
void
vm_get_page_lazy (enum palloc_flags flags)
{
}
*/

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

static unsigned
vm_hash_hash_func (const struct hash_elem *a, void *aux UNUSED)
{
  struct page *page = hash_entry (a, struct page, vm_elem);

  return hash_int ((int) page->uaddr);
}

static bool
vm_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct page *page_a = hash_entry (a, struct page, vm_elem);
  struct page *page_b = hash_entry (b, struct page, vm_elem);

  return page_a->uaddr < page_b->uaddr;
}

static void
vm_hash_free_func (struct hash_elem *a, void *aux UNUSED)
{
  struct page *page = hash_entry (a, struct page, vm_elem);

  free (page);
}

