/* title: X11-Subsystem
   Offers window management for X11 graphics environment.

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

   file: C-kern/api/os/X11/x11.h
    Header file of <X11-Subsystem>.

   file: C-kern/os/X11/x11.c
    Implementation file of <X11-Subsystem>.
*/
#ifndef CKERN_OS_X11_HEADER
#define CKERN_OS_X11_HEADER

// section: inline implementations

// group: KONFIG_GRAPHIK

#define X11 1
#if (KONFIG_GRAPHIK!=X11)
/* define: initprocess_X11
 * Implement init as a no op if (KONFIG_GRAPHIK!=X11).
 * > #define initprocess_X11()  (0) */
#define initprocess_X11()  (0)
/* define: freeprocess_X11
 * Implement free as a no op if (KONFIG_GRAPHIK!=X11).
 * > #define freeprocess_X11()  (0) */
#define freeprocess_X11()  (0)
#endif
#undef X11

#endif