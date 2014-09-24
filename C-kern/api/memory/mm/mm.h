/* title: MemoryManager-Object

   Defines interfaceable object which offers memory manager functionality.
   Implemented by <mm_impl_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/memory/mm/mm.h
    Header file of <MemoryManager-Object>.

   file: C-kern/memory/mm/mm.c
    Implements unittest <MemoryManager-Object impl>.
*/
#ifndef CKERN_MEMORY_MM_MM_HEADER
#define CKERN_MEMORY_MM_MM_HEADER

// forward
struct memblock_t ;

/* typedef: struct mm_t
 * Export <mm_t>. Memory manager interfaceable object. */
typedef struct mm_t                       mm_t ;

/* typedef: struct mm_it
 * Export interface <mm_it>.
 * See <mm_it_DECLARE> for adaption to a specific implementation. */
typedef struct mm_it                      mm_it ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_mm_mm
 * Test <mm_it> functionality. */
int unittest_memory_mm_mm(void) ;
#endif

/* struct: mm_t
 * Uses <iobj_DECLARE> to declare interfaceable object.
 * See also <mm_impl_t> which is the default implementation. */
iobj_DECLARE(mm_t, mm) ;

// group: lifetime

/* define: mm_FREE
 * Static initializer. */
#define mm_FREE iobj_FREE

/* define: mm_INIT
 * Static initializer. */
#define mm_INIT(object, iimpl)            iobj_INIT(object, iimpl)

// group: call

/* function: malloc_mm
 * Calls mm->iimpl->malloc with mm->object as first parameter. See <mm_it.malloc>. */
int malloc_mm(mm_t mm, size_t size, /*out*/struct memblock_t * memblock) ;

/* function: mresize_mm
 * Calls mm->iimpl->mresize with mm->object as first parameter. See <mm_it.mresize>. */
int mresize_mm(mm_t mm, size_t newsize, struct memblock_t * memblock) ;

/* function: mfree_mm
 * Calls mm->iimpl->mfree with mm->object as first parameter. See <mm_it.mfree>. */
int mfree_mm(mm_t mm, struct memblock_t * memblock) ;

/* function: sizeallocated_mm
 * Calls mm->iimpl->sizeallocated with mm->object as first parameter. See <mm_it.sizeallocated>. */
size_t sizeallocated_mm(mm_t mm) ;


/* struct: mm_it
 * The function table describing the interface to a memory manager.
 * If you change the interface of <mm_it> to do not forget to adapt
 * <mm_it_DECLARE> to the same signature. */
struct mm_it {
   /* function: malloc
    * See <mm_impl_t.malloc_mmimpl> for an implementation. */
   int (* malloc)  (struct mm_t * mman, size_t size, /*out*/struct memblock_t * memblock) ;
   /* function: mresize
    * See <mm_impl_t.mresize_mmimpl> for an implementation. */
   int (* mresize) (struct mm_t * mman, size_t newsize, struct memblock_t * memblock) ;
   /* function: mfree
    * See <mm_impl_t.mfree_mmimpl> for an implementation. */
   int (* mfree)   (struct mm_t * mman, struct memblock_t * memblock) ;
   /* function: sizeallocated
    * See <mm_impl_t.sizeallocated_mmimpl> for an implementation. */
   size_t (* sizeallocated) (struct mm_t * mman) ;
} ;

// group: lifetime

/* define: mm_it_FREE
 * Static initializer. Set all fields to 0. */
#define mm_it_FREE { 0, 0, 0, 0 }

/* define: mm_it_INIT
 * Static initializer. Set all function pointers to the provided values.
 *
 * Parameters:
 * malloc_f        - Function pointer to allocate a new memory block. See <mm_it.malloc>.
 * mresize_f       - Function pointer to allocate and resize memory. See <mm_it.mresize>.
 * mfree_f         - Function pointer to free memory blocks. See <mm_it.mfree>.
 * sizeallocated_f - Function pointer to query the size of all allocated memory nlocks. See <mm_it.sizeallocated>. */
#define mm_it_INIT(malloc_f, mresize_f, mfree_f, sizeallocated_f) \
            { (malloc_f), (mresize_f), (mfree_f), (sizeallocated_f) }

// group: generic

/* function: cast_mmit
 * Casts parameter mminterface into pointer to interface <mm_it>.
 * The parameter *mminterface* has to be of type pointer to type declared with <mm_it_DECLARE>.
 * The other parameters must be the same as in <mm_it_DECLARE> without the first. */
mm_it * cast_mmit(void * mminterface, TYPENAME memorymanager_t) ;

/* function: mm_it_DECLARE
 * Declares an interface function table for accessing a memory manager service.
 * The declared interface is structural compatible with the generic interface <mm_it>.
 * The difference between the newly declared interface and the generic interface
 * is the type of the object. Every interface function takes a pointer to this object type
 * as its first parameter.
 *
 * Parameter:
 * declared_it       - The name of the structure which is declared as the interface.
 *                     The name should have the suffix "_it".
 * memorymanager_t   - The type of the memory manager object which is the first parameter of all interface functions.
 *
 * See <mm_it> for a list of declared functions.
 * */
void mm_it_DECLARE(TYPENAME declared_it, TYPENAME memorymanager_t) ;



// section: inline implementation

// group: mm_t

/* define: malloc_mm
 * Implements <mm_t.malloc_mm>. */
#define malloc_mm(mm, size, /*out*/memblock) \
         ((mm).iimpl->malloc((mm).object, (size), (memblock)))

/* define: mresize_mm
 * Implements <mm_t.mresize_mm>. */
#define mresize_mm(mm, newsize, memblock) \
         ((mm).iimpl->mresize((mm).object, (newsize), (memblock)))

/* define: mfree_mm
 * Implements <mm_t.mfree_mm>. */
#define mfree_mm(mm, memblock)   \
         ((mm).iimpl->mfree((mm).object, (memblock)))

/* define: sizeallocated_mm
 * Implements <mm_t.sizeallocated_mm>. */
#define sizeallocated_mm(mm)   \
         ((mm).iimpl->sizeallocated((mm).object))

// group: mm_it

/* define: cast_mmit
 * Implements <mm_it.cast_mmit>. */
#define cast_mmit(mminterface, memorymanager_t) \
         ( __extension__ ({                                 \
            static_assert(                                  \
               &((typeof(mminterface))0)->malloc            \
               == (int (**) (memorymanager_t*,              \
                             size_t, struct memblock_t*))   \
                  &((mm_it*)0)->malloc                      \
               && &((typeof(mminterface))0)->mresize        \
                  == (int (**) (memorymanager_t*,           \
                             size_t, struct memblock_t*))   \
                  &((mm_it*)0)->mresize                     \
               && &((typeof(mminterface))0)->mfree          \
                  == (int (**) (memorymanager_t*,           \
                                struct memblock_t*))        \
                  &((mm_it*)0)->mfree                       \
               && &((typeof(mminterface))0)->sizeallocated  \
                  == (size_t (**) (memorymanager_t*))       \
                  &((mm_it*)0)->sizeallocated,              \
               "ensure same structure") ;                   \
            (mm_it*) (mminterface) ;                        \
         }))


/* define: mm_it_DECLARE
 * Implements <mm_it.mm_it_DECLARE>. */
#define mm_it_DECLARE(declared_it, memorymanager_t)   \
         typedef struct declared_it       declared_it ;     \
         struct declared_it {                               \
            int    (* malloc)  (memorymanager_t * mman, size_t size, /*out*/struct memblock_t * memblock) ;  \
            int    (* mresize) (memorymanager_t * mman, size_t newsize, struct memblock_t * memblock) ;  \
            int    (* mfree)   (memorymanager_t * mman, struct memblock_t * memblock) ;                  \
            size_t (* sizeallocated) (memorymanager_t * mman) ;                                          \
         } ;


#endif
