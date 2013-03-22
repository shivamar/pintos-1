#include "vm/frame.h"
#include <stdio.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Synchronization primitives for the frame table. */
static struct lock frame_lock;
static struct lock evict_lock;
/* Hash table of frames for fast lookup. */
static struct hash vm_frames;
/* List of frames for the clock eviction algorithm. */
static struct list vm_frames_list;
static struct list_elem *e_next;

/* Frame hash table helper functions. */
static unsigned frame_hash (const struct hash_elem *, void *);
static bool frame_less (const struct hash_elem *, 
                        const struct hash_elem *, void *);
/* Functions for frame lookup and frame delete. */
static struct vm_frame *find_frame (void *);
static void delete_frame (struct vm_frame *);

/* Eviction helper function. */
static bool eviction_scan_and_flip (struct vm_frame *);
static void eviction (void);

/* Clock algorithm helper functions. */
static void eviction_remove_pointer (struct vm_frame *);
static struct vm_frame *eviction_get_next (void);
static void eviction_move_next (void);

/* Initialise the frame table. */
void
vm_frame_init ()
{
  lock_init (&frame_lock);
  lock_init (&evict_lock);
  hash_init (&vm_frames, frame_hash, frame_less, NULL);
  list_init (&vm_frames_list);
}

/* Sharing - Looks thourgh all the frames if there is one that contains
   the required data. This function will be called on each page load of
   a read only file segment. Because is called quite rare we don't need
   a fast lookup in a hash table and choosed a list for simplicity. */
void *
vm_lookup_frame (off_t block_id)
{
  void *addr = NULL;

  struct hash_iterator it;
  
  /* Ensure synchronization with other access on the frame's table. */
  lock_acquire (&frame_lock);
  hash_first (&it, &vm_frames);
  while (hash_next (&it) && addr == NULL)
    {
      struct vm_frame *vf = NULL;
      vf = hash_entry (hash_cur (&it), struct vm_frame, hash_elem);

      /* Syncronize with the frame page list. */
      lock_acquire (&vf->list_lock);
      
      struct list_elem *e = list_begin (&vf->pages);
      struct vm_page *page = list_entry (e, struct vm_page, frame_elem);

      /* Takes a page to see if the frame contains the same data block. */
      if (page->type == FILE && page->file_data.block_id == block_id)
        {
          addr = vf->addr;
          vf->pinned = true;
        }

      lock_release (&vf->list_lock);
    }
  lock_release (&frame_lock);
  
  return addr;
}

/* Obtains a free frame. Evicts a frame if memory allocation fails. */
void *
vm_get_frame (enum palloc_flags flags)
{
  void *addr = palloc_get_page (flags);
  
  /* If memory allocation was successful. */
  if (addr != NULL) 
  {
    struct vm_frame *vf;
    vf = (struct vm_frame *) malloc (sizeof (struct vm_frame) );

    if (vf == NULL)
      return false;

    vf->addr = addr;
    /* A new frame will be pinned until the caller will load the data to it.
       This way pe make sure it won't be evicted anytime in between. */
    vf->pinned = true;
    list_init (&vf->pages);
    lock_init (&vf->list_lock);

    lock_acquire (&frame_lock);
		list_push_back (&vm_frames_list, &vf->list_elem);
    hash_insert (&vm_frames, &vf->hash_elem);   
    lock_release (&frame_lock);
  }
  else
  {
#ifndef VM
    /* We have fixed size memory in this case. */
    sys_t_exit (-1);
#endif

    /* Evict a frame and try again. */
    eviction ();
    return vm_get_frame (flags);
  }

  return addr;
}

/* Frees the given frame and writes the data back to swap
   or file. This function will be called on process exit. */
void 
vm_free_frame (void *addr, uint32_t *pagedir)
{
  lock_acquire (&evict_lock);
  struct vm_frame *vf = find_frame (addr);  
  struct list_elem *e;
  
  if (vf == NULL) 
    {
      lock_release (&evict_lock);
      return; 
    }

  if (pagedir == NULL)
    {
      /* Unloads and removes from the list all the pages that share 
         this frame. This will be called when we evict a frame. */
      lock_acquire (&vf->list_lock);
      while (!list_empty (&vf->pages) )
        {
          e = list_begin (&vf->pages);
          struct vm_page *page = list_entry (e, struct vm_page, frame_elem);
          list_remove (&page->frame_elem);
          vm_unload_page (page, vf->addr);
        }
      lock_release (&vf->list_lock);
    }
  else
    {
      /* Frees only one page and the frame remains if it contains other
         pages. Will be used this way on process_exit or file unmap. */
      struct vm_page *page = vm_frame_get_page (addr, pagedir);
      
      if (page != NULL)
        {
          lock_acquire (&vf->list_lock);
          list_remove (&page->frame_elem);
          lock_release (&vf->list_lock);
          vm_unload_page (page, vf->addr);
        }
    }

  /* If the frames doen't contain any more pages we can free
     the frame struct. */
  if (list_empty (&vf->pages))
  {
    delete_frame (vf);
    palloc_free_page (addr);
  }
  lock_release (&evict_lock);
}

/* Creates a mapping for the page to the vm_frame. */
bool
vm_frame_set_page (void *frame, struct vm_page *page)
{
  struct vm_frame *vf = find_frame (frame);
  
  if (vf == NULL)
    return false;

  lock_acquire (&vf->list_lock);
  list_push_back (&vf->pages, &page->frame_elem);
  lock_release (&vf->list_lock);
  return true;
}

/* Obtains a reference to a page struct from a frame page list.
   A page is uniquely indentified by its pagedir and kernel 
   page. Since a frame can be shared between multiple pages we
   need to make a unique choice. */
struct vm_page*
vm_frame_get_page (void *frame, uint32_t *pagedir)
{
  struct vm_frame *vf = find_frame (frame);
  struct list_elem *e;

  if (vf == NULL)
    return NULL;

  lock_acquire (&vf->list_lock);
  for (e = list_begin (&vf->pages); e != list_end (&vf->pages);
       e = list_next (e))
    {
      struct vm_page *page = list_entry (e, struct vm_page, frame_elem);
      if (page->pagedir == pagedir)
        {
          lock_release (&vf->list_lock);
          return page;
        }
    }
  lock_release (&vf->list_lock);
  
  return NULL; 
}

/* Removes the given page from its frame. Sets the clock eviction
   pointer to the next frame. Reclaims memory of the frame struct. */
static void
delete_frame (struct vm_frame *vf)
{
  lock_acquire (&frame_lock);
	eviction_remove_pointer (vf);
  hash_delete (&vm_frames, &vf->hash_elem);
	list_remove (&vf->list_elem);
	free (vf);
  lock_release (&frame_lock);
}

/* Iterates over all the pages which are sharing the given frame.
   Looks at the accesed bit of each page. If all of them are 0 
   then we have found a victim frame, otherwise flip the first 1
   bit and continue. Synchronization must be done by the caller.*/
static bool
eviction_scan_and_flip (struct vm_frame *vf)
{
  struct list_elem *e;
  
  for (e = list_begin (&vf->pages); e != list_end (&vf->pages);
       e = list_next (e))
    {
      struct vm_page *page = list_entry (e, struct vm_page, frame_elem);
      if (pagedir_is_accessed (page->pagedir, page->addr) )
        {
          pagedir_set_accessed (page->pagedir, page->addr, false);
          return false;
        }
    }

  return true;
}

/* The Clock page replacement algorithm. We keep a circular list
   where a hand points to the oldest page. When a page fault occurs
   we look at the accessed bit. If it's 1 we set it to 0 and move on.
   This approach has better performance than the second chance 
   algorithm. For further reference and a more complete explication 
   see MODERN OPERATING SYSTEMS, [Andrew S. Tanenbaum] page 111. */
static void
eviction ()
{
  struct vm_frame *victim = NULL;

  lock_acquire (&evict_lock);
	lock_acquire (&frame_lock);

  while (victim == NULL)
    {
			struct vm_frame *vf = eviction_get_next ();
      ASSERT (vf != NULL);

      /* If the frame is pinned or accessed move on. */
      if (vf->pinned == true || eviction_scan_and_flip(vf) == false)
        {
          eviction_move_next ();
      	  continue;  
        }    

      victim = vf;
    }

	lock_release (&frame_lock);
  lock_release (&evict_lock);
  vm_free_frame (victim->addr, NULL);
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

/* Returns the frame containing the given page, or a null pointer in not 
   found. */
static struct vm_frame *
find_frame (void *addr)
{
  struct vm_frame vf;
  struct hash_elem *e;

  vf.addr = addr;
  lock_acquire (&frame_lock);
  e = hash_find (&vm_frames, &vf.hash_elem);
  lock_release (&frame_lock);
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

/* Sets the eviction pointer to the next frame from the frame list. 
   This function will be called when we delete a frame so we don't
   end up with a dangling pointer. */
static void
eviction_remove_pointer (struct vm_frame *victim)
{
	if (e_next == NULL || e_next == list_end (&vm_frames_list) )
		return;
	struct vm_frame *vf = list_entry (e_next, struct vm_frame, list_elem);

	if (vf == victim)
		eviction_move_next ();
}

/* Returns the next frame to be evicted. */
static struct vm_frame *
eviction_get_next (void)
{
	if (e_next == NULL || e_next == list_end (&vm_frames_list) )
  	e_next = list_begin (&vm_frames_list);
  if (e_next != NULL)
    {
      /* Get the frame struct from the frame list. */
		  struct vm_frame *vf = list_entry (e_next, struct vm_frame, list_elem);
      return vf;
    }  

  NOT_REACHED ();
}

/* Moves the clock pointer to the next frame. If we reached the end
   we start again from the beginning like in a circular list. */
static void
eviction_move_next (void)
{
  if (e_next == NULL || e_next == list_end (&vm_frames_list) )
    e_next = list_begin (&vm_frames_list);
  else
    e_next = list_next (e_next); 
}
