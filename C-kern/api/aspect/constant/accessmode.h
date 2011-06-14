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

   file: C-kern/api/aspect/constant/accessmode.h
    Header file of <Accessmode-Aspect>.
*/
#ifndef CKERN_ASPECT_CONSTANT_ACCESSMODE_HEADER
#define CKERN_ASPECT_CONSTANT_ACCESSMODE_HEADER

/* enums: accessmode_aspect_e
 * Determines how you can access a data block (on disk or in memory).
 * You can view it either as a right what you can do with it
 * or as some kind of protection what you can not do with it if
 * you have not set the corresponding mode bit.
 *
 * accessmode_READ      - Allows for reading (only).
 * accessmode_WRITE     - Allows for writing (only).
 * accessmode_RDWR      - Allows for reading and writing.
 * accessmode_EXEC      - Allows for executing (only). This is normally supported only in combination with <accessmode_READ>.
 * accessmode_RDEXEC    - Allows for reading and executing.
 * accessmode_RDWREXEC  - Allows for reading, writing, and executing.
 * accessmode_PRIVATE   - Write access to data or memory block is private to its process (COPY_ON_WRITE).
 *                        This must be ored together with another value. It is not useful for its own.
 *                        This value is default for allocated memory or mapped (virtual) memory even if you do not
 *                        request it explicitly.
 * accessmode_SHARED    - A write to a data or memory block is shared between all processes.
 *                        This mode needs some external synchronisation mechanism to prevent race conditions.
 *                        This must be ored together with another value. It is not useful for its own.
 *                        This value is default for accessing persistent data blocks on files or other devices
 *                        even if you do not request it explicitly.
 *                        Most devices do not support <accessmode_PRIVATE>. */
enum accessmode_aspect_e {
    accessmode_NONE        = 0
   ,accessmode_READ        = 1
   ,accessmode_WRITE       = 2
   ,accessmode_EXEC        = 4
   ,accessmode_PRIVATE     = 8
   ,accessmode_SHARED      = 16
} ;

/* enums: accessmoderw_aspect_e
 * Subtype of <accessmode_aspect_e>.
 * It excludes <accessmode_PRIVATE> and <accessmode_SHARED>.
 *
 * accessmoderw_READ       - Allows for reading (only).
 * accessmoderw_WRITE      - Allows for writing (only).
 * accessmoderw_RDWR       - Allows for reading and writing.
 * accessmoderw_EXEC       - Allows for executing (only). This is normally supported only in combination with <accessmode_READ>.
 * accessmoderw_RDEXEC     - Allows for reading and executing.
 * accessmoderw_RDWREXEC   - Allows for reading, writing, and executing. */
enum accessmoderw_aspect_e {
    accessmoderw_NONE      = accessmode_NONE
   ,accessmoderw_READ      = accessmode_READ
   ,accessmoderw_WRITE     = accessmode_WRITE
   ,accessmoderw_RDWR      = (accessmode_READ|accessmode_WRITE) // 3
   ,accessmoderw_EXEC      = accessmode_EXEC
   ,accessmoderw_RDEXEC    = (accessmode_READ|accessmode_EXEC)  // 5
   ,accessmoderw_RDWREXEC  = (accessmode_READ|accessmode_WRITE|accessmode_EXEC)  // 7
} ;

/* typedef: accessmode_aspect_e
 * Shortcut for <accessmode_aspect_e>. */
typedef enum accessmode_aspect_e       accessmode_aspect_e ;

/* typedef: accessmoderw_aspect_e
 * Shortcut for <accessmoderw_aspect_e>. */
typedef enum accessmoderw_aspect_e     accessmoderw_aspect_e ;

/* define: accessmode_NEXTFREE_BITPOS
 * Next free bit position which can be used in a subtype. */
#define accessmode_NEXTFREE_BITPOS    (2*accessmode_SHARED)

#endif
