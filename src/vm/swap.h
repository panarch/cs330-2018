#ifndef VM_PAGE
#define VM_PAGE
#include "vm/page.h"
#endif

void swap_init (void);
void swap_in (struct page *page);
void swap_out (struct page *page);

