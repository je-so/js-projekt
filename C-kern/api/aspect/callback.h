/* title: Callback-Aspect
   Describes type of first callback parameter.

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
   (C) 2011 Jörg Seebohn

   file: C-kern/api/aspect/callback.h
    Header file of <Callback-Aspect>.
*/
#ifndef CKERN_ASPECT_CALLBACK_HEADER
#define CKERN_ASPECT_CALLBACK_HEADER

/* typedef: callback_aspect_t
 * Opaque first callback parameter type used as a place holder.
 *
 * Every callback takes a pointer to any additional parameter as its first parameter
 * > int callback( callback_aspect_t * cb, ...)
 * With this pointer the callback is supplied with additional implementation specific information.
 * This makes it possible to parameterize callbacks for different usage patterns and contexts.
 *
 * Callbacks are used mostly to allow for asynchronous notifications or as a way
 * to let modules adapt to different implementation strategies or to other external dependencies.
 *
 * Code example:
 * > struct callback_aspect_t ;
 * > typedef void * (*malloc_callback_f) (callback_aspect_t * cb, size_t memory_size) ;
 * > extern int module_init(callback_aspect_t * cb, malloc_callback_f malloc_callback) ;
 * >
 * > typedef struct static_malloc_t static_malloc_t ;
 * >
 * > struct static_malloc_t {
 * >    // inherits from callback_aspect_t which can not expressed in C
 * >    uint8_t * next_free ;
 * >    size_t    free_size ;
 * > } ;
 * >
 * > void * static_malloc ( static_malloc_t * context, size_t memsize)
 * > {
 * >    void * block = 0 ;
 * >    if (context->free_size >= memsize) {
 * >       block = context->next_free ;
 * >       context->next_free += memsize ;
 * >       context->free_size -= memsize ;
 * >    }
 * >    return block ;
 * > }
 * >
 * > int main()
 * > {
 * >    static uint8_t   s_module_memory[1000] ;
 * >    static_malloc_t  malloc_context = {
 * >       .next_free = s_module_memory, .free_size = sizeof(s_module_memory)
 * >    } ;
 * > ...
 * >    int err = module_init( (callback_aspect_t*) &malloc_context, (malloc_callback_f) &static_malloc ) ;
 * > ...
 * > }
 * >
 * > int module_init( callback_aspect_t * cb, void* (*malloc_callback)(callback_aspect_t * cb, size_t memory_size) )
 * > {
 * >    ...
 * >    // now somewhere in module
 * >    void * module_specific_memory = malloc_callback( cb, memsize ) ;
 * >    ...
 * > }
 * */
typedef struct callback_aspect_t    callback_aspect_t ;

#endif
