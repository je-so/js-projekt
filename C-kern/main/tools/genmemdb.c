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
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/mmfile.h"

#define PROGNAME "genmemdb"


static int main_thread(maincontext_t * maincontext)
{
   int err;
   mmfile_t file = mmfile_FREE;

   err = init_mmfile(&file, maincontext->argv[1], 0, 0, accessmode_READ, 0);
   if (err) {
      dprintf(STDERR_FILENO, "%s: error: can not open file '%s'\n", PROGNAME, maincontext->argv[1]);
      goto ONERR;
   }

   err = ENOSYS;
   dprintf(STDERR_FILENO, "%s: error: processing of file '%s' not implemented\n", PROGNAME, maincontext->argv[1]);

ONERR:
   (void) free_mmfile(&file);
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

   maincontext_startparam_t startparam = maincontext_startparam_INIT(maincontext_CONSOLE, argc, argv, &main_thread);

   err = initstart_maincontext(&startparam);

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
