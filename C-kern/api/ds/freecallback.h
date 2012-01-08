/* title: FreeCallback
   Callback to free memory and other resources
   associated with an object stored in a data structure.

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

   file: C-kern/api/ds/freecallback.h
    Header file of <FreeCallback>.
*/
#ifndef CKERN_DS_FREECALLBACK_HEADER
#define CKERN_DS_FREECALLBACK_HEADER

/* typedef: struct freecallback_t
 * Exports <freecallback_t>. */
typedef struct freecallback_t          freecallback_t ;

/* typedef: freecallback_f
 * Callback function for handling freeing memory and other resources.
 * The called function should return 0 in case of success.
 * After successfull return the object has freed all its internal resources.
 * An error indicates that this callback could not free all resources.
 * Functions calling this kind of callback should return the error but not
 * aborting the current operation of freeing as many resources as possible. */
typedef int                         (* freecallback_f) (freecallback_t * freehandler, void * object) ;

/* define: freecallback_ADAPT
 * Adapts signature of callback function type.
 *
 * Parameter:
 * declaredcallback_f - Type name of function pointer whose signature is declared (see <freecallback_f>).
 * callback_t         - Type name of the already declared callback structure (see <freecallback_t>).
 *                      It should be defined (after this macro) as "struct callback_t { declaredcallback_f fct ; ... }".
 * object_t           - Pointer to memory or object whose resources must be freed. */
#define freecallback_ADAPT(declaredcallback_f, callback_t, object_t) \
                                                                     \
typedef int                         (* declaredcallback_f) (callback_t * freehandler, object_t * object) ;


/* struct: freecallback_t
 * Callback context for callback function <freecallback_f>. */
struct freecallback_t {
   /* variable: fct
    * Pointer to free resource handler. */
   freecallback_f    fct ;
} ;

#endif
