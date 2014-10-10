/* title: WriteBuffer

   Implements a simple write buffer.

   Used by every function which needs to return strings
   or other information of unknown size.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/memory/wbuffer.h
    Header file <WriteBuffer>.

   file: C-kern/memory/wbuffer.c
    Implementation file <WriteBuffer impl>.
*/
#ifndef CKERN_MEMORY_WBUFFER_HEADER
#define CKERN_MEMORY_WBUFFER_HEADER

// forward
struct memstream_t ;

/* typedef: struct wbuffer_t
 * Export <wbuffer_t>. */
typedef struct wbuffer_t wbuffer_t;

/* typedef: struct wbuffer_it
 * Export interface <wbuffer_it>.*/
typedef struct wbuffer_it wbuffer_it;

/* typedef: struct memstream_t
 * Export <memstream_t>. */
typedef struct memstream_t memstream_t;

/* variable: g_wbuffer_cstring
 * Adapt <wbuffer_t> to use <cstring_t> as buffer. */
extern wbuffer_it g_wbuffer_cstring;

/* variable: g_wbuffer_memblock
 * Adapt <wbuffer_t> to use <memblock_t> as buffer. */
extern wbuffer_it g_wbuffer_memblock;

/* variable: g_wbuffer_static
 * Adapt <wbuffer_t> to a static buffer. */
extern wbuffer_it g_wbuffer_static;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_memory_wbuffer
 * Test object <wbuffer_t>. */
int unittest_memory_wbuffer(void) ;
#endif


/* struct: wbuffer_it
 * Defines implementation interface for <wbuffer_t>.
 * This allows <wbuffer_t> to adapt to different memory buffer objects. */
struct wbuffer_it {
   /* variable: alloc
    * Reallocates memstr so that freesize bytes are not in use and returns it in memstr.
    * memstr contains the unused last part of the last allocated memory block:
    * (memstr->end - memstr->next) gives the number of unused bytes.
    * If a new block is allocated instead of the last one reallocated,
    * the functions should remember that all bytes from [memstr->next...memstr->end-1] are not used.
    * The size of the block returned in memstr must have at least freesize bytes. */
   int      (* alloc)   (void* impl, size_t freesize, /*inout*/struct memstream_t* memstr);
   /* variable: shrink
    * Free all memory beyond the first keepsize bytes. Return in memstr the start address of the free memory
    * after the first used keepsize bytes.
    * Unused memory blocks could be freed or kept in cache.
    * Parameter memstr is set to the unused part of the last allocated memory block before this function is called.
    * The function should return EINVAL in case keepsize is bigger than the return value of <size>. */
   int      (* shrink)  (void* impl, size_t keepsize, /*inout*/struct memstream_t* memstr);
   /* variable: size
    * Returns number of used bytes.
    * Parameter memstr contains the unused part of the last allocated memory block. */
   size_t   (* size)    (void* impl, const struct memstream_t* memstr);
} ;


/* struct: wbuffer_t
 * Supports construction of return values of unknown size.
 * The data is stored in an object of type <cstring_t>, <memblock_t>
 * or static allocated memory.
 *
 * Use <wbuffer_INIT_CSTRING>, <wbuffer_INIT_MEMBLOCK> or <wbuffer_INIT_STATIC>
 * to initialize a <wbuffer_t>. After initialization of wbuffer_t you are not
 * allowed to change the wrapped object until after the result is written into
 * the buffer. wbuffer_t caches some values so if you change <cstring_t> or a <memblock_t>
 * after having it wrapped into a wbuffer_t the behaviour is undefined.
 *
 * The size of the contained value is only stored in wbuffer_t. The wrapped object is not
 * truncated to fit the written data. You have to call <truncate_cstring>
 *
 * wbuffer_t does not allocate memory for itself so you do not need to free an initialized object.
 * But you have to free the wrapped object.
 *
 * */
struct wbuffer_t {
   /* variable: next
    * Pointer to next free memory location of the allocated memory. */
   uint8_t *            next ;
   /* variable: end
    * Points to memory address one after the end address of allocated memory. */
   uint8_t *            end ;
   /* variable: impl
    * Points to wrapped object or a static allocated buffer. */
   void *               impl ;
   /* variable: iimpl
    * Pointer to interface implementing memory allocation strategy. */
   const wbuffer_it *   iimpl ;
} ;

// group: lifetime

/* define: wbuffer_INIT_CSTRING
 * Static initializer which wraps a <cstring_t> object into a <wbuffer_t>.
 *
 * Parameter:
 * cstring - Pointer to initialized cstring. */
#define wbuffer_INIT_CSTRING(cstring) \
         wbuffer_INIT_OTHER(capacity_cstring(cstring), addr_cstring(cstring), cstring, &g_wbuffer_cstring)

/* define: wbuffer_INIT_MEMBLOCK
 * Static initializer which wraps a <memblock_t> object into a <wbuffer_t>.
 * If the memory is not big enough <RESIZE_MM> is called to resize it.
 *
 * Parameter:
 * memblock - Pointer to initialized <memblock_t>. It is allowed to set it to <memblock_FREE>. */
#define wbuffer_INIT_MEMBLOCK(memblock) \
         wbuffer_INIT_OTHER(size_memblock(memblock), addr_memblock(memblock), memblock, &g_wbuffer_memblock)

/* define: wbuffer_INIT_STATIC
 * Static initializer which wraps static memory into a <wbuffer_t>.
 * Reserving additional memory beyond buffer_size always results in ENOMEM.
 *
 * Parameter:
 * buffer_size - Size in bytes of static buffer.
 * buffer      - Memory address of static buffer. */
#define wbuffer_INIT_STATIC(buffer_size, buffer)   \
         wbuffer_INIT_OTHER(buffer_size, buffer, buffer, &g_wbuffer_static)

/* define: wbuffer_INIT_OTHER
 * Static initializer to adapt <wbuffer_t> to own memory implementation object.
 *
 * Parameter:
 * buffer_size  - Size in bytes of already reserved memory.
 * buffer       - Start address of already reserved memory.
 * impl         - Pointer to implementation specific data.
 * iimpl_it     - Pointer to interface <wbuffer_it> which implements memory allocation strategy. */
#define wbuffer_INIT_OTHER(buffer_size, buffer, impl, iimpl_it)  \
         { (buffer), (buffer) + (buffer_size), (impl), (iimpl_it) }

/* define: wbuffer_FREE
 * Static initializer for invalid <wbuffer_t>. */
#define wbuffer_FREE \
         { 0, 0, 0, 0 }

// group: query

/* function: sizefree_wbuffer
 * Returns the number of bytes which can be appended without reallocation. */
size_t sizefree_wbuffer(const wbuffer_t* wbuf);

/* function: size_wbuffer
 * Returns the number of appended bytes.
 * A returned value of 0 means no data is returned.
 * Always use this function to determine the number of
 * appended bytes. The size of the wrapped object is not correct
 * only the buffer of the wrapped object is used. */
size_t size_wbuffer(const wbuffer_t* wbuf);

// group: change

/* function: clear_wbuffer
 * Removes all appended content from wbuffer.
 * The memory is not necessarily freed but it is marked as free. */
void clear_wbuffer(wbuffer_t* wbuf);

/* function: shrink_wbuffer
 * Removes the last size_wbuffer(wbuf)-newsize bytes from wbuf.
 * The memory is not necessarily freed but it is marked as free.
 * Use this function if an error has occurred but the return parameter of type <wbuffer_t>
 * has been partially filled with content.
 * The error EINVAL is returned and nothing is done in case newsize is bigger than size_wbuffer(wbuf). */
int shrink_wbuffer(wbuffer_t* wbuf, size_t newsize);

/* function: appendbytes_wbuffer
 * Appends buffer_size bytes of unitilaized memory to wbuf.
 * A pointer to the uninitialized memory block is returned in buffer.
 * The returned pointer in buffer is valid as long as no other function is called on wbuf. */
int appendbytes_wbuffer(wbuffer_t* wbuf, size_t buffer_size, uint8_t** buffer);

/* function: appendbyte_wbuffer
 * Appends 1 byte to the buffer. */
int appendbyte_wbuffer(wbuffer_t* wbuf, const uint8_t c);

/* function: appendcopy_wbuffer
 * Appends the first buffer_size bytes from buffer to wbuf.
 * wbuf is grown if necessary. */
int appendcopy_wbuffer(wbuffer_t* wbuf, size_t buffer_size, const uint8_t* buffer);



// section: inline implementation

// group: wbuffer_t

/* define: appendbyte_wbuffer
 * Implements <wbuffer_t.appendbyte_wbuffer>. */
#define appendbyte_wbuffer(wbuf, c) \
         ( __extension__ ({                            \
            int         _err = 0;                      \
            wbuffer_t* _wb  = (wbuf);                  \
            if (  0 != sizefree_wbuffer(_wb)           \
                  || (0 == (_err = _wb->iimpl->        \
                        alloc(_wb->impl, 1,            \
                        (struct memstream_t*)_wb)))) { \
               *(_wb->next++) = c;                     \
            }                                          \
            _err;                                      \
         }))

/* define: appendbytes_wbuffer
 * Implements <wbuffer_t.appendbytes_wbuffer>. */
#define appendbytes_wbuffer(wbuf, buffer_size, buffer) \
         ( __extension__ ({                            \
            int         _err = 0;                      \
            wbuffer_t * _wb = (wbuf);                  \
            size_t      _bs = (buffer_size);           \
            if (  _bs <= sizefree_wbuffer(_wb)         \
                  || (0 == (_err = _wb->iimpl->        \
                        alloc(_wb->impl, _bs,          \
                        (struct memstream_t*)_wb)))) { \
               *(buffer)  = _wb->next;                 \
               _wb->next += _bs;                       \
            }                                          \
            _err;                                      \
         }))

/* define: clear_wbuffer
 * Implements <wbuffer_t.clear_wbuffer>. */
#define clear_wbuffer(wbuf) \
         do {                                      \
            wbuffer_t* _wb = (wbuf);               \
            _wb->iimpl->shrink(_wb->impl, 0,       \
                        (struct memstream_t*)_wb); \
         } while (0)

/* define: shrink_wbuffer
 * Implements <wbuffer_t.shrink_wbuffer>. */
#define shrink_wbuffer(wbuf, newsize) \
         ( __extension__ ({                          \
            wbuffer_t * _wb = (wbuf) ;               \
            _wb->iimpl->shrink(_wb->impl, (newsize), \
                        (struct memstream_t*)_wb);   \
         }))

/* define: sizefree_wbuffer
 * Implements <wbuffer_t.sizefree_wbuffer>. */
#define sizefree_wbuffer(wbuf) \
         ( __extension__ ({                    \
            const wbuffer_t* _wb2 = (wbuf);    \
            (size_t) (_wb2->end - _wb2->next); \
         }))

/* define: size_wbuffer
 * Implements <wbuffer_t.size_wbuffer>. */
#define size_wbuffer(wbuf) \
         ( __extension__ ({                              \
            const wbuffer_t * _wb = (wbuf) ;             \
            _wb->iimpl->size(_wb->impl,                  \
               (const struct memstream_t*)_wb) ;         \
         }))

#endif
