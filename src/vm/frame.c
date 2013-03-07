#include "vm/frame.h"
#include <stdio.h>
#include <random.h>
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct lock frame_lock;
static struct lock evict_lock;
static struct hash vm_frames;

unsigned vm_frame_hash (const struct hash_elem *, void *);
bool vm_frame_less (const struct hash_elem *, const struct hash_elem *, void *);
static bool add_page_frame (void *);
static struct vm_frame *find_page_frame (void *);
static bool delete_page_frame (void *);
static bool eviction (void);

/* Initialise the frame table. */
void
vm_frame_init ()
{
  lock_init (&frame_lock);
  lock_init (&evict_lock);
  random_init (0);
  hash_init (&vm_frames, vm_frame_hash, vm_frame_less, NULL);
}

/* Obtains a free frame. TO DO: eviction */
void *
vm_get_frame (enum palloc_flags flags)
{
  void *addr = palloc_get_page (flags);
  
  if (addr != NULL) 
  {
    printf ("[vm_get_frame] get a frame at address %p\n", addr);

    add_page_frame (addr);
    find_page_frame (addr);
  }
  else
  {
#ifndef VM
    /* We have fixed size memory in this case. */
    sys_t_exit (-1);
#endif

    ASSERT ( eviction () )

    /* Try again. */
    return vm_get_frame (flags);
  }

  return addr;
}

/* Frees the given page. */
void 
vm_free_frame (void *addr)
{
  delete_page_frame (addr);
  palloc_free_page (addr);
}

/* Creates a mapping for the user virtual page to the vm_frame. */
bool
vm_frame_add_page (void *frame, void *uva, uint32_t *pagedir) 
{
  struct vm_frame *vf = find_page_frame (frame);
  
  if (vf == NULL)
    return false;
  
  vf->pagedir = pagedir;
  vf->uva = uva;

  return true;
}

/* Insert the given page into a frame */
static bool
add_page_frame (void *addr)
{
  struct vm_frame *vf;
  vf = (struct vm_frame *) malloc (sizeof (struct vm_frame) );

  if (vf == NULL) 
    return false;

  vf->thread = thread_current ();
  vf->addr = addr;
  
  lock_acquire (&frame_lock);
  hash_insert (&vm_frames, &vf->hash_elem);
  lock_release (&frame_lock);  

  return true;
}

/* Removes the given page from it's frame. */
static bool
delete_page_frame (void *addr)
{
  struct vm_frame *vf = find_page_frame (addr);
  
  if (vf == NULL)
    return false;
  
  lock_acquire (&frame_lock);
  hash_delete (&vm_frames, &vf->hash_elem);
  free (vf);
  lock_release (&frame_lock);

  return true;
}

/* Random choose a page to evict. */
static bool
eviction ()
{
  struct hash_iterator it;
  struct vm_frame *f = NULL;

  lock_acquire (&evict_lock);
  hash_first (&it, &vm_frames);
  
  size_t size = hash_size (&vm_frames);
  size_t cnt = (size_t) (random_ulong() % (size - 1)) + 1;
  size_t i;
  for (i = 0; i < cnt; ++i)
    hash_next (&it);
  f = hash_entry (hash_cur (&it), struct vm_frame, hash_elem);
  
  printf ("[Eviction] for frame %p\n", f->addr);  

  struct vm_page *page = find_page (f->uva, f->pagedir);
  if (page == NULL)
    {
      lock_release (&evict_lock);
      return false;
    }

  vm_unload_page (page, f->addr);
  vm_free_frame(f->addr);
  lock_release (&evict_lock);

  return true;
}

/* Returns the frame containing the given page, or a null pointer in not found. */
static struct vm_frame *
find_page_frame (void *addr)
{
  struct vm_frame vf;
  struct hash_elem *e;

  vf.addr = addr;
  e = hash_find (&vm_frames, &vf.hash_elem);
  return e != NULL ? hash_entry (e, struct vm_frame, hash_elem) : NULL;
}

/* Returns a hash value for a frame f. */
unsigned
vm_frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct vm_frame *f = hash_entry (f_, struct vm_frame, hash_elem);
  return hash_int ((unsigned)f->addr);
}

/* Returns true if a frame a precedes frame b. */
bool
vm_frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
  const struct vm_frame *a = hash_entry (a_, struct vm_frame, hash_elem);
  const struct vm_frame *b = hash_entry (b_, struct vm_frame, hash_elem);
  
  return a->addr < b->addr;
}
