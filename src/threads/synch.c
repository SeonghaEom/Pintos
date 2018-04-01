/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"


static bool priority_more(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

static bool less( struct thread *a,  struct thread *b);

static int find_max(struct list *list);

static void update_highest_thread(struct semaphore *sema);

static bool cond_priority_more (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
  sema->highest_thread = NULL;
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    { 
      /* PRIORITY_DONATE: change list_push_back to list_insert_ordered */   
      list_push_back(&sema->waiters, &thread_current()->elem);
      list_sort (&sema->waiters, priority_more, NULL );
      
      /* PRIORITY: update highest_thread of sema */
      update_highest_thread(sema);

      /* list_push_back(&sema->waiters, &thread_current()->elem); */
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);

}

static bool
priority_more(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
  const struct thread *a = list_entry (a_, struct thread, elem);
  const struct thread *b = list_entry (b_, struct thread, elem);
  return a->priority > b->priority;
}


/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  //printf ("current thread: %s, waiters' size: %d\n", thread_current ()->name, list_size (&sema->waiters));
  old_level = intr_disable ();
  
  if (!list_empty (&sema->waiters)) 
  {
    list_sort (&sema->waiters, priority_more, NULL);
    thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
    if (!list_empty(&sema->waiters))
    {
      sema->highest_thread =list_entry (list_front (&sema->waiters), struct thread, elem);
    }
    else
    {
      sema->highest_thread=NULL;
    }
  }
  sema->value++;
  thread_yield();

  intr_set_level (old_level);

}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);



}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.
    list_reverse(&ready_list);
   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{ 
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  struct thread *holder= lock->holder;
  struct thread *current= thread_current();
  
  /*PRIORITY_DONATION: donate priority if needed, NESTED, CHAIN DONATIONS*/
  if (holder != NULL)
  {
    if (need_donate (holder, current))
    {
      thread_donate (holder, current);
    }
    /* NESTED_DONATION */
    while (holder->wait_lock !=NULL)
    {
      update_highest_thread(&lock->semaphore);
      update_donation_thread(holder->wait_lock->holder);
      
      holder = holder->wait_lock->holder;
    }
    current->wait_lock = lock;
  }
  
  /*success in acquiring lock*/
  sema_down (&lock->semaphore);

  current->wait_lock = NULL;
  lock->holder = thread_current ();
  list_push_back(&thread_current()->my_locks,&lock->lock_elem);
}

/* PRIORITY_DONATION: update highest priority thread in sema's waiting lock */
static void
update_highest_thread(struct semaphore * sema)
{
  priority_sort(&sema->waiters);
  struct thread * thread = list_entry(list_begin(&sema->waiters), struct thread, elem);

  sema->highest_thread = thread;
}

/* PIRORITY_DONATION: update the priority of holder by the highest thread among all waiters lists */
void
update_donation_thread(struct thread* holder)
{
  if (!list_empty (&holder->my_locks))
  {
    struct list_elem *e;
    int max = holder->priority;
    for (e = list_begin (&holder->my_locks);
        e != list_end (&holder->my_locks);
        e = list_next(e))
    {
      struct lock *lock = list_entry(e, struct lock, lock_elem);
      if (max < (&lock->semaphore)->highest_thread->priority)
      {
        max = (&lock->semaphore)->highest_thread->priority;
      }
    }
    holder->priority = max;
  }
}
/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));


  list_remove(&lock->lock_elem);
  /*PRIORITY_DONATE: restore the original priority if donated */
  if ((lock->holder)->priority != ((lock->holder)->original)){
    if (!list_empty(&lock->holder->my_locks))
        {  lock->holder->priority =find_max(&lock->holder->my_locks);   }
    else
    {
      lock->holder->priority = lock->holder->original;
    }
  }


  lock->holder = NULL;
  sema_up (&lock->semaphore);
  thread_yield();

}

static int
find_max (struct list *list)
{
  struct list_elem *max = list_begin(list); /*list=> lock->holder->my_locks*/
  struct thread *max_highest_thread = (&list_entry(max, struct lock, lock_elem)->semaphore)->highest_thread;
  struct list_elem *e;
  struct lock *next_lock = list_entry(e, struct lock, lock_elem);
  struct thread *highest_thread = (&next_lock->semaphore)->highest_thread;

  if (max != list_end (list))
  {
    for (e = list_next (max); e!=list_end(list); e=list_next(e))
      if (less(max_highest_thread, highest_thread))
        max = e;
  }
  return max_highest_thread->priority;
}


static bool
less(struct thread *a, struct thread *b)
{
  return a->priority < b->priority;
}


/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);

  list_push_back (&cond->waiters, &waiter.elem);
  (&waiter.semaphore)->highest_thread = lock->holder;     /* CONDVAR: storing threads to remember priority */

  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

static bool
cond_priority_more(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
  const struct semaphore_elem *a = list_entry (a_, struct semaphore_elem, elem);
  const struct semaphore_elem *b = list_entry (b_, struct semaphore_elem, elem);

  return (&a->semaphore)->highest_thread->priority > (&b->semaphore)->highest_thread->priority;
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters))
    list_sort ( &cond->waiters, cond_priority_more, NULL);

    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
