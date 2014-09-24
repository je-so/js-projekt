/* title: MemoryPointer

   Supports extracting and adding bits to pointers.
   Aligned pointers do not need all bits in their
   integer representation. This module allows to
   modify unused bits to encode additional information.
   For example the type of memory the pointer points to
   could be encoded.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/memory/ptr.h
    Header file <MemoryPointer>.

   file: C-kern/memory/ptr.c
    Implementation file <MemoryPointer impl>.
*/
#ifndef CKERN_MEMORY_PTR_HEADER
#define CKERN_MEMORY_PTR_HEADER

/* typedef: struct ptr_t
 * Export <ptr_t> into global namespace. */
typedef void * ptr_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_ptr
 * Test <ptr_t> and <ptrf_t> functionality. */
int unittest_memory_ptr(void);
#endif


/* struct: ptr_t
 * Defines generic pointer type which points to data in memory. */
typedef void * ptr_t;

// group: lifetime

/* define: ptr_FREE
 * Static initializer. */
#define ptr_FREE \
         { 0 }

// group: query

/* function: isaligned_ptr
 * Returns true if all nrbits least significant bits of ptr are zero.
 *
 * Unchecked Precondition:
 * nrbits < bitsof(void*) */
int isaligned_ptr(const ptr_t ptr, unsigned nrbits);

/* function: lsbits_ptr
 * Returns the value of all nrbits least significant bits of ptr.
 *
 * Unchecked Precondition:
 * nrbits < bitsof(void*) */
int lsbits_ptr(const ptr_t ptr, unsigned nrbits);

// group: update

/* function: clearlsbits_ptr
 * Sets all nrbits least significant bits of ptr to zero.
 *
 * Unchecked Precondition:
 * nrbits < bitsof(void*) */
ptr_t clearlsbits_ptr(const ptr_t ptr, unsigned nrbits);

/* function: orlsbits_ptr
 * The value of all nrbits least significant bits of value are ored into ptr.
 * This function assumes that <isaligned_ptr>(ptr, nrbits) returns true.
 *
 * Unchecked Precondition:
 * nrbits < bitsof(void*) */
ptr_t orlsbits_ptr(const ptr_t ptr, unsigned nrbits, uintptr_t value);



// section: inline implementation

/* define: clearlsbits_ptr
 * Implements <ptr_t.clearlsbits_ptr>. */
#define clearlsbits_ptr(ptr, nrbits) \
         ((typeof(ptr))((uintptr_t)(ptr) & ((uintptr_t)-1 << (nrbits))))

/* define: isaligned_ptr
 * Implements <ptr_t.isaligned_ptr>. */
#define isaligned_ptr(ptr, nrbits) \
         (0 == lsbits_ptr(ptr, nrbits))

/* define: lsbits_ptr
 * Implements <ptr_t.lsbits_ptr>. */
#define lsbits_ptr(ptr, nrbits) \
         ((uintptr_t)(ptr) & (((uintptr_t)1 << (nrbits))-1))

/* define: orlsbits_ptr
 * Implements <ptr_t.orlsbits_ptr>. */
#define orlsbits_ptr(ptr, nrbits, value) \
         ((typeof(ptr))((uintptr_t)(ptr) | ((uintptr_t)(value) & (((uintptr_t)1 << (nrbits))-1))))


#endif
