#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <inttypes.h>

static void syscall_handler (struct intr_frame *);
static int get_user_byte (const uint8_t *uaddr);
static bool write_user_byte (uint8_t *uaddr, uint8_t byte);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t sys_call_number = *((uint32_t*) f->esp);
  printf ("system call number %"PRIu32"!\n", sys_call_number);
  thread_exit ();
}

static int
get_user_byte (const uint8_t *uaddr)
{
  if (is_user_vaddr (uaddr))
    {
      return get_user (uaddr);
    }
  else
    {
      return -1;
    }
}

static bool
write_user_byte (uint8_t *uaddr, uint8_t byte)
{
  if (is_user_vaddr (uaddr))
    {
      return put_user (uaddr, byte);
    }
  else
    {
      return false;
    }
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
