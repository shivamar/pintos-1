#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <stddef.h>

/* Initialise swap table bitmap. */
void vm_swap_init (void);
/* Swap table operations. */
void vm_swap_load (size_t, void *);
size_t vm_swap_store (void *);
void vm_swap_free (size_t);

#endif /* vm/swap.h */
