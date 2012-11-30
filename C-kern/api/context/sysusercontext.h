/* title: SystemUserContext

   The context used in implementation of <sysuser_t>.

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

   file: C-kern/api/context/sysusercontext.h
    Header file <SystemUserContext>.

   file: C-kern/platform/Linux/sysuser.c
    Implementation file <SystemUser Linuximpl>.
*/
#ifndef CKERN_CONTEXT_SYSUSERCONTEXT_HEADER
#define CKERN_CONTEXT_SYSUSERCONTEXT_HEADER

/* typedef: struct sysusercontext_t
 * Export <sysusercontext_t> into global namespace. */
typedef struct sysusercontext_t         sysusercontext_t ;


/* struct: sysusercontext_t
 * Context for <sysuser_t> module. */
struct sysusercontext_t {
   /* variable: realuser
    * Contains user which started the process. */
   sys_userid_t   realuser ;
   /* variable: privilegeduser
    * Contains privileged user which is set at process creation from the system. */
   sys_userid_t   privilegeduser ;
} ;

// group: lifetime

/* define: sysusercontext_INIT_FREEABLE
 * Static initializer. */
#define sysusercontext_INIT_FREEABLE                     sysusercontext_INIT(sys_userid_INIT_FREEABLE, sys_userid_INIT_FREEABLE)

/* define: sysusercontext_INIT
 * Static initializer.
 *
 * Parameter:
 * realuser       - See <sysusercontext_t.realuser>
 * privilegeduser - See <sysusercontext_t.privilegeduser> */
#define sysusercontext_INIT(realuser, privilegeduser)    { realuser, privilegeduser }

// group: query

/* function: isequal_sysusercontext
 * Compares two <sysusercontext_t> objects for equality. */
bool isequal_sysusercontext(sysusercontext_t * left, sysusercontext_t * right) ;


#endif
