/* title: Accessmode-Aspect
   Defines how you can access a data block.
   This aspect is shared between all objects / modules
   supporting accessing persistent data block or transient
   memory blocks.

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

   file: C-kern/api/aspect/constant/access_mode.h
    Header file of <Accessmode-Aspect>.
*/
#ifndef CKERN_ASPECT_CONSTANT_ACCESS_MODE_HEADER
#define CKERN_ASPECT_CONSTANT_ACCESS_MODE_HEADER

/* enums: access_mode_aspect_e
 * Determines how you can access a data block (on disk or in memory).
 * You can view it either as a right what you can do with it
 * or as some kind of protection what you can not do with it if
 * you have not set the corresponding mode bit.
 *
 * access_mode_READ       - Allows for reading (only).
 * access_mode_WRITE      - Allows for writing (only).
 * access_mode_RDWR       - Allows for reading and writing.
 * access_mode_EXEC       - Allows for executing (only). This is normally supported only in combination with <access_mode_READ>.
 * access_mode_RDEXEC     - Allows for reading and executing.
 * access_mode_RDWREXEC   - Allows for reading, writing, and executing.
 * access_mode_PRIVATE    - Write access to data or memory block is private to its process (COPY_ON_WRITE).
 *                          This must be ored together with another value. It is not useful for its own.
 *                          This value is default for allocated memory or mapped (virtual) memory even if you do not
 *                          request it explicitly.
 * access_mode_SHARED     - A write to a data or memory block is shared between all processes.
 *                          This mode needs some external synchronisation mechanism to prevent race conditions.
 *                          This must be ored together with another value. It is not useful for its own.
 *                          This value is default for accessing persistent data blocks on files or other devices
 *                          even if you do not request it explicitly.
 *                          Most devices do not support <access_mode_PRIVATE>. */
enum access_mode_aspect_e {
    access_mode_READ       = 1
   ,access_mode_WRITE      = 2
   ,access_mode_EXEC       = 4
   ,access_mode_PRIVATE    = 8
   ,access_mode_SHARED     = 16
} ;

/* enums: access_moderw_aspect_e
 * Subtype of <access_mode_aspect_e> except that it excludes
 * <access_mode_PRIVATE> and <access_mode_SHARED>.
 *
 *
 * access_moderw_READ       - Allows for reading (only).
 * access_moderw_WRITE      - Allows for writing (only).
 * access_moderw_RDWR       - Allows for reading and writing.
 * access_moderw_EXEC       - Allows for executing (only). This is normally supported only in combination with <access_mode_READ>.
 * access_moderw_RDEXEC     - Allows for reading and executing.
 * access_moderw_RDWREXEC   - Allows for reading, writing, and executing. */
enum access_moderw_aspect_e {
    access_moderw_READ     = access_mode_READ
   ,access_moderw_WRITE    = access_mode_WRITE
   ,access_moderw_RDWR     = (access_mode_READ|access_mode_WRITE) // 3
   ,access_moderw_EXEC     = access_mode_EXEC
   ,access_moderw_RDEXEC   = (access_mode_READ|access_mode_EXEC)  // 5
   ,access_moderw_RDWREXEC = (access_mode_READ|access_mode_WRITE|access_mode_EXEC)  // 7
} ;

/* typedef: access_mode_aspect_e
 * Shortcut for <access_mode_aspect_e>. */
typedef enum access_mode_aspect_e      access_mode_aspect_e ;

/* typedef: access_moderw_aspect_e
 * Shortcut for <access_moderw_aspect_e>. */
typedef enum access_moderw_aspect_e    access_moderw_aspect_e ;

/* define: access_mode_NEXTFREE_BITPOS
 * Next free bit position which can be used in a subtype. */
#define access_mode_NEXTFREE_BITPOS    (2*access_mode_SHARED)

#endif
