#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void exit (int status);
struct lock * file_lock;
void close_all_files (void);

#endif /* userprog/syscall.h */

