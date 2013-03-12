#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/swap.h"

static struct lock load_lock;

static bool vm_load_file_page (uint8_t *kpage, struct vm_page *page);
static bool vm_load_swap_page (uint8_t *kpage, struct vm_page *page);
static bool vm_load_zero_page (uint8_t *kpage);

static void add_page (struct vm_page *page);

void
vm_page_init (void)
{
  lock_init (&load_lock);
}

static int cnt = 0;

/* Creates a new page from a file segment. */
struct vm_page*
vm_new_file_page (void *addr, struct file *file, off_t ofs, size_t read_bytes,
                  size_t zero_bytes, bool writable)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));
  
  //printf ("[new_file_page] %d\n", ++cnt);

  if (page == NULL)
    return NULL;

  page->type = FILE;
  page->addr = addr;
  page->pagedir = thread_current ()->pagedir;
  page->file_data.file = file;
  page->file_data.ofs = ofs;
  page->file_data.read_bytes = read_bytes;
  page->file_data.zero_bytes = zero_bytes;
  page->writable = writable;
  page->loaded = false;

  add_page (page);

  return page; 
}

/* Creates a new page from a swap page. (?) */
struct vm_page*
vm_new_swap_page (void *addr, size_t index, bool writable)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));

  //printf ("[new_swap_page] %d\n", ++cnt);

  if (page == NULL)
    return NULL;

  page->type = SWAP;
  page->addr = addr;
  page->pagedir = thread_current ()->pagedir;
  page->swap_data.index = index;
  page->writable = writable;
  page->loaded = false;

  add_page (page);

  return page;
}

/* Creates a new page initialized to zero on loading. */
struct vm_page*
vm_new_zero_page (void *addr, bool writable)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));

  //printf ("[new_zero_page] %d\n", ++cnt);

  if (page == NULL)
    return NULL;

  page->type = ZERO;
  page->addr = addr;
  page->pagedir = thread_current ()->pagedir;
  page->writable = writable;
  page->loaded = false;  

  add_page (page);

  return page;
}

/* Pins a page into memory. */
void
vm_pin_page (struct vm_page *page)
{
  if (page->kpage != NULL)
    return;
  vm_frame_pin (page->kpage);
}

/* Unpins a page from memory. */
void
vm_unpin_page (struct vm_page *page)
{
  if (page->kpage != NULL)
    return;
  vm_frame_unpin (page->kpage);
}

/* Loads a page and obtains a frame where for it. If PINNED
   is true the frame will be left pinned and the caller has
   to unpin it after usage. This is helpful on read / write
   operation and makes sure the frame won't be evicted by
   another thread in meantime. */
bool 
vm_load_page (struct vm_page *page, void *uva, bool pinned)
{
  /* Get a frame of memory. */
  lock_acquire (&load_lock);
  page->kpage = vm_get_frame (PAL_USER);
  lock_release (&load_lock);
  vm_frame_set_page (page->kpage, page);

  //printf ("Page load for %p %p\n", page->addr, page->pagedir);

  ASSERT (page->kpage != NULL);

  bool success = true;
  if (page->type == FILE)
    success = vm_load_file_page (page->kpage, page);
  else if (page->type == ZERO)
    success = vm_load_zero_page (page->kpage);
  else
    success = vm_load_swap_page (page->kpage, page);

  if (!success)
    {
      vm_frame_unpin (page->kpage);
      return false;
    }

  pagedir_clear_page (page->pagedir, uva);
  if (!pagedir_set_page (page->pagedir, uva, page->kpage, page->writable) )
    {
      ASSERT (true);
      vm_frame_unpin (page->kpage);
      return false;
    }

  if (!pagedir_get_page (page->pagedir, uva) )
    {
      ASSERT (true);
      vm_frame_unpin (page->kpage);
      return false;
    }

  pagedir_set_dirty (page->pagedir, uva, false);
  pagedir_set_accessed (page->pagedir, uva, true);
  page->loaded = true;
  /* On succes we leave the frame pinned if the caller wants so. */
  if (!pinned)
    vm_frame_unpin (page->kpage);
  return true;
}

void
vm_unload_page (struct vm_page *page, void *kpage)
{
  //printf ("Page unload for %p %p\n", page->addr, page->pagedir);
  //ASSERT (page->loaded);

  if (page->type == FILE && pagedir_is_dirty (page->pagedir, page->addr) &&
      file_writable (page->file_data.file) == false)
    {
      //printf ("Unload %d %s..\n", ++cnt2, t->name);

      /* Write the page back to the file. */
      vm_frame_pin (kpage);
      sys_t_filelock (true);
      file_seek (page->file_data.file, page->file_data.ofs);
      file_write (page->file_data.file, kpage, page->file_data.read_bytes);
      sys_t_filelock (false);
      vm_frame_unpin (kpage);
    }
  else if (page->type == SWAP || pagedir_is_dirty (page->pagedir, page->addr))
    {
      /* Store the page to swap. */
      page->type = SWAP;
      page->swap_data.index = vm_swap_store (kpage);
    }
 
  pagedir_clear_page (page->pagedir, page->addr);
  page->loaded = false;
  page->kpage = NULL;
}

static bool
vm_load_file_page (uint8_t *kpage, struct vm_page *page)
{
  /* Read the content of the page from file. */
  //ASSERT (!page->loaded); 

  sys_t_filelock (true);
  file_seek (page->file_data.file, page->file_data.ofs);
  size_t ret = file_read (page->file_data.file, kpage, page->file_data.read_bytes);
  sys_t_filelock (false);
   
  if (ret != page->file_data.read_bytes)
    {
      vm_free_frame (kpage);
      return false;
    }
  
  /* Fill the rest of the page with zeroes. */
  memset (kpage + page->file_data.read_bytes, 0, page->file_data.zero_bytes);
  return true;
}

static bool
vm_load_zero_page (uint8_t *kpage)
{
  /* Fill this page only with zeroes. */
  memset (kpage, 0, PGSIZE);
  return true;
}

static bool
vm_load_swap_page (uint8_t *kpage, struct vm_page *page)
{
  vm_swap_load (page->swap_data.index, kpage);
  vm_swap_free (page->swap_data.index);
  return true;
}

struct vm_page *
vm_grow_stack (void *uva, bool pinned)
{
  struct vm_page *page = vm_new_zero_page (uva, true);
  if ( !vm_load_page (page, uva, pinned) )
    return NULL;

  return page;
}

struct vm_page *
find_page (void *addr)
{
  //printf ("[find page] for adr %d in %p\n", addr, pagedir);

  struct thread *t = thread_current ();
  struct vm_page page;
  struct hash_elem *e;

  page.addr = addr;
  e = hash_find (&t->pages, &page.hash_elem);
  return e != NULL ? hash_entry (e, struct vm_page, hash_elem) : NULL;
}

static void
add_page (struct vm_page *page)
{
  //printf ("\n[add page] for adr %d in %p\n", page->addr, page->pagedir);
  
  struct thread *t = thread_current ();
  hash_insert (&t->pages, &page->hash_elem);
}

void
vm_delete_page (struct vm_page *page)
{
  /* Free the swap data of the page if necessary. */
  if (page->type == SWAP && page->loaded == false)
    vm_swap_free (page->swap_data.index);

  struct thread *t = thread_current ();
  hash_delete (&t->pages, &page->hash_elem);
  --cnt;
}

/* Frees all pages of the given thread. Assumes all pages that remained
   are not loaded so we don't need to clear the frame also. */
void
vm_free_pages (void)
{
  struct thread *t = thread_current ();
  struct vm_page *page;

  while (!hash_empty (&t->pages) )
    {
      struct hash_iterator it;
      hash_first (&it, &t->pages);
      page = hash_entry (hash_next (&it), struct vm_page, hash_elem);

      vm_delete_page (page);
    }
}

bool
stack_access (const void *esp, void *addr)
{
  return (uint32_t)addr > 0 && addr >= (esp - 32) &&
     (PHYS_BASE - pg_round_down (addr)) <= (1<<23);
}

/* Returns a hash value for a page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct vm_page *p = hash_entry (p_, struct vm_page, hash_elem);
  return hash_int ((unsigned)p->addr);
}

/* Returns true if a page a preceds page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct vm_page *a = hash_entry (a_, struct vm_page, hash_elem);
  const struct vm_page *b = hash_entry (b_, struct vm_page, hash_elem);

  return a->addr < b->addr;
}
