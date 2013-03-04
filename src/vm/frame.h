#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"

struct vm_frame 
  {
    void *page;                 /* Actual page. */
    struct thread *thread;      /* Owner thread. */
    uint32_t *pte;              /* Page table entry of the frame's page. */
    void *uva;                  /* Address of the frame's page. */
    struct hash_elem hash_elem; /* Hash element for the hash frame table. */
  };

/* Public functions of the frame table. */
void vm_frame_init (void);
void *vm_get_frame (enum palloc_flags flags);
void vm_free_frame (void *);
bool vm_frame_create_user (void *, uint32_t *, void *);

#endif /* vm/frame.h */
