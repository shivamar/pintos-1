#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include <stdbool.h>
#include <stddef.h>
#include "filesys/file.h"

enum vm_page_type
  {
    SWAP,
    FILE,
    ZERO
  };

struct vm_page
{
  enum vm_page_type type;        /* Page type from vm_page_type enum. */
  bool loaded;                   /* If the page is loaded. */
  bool writable;                 /* If the page is writable. */
  void *addr;                    /* User virtual address of the page. */
  void *kpage;                   /* Physical address of the page if loaded. */
  uint32_t *pagedir;             /* Page's hardware pagedir. */ 
  struct list_elem frame_elem;   /* List elem for frame shared pages list. */

  struct        
  {
    struct file *file;           /* File struct for the page. */
    off_t ofs;                   /* Offset in the file. */
    size_t read_bytes;           /* Rad bytes of the file. */
    size_t zero_bytes;           /* Zero bytes of the file. */
    off_t block_id;              /* Inode block index for shared files. */
  } file_data;

  struct
  {
    size_t index;                 /* Swap block index. */
  } swap_data;
};

/* Initialize the page locks. */
void vm_page_init (void);
/* Create a new page. */
struct vm_page *vm_new_file_page (void *, struct file *, off_t, uint32_t, 
                                  uint32_t, bool, off_t);
struct vm_page *vm_new_zero_page (void *, bool);
/* Load or unload the given page. */
bool vm_load_page (struct vm_page *, bool);
void vm_unload_page (struct vm_page *, void *);
/* Grow a thread's stack. */
struct vm_page *vm_grow_stack (void *, bool);
/* Pin or unpin a page's underlying frame. */
void vm_pin_page (struct vm_page *);
void vm_unpin_page (struct vm_page *);
/* Find / Free a given page. */
struct vm_page *vm_find_page (void *);
void vm_free_page (struct vm_page *);
/* Heuristic for stack access. */
bool stack_access (const void *, void *);

#endif /* vm/page.h */
