#include "vm/frame.h"
#include <stdio.h>
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct lock frame_lock;
static struct hash vm_frames;

unsigned vm_frame_hash (const struct hash_elem *, void *);
bool vm_frame_less (const struct hash_elem *, const struct hash_elem *, void *);
static bool add_page_frame (void *);
static struct vm_frame *find_page_frame (void *);
static bool delete_page_frame (void *);

/* Initialise the frame table. */
void
vm_frame_init ()
{
  lock_init (&frame_lock);
  hash_init (&vm_frames, vm_frame_hash, vm_frame_less, NULL);
}

/* Obtains a free frame. TO DO: eviction */
void *
vm_get_frame (enum palloc_flags flags)
{
  void *page = palloc_get_page (flags);
  
  if (page != NULL) 
  {
    add_page_frame (page);
    find_page_frame (page);
  }
  else
  {
#ifndef VM
    /* We have fixed size memory in this case. */
    sys_t_exit (-1);
#endif

    // Panic the kernel for the moment
    PANIC ("Eviction is required!");
  }

  return page;
}

/* Frees the given page. */
void 
vm_free_frame (void *page)
{
  delete_page_frame (page);
  palloc_free_page (page);
}

/* Creates a mapping for the user virtual page to the vm_frame. */
bool
vm_frame_create_user (void *frame, uint32_t *pte, void *uva) 
{
  struct vm_frame *vf = find_page_frame (frame);
  
  if (vf == NULL)
    return false;
  
  vf->pte = pte;
  vf->uva = uva;
  return true;
}

/* Insert the given page into a frame */
static bool
add_page_frame (void *page)
{
  struct vm_frame *vf;
  vf = (struct vm_frame *) malloc (sizeof (struct vm_frame) );

  if (vf == NULL) 
    return false;

  vf->thread = thread_current ();
  vf->page = page;
  
  lock_acquire (&frame_lock);
  hash_insert (&vm_frames, &vf->hash_elem);
  lock_release (&frame_lock);  

  return true;
}

/* Removes the given page from it's frame. */
static bool
delete_page_frame (void *page)
{
  struct vm_frame *vf = find_page_frame (page);
  
  if (vf == NULL)
    return false;
  
  lock_acquire (&frame_lock);
  hash_delete (&vm_frames, &vf->hash_elem);
  free (vf);
  lock_release (&frame_lock);

  return true;
}

/* Returns the frame containing the given page, or a null pointer in not found. */
static struct vm_frame *
find_page_frame (void *page)
{
  struct vm_frame vf;
  struct hash_elem *e;

  vf.page = page;
  e = hash_find (&vm_frames, &vf.hash_elem);
  return e != NULL ? hash_entry (e, struct vm_frame, hash_elem) : NULL;
}

/* Returns a hash value for a frame f. */
unsigned
vm_frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct vm_frame *f = hash_entry (f_, struct vm_frame, hash_elem);
  return hash_int ((unsigned)f->page);
}

/* Returns true if a frame a precedes frame b. */
bool
vm_frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
  const struct vm_frame *a = hash_entry (a_, struct vm_frame, hash_elem);
  const struct vm_frame *b = hash_entry (b_, struct vm_frame, hash_elem);
  
  return a->page < b->page;
}
