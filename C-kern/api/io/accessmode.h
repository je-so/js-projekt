/* title: IO-Accessmode
   Defines how you can access a data block.
   This aspect is shared between all objects / modules
   which support accessing of persistent or transient data.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/io/accessmode.h
    Header file <IO-Accessmode>.
*/
#ifndef CKERN_IO_ACCESSMODE_HEADER
#define CKERN_IO_ACCESSMODE_HEADER

/* enums: accessmode_e
 * Determines how you can access a data block (on disk or in memory).
 * You can view it either as a right what you can do with it
 * or as some kind of protection what you can not do with it if
 * you have not set the corresponding mode bit.
 *
 * accessmode_NONE      - Neither read nor write nor exec is allowed.
 * accessmode_READ      - Allows for reading (only).
 * accessmode_WRITE     - Allows for writing (only).
 * accessmode_EXEC      - Allows for executing (only). This is normally supported only in combination with <accessmode_READ>.
 * accessmode_RDWR      - Combination of <accessmode_READ> and <accessmode_WRITE>.
 * accessmode_RDEX      - Combination of <accessmode_READ> and <accessmode_EXEC>.
 * accessmode_PRIVATE   - Write access to data or memory block is private to its process (COPY_ON_WRITE).
 *                        This must be ored together with another value. It is not useful of its own.
 *                        This value is default for allocated memory or mapped (virtual) memory even if you do not
 *                        request it explicitly.
 * accessmode_SHARED    - A write to a data or memory block is shared between all processes.
 *                        This mode needs some external synchronisation mechanism to prevent race conditions.
 *                        This must be ored together with another value. It is not useful of its own.
 *                        This value is default for accessing persistent data blocks on files or other devices
 *                        even if you do not request it explicitly.
 *                        Most devices do not support <accessmode_PRIVATE>.
 * accessmode_RDWR_SHARED     - Combination of <accessmode_RDWR> and <accessmode_SHARED>.
 * accessmode_NEXTFREE_BITPOS - Next free bit position useful in a subtype of <accessmode_e>.
 * */
typedef enum accessmode_e {
   accessmode_NONE        = 0,
   accessmode_READ        = 1,
   accessmode_WRITE       = 2,
   accessmode_EXEC        = 4,
   accessmode_PRIVATE     = 8,
   accessmode_SHARED      = 16
} accessmode_e;

#define accessmode_RDWR                   (accessmode_READ|accessmode_WRITE)

#define accessmode_RDEX                   (accessmode_READ|accessmode_EXEC)

#define accessmode_RDWR_SHARED            (accessmode_RDWR|accessmode_SHARED)

#define accessmode_NEXTFREE_BITPOS        (2*accessmode_SHARED)

#endif
