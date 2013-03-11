#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <list.h>
#include "filesys/file.h"
#include "threads/interrupt.h"

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
  struct list_elem list_elem;
  uint32_t *pagedir;

  struct        
  {
    struct file *file;
    off_t ofs;
    size_t read_bytes;
    size_t zero_bytes;
  } file_data;

  struct
  {
    size_t index;
  } swap_data;
};

struct vm_page *vm_new_file_page (void *, struct file *, off_t, uint32_t, 
                                  uint32_t, bool);
struct vm_page *vm_new_swap_page (void *, size_t, bool);
struct vm_page *vm_new_zero_page (void *, bool);
bool vm_load_page (struct vm_page *, void *);
void vm_unload_page (struct vm_page *, void *);
bool vm_delete_page (struct vm_page *);
struct vm_page *vm_grow_stack (void *);
void vm_page_init (void);
void vm_pin_page (struct vm_page *);
void vm_unpin_page (struct vm_page *);
struct vm_page *find_page (void *);
bool stack_access (void *, void *);

#endif /* vm/page.h */
