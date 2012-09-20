/* title: genfile

   Generates header and source file skeleton.
   This generator is onot configurable you must change
   the soruce coe if you want to adapt or customize it.

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

   file: C-kern/main/tools/genfile.c
    Main implementation of tool <genfile>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/filedescr.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/string/cstring.h"


static const char * s_programname ;
static const char * s_filetitle ;
static const char * s_typename ;
static const char * s_headerpath ;
static const char * s_sourcepath ;
static cstring_t    s_headertag    = cstring_INIT ;
static cstring_t    s_unittestname = cstring_INIT ;

enum variable_e {
      variable_TITLE,
      variable_HEADERPATH,
      variable_HEADERTAG,
      variable_SOURCEPATH,
      variable_TYPENAME,
      variable_UNITTESTNAME
} ;

typedef enum variable_e                variable_e ;

static const char * s_templateheader =
"/* title: @TITLE\n\n\
   TODO: describe module interface\n\n\
   about: Copyright\n\
   This program is free software.\n\
   You can redistribute it and/or modify\n\
   it under the terms of the GNU General Public License as published by\n\
   the Free Software Foundation; either version 2 of the License, or\n\
   (at your option) any later version.\n\n\
   This program is distributed in the hope that it will be useful,\n\
   but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
   GNU General Public License for more details.\n\n\
   Author:\n\
   (C) 2012 Jörg Seebohn\n\n\
   file: @HEADERPATH\n\
    Header file <@TITLE>.\n\n\
   file: @SOURCEPATH\n\
    Implementation file <@TITLE impl>.\n\
*/\n\
#ifndef CKERN_@HEADERTAG_HEADER\n\
#define CKERN_@HEADERTAG_HEADER\n\n\
/* typedef: struct @TYPENAME\n\
 * Export <@TYPENAME> into global namespace. */\n\
typedef struct @TYPENAME         @TYPENAME ;\n\n\n\
// section: Functions\n\n\
// group: test\n\n\
#ifdef KONFIG_UNITTEST\n\
/* function: @UNITTESTNAME\n\
 * Test <@TYPENAME> functionality. */\n\
int @UNITTESTNAME(void) ;\n\
#endif\n\n\n\
/* struct: @TYPENAME\n\
 * TODO: describe type */\n\
struct @TYPENAME {\n\n\
} ;\n\n\n\
#endif\n" ;

static const char * s_templatesource =
"/* title: @TITLE impl\n\n\
   Implements <@TITLE>.\n\n\
   about: Copyright\n\
   This program is free software.\n\
   You can redistribute it and/or modify\n\
   it under the terms of the GNU General Public License as published by\n\
   the Free Software Foundation; either version 2 of the License, or\n\
   (at your option) any later version.\n\n\
   This program is distributed in the hope that it will be useful,\n\
   but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
   GNU General Public License for more details.\n\n\
   Author:\n\
   (C) 2012 Jörg Seebohn\n\n\
   file: @HEADERPATH\n\
    Header file <@TITLE>.\n\n\
   file: @SOURCEPATH\n\
    Implementation file <@TITLE impl>.\n\
*/\n\n\
#include \"C-kern/konfig.h\"\n\
#include \"@HEADERPATH\"\n\
#include \"C-kern/api/err.h\"\n\
#ifdef KONFIG_UNITTEST\n\
#include \"C-kern/api/test.h\"\n\
#include \"C-kern/api/memory/mm/mmtest.h\"\n\
#endif\n\n\n\n\
// group: test\n\n\
#ifdef KONFIG_UNITTEST\n\n\
static int test_initfree(void)\n\
{\n\n\
   return 0 ;\n\
ONABORT:\n\
   return EINVAL ;\n\
}\n\n\
int @UNITTESTNAME()\n\
{\n\
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;\n\n\
   TEST(0 == switchon_mmtest()) ;\n\
   TEST(0 == init_resourceusage(&usage)) ;\n\n\
   if (test_initfree())       goto ONABORT ;\n\n\
   TEST(0 == same_resourceusage(&usage)) ;\n\
   TEST(0 == free_resourceusage(&usage)) ;\n\
   TEST(0 == switchoff_mmtest()) ;\n\n\
   return 0 ;\n\
ONABORT:\n\
   (void) free_resourceusage(&usage) ;\n\
   switchoff_mmtest() ;\n\
   return EINVAL ;\n\
}\n\n\
#endif\n" ;

static int construct_strings(const char * filepath, cstring_t * headertag, cstring_t * unittestname)
{
   int err ;

   // remove path prefix

   const char * removeprefix[] = { "C-kern", "api", "/", "home" } ;
   for (unsigned i = 0; i < nrelementsof(removeprefix); ) {
      size_t prefixlen = strlen(removeprefix[i]) ;
      if (0 == strncmp(filepath, removeprefix[i], prefixlen)) {
         filepath += prefixlen ;
         i = 0 ;
      } else {
         ++ i ;
      }
   }

   // remove path suffix
   size_t filepath_len = strlen(filepath) ;
   if (  strrchr(filepath, '.')
         && strlen(strrchr(filepath, '.')) < 5) {
      filepath_len -= strlen(strrchr(filepath, '.')) ;
   }

   clear_cstring(headertag) ;
   clear_cstring(unittestname) ;

   err = append_cstring(unittestname, 9, "unittest_") ;
   if (err) goto ONABORT ;

   for (size_t i = 0; i < filepath_len; ++i) {
      char c  = filepath[i] ;
      if ('a' <= c && c <= 'z') {
         c = (char) (c + ('A' - 'a')) ;
      } else if (! ( ('A' <= c && c <= 'Z')
                     || ('0' <= c && c <= '9'))) {
         c = '_' ;
      }
      err = append_cstring(headertag, 1, &c) ;
      if (err) goto ONABORT ;

      if ('A' <= c && c <= 'Z') {
         c = (char) (c + ('a' - 'A')) ;
      }
      err = append_cstring(unittestname, 1, &c) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int check_variable(const char * filetemplate, /*out*/variable_e * varindex, /*out*/unsigned * varlength)
{
   static const char * varnames[] = {
      [variable_TITLE] = "TITLE",
      [variable_HEADERPATH] = "HEADERPATH",
      [variable_HEADERTAG] = "HEADERTAG",
      [variable_SOURCEPATH] = "SOURCEPATH",
      [variable_TYPENAME] = "TYPENAME",
      [variable_UNITTESTNAME] = "UNITTESTNAME"
   } ;

   for (unsigned i = 0; i < nrelementsof(varnames); ++i) {
      if (  varnames[i]
            && 0 == strncmp(varnames[i], filetemplate, strlen(varnames[i]))) {
         *varlength = strlen(varnames[i]) ;
         *varindex  = i ;
         return 0 ;
      }
   }

   return EINVAL ;
}

static int substitute_variable(file_t * outfile, variable_e varindex)
{
   int err ;

   switch (varindex) {
   case variable_TITLE:
      err = write_file(outfile, strlen(s_filetitle), s_filetitle, 0) ;
      break ;
   case variable_HEADERPATH:
      err = write_file(outfile, strlen(s_headerpath), s_headerpath, 0) ;
      break ;
   case variable_HEADERTAG:
      err = write_file(outfile, length_cstring(&s_headertag), str_cstring(&s_headertag), 0) ;
      break ;
   case variable_SOURCEPATH:
      err = write_file(outfile, strlen(s_sourcepath), s_sourcepath, 0) ;
      break ;
   case variable_TYPENAME:
      err = write_file(outfile, strlen(s_typename), s_typename, 0) ;
      break ;
   case variable_UNITTESTNAME:
      err = write_file(outfile, length_cstring(&s_unittestname), str_cstring(&s_unittestname), 0) ;
      break ;
   }

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

static int generate_file(const char * filetemplate, const char * filepath)
{
   int err ;
   file_t outfile = file_INIT_FREEABLE ;

   err = initcreat_file(&outfile, filepath, 0) ;
   if (err) goto ONABORT ;

   for (size_t toff = 0; filetemplate[toff]; ++toff) {
      char c = filetemplate[toff] ;

      if (c == '@') {
         // variable substitution
         variable_e varindex ;
         unsigned varlength ;
         if (0 == check_variable(&filetemplate[toff+1], &varindex, &varlength)) {
            toff += varlength ;
            err = substitute_variable(&outfile, varindex) ;
            if (err) goto ONABORT ;
            continue ;
         }
      }

      err = write_file(&outfile, 1, &c, 0) ;
      if (err) goto ONABORT ;
   }

   err = free_file(&outfile) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   (void) free_file(&outfile) ;
   return err ;
}


static int process_arguments(int argc, const char * argv[])
{
   s_programname = strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0] ;

   if (argc != 5) goto ONABORT ;

   s_filetitle  = argv[1] ;
   s_typename   = argv[2] ;
   s_headerpath = argv[3] ;
   s_sourcepath = argv[4] ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int main(int argc, const char * argv[])
{
   int err ;

   err = init_maincontext(maincontext_DEFAULT, argc, argv) ;
   if (err) goto ONABORT ;

   err = process_arguments(argc, argv) ;
   if (err) goto PRINT_USAGE ;

   err = construct_strings(s_headerpath, &s_headertag, &s_unittestname) ;
   if (err) goto PRINT_USAGE ;

   // parse templates => expand variables => write header + source files
   err = generate_file(s_templateheader, s_headerpath) ;
   if (err) goto ONABORT ;
   err = generate_file(s_templatesource, s_sourcepath) ;
   if (err) goto ONABORT ;

   free_maincontext() ;
   return 0 ;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "Genfile version 0.1 - Copyright (c) 2012 Joerg Seebohn\n" ) ;
   dprintf(STDERR_FILENO, "\nDescription:\n Generates a simple header and source\n file skeleton for use in this project.\n" ) ;
   dprintf(STDERR_FILENO, "\nUsage:\n %s <filetitle> <typename> <headerfilename> <sourcefilename>\n", s_programname) ;
ONABORT:
   free_maincontext() ;
   return 1 ;
}
