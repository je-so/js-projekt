/* title: MemoryStream

   Wraps a memory block which points to start and end address.
   Use this structure to stream memory, i.e. read bytes and increment
   the start address to point to the next unread byte.

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

   file: C-kern/api/memory/memstream.h
    Header file <MemoryStream>.

   file: C-kern/memory/memstream.c
    Implementation file <MemoryStream impl>.
*/
#ifndef CKERN_MEMORY_MEMSTREAM_HEADER
#define CKERN_MEMORY_MEMSTREAM_HEADER

/* typedef: struct memstream_t
 * Export <memstream_t> into global namespace. */
typedef struct memstream_t                memstream_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_memstream
 * Test <memstream_t> functionality. */
int unittest_memory_memstream(void) ;
#endif


/* struct: memstream_t
 * Wraps a memory block which points to start and end address.
 * The start address is the lowest address of an allocated memory block.
 * The end address points one after the highest address of the allocated memory block. */
struct memstream_t {
   /* variable: next
    * Points to next unread memory byte. next is always lower or equal to <end>. */
   uint8_t *   next ;
   /* variable: end
    * Points one after the last memory byte. The number of unread bytes can be determined
    * by "end - next". If next equals end all bytes are read. */
   uint8_t *   end ;
} ;

// group: lifetime

/* define: memstream_INIT_FREEABLE
 * Static initializer. */
#define memstream_INIT_FREEABLE           { 0, 0 }

/* define: memstream_INIT
 * Initializes memstream_t.
 *
 * Parameter:
 * start - Points to first unread byte.
 * end   - Points one after the last unread byte .
 *         The difference (end - start) is the positive number of the memory block size.
 * */
#define memstream_INIT(start, end)        { start, end }

// group: query

/* function: size_memstream
 * Returns the nuber of unread bytes. */
size_t size_memstream(const memstream_t * memstr) ;

// group: generic

/* function: genericcast_memstream
 * Casts pointer obj into pointer to <memstream_t>.
 * obj must point a generic object with two members nameprefix##next and nameprefix##end
 * of the same type as the members of <memstream_t> and in the same order. */
memstream_t * genericcast_memstream(void * obj, IDNAME nameprefix) ;



// section: inline implementation

/* define: genericcast_memstream
 * Implements <memstream_t.genericcast_memstream>. */
#define genericcast_memstream(obj, nameprefix)           \
         ( __extension__ ({                              \
            typeof(obj) _obj = (obj) ;                   \
            static_assert(                               \
               &((memstream_t*)((uintptr_t)              \
               &_obj->nameprefix##next))->next           \
               == &_obj->nameprefix##next                \
               && &((memstream_t*)((uintptr_t)           \
                  &_obj->nameprefix##next))->end         \
                  == &_obj->nameprefix##end,             \
               "obj is compatible") ;                    \
            (memstream_t *)(&_obj->nameprefix##next) ;   \
         }))

/* define: size_memstream
 * Implements <memstream_t.size_memstream>. */
#define size_memstream(memstr)         \
         ( __extension__ ({            \
            const memstream_t * _m ;   \
            _m = (memstr) ;            \
            (size_t) (_m->end          \
                      - _m->next) ;    \
         }))

#endif
