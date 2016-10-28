/* title: genfile

   Generates header and source file skeleton.
   This generator is not configurable you must change
   the source code if you want to adapt or customize it.

   Call Signature:
   genfile <filetitle> <typename> <headerfilename> <sourcefilename>

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/main/tools/genfile.c
    Main implementation of tool <genfile>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/string/cstring.h"

static const char* s_programname;
static const char* s_filetitle;
static const char* s_typename;
static const char* s_headerpath;
static const char* s_sourcepath;
static cstring_t   s_fctsuffix    = cstring_INIT;
static cstring_t   s_headertag    = cstring_INIT;
static cstring_t   s_typename2    = cstring_INIT;
static cstring_t   s_unittestname = cstring_INIT;


typedef enum variable_e {
      variable_TITLE,
      variable_FCTSUFFIX,
      variable_HEADERPATH,
      variable_HEADERTAG,
      variable_SOURCEPATH,
      variable_TYPENAME2,
      variable_TYPENAME,
      variable_UNITTESTNAME
} variable_e;


static const char* s_templateheader =
"/* title: @TITLE\n\n\
   TO""DO: describe module interface\n\n\
   Copyright:\n\
   This program is free software. See accompanying LICENSE file.\n\n\
   Author:\n\
   (C) 2016 Jörg Seebohn\n\n\
   file: @HEADERPATH\n\
    Header file <@TITLE>.\n\n\
   file: @SOURCEPATH\n\
    Implementation file <@TITLE impl>.\n\
*/\n\
#ifndef CKERN_@HEADERTAG_HEADER\n\
#define CKERN_@HEADERTAG_HEADER\n\n\
// === exported types\n\
struct @TYPENAME;\n\n\n\
// section: Functions\n\n\
// group: test\n\n\
#ifdef KONFIG_UNITTEST\n\
/* function: @UNITTESTNAME\n\
 * Test <@TYPENAME> functionality. */\n\
int @UNITTESTNAME(void);\n\
#endif\n\n\n\
/* struct: @TYPENAME\n\
 * TO""DO: describe type */\n\
typedef struct @TYPENAME {\n\
   int dummy; // TO""DO: remove line\n\
} @TYPENAME;\n\n\
// group: lifetime\n\
\n\
/* define: @TYPENAME2_FREE\n\
 * Static initializer. */\n\
#define @TYPENAME2_FREE \\\n\
         { 0 }\n\
\n\
/* function: init_@FCTSUFFIX\n\
 * TO""DO: Describe Initializes object. */\n\
int init_@FCTSUFFIX(/*out*/@TYPENAME *obj);\n\
\n\
/* function: free_@FCTSUFFIX\n\
 * TO""DO: Describe Frees all associated resources. */\n\
int free_@FCTSUFFIX(@TYPENAME *obj);\n\
\n\
// group: query\n\
\n\
// group: update\n\
\n\n\n\
// section: inline implementation\n\
\n\
/* define: init_@FCTSUFFIX\n\
 * Implements <@TYPENAME.init_@FCTSUFFIX>. */\n\
#define init_@FCTSUFFIX(obj) \\\n\
         // TODO: implement\n\
\n\n\
#endif\n";

static const char* s_templatesource =
"/* title: @TITLE impl\n\n\
   Implements <@TITLE>.\n\n\
   Copyright:\n\
   This program is free software. See accompanying LICENSE file.\n\n\
   Author:\n\
   (C) 2016 Jörg Seebohn\n\n\
   file: @HEADERPATH\n\
    Header file <@TITLE>.\n\n\
   file: @SOURCEPATH\n\
    Implementation file <@TITLE impl>.\n\
*/\n\n\
#include \"C-kern/konfig.h\"\n\
#include \"@HEADERPATH\"\n\
#include \"C-kern/api/err.h\"\n\
#ifdef KONFIG_UNITTEST\n\
#include \"C-kern/api/test/unittest.h\"\n\
#endif\n\n\
// === private types\n\
// TODO: struct helper_@TYPENAME;\n\n\n\
// section: @TYPENAME\n\n\
// group: lifetime\n\n\n\
// section: Functions\n\n\
// group: test\n\n\
#ifdef KONFIG_UNITTEST\n\n\
static int test_initfree(void)\n\
{\n\
   @TYPENAME obj = @TYPENAME2_FREE;\n\n\
   // TEST @TYPENAME2_FREE\n\
   TEST(0 == obj.dummy);\n\n\
   return 0;\n\
ONERR:\n\
   return EINVAL;\n\
}\n\n\
static int childprocess_unittest(void)\n\
{\n\
   // NEED TO #include \"C-kern/api/test/resourceusage.h\"\n\
   resourceusage_t   usage = resourceusage_FREE;\n\n\
   TEST(0 == init_resourceusage(&usage));\n\n\
   if (test_initfree())       goto ONERR;\n\n\
   TEST(0 == same_resourceusage(&usage));\n\
   TEST(0 == free_resourceusage(&usage));\n\n\
   return 0;\n\
ONERR:\n\
   (void) free_resourceusage(&usage);\n\
   return EINVAL;\n\
}\n\n\
int @UNITTESTNAME()\n\
{\n\
   // select between (1) or (2)\n\
   // == (1) ==\n\
   int err;\n\n\
   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));\n\n\
   return err;\n\
   // == (2) ==\n\
   // NEED TO remove childprocess_unittest\n\
   if (test_initfree())       goto ONERR;\n\n\
   return 0;\n\
ONERR:\n\
   return EINVAL;\n\
}\n\n\
#endif\n";

static int convertpath(const char * filepath, cstring_t * headertag, cstring_t * unittestname)
{
   int err;

   // remove path prefix

   const char * removeprefix[] = { "C-kern", "api", "/", "home" };
   for (unsigned i = 0; i < lengthof(removeprefix); ) {
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
   if (err) goto ONERR;

   for (size_t i = 0; i < filepath_len; ++i) {
      char c  = filepath[i] ;
      if ('a' <= c && c <= 'z') {
         c = (char) (c + ('A' - 'a')) ;
      } else if (! ( ('A' <= c && c <= 'Z')
                     || ('0' <= c && c <= '9'))) {
         c = '_' ;
      }
      err = append_cstring(headertag, 1, &c) ;
      if (err) goto ONERR;

      if ('A' <= c && c <= 'Z') {
         c = (char) (c + ('a' - 'A')) ;
      }
      err = append_cstring(unittestname, 1, &c) ;
      if (err) goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

static int converttype(const char * typenamestr, cstring_t * tname2, cstring_t * fctsuffix)
{
   int err ;

   // remove typename suffix
   size_t typename_len = strlen(typenamestr) ;
   if (  strrchr(typenamestr, '_')
         && strlen(strrchr(typenamestr, '_')) < 5) {
      typename_len -= strlen(strrchr(typenamestr, '_')) ;
   }

   clear_cstring(fctsuffix) ;
   clear_cstring(tname2) ;

   // construct typename2 without suffix
   append_cstring(tname2, typename_len, typenamestr) ;

   for (size_t i = 0; i < typename_len; ++i) {
      char c  = typenamestr[i] ;
      if ('A' <= c && c <= 'Z') {
         c = (char) (c + ('a' - 'A')) ;
      } else if (c == '_') {
         continue ;
      }

      err = append_cstring(fctsuffix, 1, &c) ;
      if (err) goto ONERR;
   }

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

static int check_variable(const char * filetemplate, /*out*/variable_e * varindex, /*out*/size_t * varlength)
{
   static const char * varnames[] = {
      [variable_TITLE] = "TITLE",
      [variable_FCTSUFFIX]  = "FCTSUFFIX",
      [variable_HEADERPATH] = "HEADERPATH",
      [variable_HEADERTAG]  = "HEADERTAG",
      [variable_SOURCEPATH] = "SOURCEPATH",
      [variable_TYPENAME2]  = "TYPENAME2",
      [variable_TYPENAME]   = "TYPENAME",
      [variable_UNITTESTNAME] = "UNITTESTNAME"
   } ;

   for (unsigned i = 0; i < lengthof(varnames); ++i) {
      if (  varnames[i]
            && 0 == strncmp(varnames[i], filetemplate, strlen(varnames[i]))) {
         *varlength = strlen(varnames[i]);
         *varindex  = i;
         return 0;
      }
   }

   return EINVAL ;
}

static int substitute_variable(file_t outfile, variable_e varindex)
{
   int err ;

   switch (varindex) {
   case variable_TITLE:
      err = write_file(outfile, strlen(s_filetitle), s_filetitle, 0) ;
      break ;
   case variable_FCTSUFFIX:
      err = write_file(outfile, size_cstring(&s_fctsuffix), str_cstring(&s_fctsuffix), 0) ;
      break ;
   case variable_HEADERPATH:
      err = write_file(outfile, strlen(s_headerpath), s_headerpath, 0) ;
      break ;
   case variable_HEADERTAG:
      err = write_file(outfile, size_cstring(&s_headertag), str_cstring(&s_headertag), 0) ;
      break ;
   case variable_SOURCEPATH:
      err = write_file(outfile, strlen(s_sourcepath), s_sourcepath, 0) ;
      break ;
   case variable_TYPENAME2:
      err = write_file(outfile, size_cstring(&s_typename2), str_cstring(&s_typename2), 0) ;
      break ;
   case variable_TYPENAME:
      err = write_file(outfile, strlen(s_typename), s_typename, 0) ;
      break ;
   case variable_UNITTESTNAME:
      err = write_file(outfile, size_cstring(&s_unittestname), str_cstring(&s_unittestname), 0) ;
      break ;
   }

   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}

static int generate_file(const char * filetemplate, const char * filepath)
{
   int err ;
   file_t outfile = file_FREE ;

   err = initcreate_file(&outfile, filepath, 0) ;
   if (err) goto ONERR;

   for (size_t toff = 0; filetemplate[toff]; ++toff) {
      uint8_t c = (uint8_t) filetemplate[toff] ;

      if (c == '@') {
         // variable substitution
         variable_e varindex;
         size_t varlength;
         if (0 == check_variable(&filetemplate[toff+1], &varindex, &varlength)) {
            toff += varlength;
            err = substitute_variable(outfile, varindex);
            if (err) goto ONERR;
            continue;
         }
      }

      err = write_file(outfile, 1, &c, 0);
      if (err) goto ONERR;
   }

   err = free_file(&outfile);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   (void) free_file(&outfile);
   return err;
}


static int process_arguments(int argc, const char * argv[])
{
   s_programname = strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0] ;

   if (argc != 5) goto ONERR;

   s_filetitle  = argv[1] ;
   s_typename   = argv[2] ;
   s_headerpath = argv[3] ;
   s_sourcepath = argv[4] ;

   return 0 ;
ONERR:
   return EINVAL ;
}

static int main_thread(maincontext_t * maincontext)
{
   int err ;

   err = process_arguments(maincontext->argc, maincontext->argv) ;
   if (err) goto PRINT_USAGE ;

   err = convertpath(s_headerpath, &s_headertag, &s_unittestname) ;
   if (err) goto PRINT_USAGE ;

   err = converttype(s_typename, &s_typename2, &s_fctsuffix) ;
   if (err) goto PRINT_USAGE ;

   // parse templates => expand variables => write header + source files
   err = generate_file(s_templateheader, s_headerpath) ;
   if (err) goto ONERR;
   err = generate_file(s_templatesource, s_sourcepath) ;
   if (err) goto ONERR;

   return 0 ;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "Genfile version 0.1 - Copyright (c) 2012 Joerg Seebohn\n" ) ;
   dprintf(STDERR_FILENO, "\nDescription:\n Generates a simple header and source\n file skeleton for use in this project.\n" ) ;
   dprintf(STDERR_FILENO, "\nUsage:\n %s <filetitle> <typename> <headerfilename> <sourcefilename>\n", s_programname) ;
ONERR:
   return 1 ;
}

int main(int argc, const char * argv[])
{
   int err ;

   err = initrun_maincontext(maincontext_CONSOLE, &main_thread, 0, argc, argv);

   return err ;
}
