#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"

struct vm_frame 
  {
    void *addr;                 /* Physical address of the frame. */
    uint32_t *pagedir;          /* Page directory of the frame's page. */
    void *uva;                  /* Address of the frame's page. */
    bool pinned;                /* If the frame is pinned. */
    struct vm_page *page;       /* The actual page. */
    struct hash_elem hash_elem; /* Hash element for the hash frame table. */
  };

/* Public functions of the frame table. */
void vm_frame_init (void);
void *vm_get_frame (enum palloc_flags flags);
void vm_free_frame (void *);
void vm_free_page (void *);
bool vm_frame_add_page (void *, void *, uint32_t *);
bool vm_frame_set_page (void *, struct vm_page *);
void vm_frame_pin (void *);
void vm_frame_unpin (void *);

#endif /* vm/frame.h */
