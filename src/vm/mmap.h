#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <hash.h>

/* Map region identifier. */
typedef int mapid_t;

struct vm_mfile
  {
    mapid_t mapping;
    int fd;                      /* File descriptor. */
    struct hash_elem hash_elem;  /* Hash element for the hash frame table. */
    void *addr;
  };

struct vm_mfile *vm_find_mfile (mapid_t);
void vm_mfile_insert (mapid_t, int);
bool vm_delete_mfile (void *);

#endif /* vm/mmap.h */
