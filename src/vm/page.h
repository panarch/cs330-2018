#include <list.h>
#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/file.h"

struct page
  {
    bool writable;
    bool is_swapped;
    bool is_loaded;

    enum palloc_flags flags;
    void *uaddr;
    void *kaddr;

    size_t swap_idx;

    struct thread *owner;
    struct file *file;

    struct hash_elem vm_elem;
    struct list_elem frame_elem;
  };

