/* title: CompiletimeTest KONFIG_VALUE
   Test that definitions for KONFIG_... (e.g. KONFIG_MEMALIGN) are valid.

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

   file: C-kern/test/compiletime/konfig_value.h
    Header file of <CompiletimeTest KONFIG_VALUE>.

   file: C-kern/api/test/compiletime.h
    Included from <Compiletime-Tests>.
*/
#ifndef CKERN_TEST_COMPILETIME_KONFIG_VALUE_HEADER
#define CKERN_TEST_COMPILETIME_KONFIG_VALUE_HEADER

/* about: KONFIG_MEMALIGN Test
 * Test that <KONFIG_MEMALIGN> is set to a valid value out of [1,4,8,16,32]. */
#if !defined(KONFIG_MEMALIGN) || (KONFIG_MEMALIGN < 1) || (KONFIG_MEMALIGN > 32) || ((KONFIG_MEMALIGN-1)&KONFIG_MEMALIGN)
#define KONFIG_de 1
#if (KONFIG_LANG==KONFIG_de)
#   error Setze KONFIG_MEMALIGN auf einen Wert aus [2,4,8,16,32]
#else
#   error Choose KONFIG_MEMALIGN from the set of supported values [2,4,8,16,32]
#endif
#undef KONFIG_de
#endif

/* about: KONFIG_USERINTERFACE Test
 * Test that <KONFIG_USERINTERFACE> is set to a valid value. */
#define NONE 1
#define EGL  2
#define X11  4
#if (KONFIG_USERINTERFACE!=NONE) && \
    (KONFIG_USERINTERFACE!=X11)  && \
    (KONFIG_USERINTERFACE!=(EGL|X11))
#define KONFIG_de 1
#if (KONFIG_LANG==KONFIG_de)
#   error Setze KONFIG_USERINTERFACE auf einen Wert aus [NONE,X11,(EGL|X11)]
#else
#   error Choose KONFIG_USERINTERFACE from set of supported values [NONE,X11,(EGL|X11)]
#endif
#undef KONFIG_de
#endif
#undef NONE
#undef EGL
#undef X11

#endif
