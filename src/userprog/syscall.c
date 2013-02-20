#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <inttypes.h>
#include <list.h>

/* Process identifier. */
typedef int pid_t;

static void syscall_handler (struct intr_frame *);

static void      sys_halt (void);
static void      sys_exit (int status);
static pid_t     sys_exec (const char *file);
static int       sys_wait (pid_t pid);
static bool      sys_create (const char *file, unsigned initial_size);
static bool      sys_remove (const char *file);
static int       sys_open (const char *file);
static int       sys_filesize (int fd);
static int       sys_read (int fd, void *buffer, unsigned size);
static int       sys_write (int fd, const void *buffer, unsigned size);
static void      sys_seek (int fd, unsigned position);
static unsigned  sys_tell (int fd);
static void      sys_close (int fd);

static int get_user_byte (const uint8_t *uaddr);
static bool write_user_byte (uint8_t *uaddr, uint8_t byte);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

static struct lock file_lock;

typedef int (*handler) (uint32_t, uint32_t, uint32_t);
static handler syscall_map[32];

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  syscall_map[SYS_HALT]     = (handler)sys_halt;
  syscall_map[SYS_EXIT]     = (handler)sys_exit;
  syscall_map[SYS_EXEC]     = (handler)sys_exec;
  syscall_map[SYS_WAIT]     = (handler)sys_wait;
  syscall_map[SYS_CREATE]   = (handler)sys_create;
  syscall_map[SYS_REMOVE]   = (handler)sys_remove;
  syscall_map[SYS_OPEN]     = (handler)sys_open;
  syscall_map[SYS_FILESIZE] = (handler)sys_filesize;
  syscall_map[SYS_READ]     = (handler)sys_read;
  syscall_map[SYS_WRITE]    = (handler)sys_write;
  syscall_map[SYS_SEEK]     = (handler)sys_seek;
  syscall_map[SYS_TELL]     = (handler)sys_tell;
  syscall_map[SYS_CLOSE]    = (handler)sys_close;

  lock_init (&file_lock);   
}

static void
syscall_handler (struct intr_frame *f) 
{
  handler function;
  int *param = f->esp, ret;

  if ( is_user_vaddr(param) == -1) 
    sys_exit(-1);

  if (!( is_user_vaddr (param + 1) && is_user_vaddr (param + 2) && is_user_vaddr (param + 3)))
    sys_exit(-1); 

  if (*param < SYS_HALT || *param > SYS_INUMBER)
    sys_exit(-1);

  function = syscall_map[*param];
  
  ret = function (*(param + 1), *(param + 2), *(param + 3));    
  f->eax = ret;

  return; 
}

/* Halt the operating system. */
static void
sys_halt (void)
{
    shutdown_power_off ();
}

/* Terminate this process. */
static void
sys_exit (int status)
{
    struct thread *t;
    
    t = thread_current ();
    // TO DO: close files

    t->ret_status = status;
    thread_exit ();
    return -1;
}

/* Start another process. */
static pid_t
sys_exec (const char *file)
{
    // TO DO:
}

/* Wait for a child process to die. */
static int
sys_wait (pid_t pid)
{
    return process_wait (pid);
}

/* Create a file. */
static bool
sys_create (const char *file, unsigned intitial_size)
{
    // TO DO:
}

/* Delete a file. */
static bool
sys_remove (const char *file)
{
    // TO DO:
}

/* Open a file. */
static int
sys_open (const char *file)
{
    // TO DO:
}

/* Obtain a file's size. */
static int
sys_filesize (int fd)
{
    // TO DO:
}

/* Read from a file. */
static int
sys_read (int fd, void *buffer, unsigned length)
{
    // TO DO:   
}

/* Write to a file. */
static int
sys_write (int fd, const void *buffer, unsigned length)
{
    struct file *f;
    int ret = -1;

    lock_acquire (&file_lock);
    if (fd == STDOUT_FILENO)
        putbuf (buffer, length);
    else if (fd == STDIN_FILENO)
        ;//done
    else if ( !is_user_vaddr (buffer) || !is_user_vaddr (buffer + length) )
      {
        lock_release (&file_lock);
        sys_exit (-1);
      } 
    else
      {
        // TO DO: write to a file
      }

    lock_release (&file_lock);
    return ret;
}

/* Change position in a file. */
static void
sys_seek (int fd, unsigned position)
{
    //TO DO:
}

/* Report current position in a file. */
static unsigned
sys_tell (int fd)
{
    // TO DO:
}

/* Close a file. */
static void
sys_close (int fd)
{
    // TO DO:
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
