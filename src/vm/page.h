#include <list.h>
#include <hash.h>
#include "threads/thread.h"

struct page
  {
    bool writable;
    bool is_swapped;
    bool is_loaded;

    void *uaddr;
    void *kaddr;

    struct thread *owner;

    struct hash_elem vm_elem;
    struct list_elem frame_elem;
  };

