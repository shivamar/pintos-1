#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <hash.h>
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
  struct hash_elem hash_elem;
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
bool vm_load_page (struct vm_page *, void *, bool);
void vm_unload_page (struct vm_page *, void *);
struct vm_page *vm_grow_stack (void *, bool);
void vm_page_init (void);
void vm_pin_page (struct vm_page *);
void vm_unpin_page (struct vm_page *);
struct vm_page *find_page (void *);
void vm_delete_page (struct vm_page *);
bool stack_access (const void *, void *);
void vm_free_pages (void);

/* Hash helper functions. */
unsigned page_hash (const struct hash_elem *, void *UNUSED);
bool page_less (const struct hash_elem *, const struct hash_elem *, 
                void *UNUSED);

#endif /* vm/page.h */
