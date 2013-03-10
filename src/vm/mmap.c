#include "vm/mmap.h"
#include <hash.h>
#include "threads/malloc.h"
#include "threads/synch.h"

static struct lock vm_mfile_lock;
static struct hash vm_mfiles;

static unsigned vm_mfile_hash (const struct hash_elem *, void *);
static bool vm_mfile_less (const struct hash_elem *, const struct hash_elem *,
                    void *);

void
vm_mfile_init ()
{
  lock_init (&vm_mfile_lock);
  hash_init (&vm_mfiles, vm_mfile_hash, vm_mfile_less, NULL);
}

unsigned
vm_mfile_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct vm_mfile *f = hash_entry (f_, struct vm_mfile, hash_elem);
  return hash_int ((unsigned)f->mapping);
}

bool
vm_mfile_less (const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
  const struct vm_mfile *a = hash_entry (a_, struct vm_mfile, hash_elem);
  const struct vm_mfile *b = hash_entry (b_, struct vm_mfile, hash_elem);

  return a->mapping < b->mapping;
}

/* Returns the mfile with the given mapid, or a null pointer if not found. */
struct vm_mfile *
vm_find_mfile (mapid_t mapping)
{
  struct vm_mfile *f;
  struct hash_elem *e;

  f->mapping = mapping;
  e = hash_find (&vm_mfiles, &f->hash_elem);
  return e != NULL ? hash_entry (e, struct vm_mfile, hash_elem) : NULL;
}

/* Removes the given mapid from the fd. */
bool
vm_delete_mfile (void *mapping)
{
  struct vm_mfile *f = vm_find_mfile (mapping);
  if (f == NULL)
    return false;

  lock_acquire (&vm_mfile_lock);
  hash_delete (&vm_mfiles, &f->hash_elem);
  free (f);
  lock_release (&vm_mfile_lock);

  return true; 
}

void
vm_mfile_insert (mapid_t mapping, int fd)
{
  struct vm_mfile *f = (struct vm_mfile *) malloc (sizeof (struct vm_mfile) );
  lock_acquire (&vm_mfile_lock);
  hash_insert (&vm_mfiles, &f->hash_elem);
  lock_release (&vm_mfile_lock);
}
