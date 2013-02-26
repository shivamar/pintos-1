#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
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
void             sys_exit (int status);
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

#ifdef MEMORY_TEST
static bool read_user_byte (const uint8_t *uaddr);
static bool write_user_byte (uint8_t *uaddr, uint8_t byte);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
#endif
static struct file *file_by_fid (fid_t);
static struct file *file_by_fid_in_thread (fid_t);
static fid_t allocate_fid (void);

static struct lock file_lock;
static struct list file_list;

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
  list_init (&file_list);  
}

static void
syscall_handler (struct intr_frame *f) 
{
  handler function;
  int *param = f->esp, ret;

  if ( !is_user_vaddr(param) )
    sys_exit (-1);

  if (!( is_user_vaddr (param + 1) && is_user_vaddr (param + 2) && is_user_vaddr (param + 3))) 
    sys_exit (-1); 

  if (*param < SYS_HALT || *param > SYS_INUMBER)
    sys_exit (-1);

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
void
sys_exit (int status)
{
  struct thread *t;
  struct list_elem *e;    

  t = thread_current ();
  if (lock_held_by_current_thread (&file_lock) )
    lock_release (&file_lock);

  while (!list_empty (&t->files) )
    {
      e = list_begin (&t->files);
      sys_close ( list_entry (e, struct file, thread_elem)->fid );
    }

  t->ret_status = status;
  thread_exit ();
}

/* Start another process. */
static pid_t
sys_exec (const char *file)
{
  lock_acquire (&file_lock);
  int ret = process_execute (file);
  lock_release (&file_lock);
  return ret;
}

/* Wait for a child process to die. */
static int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

/* Create a file. */
static bool
sys_create (const char *file, unsigned initial_size)
{
  if (file == NULL)
    sys_exit (-1);
  return filesys_create (file, initial_size);
}

/* Delete a file. */
static bool
sys_remove (const char *file)
{   
   if (file == NULL)
     sys_exit (-1);
   return filesys_remove (file);
}

/* Open a file. */
static int
sys_open (const char *file)
{
  struct file *f;

  if (file == NULL)
    return -1;
  
  f = filesys_open (file);
  if (f == NULL)
    return -1;

  lock_acquire (&file_lock);
  f->fid = allocate_fid ();
  list_push_back (&file_list, &f->file_elem);
  list_push_back (&thread_current ()->files, &f->thread_elem);
  lock_release (&file_lock);

  return f->fid;
}

/* Obtain a file's size. */
static int
sys_filesize (int fd)
{
  struct file *f;
  int size = -1;  

  f = file_by_fid (fd);
  if (f == NULL)
    return -1;

  lock_acquire (&file_lock);
  size = file_length (f);  
  lock_release (&file_lock);

  return size;
}

/* Read from a file. */
static int
sys_read (int fd, void *buffer, unsigned length)
{
  struct file *f;  
  int ret = -1;

  lock_acquire (&file_lock);
  if (fd == STDIN_FILENO)
    {
      unsigned i;
      for (i = 0; i < length; ++i)
        *(uint8_t *)(buffer + i) = input_getc ();
      ret = length;
    }
  else if (fd == STDOUT_FILENO) 
    ret = -1;
  else if ( !is_user_vaddr (buffer) || !is_user_vaddr (buffer + length) )
    {
      lock_release (&file_lock);
      sys_exit (-1);
    }
  else
    {
      f = file_by_fid (fd);
      if (f == NULL)
        ret = -1;
      else
        ret = file_read (f, buffer, length);
    }
  lock_release (&file_lock);

  return ret;
}

/* Write to a file. */
static int
sys_write (int fd, const void *buffer, unsigned length)
{
  struct file *f;
  int ret = -1;

  lock_acquire (&file_lock);
  if (fd == STDIN_FILENO)
    ret = -1;
  else if (fd == STDOUT_FILENO) 
    {
      putbuf (buffer, length);
      ret = length;
    }
  else if ( !is_user_vaddr (buffer) || !is_user_vaddr (buffer + length) )
    {
      lock_release (&file_lock);
      sys_exit (-1);
    } 
  else
    {
      f = file_by_fid (fd);
      if (f == NULL)
        ret = -1;
      else 
        ret = file_write (f, buffer, length); 
    }
  lock_release (&file_lock);
    
  return ret;
}

/* Change position in a file. */
static void
sys_seek (int fd, unsigned position)
{
  struct file *f;

  f = file_by_fid (fd);
  if (!f)
    sys_exit (-1);

  lock_acquire (&file_lock);
  file_seek (f, position);
  lock_release (&file_lock);
}

/* Report current position in a file. */
static unsigned
sys_tell (int fd)
{
  struct file *f;
  unsigned status;

  f = file_by_fid (fd);
  if (!f)
    sys_exit (-1);
  
  lock_acquire (&file_lock);
  status = file_tell (f); 
  lock_release (&file_lock);
  
  return status;
}

/* Close a file. */
static void
sys_close (int fd)
{
  struct file *f;

  f = file_by_fid_in_thread (fd);     

  if (f == NULL)
    sys_exit (-1);
  
  lock_acquire (&file_lock);
  list_remove (&f->file_elem);
  list_remove (&f->thread_elem);
  file_close (f);
  lock_release (&file_lock);
}

static fid_t
allocate_fid (void)
{
  static fid_t next_fid = 2;
  return next_fid++;
}

/* Returns the file with the given fid from all files */
struct file *
file_by_fid (fid_t fid)
{
  struct list_elem *e;
  
  for (e = list_begin (&file_list); e != list_end (&file_list);
       e = list_next (e))
    {
      struct file *f = list_entry (e, struct file, file_elem);
      if (f->fid == fid)
        return f;
    }

  return NULL;
}
 
/* Returns the file with the given fid from the thread files */
static struct file *
file_by_fid_in_thread (int fid)
{
  struct list_elem *e;
  struct thread *t;

  t = thread_current();
  for (e = list_begin (&t->files); e != list_end (&t->files); 
       e = list_next (e))
    {
      struct file *f = list_entry (e, struct file, thread_elem);
      if (f->fid == fid)
        return f;
    }
  
  return NULL;
}

#ifdef MEMORY_TEST
static bool
read_user_byte (const uint8_t *uaddr)
{
  if (is_user_vaddr (uaddr))
    {
      return get_user (uaddr) != -1;
    }
  else
    {
      return false;
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
#endif
