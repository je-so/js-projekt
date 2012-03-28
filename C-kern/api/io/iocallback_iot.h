/* title: IOCallback
   Defines the callback signature for function which handle I/O events.

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

   file: C-kern/api/io/iocallback_iot.h
    Header file <IOCallback>.
*/
#ifndef CKERN_IO_IOCALLBACK_IOT_HEADER
#define CKERN_IO_IOCALLBACK_IOT_HEADER

/* typedef: struct iocallback_iot
 * Export <iocallback_iot> - callback interface implementing object. */
typedef struct iocallback_iot          iocallback_iot ;

/* typedef: iocallback_f
 * Callback function for handling <ioevent_t>.
 *
 * (In) Parameter:
 * iohandler  - The object which implements the io callback.
 * ioevents   - Every set bit in this integer value signals a different io event.
 *              See <ioevent_e> for a list of all possible set bits and their meaning.
 * fd         - The file descriptior for which all ioevents have occurred.
 */
typedef void                        (* iocallback_f) (void * iohandler, sys_filedescr_t fd, uint8_t ioevents) ;


// section: Functions



/* struct: iocallback_iot
 * Declares a pointer to callback interface implementing object. */
__extension__ struct iocallback_iot {
   union { struct {  // only there to make it compatible with <iocallback_iot_DECLARE>.
   /* variable: object
    * The value of the first argument of <iocallback_f>. */
   void           * object ;
   /* variable: iimpl
    * The pointer to callback function <iocallback_f>. */
   iocallback_f   iimpl ;
   } ; } ;
} ;

// group: lifetime

/* define: iocallback_iot_INIT_FREEABLE
 * Static initializer. Sets pointer to object and pointer to callback function to 0. */
#define iocallback_iot_INIT_FREEABLE         { { { 0, 0 } } }

/* define: iocallback_iot_INIT
 * Static initializer.
 * Sets pointer to object and pointer to callback function to the supplied parameter values. */
#define iocallback_iot_INIT(object, iimpl)   { { { (object), (iimpl) } } }

// group: query

/* function: isinit_iocallback
 * Returns true if <iocallback_iot.iimpl> is not 0. */
bool isinit_iocallback(const iocallback_iot * iocb) ;

// group: execute

/* function: handleioevent_iocallback
 * Calls <iocallback_iot.iimpl> with <iocallback_iot.object> as its first parameter. */
void handleioevent_iocallback(const iocallback_iot * iocb, sys_filedescr_t fd, uint8_t ioevents) ;

// group: generic

/* define: iocallback_iot_DECLARE
 * Declares a subtype of <iocallback_iot>, i.e. a specific io handler implementation.
 * The declared callback type is structural compatible with the generic <iocallback_iot>.
 * See <iocallback_iot> for a list of declared fields.
 * The
 *
 * Parameter:
 * declared_iot   - The name of the structure which is declared as a subtype of <iocallback_iot>.
 *                  The name should have the suffix "_iot".
 * iohandler_t    - The object which implements the io callback handler.
 *                  The type of <iocallback_iot.object> is set to (iohandler_t*) instead of (void*).
 *                  Also the type of the first parameter of <iocallback_f> is set to (iohandler_t*).
 * */
#define iocallback_iot_DECLARE(declared_iot, iohandler_t)   \
      __extension__ struct declared_iot {                   \
         union {                                            \
         struct {                                           \
         iohandler_t    * object ;                          \
         void          (* iimpl) (iohandler_t * iohandler, sys_filedescr_t fd, uint8_t ioevents) ; \
         } ;                                                \
         iocallback_iot generic ;                           \
         } ;                                                \
      } ;


// section: inline implementation

/* define: handleioevent_iocallback
 * Implements <iocallback_iot.handleioevent_iocallback>. */
#define handleioevent_iocallback(iocb, fd, ioevents)     ((iocb)->iimpl((iocb)->object, (fd), (ioevents)))

/* define: isinit_iocallback
 * Implements <iocallback_iot.isinit_iocallback>. */
#define isinit_iocallback(iocb)                          (0 != (iocb)->iimpl)

#endif
