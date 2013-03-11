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

static struct lock list_lock;
static struct lock load_lock;
static struct list page_list; 

static bool vm_load_file_page (uint8_t *kpage, struct vm_page *page);
static bool vm_load_swap_page (uint8_t *kpage, struct vm_page *page);
static bool vm_load_zero_page (uint8_t *kpage);

static bool add_page (struct vm_page *page);

void
vm_page_init (void)
{
  lock_init (&list_lock);
  lock_init (&load_lock);
  list_init (&page_list);
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
  ASSERT (page->kpage != NULL);
  vm_frame_pin (page->kpage);
}

/* Unpins a page from memory. */
void
vm_unpin_page (struct vm_page *page)
{
  ASSERT (page->kpage != NULL);
  vm_frame_unpin (page->kpage);
}

bool 
vm_load_page (struct vm_page *page, void *uva)
{
  /* Get a frame of memory. */
  lock_acquire (&load_lock);
  page->kpage = vm_get_frame (PAL_USER);
  vm_frame_pin (page->kpage);
  lock_release (&load_lock);
  vm_frame_set_page (page->kpage, page);

  //printf ("Page load for %p %p\n", page->addr, page->pagedir);

  if (page->kpage == NULL) 
    {
      vm_frame_unpin (page->kpage);
      return false;
    }

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
      vm_frame_unpin (page->kpage);
      return false;
    }

  if (!pagedir_get_page (page->pagedir, uva) )
    {
      vm_frame_unpin (page->kpage);
      return false;
    }

  pagedir_set_dirty (page->pagedir, uva, false);
  pagedir_set_accessed (page->pagedir, uva, true);
  page->loaded = true;

  vm_frame_unpin (page->kpage);
  return true;
}

void
vm_unload_page (struct vm_page *page, void *kpage)
{
  //printf ("Page unload for %p %p\n", page->addr, page->pagedir);

  if (page->type == FILE && pagedir_is_dirty (page->pagedir, page->addr) &&
      file_writable (page->file_data.file) == false)
    {
      /* Write the page back to the file. */
      vm_frame_pin (page->kpage);
      file_seek (page->file_data.file, page->file_data.ofs);
      file_write (page->file_data.file, kpage, page->file_data.read_bytes);
      vm_frame_unpin (page->kpage);
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
  file_seek (page->file_data.file, page->file_data.ofs);
  
  /* Load this page. */
  if (file_read (page->file_data.file, kpage, page->file_data.read_bytes) 
      != (int) page->file_data.read_bytes)
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
vm_grow_stack (void *uva)
{
  struct vm_page *page = vm_new_zero_page (uva, true);
  if ( !vm_load_page (page, uva) )
    return NULL;

  return page;
}

struct vm_page *
find_page (void *addr)
{
  //printf ("[find page] for adr %d in %p\n", addr, pagedir);

  uint32_t *pagedir = thread_current ()->pagedir;
  struct list_elem *e;
  lock_acquire (&list_lock);

  for (e = list_begin (&page_list); e != list_end (&page_list);
       e = list_next(e))
    {
      struct vm_page *page = list_entry (e, struct vm_page, list_elem);
      if (page->addr == addr && page->pagedir == pagedir)
        {
          lock_release (&list_lock);
          return page;
        }
    }

  lock_release (&list_lock);
  return NULL;
}

static bool
add_page (struct vm_page *page)
{
  //printf ("\n[add page] for adr %d in %p\n", page->addr, page->pagedir);

  lock_acquire (&list_lock);
  list_push_back (&page_list, &page->list_elem);
  lock_release (&list_lock);

  return true;
}

bool 
vm_delete_page (struct vm_page *page)
{
  lock_acquire (&list_lock);
  list_remove (&page->list_elem);
  free (page);

  --cnt;

  lock_release (&list_lock);
  return true;
}

bool
stack_access (void *esp, void *addr)
{
  return (uint32_t)addr > 0 && addr >= (esp - 32) &&
     (PHYS_BASE - pg_round_down (addr)) <= (1<<23);
}
