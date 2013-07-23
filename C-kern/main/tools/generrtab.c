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
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/platform/locale.h"
#include "C-kern/api/platform/startup.h"


typedef struct strtable_t     strtable_t ;

struct strtable_t {
   size_t   datasize ;
   uint16_t offset[256] ;
   uint8_t  data[65536] ;
} ;


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

static const char * strerror2(size_t errnr)
{
   static char s_unknownerr[100] = { 0 } ;
   if (errnr <= errnomax_errtable()) return strerror((int)errnr) ;
   strncpy(s_unknownerr, strerror((int)errnomax_errtable()+1), sizeof(s_unknownerr)-1u) ;
   char * eos = strrchr(s_unknownerr, ' ') ;
   if (eos) *eos = 0 ;
   return s_unknownerr ;
}

static int writestring_errtable(file_t file, const char * string)
{
   int err ;
   size_t written ;
   size_t len = strlen(string) ;

   err = write_file(file, len, string, &written) ;
   if (len != written) {
      err = EIO ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return err ;
}

static void build_errtable(strtable_t * table)
{
   size_t   errnomax = errnomax_errtable() ;
   size_t   offset   = 0 ;

   assert(errnomax < lengthof(table->offset)-1) ;

   for (size_t i = 0; i <= errnomax+1; ++i) {
      const char * errstr = strerror2(i) ;
      size_t       errlen = strlen(errstr) ;

      assert(offset < sizeof(table->data) - errlen) ;

      table->offset[i] = (uint16_t) offset ;
      memcpy(table->data + offset, errstr, errlen) ;
      offset += errlen ;
      table->data[offset ++] = 0 ;
   }

   for (size_t i = errnomax+2; i < lengthof(table->offset); ++i) {
      table->offset[i] = table->offset[errnomax+1] ;
   }

   table->datasize = offset ;
}

static int write_table(file_t file, const char * langid, strtable_t table[2])
{
   int err ;
   char buffer[256] ;

   err = writestring_errtable(file, "### GENERATED BY GENERRTAB ###\n") ;
   if (err) goto ONABORT ;

   for (size_t i = 0; i <= errnomax_errtable()+1; ++i) {
      sprintf(buffer, "0x%02zx\n", i) ;
      err = writestring_errtable(file, buffer) ;
      if (err) goto ONABORT ;
      sprintf(buffer, "%s: \"", langid) ;
      err = writestring_errtable(file, buffer) ;
      if (err) goto ONABORT ;
      err = writestring_errtable(file, (const char*)table[0].data + table[0].offset[i]) ;
      if (err) goto ONABORT ;
      err = writestring_errtable(file, "\"\nen: \"") ;
      if (err) goto ONABORT ;
      err = writestring_errtable(file, (const char*)table[1].data + table[1].offset[i]) ;
      if (err) goto ONABORT ;
      err = writestring_errtable(file, "\"\n") ;
      if (err) goto ONABORT ;
   }

   for (size_t i = errnomax_errtable()+2; i <= 255; ++i) {
      sprintf(buffer, "0x%02zx -> 0x%02zx\n", i, errnomax_errtable()+1) ;
      err = writestring_errtable(file, buffer) ;
      if (err) goto ONABORT ;
   }

   err = writestring_errtable(file, "### END ###\n") ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

static int main_thread(int argc, const char * argv[])
{
   int err ;
   static strtable_t errtable[2] ;
   const char *      filename ;
   wbuffer_t         filecontent = wbuffer_INIT_DYNAMIC ;
   char              langid[10] ;
   memblock_t        filedata ;
   file_t            file = file_INIT_FREEABLE ;

   err = init_maincontext(maincontext_DEFAULT, argc, argv) ;
   if (err) goto ONABORT ;

   if (argc != 2) goto PRINT_USAGE ;

   filename = argv[1] ;

   // load old file content
   err = load_file(filename, &filecontent, 0) ;
   if (err) goto ONABORT ;
   err = firstmemblock_wbuffer(&filecontent, &filedata) ;
   if (err) goto ONABORT ;
   uint8_t endbyte = filedata.addr[filedata.size-1] ;
   filedata.addr[filedata.size-1] = 0 ;

   // get current language id
   strncpy(langid, current_locale(), sizeof(langid)) ;
   langid[sizeof(langid)-1] = 0 ;
   if (strstr(langid, "_"))  *strstr(langid, "_") = 0 ;

   // build user language and C table
   build_errtable(&errtable[0]) ;
   resetmsg_locale() ;
   build_errtable(&errtable[1]) ;
   //

   err = init_file(&file, filename, accessmode_RDWR, 0) ;

   char * head = strstr((const char*)filedata.addr, "### GENERATED BY GENERRTAB ###\n") ;
   if (head) {
      head[0] = 0 ;
      if (!err) err = writestring_errtable(file, (const char*)filedata.addr) ;
   }

   if (!err) err = write_table(file, langid, errtable) ;

   char * tail = head ? strstr(head+1, "### END ###\n") : 0 ;
   filedata.addr[filedata.size-1] = endbyte ;
   if (tail) {
      size_t tailsize = filedata.size - (size_t)((uint8_t*)tail + 12 - filedata.addr) ;
      memcpy(tail, tail + 12, tailsize) ;
      tail[tailsize] = 0 ;
      if (!err) err = writestring_errtable(file, tail) ;
   }
   if (!err) err = free_file(&file) ;

   if (err) {
      dprintf(STDERR_FILENO, "%s: error writing '%s'\n", progname_maincontext(), filename) ;
      goto ONABORT ;
   }

   (void) free_wbuffer(&filecontent) ;

   err = free_maincontext() ;
   if (err) goto ONABORT ;

   return 0 ;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "Generrtab version 0.1 - Copyright (c) 2013 Joerg Seebohn\n" ) ;
   dprintf(STDERR_FILENO, "\nDescription:\n Generates a table with all system error codes encoded as strings.\n" ) ;
   dprintf(STDERR_FILENO, " The table is merged into the existing <file>\n" ) ;
   dprintf(STDERR_FILENO, "\nUsage:\n %s <file>\n", progname_maincontext()) ;
ONABORT:
   free_file(&file) ;
   free_wbuffer(&filecontent) ;
   free_maincontext() ;
   return 1 ;
}

int main(int argc, const char * argv[])
{
   int err ;

   err = startup_platform(argc, argv, &main_thread) ;

   return err ;
}
