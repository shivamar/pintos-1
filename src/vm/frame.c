#include "vm/frame.h"
#include <stdio.h>
#include <random.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct lock frame_lock;
static struct lock evict_lock;
static struct hash vm_frames;

static unsigned frame_hash (const struct hash_elem *, void *);
static bool frame_less (const struct hash_elem *, const struct hash_elem *, void *);
static struct vm_frame *find_frame (void *);
static bool delete_frame (void *);
static bool eviction (void);

/* Initialise the frame table. */
void
vm_frame_init ()
{
  lock_init (&frame_lock);
  lock_init (&evict_lock);
  random_init (0);
  hash_init (&vm_frames, frame_hash, frame_less, NULL);

  //printf ("frame=%d page=%d\n", sizeof (struct vm_frame), sizeof (struct vm_page) );
}

static int cnt = 0;

/* Obtains a free frame. TO DO: eviction */
void *
vm_get_frame (enum palloc_flags flags)
{
  void *addr = palloc_get_page (flags);
  
  if (addr != NULL) 
  {
    //printf ("[vm_get_frame] get a frame at address %p %d\n", addr, ++cnt);

    struct vm_frame *vf;
    vf = (struct vm_frame *) malloc (sizeof (struct vm_frame) );

    if (vf == NULL)
      return false;

    vf->addr = addr;
    /* A new frame will be pinned until the caller will load the data to it.
       This way pe make sure it won't be evicted anytime in between. */
    vf->pinned = true;
    vf->uva = vf->pagedir = NULL;
    vf->page = NULL;

    lock_acquire (&frame_lock);
    hash_insert (&vm_frames, &vf->hash_elem);
    lock_release (&frame_lock);
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

/* Frees the given frame and writes the data back to swap
   or file. This function will be called on process exit. */
void 
vm_free_frame (void *addr)
{
  struct vm_frame *vf = find_frame (addr);
  
  if (vf == NULL)
    return;
  ASSERT (vf->page != NULL);

  vm_unload_page (vf->page, vf->addr);
  delete_frame (addr);
  palloc_free_page (addr);
}

/* Creates a mapping for the page to the vm_frame. */
bool
vm_frame_set_page (void *frame, struct vm_page *page)
{
  struct vm_frame *vf = find_frame (frame);
  
  if (vf == NULL)
    return false;

  vf->page = page;
  return true;
}

/* Creates a mapping for the user virtual page to the vm_frame. */
bool
vm_frame_add_page (void *frame, void *uva, uint32_t *pagedir) 
{
  struct vm_frame *vf = find_frame (frame);
  
  if (vf == NULL)
    return false;
  
  vf->pagedir = pagedir;
  vf->uva = uva;

  return true;
}

/* Removes the given page from its frame. */
static bool
delete_frame (void *addr)
{
  struct vm_frame *vf = find_frame (addr);
  
  //printf ("[delete frame] %p\n", vf->addr);

  if (vf == NULL)
    return false;
  
  lock_acquire (&frame_lock);
  hash_delete (&vm_frames, &vf->hash_elem);
  free (vf);
  lock_release (&frame_lock);

  --cnt;

  return true;
}

/* Random choose a page to evict. */
static bool
eviction ()
{
  struct vm_frame *f = NULL, *victim = NULL;

  lock_acquire (&evict_lock);

  size_t iteration = 0;
  while (victim == NULL)
    {
      //ASSERT (iteration < 5);

      struct hash_iterator it;
      hash_first (&it, &vm_frames);
      size_t cnt = 0;    

      while (hash_next (&it))
        {
          f = hash_entry (hash_cur (&it), struct vm_frame, hash_elem);
          if (f->pinned == true)
            { ++cnt; continue; }  

          ASSERT (f->pagedir != NULL);
          ASSERT (f->uva != NULL);

          if ( pagedir_is_accessed (f->pagedir, f->uva) )
            {
              pagedir_set_accessed (f->pagedir, f->uva, false);
              continue;
            }      

          victim = f;
          break;
        }
      ++iteration;

      if (iteration > 4) {
        printf ("At the %d iteration %d pinned out of %d\n", iteration, cnt, hash_size (&vm_frames) );
        ASSERT (true);
      }
    }

  //printf ("[Eviction] for frame %p and page %p\n", victim->addr, victim->page->addr);

  vm_free_frame (victim->addr);
  lock_release (&evict_lock);

  return true;
}

/* Pinns the frame at the given address. A pinned frame can;t be evicted. */
void
vm_frame_pin (void *addr)
{
  struct vm_frame *vf = find_frame (addr);
  if (vf != NULL)
    vf->pinned = true;
}

/* Unpinns the frame at the given address. */
void
vm_frame_unpin (void *addr)
{
  struct vm_frame *vf = find_frame (addr);
  if (vf != NULL)
    vf->pinned = false;
}

/* Returns the frame containing the given page, or a null pointer in not found. */
static struct vm_frame *
find_frame (void *addr)
{
  struct vm_frame vf;
  struct hash_elem *e;

  vf.addr = addr;
  e = hash_find (&vm_frames, &vf.hash_elem);
  return e != NULL ? hash_entry (e, struct vm_frame, hash_elem) : NULL;
}

/* Returns a hash value for a frame f. */
static unsigned
frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct vm_frame *f = hash_entry (f_, struct vm_frame, hash_elem);
  return hash_int ((unsigned)f->addr);
}

/* Returns true if a frame a preceds frame b. */
static bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
  const struct vm_frame *a = hash_entry (a_, struct vm_frame, hash_elem);
  const struct vm_frame *b = hash_entry (b_, struct vm_frame, hash_elem);
  
  return a->addr < b->addr;
}
