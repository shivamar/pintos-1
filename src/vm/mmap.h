#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <hash.h>

/* Map region identifier. */
typedef int mapid_t;

struct vm_mfile
  {
    mapid_t mapid;
    int fid;                     /* File descriptor. */
    struct hash_elem hash_elem;  /* Hash element for the hash frame table. */
    struct list_elem thread_elem;/* List elem for a thread's mfile list. */
    void *start_addr;            /* User virtual address of start and end */
    void *end_addr;              /* of the mapped file as it might span on */
                                 /* multiple pages. */
  };

void vm_mfile_init (void);
struct vm_mfile *vm_find_mfile (mapid_t);
void vm_insert_mfile (mapid_t, int, void *, void *);
bool vm_delete_mfile (mapid_t);

#endif /* vm/mmap.h */
