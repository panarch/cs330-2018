#ifndef VM_PAGE
#define VM_PAGE
#include "vm/page.h"
#endif

void frame_init (void);
bool frame_load_page (struct page *page);

