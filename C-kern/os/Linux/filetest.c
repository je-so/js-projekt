/* title: File-Test impl
   Implements <openfd_filetest>.

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

   file: C-kern/api/test/filetest.h
    Header file of <File-Test>.

   file: C-kern/os/Linux/filetest.c
    Implementation file of <File-Test impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/test/filetest.h"
#include "C-kern/api/errlog.h"
#include "C-kern/api/os/filesystem/directory.h"


/* function: openfd_filetest impl
 * Uses Linux specific "/proc/self/fd" interface. */
size_t openfd_filetest()
{
   int err ;
   directory_stream_t procself = directory_stream_INIT_FREEABLE ;

   err = init_directorystream(&procself, "/proc/self/fd", 0) ;
   if (err) goto ABBRUCH ;

   size_t    open_fds = (size_t)0 ;
   const char  * name ;
   do {
      ++ open_fds ;
      err = readnext_directorystream(&procself, &name, 0) ;
      if (err) goto ABBRUCH ;
   } while( name ) ;

   err = free_directorystream(&procself) ;
   if (err) goto ABBRUCH ;

   /* adapt open_fds for
      1. counts one too high
      2. counts "."
      3. counts ".."
      4. counts fd opened in init_directorystream
   */
   return open_fds >= 4 ? open_fds-4 : 0 ;
ABBRUCH:
   free_directorystream(&procself) ;
   LOG_ABORT(err) ;
   return 0 ;
}
