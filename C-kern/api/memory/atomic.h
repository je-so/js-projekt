/* title: AtomicOps

   Offers atomic operations for type integers.

   Full Memory Barrier:
   The barrier waits until all previous read and write operations are done and it ensures that all future read
   and write operations cannot be moved before the barrier.

   Acquire Memory Barrier:
   All future read and write operations cannot be moved before the barrier.
   Previous read and write operations may not be done yet.

   Release Memory Barrier:
   Ensures that all all previous read and write operations are done.
   But future read and writer operations could be moved before the barrier.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
int unittest_memory_atomic(void);
#endif


/* struct: atomicint_t
 * Offers some atomic operations on integers.
 * If a processor does not support atomic ops then
 * this implementation relies on the compiler to emulate them. */
typedef int atomicint_t;

// group: atomicops

/* function: read_atomicint
 * Reads last written value from memory location i.
 * The value must be written with <write_atomicint> or any other atomic operation.
 * This operation ensures also an acquire memory barrier. After this operation completes
 * this thread will see the newest values written by other threads. */
int read_atomicint(int* i);

/* function: write_atomicint
 * Writes newval into memory located at i.
 * This operation ensures also a release memory barrier. After this operation completes
 * other threads will see all write operations done by this thread before this atomic write operation. */
void write_atomicint(int* i, int newval);

/* function: clear_atomicint
 * Setzt *i auf 0 und gibt alten Wert von *i zurück als atomare Operation.
 * Diese Operation beinhaltet auch eine full memory barrier. */
int clear_atomicint(int* i);

/* function: add_atomicint
 * Add increment to i and return old value atomically.
 * This operation ensures also a full memory barrier. */
int add_atomicint(int* i, uint32_t increment);

/* function: sub_atomicint
 * Sub decrement from i and return old value atomically.
 * This operation ensures also a full memory barrier. */
int sub_atomicint(int* i, uint32_t decrement);

/* function: cmpxchg_atomicint
 * If *i is equal to oldval change it into newval atomically else do nothing.
 * The operation returns the old value of i. If the returned value equals
 * oldval the operation was successful.
 * This operation ensures also a full memory barrier. */
int cmpxchg_atomicint(int* i, int oldval, int newval);


/* struct: atomicflag_t
 * Offers some atomic operations on byte flags.
 * If a processor does not support atomic ops then
 * this implementation relies on the compiler to emulate them. */
typedef uint8_t atomicflag_t;

/* function: set_atomicflag
 * Sets flag to value != 0 and returns previous value.
 * A return value of 0 means flag has not been set before therefore the lock is acquired.
 * A return value of != 0 means flag has been set before therefore the lock is not acquired.
 * This operation ensures also an acquire memory barrier. The thread will see all values written
 * by other threads before this operation if it reads them after this operation.
 * Other threads may not see the values written by this thread. */
int set_atomicflag(uint8_t* flag);

/* function: clear_atomicflag
 * Sets flag to value 0. Call this function only if <set_atomicflag> was called before and returned 0.
 * This operation ensures also a release memory barrier. Other threads will see all values written
 * by this thread before this operation (at least after they have executed an acquire barrier).
 * This thread may not see the values written by other threads. */
void clear_atomicflag(uint8_t* flag);



// section: inline implementation

// group: atomicint_t

/* define: add_atomicint
 * Implements <atomicint_t.add_atomicint>. */
#define add_atomicint(i, increment) \
         (__sync_fetch_and_add((i), (increment)))

/* define: clear_atomicint
 * Implements <atomicint_t.clear_atomicint>. */
#define clear_atomicint(i) \
         (__sync_fetch_and_and((i), 0))

/* define: read_atomicint
 * Implements <atomicint_t.read_atomicint>.
 * TODO: Replace atomicswap (full memory barrier) with load fence. */
#define read_atomicint(i) \
         (__sync_fetch_and_add((i), 0))

/* define: sub_atomicint
 * Implements <atomicint_t.sub_atomicint>. */
#define sub_atomicint(i, decrement) \
         (__sync_fetch_and_sub((i), (decrement)))

/* define: write_atomicint
 * Implements <atomicint_t.write_atomicint>.
 * TODO: Replace atomicswap (full memory barrier) with store fence. */
#define write_atomicint(i, newval) \
         ( __extension__ ({                    \
            typeof(i)     _i  = (i);           \
            typeof(*_i) _new  = (newval);      \
            typeof(*_i) _old  = *_i;           \
            typeof(*_i) _old2;                 \
            for (;;) {                         \
               _old2 =                         \
                  __sync_val_compare_and_swap( \
                              _i, _old, _new); \
               if (_old2 == _old) break;       \
               _old = _old2;                   \
            }                                  \
            _old;                              \
         }))

/* define: cmpxchg_atomicint
 * Implements <atomicint_t.cmpxchg_atomicint>. */
#define cmpxchg_atomicint(i, oldval, newval) \
         (__sync_val_compare_and_swap(i, oldval, newval))

// group: atomicflag_t

/* define: set_atomicflag
 * Implements <atomicflag_t.set_atomicflag>. */
#define set_atomicflag(flag) \
         (__sync_lock_test_and_set(flag, 1))

/* define: clear_atomicflag
 * Implements <atomicflag_t.clear_atomicflag>. */
#define clear_atomicflag(flag) \
         (__sync_lock_release(flag))

#endif
