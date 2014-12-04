/* title: MemoryBlockVector

   Verwaltet eine Array (Vevtor) von <memblock_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/memory/memvec.h
    Header file <MemoryBlockVector>.

   file: C-kern/memory/memvec.c
    Implementation file <MemoryBlockVector impl>.
*/
#ifndef CKERN_MEMORY_MEMVEC_HEADER
#define CKERN_MEMORY_MEMVEC_HEADER

#include "C-kern/api/memory/memblock.h"

/* typedef: struct memvec_t
 * Export <memvec_t> into global namespace. */
typedef struct memvec_t memvec_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_memvec
 * Test <memvec_t> functionality. */
int unittest_memory_memvec(void);
#endif


/* struct: memvec_t
 * Beschreibt ein Array (Vektor) von <memblock_t>.
 * Das Array hat eine Länge von <size>.
 * Auf den i-ten Eintrag kann mittels memvec_t->vec[i] zugegriffen werden.
 *
 * Kapazität:
 * Die Kapazität ist auf einen <memblock_t> beschränkt.
 * Mit Hilfe von <memvec_DECLARE>(size) kann ein (anonymer aber)
 * zu memvec_t kompatibler Typ deklariert werden, der size <memblock_t>
 * aufnehmen kann.
 */
struct memvec_t {
   size_t     size;
   memblock_t vec[1/*size*/];
};

// group: lifetime

/* define: memvec_FREE
 * Static initializer. */
#define memvec_FREE \
         { 0, { memblock_FREE } }

/* define: init_memvec
 * Setzt memvec->size auf lengthof(memvec->vec). */
void init_memvec(/*out*/memvec_t* memvec);

// group: generic

/* function: memvec_DECLARE
 * Deklariert einen zu <memvec_t> kompatiblen Typ, der size <memblock_t> aufnehmen kann. */
void memvec_DECLARE(unsigned size);

/* function: cast_memvec
 * Caste memvec in Zeiger auf <memvec_t>. */
memvec_t* cast_memvec(void* memvec);


// section: inline implementation

/* define: cast_memvec
 * Implements <memvec_t.cast_memvec>. */
#define cast_memvec(memvec) \
         ( __extension__ ({                        \
            static_assert(                         \
               offsetof(typeof(*(memvec)), size)   \
               == offsetof(memvec_t, size)         \
               && (typeof(&(memvec)->size))0       \
                  == (size_t*)0                    \
               && offsetof(typeof(*(memvec)), vec) \
               == offsetof(memvec_t, vec)          \
               && (typeof(&(memvec)->vec[0]))0     \
                  == (memblock_t*)0,               \
            "ensure type is compatible");          \
            (memvec_t*) (memvec);                  \
         }))

/* define: memvec_DECLARE
 * Implements <memvec_t.memvec_DECLARE>. */
#define memvec_DECLARE(_size) \
         struct {                  \
            size_t     size;       \
            memblock_t vec[_size]; \
         }

/* define: init_memvec
 * Implements <memvec_t.init_memvec>. */
#define init_memvec(memvec) \
         ((void)((memvec)->size = lengthof((memvec)->vec)))

#endif
