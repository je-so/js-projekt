/*
   C-System-Layer: C-kern/api/test.h
   Copyright (C) 2010 Jörg Seebohn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/
#ifndef CKERN_API_TEST_HEADER
#define CKERN_API_TEST_HEADER

/// If CONDITION fails an error is printed and further tests are skipped with help of goto ERROR_LABEL.
#define TEST_ONERROR_GOTO(CONDITION,TEST_FUNCTION_NAME,ERROR_LABEL) \
   if ( !(CONDITION) ) {\
      dprintf( STDERR_FILENO, "%s:%d: %s():\n FAILED TEST (%s)\n", __FILE__, __LINE__, #TEST_FUNCTION_NAME, #CONDITION) ;\
      goto ERROR_LABEL ;\
   }


#endif
