#include <hash.h>
#include "threads/palloc.h"

#ifndef VM_PAGE
#define VM_PAGE
#include "vm/page.h"
#endif

void vm_init (struct hash *vm);
void vm_destroy (struct hash *vm);

bool vm_get_and_install_page (enum palloc_flags flags, void *upage, bool writable);
struct page * vm_get_page_instant (enum palloc_flags flags, void *upage, bool writable);
bool vm_install_page (struct page *page);

bool vm_has_page (void *upage);

bool vm_pin_pages (void *upage, off_t size);
void vm_unpin_pages (void *upage, off_t size);

int vm_mmap (void *upage, struct file *file);
void vm_munmap (int mapid);
void vm_munmap_all (void);

