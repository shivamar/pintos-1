#include "vm/mmap.h"
#include <hash.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"

static struct lock mfile_lock;
static struct hash vm_mfiles;

static unsigned vm_mfile_hash (const struct hash_elem *, void *);
static bool vm_mfile_less (const struct hash_elem *, const struct hash_elem *,
                    void *);

/* Initialise the mmap table. */
void
vm_mfile_init (void)
{
  lock_init (&mfile_lock);
  hash_init (&vm_mfiles, vm_mfile_hash, vm_mfile_less, NULL);
}

/* Returns the mfile with the given mapid, or a null pointer if not found. */
struct vm_mfile *
vm_find_mfile (mapid_t mapid)
{
  struct vm_mfile mf;
  struct hash_elem *e;

  mf.mapid = mapid;
  e = hash_find (&vm_mfiles, &mf.hash_elem);
  return e != NULL ? hash_entry (e, struct vm_mfile, hash_elem) : NULL;
}

/* Removes the given mapid from the fid. */
bool
vm_delete_mfile (mapid_t mapid)
{
  struct vm_mfile *mf = vm_find_mfile (mapid);
  if (mf == NULL)
    return false;

  /* Remove the given file from the hash table. */
  lock_acquire (&mfile_lock);
  hash_delete (&vm_mfiles, &mf->hash_elem);
  list_remove (&mf->thread_elem);
  free (mf);
  lock_release (&mfile_lock);

  return true; 
}

/* Creates a new mfile from a given mapid and a fid. Intserts the new mfile
in the mfile hash table so it will be found on future lookups. */
void
vm_insert_mfile (mapid_t mapid, int fid, void *start_addr, void *end_addr)
{
  struct vm_mfile *mf = (struct vm_mfile *) malloc (sizeof (struct vm_mfile));
  mf->fid = fid;
  mf->mapid = mapid;
  mf->start_addr = start_addr;
  mf->end_addr = end_addr;

  /* Insert the new file in the hash table. */
  lock_acquire (&mfile_lock);
  list_push_back (&thread_current ()->mfiles, &mf->thread_elem);
  hash_insert (&vm_mfiles, &mf->hash_elem);
  lock_release (&mfile_lock);
}

/* Returns a hash value for a mfile f. */
static unsigned
vm_mfile_hash (const struct hash_elem *mf_, void *aux UNUSED)
{
  const struct vm_mfile *mf = hash_entry (mf_, struct vm_mfile, hash_elem);
  return hash_int ((unsigned)mf->mapid);
}

/* Returns true if a mfile precedes mfile b. */
static bool
vm_mfile_less (const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
  const struct vm_mfile *a = hash_entry (a_, struct vm_mfile, hash_elem);
  const struct vm_mfile *b = hash_entry (b_, struct vm_mfile, hash_elem);

  return a->mapid < b->mapid;
}
