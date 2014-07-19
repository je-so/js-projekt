/* title: AtomicOps

   Offers atomic operations for type integers.

   Full Memory Barrier:
   The barrier wait until all previous read and write operations are done and it ensures that all future read
   and write operations cannot be moved before the barrier.

   Acquire Memory Barrier:
   All future read and writer operations cannot be moved before the barrier.
   Previous read and write operations may not be done yet.

   Release Memory Barrier:
   Ensures that all all previous read and write operations are done.
   But future read and writer operations could be moved before the barrier.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/atomic.h
    Header file <AtomicOps>.

   file: C-kern/memory/atomic.c
    Implementation file <AtomicOps impl>.
*/
#ifndef CKERN_MEMORY_ATOMIC_HEADER
#define CKERN_MEMORY_ATOMIC_HEADER


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_atomic
 * Test functionality of atomic operations. */
int unittest_memory_atomic(void) ;
#endif


/* struct: int_t
 * Offers some atomic operations on integers.
 * If a processor does not support atomic ops then
 * this implementation relys one the compiler to emulate them. */
struct int_t ;

// group: atomicops

/* function: atomicread_int
 * Reads last written value from memory location i.
 * The value must be written with <atomicwrite_int> or any other atomic operation.
 * This operation ensures also an acquire memory barrier. After this operation completed
 * this thread will see the newest values written by other threads. */
int atomicread_int(int * i) ;

/* function: atomicwrite_int
 * Writes newval into memory located at i.
 * This operation ensures also a release memory barrier. After this operation has completed
 * other threads will see all write operations done by this thread before this atomic write operation. */
void atomicwrite_int(int * i, int newval) ;

/* function: atomicadd_int
 * Add increment to i and return old value atomically.
 * This operation ensures also a full memory barrier. */
int atomicadd_int(int * i, uint32_t increment) ;

/* function: atomicsub_int
 * Sub decrement from i and return old value atomically.
 * This operation ensures also a full memory barrier. */
int atomicsub_int(int * i, uint32_t decrement) ;

/* function: atomicswap_int
 * If *i is equal to oldval change it into newval atomically else do nothing.
 * The operation returns the old value of i. If the returned value equals
 * oldval the operation was successful.
 * This operation ensures also a full memory barrier. */
int atomicswap_int(int * i, int oldval, int newval) ;

/* function: atomicset_int
 * Sets flag to value != 0 and returns previous value.
 * A return value of 0 means flag was not set before therefore the lock is acquired.
 * A return value of != 0 means flag was set before therefore the lock is not acquired.
 * This operation ensures also an acquire memory barrier. The thread will see all values written
 * by other threads before this operation if it reads them after this operation.
 * Other threads may not see the values written by this thread. */
int atomicset_int(uint8_t * flag) ;

/* function: atomicclear_int
 * Sets flag to value 0. Call this function only if <atomicset_int> returned true.
 * This operation ensures also a release memory barrier. Other threads will see all values written
 * by this thread before this operation (at least after they have executed an acquire barrier).
 * This thread may not see the values written by other threads. */
void atomicclear_int(uint8_t * flag) ;



// section: inline implementation

/* define: atomicadd_int
 * Implements <int_t.atomicadd_int>. */
#define atomicadd_int(i, increment)   \
         (__sync_fetch_and_add((i), (increment)))

/* define: atomicclear_int
 * Implements <int_t.atomicclear_int>. */
#define atomicclear_int(flag)   \
         (__sync_lock_release(flag))

/* define: atomicread_int
 * Implements <int_t.atomicread_int>. */
#define atomicread_int(i)  \
         (__sync_fetch_and_add((i), 0))

/* define: atomicsub_int
 * Implements <int_t.atomicsub_int>. */
#define atomicsub_int(i, decrement)   \
         (__sync_fetch_and_sub((i), (decrement)))

/* define: atomicset_int
 * Implements <int_t.atomicset_int>. */
#define atomicset_int(flag) \
         (__sync_lock_test_and_set(flag, 1))

/* define: atomicswap_int
 * Implements <int_t.atomicswap_int>. */
#define atomicswap_int(i, oldval, newval) \
         (__sync_val_compare_and_swap(i, oldval, newval))

/* define: atomicwrite_int
 * Implements <int_t.atomicwrite_int>.
 * TODO: Replace atomicswap (full memory barrier) with store fence. */
#define atomicwrite_int(i, newval)              \
         ( __extension__ ({                     \
            typeof(i)     _i  = (i) ;           \
            typeof(*_i) _new  = (newval) ;      \
            typeof(*_i) _old  = *_i ;           \
            typeof(*_i) _old2 ;                 \
            for (;;) {                          \
               _old2 =                          \
                  __sync_val_compare_and_swap(  \
                              _i, _old, _new) ; \
               if (_old2 == _old) break ;       \
               _old = _old2 ;                   \
            }                                   \
            _old ;                              \
         }))

#endif
