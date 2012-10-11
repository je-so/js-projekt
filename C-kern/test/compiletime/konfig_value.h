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
#define de 1
#if (KONFIG_LANG==de)
#   error Setze KONFIG_MEMALIGN auf einen Wert aus [1,4,8,16,32]
#else
#   error Choose KONFIG_MEMALIGN from the set of supported values [1,4,8,16,32]
#endif
#undef de
#endif

/* about: KONFIG_USERINTERFACE Test
 * Test that <KONFIG_USERINTERFACE> is set to a valid value out of [none,HTML5,X11]. */
#define none  1
#define HTML5 2
#define X11   4
#if ((KONFIG_USERINTERFACE)!=none) && ((KONFIG_USERINTERFACE)!=HTML5) && \
    ((KONFIG_USERINTERFACE)!=X11)  && ((KONFIG_USERINTERFACE)!=(HTML5|X11))
#define de 1
#if (KONFIG_LANG==de)
#   error Setze KONFIG_USERINTERFACE auf einen Wert aus [none,HTML5,X11]
#else
#   error Choose KONFIG_USERINTERFACE from set of supported values [none,HTML5,X11]
#endif
#undef de
#endif
#undef none
#undef HTML5
#undef X11

#endif
