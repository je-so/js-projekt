/* title: IOCallback
   Defines the callback signature for
   function which handle I/O events.

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

   file: C-kern/api/io/iocallback.h
    Header file <IOCallback>.
*/
#ifndef CKERN_IO_IOCALLBACK_HEADER
#define CKERN_IO_IOCALLBACK_HEADER

#include "C-kern/api/io/ioevent.h"

/* typedef: struct iocallback_t
 * Exports <iocallback_t>. */
typedef struct iocallback_t            iocallback_t ;

/* typedef: iocallback_f
 * Callback function for handling <ioevent_t>. */
typedef void                        (* iocallback_f) (iocallback_t * iohandler, sys_filedescr_t fd, ioevent_t ioevent) ;


/* struct: iocallback_t
 * Callback context for callback function <iocallback_f>. */
struct iocallback_t {
   /* variable: fct ;
    * Pointer to io event handler. */
   iocallback_f   fct ;
} ;

// group: lifetime

/* define: iocallback_INIT_FREEABLE
 * Static initializer. */
#define iocallback_INIT_FREEABLE       { 0 }

/* function: init_iocallback
 * Sets all fields in <iocallback_t> object.
 * If parameter *fct* is set to a value != 0 <isfct_iocallback> will return true. */
void init_iocallback(/*out*/iocallback_t * iohandler, iocallback_f fct) ;

/* function: init_iocallback
 * Clears all fields in <iocallback_t> object.
 * The field <iohandler_t.fct> is set to 0 and <isfct_iocallback> will return false. */
void free_iocallback(iocallback_t * iohandler) ;

// group: query

/* function: isinit_iocallback
 * Returns true if <iohandler_t.fct> is != 0. */
bool isinit_iocallback(const iocallback_t * iohandler) ;


// section: inline implementation

/* define: init_iohandler
 * Implements <iocallback_t.init_iohandler>. */
#define init_iocallback(iohandler, _fct)  ((void)((iohandler)->fct = (_fct)))

/* define: free_iocallback
 * Implements <iocallback_t.free_iocallback>. */
#define free_iocallback(iohandler)     ((void)((iohandler)->fct = 0))

/* define: isinit_iocallback
 * Implements <iocallback_t.isinit_iocallback>. */
#define isinit_iocallback(iohandler)   (0 != (iohandler)->fct)

#endif
