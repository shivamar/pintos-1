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

#define PAGE_MAGIC 0xcd6abf4b

static struct lock load_lock;
static struct lock unload_lock;

static bool vm_load_file_page (uint8_t *kpage, struct vm_page *page);
static void vm_load_swap_page (uint8_t *kpage, struct vm_page *page);
static void vm_load_zero_page (uint8_t *kpage);

static void add_page (struct vm_page *page);

void
vm_page_init (void)
{
  lock_init (&load_lock);
  lock_init (&unload_lock);
}

static int cnt = 0;

/* Creates a new page from a file segment. */
struct vm_page*
vm_new_file_page (void *addr, struct file *file, off_t ofs, size_t read_bytes,
                  size_t zero_bytes, bool writable)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));
  
  //printf ("[new_file_page] %d %p %p\n", ++cnt, addr, thread_current ()->pagedir);

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
  page->magic = PAGE_MAGIC;

  add_page (page);

  return page; 
}

/* Creates a new page from a swap page. (?) */
struct vm_page*
vm_new_swap_page (void *addr, size_t index, bool writable)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));

  //printf ("[new_swap_page] %d %p\n", ++cnt, addr);

  if (page == NULL)
    return NULL;

  page->type = SWAP;
  page->addr = addr;
  page->pagedir = thread_current ()->pagedir;
  page->swap_data.index = index;
  page->writable = writable;
  page->loaded = false;
  page->magic = PAGE_MAGIC;

  add_page (page);

  return page;
}

/* Creates a new page initialized to zero on loading. */
struct vm_page*
vm_new_zero_page (void *addr, bool writable)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));

  //printf ("[new_zero_page] %d %p\n", ++cnt, addr);

  if (page == NULL)
    return NULL;

  page->type = ZERO;
  page->addr = addr;
  page->pagedir = thread_current ()->pagedir;
  page->writable = writable;
  page->loaded = false;  
  page->magic = PAGE_MAGIC;

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
  /* Fr debug. */
  ASSERT (page->magic == PAGE_MAGIC);

  /* Get a frame of memory. */
  lock_acquire (&load_lock);
  page->kpage = vm_get_frame (PAL_USER);
  lock_release (&load_lock);
  vm_frame_set_page (page->kpage, page);

  //printf ("\nPage load for %p %p type=%d\n", page->addr, page->pagedir, page->type);

  ASSERT (page->kpage != NULL);

  bool success = true;
  if (page->type == FILE)
    success = vm_load_file_page (page->kpage, page);
  else if (page->type == ZERO)
    vm_load_zero_page (page->kpage);
  else
    vm_load_swap_page (page->kpage, page);

  if (!success)
    {
      vm_frame_unpin (page->kpage);
      return false;
    }

  //printf ("Find a page (%p) (%p)\n", vm_find_page (page->addr), (void *)page );

  pagedir_clear_page (page->pagedir, uva);
  if (!pagedir_set_page (page->pagedir, uva, page->kpage, page->writable) )
    {
      ASSERT (false);
      vm_frame_unpin (page->kpage);
      return false;
    }

  ASSERT ( pagedir_get_page (page->pagedir, uva) );

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
  ASSERT (page->magic == PAGE_MAGIC);
  //printf ("Page unload for %p %p %d %p\n", page->addr, page->pagedir, page->type, page->magic);

  lock_acquire (&unload_lock);
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
  lock_release (&unload_lock);

  pagedir_clear_page (page->pagedir, page->addr);
  pagedir_add_page (page->pagedir, page->addr, (void *)page);
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

static void
vm_load_zero_page (uint8_t *kpage)
{
  /* Fill this page only with zeroes. */
  memset (kpage, 0, PGSIZE);
}

static void
vm_load_swap_page (uint8_t *kpage, struct vm_page *page)
{
  /* Read the content from swap and free the swap slot. */
  vm_swap_load (page->swap_data.index, kpage);
  vm_swap_free (page->swap_data.index);
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
vm_find_page (void *addr)
{
  //printf ("[find page] for addr %p\n", addr);

  uint32_t *pagedir = thread_current ()->pagedir;
  struct vm_page *page = NULL;
  page = (struct vm_page *) pagedir_find_page (pagedir, (const void *)addr);

  //printf (">>>> (%d) %p\n", (page == NULL), ret);
  return page;
}

static void
add_page (struct vm_page *page)
{
  //printf ("[add page] for addr %p in %p\n", page->addr, page->pagedir);
  
  pagedir_add_page (page->pagedir, page->addr, (void *)page);
}

void
vm_free_page (struct vm_page *page)
{
  if (page == NULL)
    return;
  //printf ("[delete page] for addr %p in %p\n", page->addr, page->pagedir);

  /* Free the swap data of the page if necessary. */
  if (page->type == SWAP && page->loaded == false)
    vm_swap_free (page->swap_data.index);

  pagedir_clear_page (page->pagedir, page->addr);
  free (page);
  --cnt;
}

bool
stack_access (const void *esp, void *addr)
{
  return (uint32_t)addr > 0 && addr >= (esp - 32) &&
     (PHYS_BASE - pg_round_down (addr)) <= (1<<23);
}

