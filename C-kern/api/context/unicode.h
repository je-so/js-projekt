/* title: Unicode

   Global definitions for supporting unicode encoded wide characters.

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

   file: C-kern/api/context/unicode.h
    Header file <Unicode>.
*/
#ifndef CKERN_CONTEXT_UNICODE_HEADER
#define CKERN_CONTEXT_UNICODE_HEADER

/* typedef: char32_t
 * Defines the unicode *code point* as »32 bit unsigned integer«.
 * The unicode character set assigns to every contained abstract character
 * an integer number - also named its *code point*. The range for all
 * available code points ("code space") is [0 ... 10FFFF].
 *
 * UTF-32:
 * <char32_t> represents a single unicode character as a single integer value
 * - same as UTF-32.
 *
 * wchar_t:
 * The wide character type wchar_t is either of type signed 32 bit or unsigned 16 bit.
 * Depending on the chosen platform. So it is not portable. */
typedef uint32_t                       char32_t ;


#endif
