#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <list.h>
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
  struct list_elem list_elem;  

  struct        
  {
    struct file *file;
    off_t ofs;
    size_t read_bytes;
    size_t zero_bytes;   
  } file_data;

  struct
  {
    // TO DO:
    int index;
  } swap_data;
};

struct vm_page *vm_new_file_page (void *, struct file *, off_t, uint32_t, 
                                  uint32_t, bool);
struct vm_page *vm_new_swap_page (void *, bool);
struct vm_page *vm_new_zero_page (void *, bool);
bool vm_load_page (struct vm_page *, void *, uint32_t *);
void vm_free_page (struct vm_page *);
bool vm_grow_stack (void *);
void vm_page_init (void);
struct vm_page *find_page (void *);

#endif /* vm/page.h */
