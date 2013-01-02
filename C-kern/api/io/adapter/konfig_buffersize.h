/* title: Adapter-Konfig-Buffersize

   Defines static configuration value for buffersize
   used by implementations of <instream_t>.

   TODO: Replace static value with a dynamic configuration system.

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

   file: C-kern/api/io/adapter/konfig_buffersize.h
    Header file <Adapter-Konfig-Buffersize>.
*/
#ifndef CKERN_IO_ADAPTER_KONFIG_BUFFERSIZE_HEADER
#define CKERN_IO_ADAPTER_KONFIG_BUFFERSIZE_HEADER

/* define: KONFIG_BUFFERSIZE_INSTREAM_READNEXT
 * Static size in bytes of buffer returned from <instream_it.readnext>.
 * The read ahead buffer size is double the size of this value. */
#define KONFIG_BUFFERSIZE_INSTREAM_READNEXT        (8192)

#endif
