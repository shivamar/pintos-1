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
  enum vm_page_type type;
  bool loaded;
  bool writable;
  void *addr;
  void *kpage;
  uint32_t *pagedir;
  struct list_elem frame_elem;

  struct        
  {
    struct file *file;
    off_t ofs;
    size_t read_bytes;
    size_t zero_bytes;
    off_t block_id;
  } file_data;

  struct
  {
    size_t index;
  } swap_data;

  unsigned magic;
};

struct vm_page *vm_new_file_page (void *, struct file *, off_t, uint32_t, 
                                  uint32_t, bool, off_t);
struct vm_page *vm_new_swap_page (void *, size_t, bool);
struct vm_page *vm_new_zero_page (void *, bool);
bool vm_load_page (struct vm_page *, void *, bool);
void vm_unload_page (struct vm_page *, void *);
struct vm_page *vm_grow_stack (void *, bool);
void vm_page_init (void);
void vm_pin_page (struct vm_page *);
void vm_unpin_page (struct vm_page *);
struct vm_page *vm_find_page (void *);
void vm_free_page (struct vm_page *);
bool stack_access (const void *, void *);
void vm_free_pages (void);

#endif /* vm/page.h */
