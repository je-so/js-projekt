/* title: GenerateErrorStringTable-Tool

   Generate a table which contains error strings
   of all system error codes.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/clocale.h"


typedef struct strtable_t {
   size_t   datasize;
   uint16_t offset[256];
   uint8_t  data[65536];
} strtable_t;


static unsigned errnomax_errtable(void)
{
   static unsigned errnomax = 0;

   if (!errnomax) {
      for (unsigned i = 1024; i >= 1; --i) {
         const char *   errstr = strerror((int)i);
         char           strnr[10];

         snprintf(strnr, sizeof(strnr), "%d", i);

         if (strstr(errstr, strnr)) {
            // encoded error number means unknown error
            errnomax = i;
         } else {
            break; // first defined error
         }
      }
   }

   return errnomax;
}

static const char * strerror2(unsigned errnr)
{
   static char s_unknownerr[100] = { 0 };

   if (errnr < errnomax_errtable()) return strerror((int)errnr);

   // rebuild in case of changing lang
   strncpy(s_unknownerr, strerror((int)errnomax_errtable()), sizeof(s_unknownerr)-1u);
   char * eos = strrchr(s_unknownerr, ' ');
   if (eos) *eos = 0;

   return s_unknownerr;
}

static int writestring_errtable(file_t file, const char * string)
{
   int err;
   size_t written = 0;
   size_t len = strlen(string);

   err = write_file(file, len, string, &written);
   if (err) goto ONERR;
   if (len != written) {
      err = EIO;
      goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

static void build_errtable(strtable_t * table)
{
   unsigned errnomax = errnomax_errtable();
   size_t   offset   = 0;

   assert(errnomax < (int)lengthof(table->offset));

   for (unsigned i = 0; i <= errnomax; ++i) {
      const char * errstr = strerror2(i);
      size_t       errlen = strlen(errstr);

      assert(errlen < sizeof(table->data) - offset);

      table->offset[i] = (uint16_t) offset;
      memcpy(table->data + offset, errstr, errlen);
      offset += errlen;
      table->data[offset ++] = 0;
   }

   for (unsigned i = errnomax; i < lengthof(table->offset) -1; ++i) {
      table->offset[i+1] = table->offset[errnomax];
   }

   table->datasize = offset;
}

static int write_table(file_t file, const char * langid, strtable_t table[2])
{
   int err;
   char buffer[256];

   err = writestring_errtable(file, "### GENERATED BY GENERRTAB ###\n");
   if (err) goto ONERR;

   buffer[sizeof(buffer)-1] = 0;
   for (unsigned i = 0; i <= errnomax_errtable(); ++i) {
      snprintf(buffer, sizeof(buffer)-1, "0x%02x\n", i);
      err = writestring_errtable(file, buffer);
      if (err) goto ONERR;
      snprintf(buffer, sizeof(buffer)-1, "%s: \"", langid);
      err = writestring_errtable(file, buffer);
      if (err) goto ONERR;
      err = writestring_errtable(file, (const char*)table[0].data + table[0].offset[i]);
      if (err) goto ONERR;
      err = writestring_errtable(file, "\"\nen: \"");
      if (err) goto ONERR;
      err = writestring_errtable(file, (const char*)table[1].data + table[1].offset[i]);
      if (err) goto ONERR;
      err = writestring_errtable(file, "\"\n");
      if (err) goto ONERR;
   }

   for (unsigned i = errnomax_errtable(); i < lengthof(table->offset) -1; ++i) {
      snprintf(buffer, sizeof(buffer)-1, "0x%02x -> 0x%02x\n", i+1, errnomax_errtable());
      err = writestring_errtable(file, buffer);
      if (err) goto ONERR;
   }

   err = writestring_errtable(file, "### END ###\n");
   if (err) goto ONERR;

   return 0;
ONERR:
   return err;
}

static int main_thread(maincontext_t * maincontext)
{
   int err = 1;
   static strtable_t errtable[2];
   const char *      filename;
   memblock_t        filedata    = memblock_FREE;
   wbuffer_t         filecontent = wbuffer_INIT_MEMBLOCK(&filedata);
   size_t            filesize;
   char              langid[10];
   file_t            file = file_FREE;

   if (maincontext->argc != 2) goto PRINT_USAGE;

   filename = maincontext->argv[1];

   // load old file content
   err = load_file(filename, &filecontent, 0);
   if (err) goto ONERR;
   filesize = size_wbuffer(&filecontent);
   const uint8_t endbyte = filedata.addr[filesize-1];
   filedata.addr[filesize-1] = 0;

   // get current language id
   strncpy(langid, current_clocale(), sizeof(langid));
   langid[sizeof(langid)-1] = 0;
   if (strstr(langid, "_"))  *strstr(langid, "_") = 0;

   // build user language and C table
   build_errtable(&errtable[0]);
   resetmsg_clocale();
   build_errtable(&errtable[1]);
   //

   err = init_file(&file, filename, accessmode_RDWR, 0);
   if (!err) err = truncate_file(file, 0);

   char * head = strstr((const char*)filedata.addr, "### GENERATED BY GENERRTAB ###\n");
   if (head) {
      head[0] = 0 ;
      if (!err) err = writestring_errtable(file, (const char*)filedata.addr);
   }

   if (!err) err = write_table(file, langid, errtable);

   char * tail = head ? strstr(head+1, "### END ###\n") : 0;
   if (tail) {
      if (!err) err = writestring_errtable(file, tail + 12);
      filedata.addr[0] = endbyte;
      filedata.addr[1] = 0;
      if (!err) err = writestring_errtable(file, (const char*) filedata.addr);
   }
   if (!err) err = free_file(&file);

   if (err) {
      dprintf(STDERR_FILENO, "%s: error writing '%s'\n", progname_maincontext(), filename);
      goto ONERR;
   }

   (void) FREE_MM(&filedata);

   return 0;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "Generrtab version 0.1 - Copyright (c) 2013 Joerg Seebohn\n" );
   dprintf(STDERR_FILENO, "\nDescription:\n Generates a table with all system error codes encoded as strings.\n" );
   dprintf(STDERR_FILENO, " The table is merged into the existing <file>\n" );
   dprintf(STDERR_FILENO, "\nUsage:\n %s <file>\n", progname_maincontext());
ONERR:
   free_file(&file);
   (void) FREE_MM(&filedata);
   return err;
}

int main(int argc, const char * argv[])
{
   int err;

   err = initrun_maincontext(maincontext_DEFAULT, &main_thread, argc, argv);

   return err;
}
