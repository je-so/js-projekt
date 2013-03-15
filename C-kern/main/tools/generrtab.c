/* title: GenerateErrorStringTable-Tool

   Generate a table which contains error strings
   of all system error codes.

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
   (C) 2013 Jörg Seebohn

   file: C-kern/main/tools/generrtab.c
    Header file <GenerateErrorStringTable-Tool>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/locale.h"


static size_t errnomax_errtable(void)
{
   static size_t errnomax = 0 ;

   if (!errnomax) {
      for (int i = 1; i < 1024; ++i) {
         const char *   errstr  = strerror(i) ;
         char           strnr[10] ;

         snprintf(strnr, sizeof(strnr), "%d", i) ;

         if (strstr(errstr, strnr)) continue ;

         errnomax = (size_t) i ;
      }
   }

   return errnomax ;
}

static int writestring_errtable(const char * string)
{
   int err ;
   size_t len = strlen(string) ;

   if (len != (size_t)printf("%s", string)) {
      err = EIO ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return err ;
}

static int build_errtable(void)
{
   int err ;
   static uint16_t offtable[256] ;
   static char     strtable[65536] ;

   char     buffer[256] ;
   size_t   errnomax = errnomax_errtable() ;
   size_t   offset   = 0 ;

   assert(errnomax < lengthof(offtable)-1) ;

   for (size_t i = 0; i <= errnomax+1; ++i) {
      const char * errstr = (i <= errnomax ? strerror((int)i) : "Unknown error") ;
      size_t       errlen = strlen(errstr) ;

      assert(offset < 65536 - errlen) ;

      offtable[i] = (uint16_t) offset ;
      strcpy(strtable + offset, errstr) ;
      offset += errlen ;
      strtable[offset ++] = 0 ;
   }

   for (size_t i = errnomax+2; i < lengthof(offtable); ++i) {
      offtable[i] = offtable[errnomax+1] ;
   }

   const char * header = "// ================================================\n"
                         "// === Generated String Table for System Errors ===\n"
                         "// ================================================\n\n" ;
   err = writestring_errtable(header) ;
   if (err) goto ONABORT ;

   snprintf(buffer, sizeof(buffer), "static uint16_t s_errorcontext_stroffset[%d] = {\n", lengthof(offtable)) ;
   err = writestring_errtable(buffer) ;
   if (err) goto ONABORT ;

   for (size_t i = 0; i < lengthof(offtable); ++i) {
      if (i) {
         if (i % 8)
            err = writestring_errtable(",") ;
         else
            err = writestring_errtable(",\n") ;
         if (err) goto ONABORT ;
      }
      snprintf(buffer, sizeof(buffer), "   %4d", offtable[i]) ;
      err = writestring_errtable(buffer) ;
      if (err) goto ONABORT ;
   }

   err = writestring_errtable("\n} ;\n\n") ;
   if (err) goto ONABORT ;


   snprintf(buffer, sizeof(buffer), "static uint8_t s_errorcontext_strdata[%d] = {\n   \"", (int)offset) ;
   err = writestring_errtable(buffer) ;
   if (err) goto ONABORT ;

   for (size_t i = 0; i <= errnomax+1; ++i) {
      if (i) err = writestring_errtable("\\0\"\n   \"") ;
      if (err) goto ONABORT ;
      err = writestring_errtable(strtable + offtable[i]) ;
      if (err) goto ONABORT ;
   }

   err = writestring_errtable("\"\n} ;\n\n") ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

int main(int argc, const char * argv[])
{
   int err ;

   err = init_maincontext(maincontext_DEFAULT, argc, argv) ;
   if (err) goto ONABORT ;

   resetmsg_locale() ;

   if (argc != 1) goto PRINT_USAGE ;

   // build table
   err = build_errtable() ;
   if (err) {
      dprintf(STDERR_FILENO, "%s: writing error\n", progname_maincontext()) ;
      goto ONABORT ;
   }

   err = free_maincontext() ;
   if (err) goto ONABORT ;

   return 0 ;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "Generrtab version 0.1 - Copyright (c) 2013 Joerg Seebohn\n" ) ;
   dprintf(STDERR_FILENO, "\nDescription:\n Generates a table with all system error codes encoded as strings.\n" ) ;
   dprintf(STDERR_FILENO, " The table is written to stdout\n" ) ;
   dprintf(STDERR_FILENO, "\nUsage:\n %s\n", progname_maincontext()) ;
ONABORT:
   free_maincontext() ;
   return 1 ;
}
