#include "vm/swap.h"
#include <stdio.h>
#include <string.h>
#include <bitmap.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

#define BLOCKS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *swap_block;
static struct lock swap_lock;

static struct bitmap *swap_map;

static size_t swap_size = 0;

void
vm_swap_init ()
{
  swap_block = block_get_role (BLOCK_SWAP);
  lock_init (&swap_lock);  

  swap_size = block_size (swap_block); 
  swap_map = bitmap_create (swap_size);
}

/* Loads a page from the swap to main memory. */
void
vm_swap_load (size_t index, void *addr)
{
  lock_acquire (&swap_lock);
 
  size_t ofs; 
  for (ofs = 0; ofs < BLOCKS_PER_PAGE; ++ofs)
    {
      ASSERT (index < swap_size);
      ASSERT ( bitmap_test (swap_map, index) );

      block_read (swap_block, index, addr + ofs * BLOCK_SECTOR_SIZE);
      ++index;
    }

  lock_release (&swap_lock); 
}


/* Stores a page from main memory to swap disk. */
size_t
vm_swap_store (void *addr)
{
  lock_acquire (&swap_lock);
  size_t index = bitmap_scan_and_flip (swap_map, 0, BLOCKS_PER_PAGE, false);

  ASSERT (index != BITMAP_ERROR);

  size_t ofs, ind = index;
  for (ofs = 0; ofs < BLOCKS_PER_PAGE; ++ofs)
    {
      ASSERT (index < swap_size);
      ASSERT ( bitmap_test (swap_map, ind) );

      block_write (swap_block, ind, addr + ofs * BLOCK_SECTOR_SIZE);
      ++ind;
    }
  lock_release (&swap_lock);

  return index;
} 

/* Frees a swap frame. Sets the corresponding bit to zero. */
void
vm_swap_free (size_t index)
{
  lock_acquire (&swap_lock);
  
  size_t ofs;
  for (ofs = 0; ofs < BLOCKS_PER_PAGE; ++ofs)
    {
      ASSERT (index < swap_size);
      ASSERT ( bitmap_test (swap_map, index) );

      bitmap_reset (swap_map, index);
      ++index;
    }
  lock_release (&swap_lock);
}
