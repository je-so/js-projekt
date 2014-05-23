/* title: IOCallback
   Defines the callback signature for a function which handles I/O events.

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
   (C) 2012 Jörg Seebohn

   file: C-kern/api/io/iocallback.h
    Header file <IOCallback>.

   file: C-kern/io/iocallback.c
    Implementation file <IOCallback impl>.
*/
#ifndef CKERN_IO_IOCALLBACK_HEADER
#define CKERN_IO_IOCALLBACK_HEADER

/* typedef: struct iocallback_t
 * Export <iocallback_t>. Callback interface implementing object. */
typedef struct iocallback_t            iocallback_t ;

/* typedef: iocallback_f
 * Callback function for handling <ioevent_t>.
 *
 * TODO: Remove it or change signature to handle groups of events.
 *
 * (In) Parameter:
 * iohandler  - The object which implements the io callback.
 * ioevents   - Every set bit in this integer value signals a different io event.
 *              See <ioevent_e> for a list of all possible set bits and their meaning.
 * fd         - The file descriptior for which all ioevents have occurred.
 */
typedef void                        (* iocallback_f) (void * iohandler, sys_iochannel_t fd, uint8_t ioevents) ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_iocallback
 * Tests implementation of <iocallback_t> . */
int unittest_io_iocallback(void) ;
#endif


/* struct: iocallback_t
 * Declares a pointer to callback interface implementing object. */
struct iocallback_t {
   /* variable: object
    * The value of the first argument of <iocallback_f>. */
   void           * object ;
   /* variable: iimpl
    * The pointer to callback function <iocallback_f>. */
   iocallback_f   iimpl ;
} ;

// group: lifetime

/* define: iocallback_FREE
 * Static initializer. Sets pointer to object and pointer to callback function to 0. */
#define iocallback_FREE iocallback_INIT(0, 0)

/* define: iocallback_INIT
 * Static initializer.
 * Sets pointer to object and pointer to callback function to the supplied parameter values. */
#define iocallback_INIT(object, iimpl)       { (object), (iimpl) }

// group: query

/* function: isinit_iocallback
 * Returns true if <iocallback_t.iimpl> is not 0. */
bool isinit_iocallback(const iocallback_t * iocb) ;

// group: execute

/* function: call_iocallback
 * Calls <iocallback_t.iimpl> with <iocallback_t.object> as its first parameter. */
void call_iocallback(const iocallback_t * iocb, sys_iochannel_t fd, uint8_t ioevents) ;

// group: generic

/* define: genericcast_iocallback
 * Casts parameter iocb into pointer to <iocallback_t>.
 * The parameter *iocb* has to be of type "pointer to declared_t" where declared_t
 * is the name used as first parameter in <iocallback_DECLARE>.
 * The second parameter must be the same as in <iocallback_DECLARE>. */
iocallback_t * genericcast_iocallback(void * iocb, TYPENAME iohandler_t) ;

/* define: iocallback_DECLARE
 * Declares a subtype of <iocallback_t>, i.e. a specific io handler implementation.
 * The declared callback type is structural compatible with the generic <iocallback_t>.
 * See <iocallback_t> for a list of declared fields.
 *
 * Parameter:
 * declared_t     - The name of the structure which is declared as a subtype of <iocallback_t>.
 *                  The name should have the suffix "_t".
 * iohandler_t    - The object which implements the io callback handler.
 *                  The type of <iocallback_t.object> is set to (iohandler_t*) instead of (void*).
 *                  Also the type of the first parameter of <iocallback_f> is set to (iohandler_t*).
 * */
#define iocallback_DECLARE(declared_t, iohandler_t)         \
         typedef struct declared_t           declared_t ;   \
         struct declared_t {                                \
            iohandler_t    * object ;                       \
            void          (* iimpl) (                       \
                              iohandler_t *     iohandler,  \
                              sys_iochannel_t   fd,         \
                              uint8_t           ioevents) ; \
         }


// section: inline implementation

/* define: genericcast_iocallback
 * Implements <iocallback_t.genericcast_iocallback>. */
#define genericcast_iocallback(iocb, iohandler_t)           \
         ( __extension__ ({                                 \
            static_assert(                                  \
               offsetof(iocallback_t, object)               \
               == offsetof(typeof(*(iocb)), object)         \
               && (typeof((iocb)->object)*)0                \
                  == (iohandler_t**)0                       \
               && offsetof(iocallback_t, iimpl)             \
                  == offsetof(typeof(*(iocb)), iimpl)       \
               && (typeof((iocb)->iimpl)*)0                 \
                  == (void(**)(iohandler_t*,                \
                        sys_iochannel_t, uint8_t))0,        \
               "ensure same structure") ;                   \
            (iocallback_t*) (iocb) ;                        \
         }))

/* define: call_iocallback
 * Implements <iocallback_t.call_iocallback>. */
#define call_iocallback(iocb, fd, ioevents)  \
         ((iocb)->iimpl((iocb)->object, (fd), (ioevents)))

/* define: isinit_iocallback
 * Implements <iocallback_t.isinit_iocallback>. */
#define isinit_iocallback(iocb)  \
         (0 != (iocb)->iimpl)

#endif
