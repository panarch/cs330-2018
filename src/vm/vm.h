#include "vm/page.h"
#include <hash.h>
#include "threads/palloc.h"

void vm_init (struct hash *vm);
void vm_destroy (struct hash *vm);

bool vm_get_and_install_page (enum palloc_flags flags, void *upage, bool writable);

