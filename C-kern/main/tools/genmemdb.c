/* title: genmemdb

   In memory database Generator <genmemdb>.

   TODO: describe

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/main/tools/genmemdb.c
    Main implementation of tool <genmemdb>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/memory/mm/mm_macros.h"

#define PROGNAME "genmemdb"


static int main_thread(maincontext_t * maincontext)
{
   int err;
   memblock_t file_data = memblock_FREE;
   size_t     file_size = 0;

   {
      wbuffer_t wbuf = wbuffer_INIT_MEMBLOCK(&file_data);
      err = load_file(maincontext->argv[1], &wbuf, 0);
      if (err) {
         dprintf(STDERR_FILENO, "%s: error: can not open file '%s'\n", PROGNAME, maincontext->argv[1]);
         goto ONERR;
      }
      file_size = size_wbuffer(&wbuf);
   }

   printf("%s: size = %zu\n", maincontext->argv[1], file_size);

   err = ENOSYS;
   dprintf(STDERR_FILENO, "%s: error: processing of file '%s' not implemented\n", PROGNAME, maincontext->argv[1]);

ONERR:
   (void) FREE_MM(&file_data);
   return err;
}

int main(int argc, const char * argv[])
{
   int err = 0;

   if (argc != 2) {
      goto ERROR_USAGE;
   }

   if (0 == strcmp(argv[1], "-v")) {
      goto PRINT_VERSION;
   }

   if (0 == strcmp(argv[1], "-h")) {
      goto PRINT_USAGE;
   }

   err = initrun_maincontext( maincontext_CONSOLE, &main_thread, 0, argc, argv);

   return err;

ERROR_USAGE:
   err = EINVAL;
   dprintf(STDERR_FILENO, "%s: error: wrong arguments\n", PROGNAME);

PRINT_USAGE:
   printf("\nUsage: %s [options] file\n", PROGNAME);
   printf("\tfile:\tFile is read and transformed into C code.\n");
   printf("\t     \tThe content describes one or more data structures.\n");
   printf("Options:\n");
   printf("\t-h:\tDisplay command line options\n");
   printf("\t-v:\tDisplay version information\n");
   return err;

PRINT_VERSION:
   printf("%s 0.1 - Generate In-Memory Database in C\n", PROGNAME);
   printf("Copyright (C) 2015 Joerg Seebohn\n");
   printf("This is free software; see the source for copying conditions.\n");
   printf("This software is provided WITHOUT ANY WARRANTY!\n");
   return err;
}
