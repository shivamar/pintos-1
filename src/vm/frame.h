#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"

struct vm_frame 
  {
    void *addr;                 /* Physical address of the frame. */
    bool pinned;                /* If the frame is pinned. */
    struct hash_elem hash_elem; /* Hash element for the hash frame table. */
    struct list pages;          /* A list of the pages that share this frame. */
    struct lock list_lock;      /* A lock to synchronize access to page list. */
	  struct list_elem list_elem; /* List element for frame list. */
  };

/* Public functions of the frame table. */
void vm_frame_init (void);
/* Try to find a frame with the same read-only data. */
void *vm_lookup_frame (off_t);
/* Obtain a new free frame from memory. */
void *vm_get_frame (enum palloc_flags flags);
void vm_free_frame (void *, uint32_t *);
/* Creates a mapping to the frame's loaded page. */
bool vm_frame_set_page (void *, struct vm_page *);
struct vm_page *vm_frame_get_page (void *, uint32_t *);
/* Kernel pin / unpin the given frame. */
void vm_frame_pin (void *);
void vm_frame_unpin (void *);

#endif /* vm/frame.h */
