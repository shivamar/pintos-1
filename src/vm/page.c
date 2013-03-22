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

/* Ensure synchronization on load and unload. */
static struct lock load_lock;
static struct lock unload_lock;

/* Load function for the specific type of page. */
static bool vm_load_file_page (uint8_t *kpage, struct vm_page *page);
static void vm_load_swap_page (uint8_t *kpage, struct vm_page *page);
static void vm_load_zero_page (uint8_t *kpage);

static void add_page (struct vm_page *page);

/* Initialise the page table locks. */
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
                  size_t zero_bytes, bool writable, off_t block_id)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));
  
  if (page == NULL)
    return NULL;

  page->type = FILE;
  page->addr = addr;
  page->pagedir = thread_current ()->pagedir;
  page->file_data.file = file;
  page->file_data.ofs = ofs;
  page->file_data.read_bytes = read_bytes;
  page->file_data.zero_bytes = zero_bytes;
  page->file_data.block_id = block_id;
  page->writable = writable;
  page->loaded = false;
  page->kpage = NULL;

  add_page (page);

  return page; 
}

/* Creates a new page initialized to zero on loading. */
struct vm_page*
vm_new_zero_page (void *addr, bool writable)
{
  struct vm_page *page = (struct vm_page*) malloc (sizeof (struct vm_page));

  if (page == NULL)
    return NULL;

  page->type = ZERO;
  page->addr = addr;
  page->pagedir = thread_current ()->pagedir;
  page->writable = writable;
  page->loaded = false;
  page->kpage = NULL;  

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
vm_load_page (struct vm_page *page, bool pinned)
{
  /* Get a frame of memory. */
  lock_acquire (&load_lock);
  
  /* If we have a read-only file try to look for a frame if any
     that contains the same data. */
  if (page->type == FILE && page->file_data.block_id != -1)
    page->kpage = vm_lookup_frame (page->file_data.block_id);
  /* Otherwise obtain an empty frame from the frame table. */
  if (page->kpage == NULL)
    page->kpage = vm_get_frame (PAL_USER);

  lock_release (&load_lock);
  vm_frame_set_page (page->kpage, page);

  bool success = true;
  /* Performs the specific loading operation. */
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

  /* Clear any previous mapping and set a new one. */
  pagedir_clear_page (page->pagedir, page->addr);
  if (!pagedir_set_page (page->pagedir, page->addr, page->kpage, page->writable) )
    {
      ASSERT (false);
      vm_frame_unpin (page->kpage);
      return false;
    }

  pagedir_set_dirty (page->pagedir, page->addr, false);
  pagedir_set_accessed (page->pagedir, page->addr, true);

  page->loaded = true;
  /* On succes we leave the frame pinned if the caller wants so. */
  if (!pinned)
    vm_frame_unpin (page->kpage);
  return true;
}

/* Unloads a page by writing its content back to disk if the file
   is writable or to swap if not. Clears the mapping and sets 
   the pagedir entry to point to the page struct. */
void
vm_unload_page (struct vm_page *page, void *kpage)
{
  lock_acquire (&unload_lock);
  if (page->type == FILE && pagedir_is_dirty (page->pagedir, page->addr) &&
      file_writable (page->file_data.file) == false)
    {
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

/* Loads a file page into the given frame. Reads read_bytes from 
   the file and sets the remaining bytes to 0. */
static bool
vm_load_file_page (uint8_t *kpage, struct vm_page *page)
{
  /* Read the content of the page from file. */

  sys_t_filelock (true);
  file_seek (page->file_data.file, page->file_data.ofs);
  size_t ret = file_read (page->file_data.file, kpage, 
                          page->file_data.read_bytes);
  sys_t_filelock (false);
   
  if (ret != page->file_data.read_bytes)
    {
      vm_free_frame (kpage, page->pagedir);
      return false;
    }
  
  /* Fill the rest of the page with zeroes. */
  memset (kpage + page->file_data.read_bytes, 0, page->file_data.zero_bytes);
  return true;
}

/* To load a zero page we just have to set everything to 0. Usually
   pages wont't remain all zeros when they are swapped in. */
static void
vm_load_zero_page (uint8_t *kpage)
{
  /* Fill this page only with zeroes. */
  memset (kpage, 0, PGSIZE);
}

/* Loads a page from the swap into main memory and frees
   the underlying swap slot. */
static void
vm_load_swap_page (uint8_t *kpage, struct vm_page *page)
{
  /* Read the content from swap and free the swap slot. */
  vm_swap_load (page->swap_data.index, kpage);
  vm_swap_free (page->swap_data.index);
}

/* Creates a new zero page at the top of the thread's stack 
   address space. Then loads the page into memory. */
struct vm_page *
vm_grow_stack (void *uva, bool pinned)
{
  struct vm_page *page = vm_new_zero_page (uva, true);
  if ( !vm_load_page (page, pinned) )
    return NULL;

  return page;
}

/* Searches for a supplemental page in the thread's pagedir. If the page
   is not present we get the struct directly. Otherwise is the page is
   loaded we obtain it from the frame table using the physical address. */
struct vm_page *
vm_find_page (void *addr)
{
  uint32_t *pagedir = thread_current ()->pagedir;
  struct vm_page *page = NULL;
  page = (struct vm_page *) pagedir_find_page (pagedir, (const void *)addr);

  return page;
}

/* Stores inside the page table entry a pointer to the page struct
   so we can use the pagedir to track the supplemental page. */
static void
add_page (struct vm_page *page)
{
  pagedir_add_page (page->pagedir, page->addr, (void *)page);
}

/* Frees a page struct and it's corresponding swap slot. */
void
vm_free_page (struct vm_page *page)
{
  if (page == NULL)
    return;
  
  /* Free the swap data of the page if necessary. */
  if (page->type == SWAP && page->loaded == false)
    vm_swap_free (page->swap_data.index);

  /* Clear the mapping from the thread's pagedir. */
  pagedir_clear_page (page->pagedir, page->addr);
  free (page);
  --cnt;
}

/* Use a heuristic to check for stack access. We check if the
   address is in the user space and the fault access is at 
   most 32 bytes below the stack pointer. */
bool
stack_access (const void *esp, void *addr)
{
  return (uint32_t)addr > 0 && addr >= (esp - 32) &&
     (PHYS_BASE - pg_round_down (addr)) <= (1<<23);
}

