/* title: genmake

   Makefile Generator <genmake>.

   Uses <Tool-HashFunction> for its implementation.

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

   file: C-kern/main/tools/genmake.c
    Main implementation of tool <genmake>.
*/

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>


#include "C-kern/tools/hash.h"


const char*     g_programname = 0 ;
#define         g_definesprefix  " $(DefineFlag_%s)"  // space needed
#define         g_includesprefix " $(IncludeFlag_%s)" // space needed
#define         g_libsprefix     " $(LibraryFlag_%s)" // space needed
#define         g_libpathprefix  " $(LibPathFlag_%s)" // space needed

/* predefined (readonly) variables */
const char * g_predefinedIDs[] = {
    "mode"              // $(mode) is replaced by name of current buildmode (Debug,...)
   ,"projectname"       // $(projectname) is replaced by filename without path and extension
   ,"cflags"            // $(cflags) is replaced by value of CFlags for current build mode (only  valid in Compiler=, CFlags_<mode>)
   ,"lflags"            // $(lflags) is replaced by value of LFlags for current build mode (only  valid in Linker=, LFlags_<mode>)
   ,"libs"              // $(libs) is replaced by list of libraries and corresponding search paths
   ,"includes"          // $(includes) is replaced by list of include paths prefixed with $(CFlagInclude)
   ,"defines"           // $(defines) is replaced by list of defines prefixed with $(CFlagDefine)
   ,"in"                // $(in) is replaced with name of input file in (Compiler=) or object files in (Linker=)
   ,"out"               // $(out) is replaced with name of object file (Compiler=) or target file in (Linker=)
   , 0
} ;

const char * g_cmdInclude = "include" ;
const char * g_cmdLink    = "link" ;

const char * const g_varCompiler = "Compiler" ; // example: Compiler = gcc
const char * const g_varCFlagDefine   = "CFlagDefine" ;    // example: CFlagDefine  = -D
const char * const g_varCFlagInclude  = "CFlagInclude" ;   // example: CFlagInclude = -I
const char * const g_varCFlags   = "CFlags" ;   // example: CFlags_Debug = -g -fpic
const char * const g_varDefines  = "Defines" ;  // example: Defines = CONFIG_X86=1
const char * const g_varIncludes = "Includes" ; // example: Includes =  src/Includes
const char * const g_varLFlags   = "LFlags" ;   // example: LFlags_Debug = -g -shared -fpic
const char * const g_varLibs     = "Libs" ;     // example: Libs =  X11 Xext
const char * const g_varLFlagLib = "LFlagLib" ; // example: LFlagLib =  -l
const char * const g_varLibpath  = "Libpath" ;  // example: Libpath =  /usr/X11R6/lib  /usr/lib/special
const char * const g_varLFlagLibpath = "LFlagLibpath" ; // example: LFlagLibpath =  -L
const char * const g_varLFlagTarget = "LFlagTarget" ;  // example: LFlagTarget =  -o
const char * const g_varLinker   = "Linker" ;   // example: Linker = gcc
const char * const g_varModes    = "Modes" ;    // example: Modes = Debug Release
const char * const g_varObjectdir= "Objectdir" ;// example: Objectdir =  obj/$(mode)
const char * const g_varSrc      = "Src" ;      // example: Src =  src/presentation/*.c
const char * const g_varTarget   = "Target" ;   // example: Target =  bin/$(projectname)_$(mode)


#ifdef __GNUC__
void print_err(const char* error_description_format, /*arguments*/ ...) __attribute__ ((__format__ (__printf__, 1, 2))) ;
void print_warn(const char* error_description_format, /*arguments*/ ...) __attribute__ ((__format__ (__printf__, 1, 2))) ;
#endif

void print_err(const char* error_description_format, /*arguments*/ ...)
{
   va_list error_description_args ;
   va_start( error_description_args, error_description_format ) ;
   fprintf( stderr, "%s: error: ", g_programname) ;
   vfprintf( stderr, error_description_format, error_description_args) ;
   va_end( error_description_args ) ;
}

void print_warn(const char* error_description_format, /*arguments*/ ...)
{
   va_list error_description_args ;
   va_start( error_description_args, error_description_format ) ;
   fprintf( stderr, "%s: warning: ", g_programname) ;
   vfprintf( stderr, error_description_format, error_description_args) ;
   va_end( error_description_args ) ;
}

int errmalloc( void ** memblock, size_t size)
{
   void * new_memblock = malloc( size ) ;
   if (!new_memblock) {
      print_err( "Out of memory!\n" ) ;
      return ENOMEM ;
   }

   *memblock = new_memblock ;
   return 0 ;
}

typedef struct stringarray stringarray_t ;
struct stringarray {
   uint16_t size ;
   size_t   valueslen ; // sum of len of all values
   char *   strings[] ;
} ;

void free_stringarray( stringarray_t** array)
{
   if (array && *array) {
      free(*array) ;
      *array = 0 ;
   }
}

int ccopy_stringarray( size_t size, char ** strings, stringarray_t** result)
{
   assert( size == 0 || strings != 0 ) ;

   if (size > (uint16_t)-1) {
      print_err( "Out of memory!\n" ) ;
      return ENOMEM ;
   }

   size_t valuelen  = 0 ;
   for(size_t i = 0; i < size; ++i) {
         valuelen += strlen(strings[i]) ;
   }
   if (errmalloc((void**)result, sizeof(stringarray_t) + size * (1+sizeof(char*)) + valuelen)) {
      return ENOMEM ;
   }
   (*result)->size = (uint16_t) size ;
   (*result)->valueslen = valuelen ;
   char * stringvalue = ((char*)(*result)) + sizeof(stringarray_t) + size * sizeof(char*) ;
   for(size_t i = 0; i < size; ++i) {
      (*result)->strings[i] = stringvalue ;
      strcpy( stringvalue, strings[i]) ;
      stringvalue += strlen(strings[i]) + 1 ;
   }
   assert( stringvalue == ((char*)(*result))+(sizeof(stringarray_t) + size * (1+sizeof(char*)) + valuelen) ) ;
   return 0 ;
}

int new_stringarray( const char * values, const char * separators, stringarray_t** result)
{
   if (!values || !separators) return 1 ;

   int isSeparator = 1 ;
   size_t valuecount = 0 ;
   size_t valuelen  = 0 ;
   for(size_t i = 0; values[i]; ++i) {
      if (isSeparator) {
         if (!strchr( separators, values[i])) {
            isSeparator = 0 ;
            ++ valuelen ;
            ++ valuecount ;
         }
      } else {
         if (strchr( separators, values[i])) {
            isSeparator = 1 ;
         } else {
            ++ valuelen ;
         }
      }
   }
   *result = (stringarray_t*) malloc( sizeof(stringarray_t) + valuecount * (1+sizeof(char*)) + valuelen ) ;
   if (!(*result) || ((uint16_t)valuecount) < valuecount ) {
      free(*result) ;
      *result = 0 ;
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }
   (*result)->size = (uint16_t) valuecount ;
   (*result)->valueslen = valuelen ;
   char * stringvalue = ((char*)(*result)) + sizeof(stringarray_t) + valuecount * sizeof(char*) ;
   int valuesoffset = 0 ;
   for(size_t i = 0; i < valuecount; ++i) {
      (*result)->strings[i] = stringvalue ;
      while( values[valuesoffset] && strchr( separators, values[valuesoffset])) ++valuesoffset ;
      while( values[valuesoffset] && !strchr( separators, values[valuesoffset])) {
         *stringvalue = values[valuesoffset] ;
         ++stringvalue ;
         ++valuesoffset ;
      }
      *stringvalue = 0 ;
      ++stringvalue ;
      assert( stringvalue <= ((char*)(*result))+(sizeof(stringarray_t) + valuecount * (1+sizeof(char*)) + valuelen) ) ;
   }
   assert( stringvalue == ((char*)(*result))+(sizeof(stringarray_t) + valuecount * (1+sizeof(char*)) + valuelen) ) ;
   return 0 ;
}

typedef struct hash_entry_subclass hash_entry_subclass_t ;
struct hash_entry_subclass
{
   hash_entry_t hash ;
   uint8_t      isPredefinedID ;
   uint8_t      isUsed ;
   char         name[] ;
} ;

static void free_hashtableentry( hash_entry_t* entry )
{
   assert( entry->name  == ((hash_entry_subclass_t*)entry)->name ) ;
   free( entry->data ) ;
   free( entry ) ;
}

static hash_entry_subclass_t* new_hashtableentry(hash_table_t* hashindex, const char* name, size_t len )
{
   hash_entry_subclass_t * entry = (hash_entry_subclass_t*) malloc(sizeof(hash_entry_subclass_t) + len + 1) ;
   if (entry) {
      memset(entry, 0, sizeof(hash_entry_subclass_t)) ;
      entry->hash.name = entry->name ;
      memcpy(entry->name, name, len) ;
      entry->name[len] = 0 ;
      if (HASH_OK != insert_hashtable(hashindex, &entry->hash)) {
         free_hashtableentry(&entry->hash) ;
         entry = 0 ;
      }
   }
   if (!entry) {
      print_err( "Out of memory!\n" ) ;
   }
   return entry ;
}

typedef struct konfigvalues konfigvalues_t ;
struct konfigvalues {    // mode-qualifizierte konfig-Werte
   uint16_t  modecount ;
   char** compiler/*[modecount]*/;
   char** compiler_flags/*[modecount]*/ ;
   char** defines/*...*/  ;
   char** define_flag ;
   char** includes ;
   char** include_flag ;
   char** libs ;
   char** lib_flag ;
   char** libpath ;
   char** libpath_flag ;
   char** linker ;
   char** linker_flags ;
   char** modes ;
   char** objectfiles_directory ;
   char** src ;
   char** target_directory ;
   char** target_filename ;
   stringarray_t ** src_files ;
   stringarray_t ** obj_files ;
   stringarray_t ** linktargets/*...*/  ;
   stringarray_t ** linkmodefrom/*[modecount]*/  ;
} ;

void free_konfigvalues(konfigvalues_t** konfig)
{
   if (konfig && *konfig) {
      for(size_t i = 0; i < (*konfig)->modecount; ++i) {
         free((*konfig)->compiler[i]) ;
         free((*konfig)->compiler_flags[i]) ;
         free((*konfig)->defines[i]) ;
         free((*konfig)->define_flag[i]) ;
         free((*konfig)->includes[i]) ;
         free((*konfig)->include_flag[i]) ;
         free((*konfig)->libs[i]) ;
         free((*konfig)->lib_flag[i]) ;
         free((*konfig)->libpath[i]) ;
         free((*konfig)->libpath_flag[i]) ;
         free((*konfig)->linker[i]) ;
         free((*konfig)->linker_flags[i]) ;
         free((*konfig)->modes[i]) ;
         free((*konfig)->objectfiles_directory[i]) ;
         free((*konfig)->src[i]) ;
         free((*konfig)->target_directory[i]) ;
         free((*konfig)->target_filename[i]) ;
         free_stringarray(&(*konfig)->src_files[i]) ;
         free_stringarray(&(*konfig)->obj_files[i]) ;
         free_stringarray(&(*konfig)->linktargets[i]) ;
         free_stringarray(&(*konfig)->linkmodefrom[i]) ;
      }
      free(*konfig) ;
      *konfig = 0 ;
   }
}

int new_konfigvalues(stringarray_t * modes, konfigvalues_t** result)
{
   konfigvalues_t * konfig ;
   const uint16_t modecount = modes->size ;
   const size_t konfigsize = sizeof(konfigvalues_t) + 210u * modecount * sizeof(char*) ;
   konfig = (konfigvalues_t*) malloc(konfigsize) ;
   if (!konfig) {
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }
   memset(konfig, 0, konfigsize) ;
   konfig->modecount             = modecount ;
   konfig->compiler              = 0 * modecount + (char**) (&konfig[1]) ;
   konfig->compiler_flags        = 1 * modecount + (char**) (&konfig[1]) ;
   konfig->defines               = 2 * modecount + (char**) (&konfig[1]) ;
   konfig->define_flag           = 3 * modecount + (char**) (&konfig[1]) ;
   konfig->includes              = 4 * modecount + (char**) (&konfig[1]) ;
   konfig->include_flag          = 5 * modecount + (char**) (&konfig[1]) ;
   konfig->libs                  = 6 * modecount + (char**) (&konfig[1]) ;
   konfig->lib_flag              = 7 * modecount + (char**) (&konfig[1]) ;
   konfig->libpath               = 8 * modecount + (char**) (&konfig[1]) ;
   konfig->libpath_flag          = 9 * modecount + (char**) (&konfig[1]) ;
   konfig->linker                = 10 * modecount + (char**) (&konfig[1]) ;
   konfig->linker_flags          = 11 * modecount + (char**) (&konfig[1]) ;
   konfig->modes                 = 12 * modecount + (char**) (&konfig[1]) ;
   konfig->objectfiles_directory = 13 * modecount + (char**) (&konfig[1]) ;
   konfig->src                   = 14 * modecount + (char**) (&konfig[1]) ;
   konfig->target_directory      = 15 * modecount + (char**) (&konfig[1]) ;
   konfig->target_filename       = 16 * modecount + (char**) (&konfig[1]) ;
   konfig->src_files             = 17 * modecount + (stringarray_t**) (&konfig[1]) ;
   konfig->obj_files             = 18 * modecount + (stringarray_t**) (&konfig[1]) ;
   konfig->linktargets           = 19 * modecount + (stringarray_t**) (&konfig[1]) ;
   konfig->linkmodefrom          = 20 * modecount + (stringarray_t**) (&konfig[1]) ;

   for(size_t i = 0; i < modes->size; ++i) {
      konfig->modes[i] = strdup(modes->strings[i]) ;
      if (!konfig->modes[i]) {
         print_err( "Out of memory!\n" ) ;
         free_konfigvalues(&konfig) ;
         return 1 ;
      }
   }

   *result = konfig ;
   return 0 ;
}

typedef struct LinkTarget linktarget_t ;
struct LinkTarget {
   linktarget_t * next ;
   char *  mode ;
   char *  target ;
   char *  mappedFromMode ;
} ;

int new_linktarget(
   linktarget_t** result,
   const char * mode,
   size_t       modelen,
   const char * target,
   const char * mappedFromMode )
{
   assert( result && mode && target ) ;
   linktarget_t * lt = (linktarget_t*) malloc(sizeof(linktarget_t) + modelen + strlen(target) + strlen(mappedFromMode) + 3) ;
   if (!lt) {
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }

   lt->next    = 0 ;
   lt->mode    = (char*) (&lt[1]) ;
   lt->target  = lt->mode + modelen + 1 ;
   lt->mappedFromMode = lt->target + strlen(target) + 1 ;
   strncpy( lt->mode, mode, modelen ) ;
   lt->mode[modelen] = 0 ;
   strcpy( lt->target, target ) ;
   strcpy( lt->mappedFromMode, mappedFromMode ) ;

   *result = lt ;
   return 0  ;
}

typedef struct LinkCommand linkcommand_t ;
struct LinkCommand {
   linkcommand_t *  next ;
   linktarget_t  *  targets ;
   char  *          filename ;
   char  *          projectname ;
} ;

int new_linkcommand(
   linkcommand_t** result,
   const char * filename,
   const char * projectname )
{
   assert( result && filename && projectname ) ;
   linkcommand_t * lc = (linkcommand_t*) malloc(sizeof(linkcommand_t) + strlen(filename) + strlen(projectname) + 2) ;
   if (!lc) {
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }

   memset( lc, 0, sizeof(*lc)) ;
   lc->filename    = (char*) (&lc[1]) ;
   lc->projectname = lc->filename + strlen(filename) + 1 ;
   strcpy( lc->filename, filename ) ;
   strcpy( lc->projectname, projectname ) ;

   *result = lc ;
   return 0  ;
}

typedef struct genmakeproject genmakeproject_t ;
struct genmakeproject {
   hash_table_t *    index ;
   // stringarray_t *   modes ;
   char *            filename ;
   char *            name ;
   uint32_t          links_count ;
   linkcommand_t  *  links ;
   konfigvalues_t *  konfig ;
} ;

void free_genmakeproject(genmakeproject_t** genmake)
{
   assert(genmake) ;
   if (!(*genmake)) return ;

   free_hashtable( &(*genmake)->index ) ;
   (*genmake)->filename = 0 ;
   (*genmake)->name = 0 ;
   free_konfigvalues( &(*genmake)->konfig ) ;
   if ( (*genmake)->links ) {
      linkcommand_t * lc = (*genmake)->links ;
      linkcommand_t * lc_next = lc->next;
      do {
         lc = lc_next ;
         while( lc->targets ) {
            linktarget_t * next = lc->targets->next ;
            free(lc->targets) ;
            lc->targets = next ;
         }
         lc_next = lc->next ;
         free(lc) ;
      } while( lc != (*genmake)->links ) ;
   }
   free((*genmake)) ;
   *genmake = 0 ;
}

int new_genmakeproject(genmakeproject_t** result, const char * filename)
{
   assert( result && filename ) ;
   size_t         namelen = 0 ;
   const char *   projectname = 0 ;
   hash_table_t * hashindex = 0 ;

   projectname = (strrchr(filename, '/') ? strrchr(filename, '/')+1 : filename) ;
   namelen = strlen(projectname) ;
   if (namelen && strchr(projectname+1, '.')) namelen = (size_t) (strchr(projectname+1, '.') - projectname) ;

   genmakeproject_t * genmake = (genmakeproject_t*) malloc(sizeof(genmakeproject_t) + strlen(filename) + namelen + 2) ;

   if (!genmake || new_hashtable(&hashindex, 1023, free_hashtableentry)) {
      print_err( "Out of memory!\n" ) ;
      goto ABBRUCH ;
   }

   memset( genmake, 0, sizeof(*genmake)) ;
   genmake->index    = hashindex ;
   genmake->filename = (char*) (&genmake[1]) ;
   genmake->name     = genmake->filename + strlen(filename) + 1 ;
   strcpy( genmake->filename, filename) ;
   strncpy(genmake->name, projectname, namelen ) ;
   genmake->name[namelen] = 0 ;
   while( strpbrk(genmake->name, " \t") ) *strpbrk(genmake->name, " \t") = '_' ;

   for(int i = 0; g_predefinedIDs[i]; ++i) {
      // predefined variables are mapped onto itself !
      hash_entry_subclass_t* entry = new_hashtableentry(hashindex, g_predefinedIDs[i], strlen(g_predefinedIDs[i])) ;
      entry->isPredefinedID = 1 ;
      entry->isUsed    = 0 ;
      entry->hash.data = malloc( strlen(g_predefinedIDs[i]) + 4 ) ;
      if (!entry->hash.data) {
         print_err( "Out of memory!\n" ) ;
         goto ABBRUCH ;
      }
      strcpy( entry->hash.data, "$(" ) ;
      strcat( entry->hash.data, g_predefinedIDs[i] ) ;
      strcat( entry->hash.data, ")" ) ;
   }

   *result = genmake ;
   return 0 ;

ABBRUCH:
   free(genmake) ;
   free_hashtable(&hashindex) ;
   return 1 ;
}

static char * get_directory(const char * path)
{
   char * last = path ? strrchr(path, '/') : 0 ;
   char * directory ;
   if ( !last || (last == path) ) {
      directory = strdup("") ;
   } else {
      size_t len = (size_t) (last - path) ;
      directory = malloc( len + 1 ) ;
      if (directory) {
         strncpy( directory, path, len) ;
         directory[len] = 0 ;
      }
   }
   if (!directory) {
      print_err( "Out of memory!\n" ) ;
   }
   return directory ;
}

static char* replace_vars(hash_table_t * hashindex, int lineNr, const char * linebuffer, size_t len, const char* filename)
{
   if (!linebuffer || ((int)len) < 0) return 0 ;

   size_t   size   = len + 1 ;
   size_t   resultOffset = 0 ;
   size_t   bufferOffset = 0 ;
   char*    result = (char*) malloc(size) ;
   int      err = 0 ;

   while( result ) {
      size_t varStart = len ;
      for(size_t i = bufferOffset; i < len; ++i) {
         if (   linebuffer[i]=='$'
            &&  i < len-1
            &&  linebuffer[i+1]=='(' ) {
            varStart = i + 2 ;
            break ;
         } else {
            result[resultOffset++] = linebuffer[i] ;
            assert( resultOffset < size ) ;
         }
      }
      if (varStart == len) break ;
      size_t varEnd = len ;
      for(size_t i = varStart; i < len; ++i) {
         if ( linebuffer[i]==')' ) {
            varEnd = i ;
            break ;
         }
      }
      if (varStart == varEnd ) {
         print_err( "line %d expected non empty '$()' in file '%s'\n", lineNr, filename ) ;
         err = 1 ;
         break ;
      }
      if (varEnd == len) {
         print_err( "line %d expected ')' after '$(' in file '%s'\n", lineNr, filename ) ;
         err = 1 ;
         break ;
      }
      bufferOffset = varEnd + 1 ;
      hash_entry_t * hash_entry ;
      if (HASH_OK != search_hashtable2( hashindex, &linebuffer[varStart], varEnd-varStart, &hash_entry)) {
         print_err( "line %d undefined value $(%s) used in file '%s'\n", lineNr, &linebuffer[varStart], filename ) ;
         err = 1 ;
         break ;
      }
      ((hash_entry_subclass_t*)hash_entry)->isUsed = 1 ;
      size_t datalen = hash_entry->data ? strlen(hash_entry->data) : 0 ;
      if (datalen) {
         if (datalen > varEnd-varStart+3) {
            size += (datalen - (varEnd-varStart+3)) ;
            void * result2 = realloc(result, size) ;
            if (!result2) {
               free(result) ;
               result = 0 ;
               break ;
            }
            result = result2 ;
         }
         strcpy(&result[resultOffset], hash_entry->data) ;
         resultOffset += datalen ;
         assert( resultOffset < size ) ;
      }
   }
   if (!result) {
      print_err( "Out of memory!\n" ) ;
   } else if (err) {
      free(result) ;
      result = 0 ;
   } else {
      assert( resultOffset < size ) ;
      result[resultOffset] = 0 ;
   }
   return result ;
}

typedef enum MatchedCommand { CmdIgnore, CmdInclude, CmdLink, CmdAssign } command_e ;
typedef struct parse_line_result parse_line_result_t ;
struct parse_line_result
{
   size_t idStart ;
   size_t idLen ;
   command_e  command ; // 0 (cmd = param), 1 (include filename)
   char   assigntype ;  // '=' ("="), '+' ("+=")
   size_t paramStart ;
   size_t paramLen ;
   stringarray_t * modemap ;
} ;

static int parse_line(parse_line_result_t* result, int lineNr, const char* linebuffer, const char* filename)
{
   size_t ci = 0 ;
   while (linebuffer[ci] && ( linebuffer[ci] == ' ' || linebuffer[ci] == '\t' || linebuffer[ci] == '\n' ) ) {
      ++ ci ;
   }

   if (  0==linebuffer[ci] /* ignore empty lines */
      || linebuffer[ci] == '#') { /* ignore comments */
      memset( result, 0, sizeof(*result)) ;
      return 0 ;
   }

   size_t idStart = ci ;
   while (  ('0' <= linebuffer[ci] && linebuffer[ci] <= '9')
         || ('A' <= linebuffer[ci] && linebuffer[ci] <= 'Z')
         || ('a' <= linebuffer[ci] && linebuffer[ci] <= 'z')
         || (linebuffer[ci] == '_' )) {
      ++ci ;
      if (0==linebuffer[ci]) break ;
   }
   size_t idLen = ci - idStart ;

   if (  ( '0' <= linebuffer[idStart] && linebuffer[idStart] <= '9')
      || 0 == idLen ) {
      print_err( "line %d wrong identifier in file '%s'\n", lineNr, filename ) ;
      return 1 ;
   }

   while (linebuffer[ci] && ( linebuffer[ci] == ' ' || linebuffer[ci] == '\t' || linebuffer[ci] == '\n' ) ) {
      ++ ci ;
   }

   command_e matchedCommand = CmdAssign ;
   char assigntype = 0 ;
   if ( linebuffer[ci] == '=') {
      assigntype = '=' ; ++ci ;
   } else if ( linebuffer[ci] == '+' && linebuffer[ci+1] == '=') {
      assigntype = '+' ; ++ci ; ++ci ;
   } else if (  (strlen(g_cmdInclude) != idLen || strncmp(&linebuffer[idStart], g_cmdInclude, idLen))
            &&  (strlen(g_cmdLink) != idLen || strncmp(&linebuffer[idStart], g_cmdLink, idLen))) {
      print_err( "line %d expected '=' in file '%s'\n", lineNr, filename ) ;
      return 1 ;
   }

   while (linebuffer[ci] && ( linebuffer[ci] == ' ' || linebuffer[ci] == '\t' || linebuffer[ci] == '\n' ) ) {
      ++ ci ;
   }

   size_t paramStart = ci ;

   ci += strlen(&linebuffer[ci]) ;
   while (ci > paramStart && ( linebuffer[ci-1] == ' ' || linebuffer[ci-1] == '\t' || linebuffer[ci-1] == '\n' ) ) {
      -- ci ;
   }

   size_t paramLen = ci - paramStart ;
   stringarray_t * modemap = 0 ;

   if (strlen(g_cmdInclude) == idLen && !strncmp(&linebuffer[idStart], g_cmdInclude, idLen)) {
      /* parse include */
      matchedCommand = CmdInclude ;
      if (assigntype) {
         print_err( "line %d no '%s' expected in file '%s'\n", lineNr, '+'==assigntype?"+=":"=", filename ) ;
         return 1 ;
      } else if (!paramLen) {
         print_err( "line %d expected filename after %s in file '%s'\n", lineNr, g_cmdInclude, filename ) ;
         return 1 ;
      }
   } else if (strlen(g_cmdLink) == idLen && !strncmp(&linebuffer[idStart], g_cmdLink, idLen)) {
      /* parse read */
      matchedCommand = CmdLink ;
      if (assigntype) {
         print_err( "line %d no '%s' expected in file '%s'\n", lineNr, '+'==assigntype?"+=":"=", filename ) ;
         return 1 ;
      } else if (!paramLen) {
         print_err( "line %d expected filename after %s in file '%s'\n", lineNr, g_cmdLink, filename ) ;
         return 1 ;
      }
      /* build modemap */
      const char * modemapStart = strpbrk( &linebuffer[paramStart], " \t") ;
      if (modemapStart) {
         assert( modemapStart > &linebuffer[paramStart]) ;
         paramLen = (size_t) (modemapStart - &linebuffer[paramStart]) ;
         while( *modemapStart == ' ' || *modemapStart == '\t')  ++modemapStart ;
      }
      if (!modemapStart || !*modemapStart) {
         print_err( "line %d expected mode mapping after 'read %s' in file '%s'\n", lineNr, &linebuffer[paramStart], filename ) ;
         return 1 ;
      }
      if (new_stringarray( modemapStart, " \t", &modemap)) {
         return 1 ;
      }
      /* default mapping  'read <file> default=>'
       * 'Debug=>* is an abbreviation for Debug=>Debug'
       * */
      bool isDefault = false ;
      for(size_t mi = 0; mi < modemap->size; ++mi) {
         const char * sep = strstr( modemap->strings[mi], "=>" ) ;
         if (!sep) {
            print_err( "line %d expected '=>' in '%s' (no space allowed) in file '%s'\n", lineNr, modemap->strings[mi], filename ) ;
            free_stringarray(&modemap) ;
            return 1 ;
         }
         if (0==sep[2]) {
            if (isDefault) {
               print_err( "line %d default mapping '%s' allowed only once in file '%s'\n", lineNr, modemap->strings[mi], filename ) ;
               free_stringarray(&modemap) ;
               return 1 ;
            }
            isDefault = true ;
         }
         for(ci = 0; modemap->strings[mi][ci]; ++ci) {
            const char c = modemap->strings[mi][ci] ;
            if (c == '=' && (&modemap->strings[mi][ci]) == sep) {
               if (0==strcmp(&sep[2], "*")) break ;  // skip abbreviation '=>*'
               ++ci ;
               continue ;   // skip '=>'
            }
            if (  ('0' <= c && c <= '9')
               || ('a' <= c && c <= 'z')
               || ('A' <= c && c <= 'Z')
               || (' ' == c )
               || ('\t' == c )
               || ('_' == c ) )
               continue ;
            print_err( "line %d unexpected character '%c' in file '%s'\n", lineNr, c, filename ) ;
            free_stringarray(&modemap) ;
            return 1 ;
         }
      }
   } else {
      /* parse assign */
      if (strlen(g_varModes) == idLen && 0==strncmp(&linebuffer[idStart], g_varModes, idLen)) {
         for(ci = 0; ci < paramLen; ++ci) {
            const char c = linebuffer[paramStart+ci] ;
            if (  ('0' <= c && c <= '9')
               || ('a' <= c && c <= 'z')
               || ('A' <= c && c <= 'Z')
               || (' ' == c )
               || ('\t' == c )
               || ('_' == c ) )
               continue ;
            print_err( "line %d unexpected character '%c' in file '%s'\n", lineNr, c, filename ) ;
            return 1 ;
         }
      }
   }

   result->idStart = idStart ;
   result->idLen   = idLen ;
   result->command = matchedCommand ;
   result->assigntype = assigntype ;
   result->paramStart = paramStart ;
   result->paramLen   = paramLen ;
   result->modemap    = modemap ;
   return 0 ;
}


const char * get_varvalue(hash_table_t * hashindex, const char * varname, const char * mode, const char * projectfilename)
{
   hash_entry_t * hash_entry ;
   char * qualifiedName = (char*) malloc( strlen(varname) + strlen(mode?mode:"") + 2 ) ;
   if (!qualifiedName) { print_err("Out of memory!\n") ; return 0 ; }
   strcpy( qualifiedName, varname ) ;
   if (mode) {
      strcat( qualifiedName, "_" ) ;
      strcat( qualifiedName, mode) ;
   }
   void * data = 0 ;
   if (0==search_hashtable(hashindex, qualifiedName, &hash_entry)) {
      ((hash_entry_subclass_t*)hash_entry)->isUsed = 1 ;
      data = hash_entry->data ;
   } else if (0==search_hashtable(hashindex, varname, &hash_entry)) {
      ((hash_entry_subclass_t*)hash_entry)->isUsed = 1 ;
      data = hash_entry->data ;
   } else {
      if (mode)
         print_err( "expected config value '%s' or '%s' in file '%s'\n", varname, qualifiedName, projectfilename) ;
      else
         print_err( "expected config value '%s' in file '%s'\n", varname, projectfilename) ;
   }
   free(qualifiedName) ;
   return data ;
}

static int print_read_warning(const char *epath, int eerrno)
{
   print_warn( "Could not read path '%s'\n: %s\n", epath, strerror(eerrno)) ;
   return 0 ;
}

static int set_compiler_predefinedIDs(genmakeproject_t * genmake, char * mode)
{
   const char * ids[]    = { "cflags", "includes", "defines", "lflags", "libs", "in", "out", 0 } ;
   const char * values[] = { "$(CFlags_", "$(Includes_", "$(Defines_", "$(LFlags_", "$(Libs_", "'$<'", "'$@'", 0 } ;

   for(int i = 0; ids[i]; ++i) {
      hash_entry_t * hashentry = 0 ;
      if (search_hashtable(genmake->index,ids[i],&hashentry)) {
         print_err("internal error search_hashtable(,'%s',)\n", ids[i]) ;
         goto ABBRUCH ;
      }
      free(hashentry->data) ;
      char * value ;
      bool isMode  = (values[i][strlen(values[i])-1] == '_') ;
      value = malloc( strlen(values[i]) + (isMode ? strlen(mode) + 2 : 1) ) ;
      if (!value) {
         print_err( "Out of memory!\n" ) ;
         goto ABBRUCH ;
      }
      strcpy( value, values[i]) ;
      if (isMode) { strcat( value, mode ) ; strcat( value, ")" ) ; }
      hashentry->data = value ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

static int set_linker_predefinedIDs(genmakeproject_t * genmake, char * mode)
{
   const char * ids[]    = { "in", "out", 0 } ;
   const char * values[] = { "$(foreach obj,$^,'$(obj)')", "'$@'", 0 } ;

   if (set_compiler_predefinedIDs(genmake,mode)) goto ABBRUCH ;

   for(int i = 0; ids[i]; ++i) {
      hash_entry_t * hashentry = 0 ;
      if (search_hashtable(genmake->index,ids[i],&hashentry)) {
         print_err("internal error search_hashtable(,'%s',)\n", ids[i]) ;
         goto ABBRUCH ;
      }
      free(hashentry->data) ;
      char * value ;
      bool isMode  = (values[i][strlen(values[i])-1] == '_') ;
      value = malloc( strlen(values[i]) + (isMode ? strlen(mode) + 2 : 1) ) ;
      if (!value) {
         print_err( "Out of memory!\n" ) ;
         goto ABBRUCH ;
      }
      strcpy( value, values[i]) ;
      if (isMode) { strcat( value, mode ) ; strcat( value, ")" ) ; }
      hashentry->data = value ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

static int set_other_predefinedIDs(genmakeproject_t * genmake)
{
   const char * ids[]   = { "cflags",    "lflags",    "includes",   "defines",     "libs",    0 } ;
   const char * vars[]  = { g_varCFlags, g_varLFlags, g_varIncludes, g_varDefines, g_varLibs, 0 } ;

   for(int i = 0; ids[i]; ++i) {
      hash_entry_t * hashentry = 0 ;
      char * value ;
      if (search_hashtable(genmake->index, vars[i], &hashentry)) {
         value = strdup("") ;
      } else {
         value = replace_vars(genmake->index, 0/*unknown linenr*/, hashentry->data, strlen(hashentry->data), genmake->filename) ;
      }
      if (!value) {
         print_err("Out of memory!\n") ;
         goto ABBRUCH ;
      }
      if (search_hashtable(genmake->index, ids[i], &hashentry)) {
         print_err("internal error search_hashtable(,'%s',)\n", ids[i]) ;
         goto ABBRUCH ;
      }
      free(hashentry->data) ;
      hashentry->data = value ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

int build_konfiguration(genmakeproject_t * genmake)
{
   konfigvalues_t * konfig = 0 ;

   {  stringarray_t * modes = 0 ;
      const char * modesvalue = get_varvalue( genmake->index, g_varModes, 0, genmake->filename) ;
      if (!modesvalue) goto ABBRUCH ;
      if (modesvalue[0] == 0) modesvalue = "default" ;
      if (new_stringarray( modesvalue, " \t", &modes)) goto ABBRUCH ;
      if (new_konfigvalues( modes, &konfig)) goto ABBRUCH ;

      for(size_t i = 0; i < modes->size; ++i) {
         assert(konfig->modes[i]) ;
      }
      free_stringarray(&modes) ;
   }

   hash_entry_t * hashentry = 0 ;
   if (search_hashtable(genmake->index,"projectname",&hashentry)) {
      print_err("internal error search_hashtable(,'projectname',)\n") ;
      goto ABBRUCH ;
   }
   free(hashentry->data) ;
   hashentry->data = strdup(genmake->name) ;
   if (!hashentry->data) {
      print_err( "Out of memory!\n" ) ;
      goto ABBRUCH ;
   }

   for(size_t m = 0; m < konfig->modecount; ++m) {
      if (search_hashtable(genmake->index,"mode",&hashentry)) {
         print_err("internal error search_hashtable(,'mode',)\n") ;
         goto ABBRUCH ;
      }
      free(hashentry->data) ;
      hashentry->data = strdup(konfig->modes[m]) ;
      if (!hashentry->data) {
         print_err( "Out of memory!\n" ) ;
         goto ABBRUCH ;
      }

      if (set_compiler_predefinedIDs(genmake, konfig->modes[m])) goto ABBRUCH ;
      {  const char * value = get_varvalue( genmake->index, g_varCompiler, konfig->modes[m], genmake->filename) ;
         konfig->compiler[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->compiler[m]) goto ABBRUCH ; }

      if (set_other_predefinedIDs(genmake)) goto ABBRUCH ;
      {  const char * value = get_varvalue( genmake->index, g_varCFlags, konfig->modes[m], genmake->filename) ;
         konfig->compiler_flags[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->compiler_flags[m]) goto ABBRUCH ; }

      {  const char * value = get_varvalue( genmake->index, g_varDefines, konfig->modes[m], genmake->filename) ;
         stringarray_t * strarray = 0 ;
         char * value2 = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (new_stringarray( value2, " \t", &strarray)) goto ABBRUCH ;
         free(value2) ;
         konfig->defines[m] = (char*) malloc(1 + strarray->valueslen + strarray->size * (strlen(g_definesprefix)+strlen(konfig->modes[m]))) ;
         if (!konfig->defines[m]) { print_err("Out of memory!\n") ; goto ABBRUCH ; }
         int size = 0 ;
         for(uint16_t i = 0; i < strarray->size; ++i) {
            size += sprintf(konfig->defines[m]+size, g_definesprefix, konfig->modes[m]) ;
            size += sprintf(konfig->defines[m]+size, "%s", strarray->strings[i]) ;
         }
         konfig->defines[m][size] = 0 ;
         assert( size == (int)(strarray->valueslen + strarray->size * (strlen(g_definesprefix)-2+strlen(konfig->modes[m]))) ) ;
         free_stringarray(&strarray) ;
      }

      {  const char * value = get_varvalue( genmake->index, g_varCFlagDefine, konfig->modes[m], genmake->filename) ;
         konfig->define_flag[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->define_flag[m]) goto ABBRUCH ; }

      {  const char * value = get_varvalue( genmake->index, g_varIncludes, konfig->modes[m], genmake->filename) ;
         char * value2 = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         stringarray_t * strarray = 0 ;
         if (new_stringarray( value2, " \t", &strarray)) goto ABBRUCH ;
         free(value2) ;
         konfig->includes[m] = (char*) malloc(1 + strarray->valueslen + strarray->size * (strlen(g_includesprefix)+strlen(konfig->modes[m]))) ;
         if (!konfig->includes[m]) { print_err("Out of memory!\n") ; goto ABBRUCH ; }
         int size = 0 ;
         for(uint16_t i = 0; i < strarray->size; ++i) {
            size += sprintf(konfig->includes[m]+size, g_includesprefix, konfig->modes[m]) ;
            size += sprintf(konfig->includes[m]+size, "%s", strarray->strings[i]) ;
         }
         konfig->includes[m][size] = 0 ;
         assert( size <= (int)(strarray->valueslen + strarray->size * (strlen(g_includesprefix)-2+strlen(konfig->modes[m])))) ;
         free_stringarray(&strarray) ;
      }

      {  const char * value = get_varvalue( genmake->index, g_varCFlagInclude, konfig->modes[m], genmake->filename) ;
         konfig->include_flag[m] = replace_vars( genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->include_flag[m]) goto ABBRUCH ; }

      {  const char * value = get_varvalue( genmake->index, g_varLibs, konfig->modes[m], genmake->filename) ;
         char * value2 = replace_vars( genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         stringarray_t * strarray = 0 ;
         if (new_stringarray( value2, " \t", &strarray)) goto ABBRUCH ;
         free(value2) ;
         konfig->libs[m] = (char*) malloc(1 + strarray->valueslen + strarray->size * (strlen(g_libsprefix)+strlen(konfig->modes[m]))) ;
         if (!konfig->libs[m]) { print_err("Out of memory!\n") ; goto ABBRUCH ; }
         int size = 0 ;
         for(uint16_t i = 0; i < strarray->size; ++i) {
            size += sprintf( konfig->libs[m]+size, g_libsprefix, konfig->modes[m]) ;
            size += sprintf( konfig->libs[m]+size, "%s", strarray->strings[i]) ;
         }
         konfig->libs[m][size] = 0 ;
         assert( size == (int)(strarray->valueslen + strarray->size * (strlen(g_libsprefix)-2+strlen(konfig->modes[m])))) ;
         free_stringarray(&strarray) ;
      }

      {  const char * value = get_varvalue( genmake->index, g_varLFlagLib, konfig->modes[m], genmake->filename) ;
         konfig->lib_flag[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->lib_flag[m]) goto ABBRUCH ; }

      {  const char * value = get_varvalue( genmake->index, g_varLibpath, konfig->modes[m], genmake->filename) ;
         char * value2 = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         stringarray_t * strarray = 0 ;
         if (new_stringarray( value2, " \t", &strarray)) goto ABBRUCH ;
         free(value2) ;
         konfig->libpath[m] = (char*) malloc(1 + strarray->valueslen + strarray->size * (strlen(g_libpathprefix)+strlen(konfig->modes[m]))) ;
         if (!konfig->libpath[m]) { print_err("Out of memory!\n") ; goto ABBRUCH ; }
         konfig->libpath[m][0] = 0 ;
         for(uint16_t i = 0; i < strarray->size; ++i) {
            sprintf( konfig->libpath[m]+strlen(konfig->libpath[m]), g_libpathprefix, konfig->modes[m]) ;
            strcat( konfig->libpath[m], strarray->strings[i]) ;
         }
         free_stringarray(&strarray) ;
      }

      {  const char * value = get_varvalue( genmake->index, g_varLFlagLibpath, konfig->modes[m], genmake->filename) ;
         konfig->libpath_flag[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->libpath_flag[m]) goto ABBRUCH ; }

      if (set_linker_predefinedIDs(genmake, konfig->modes[m])) goto ABBRUCH ;
      {  const char * value = get_varvalue( genmake->index, g_varLinker, konfig->modes[m], genmake->filename) ;
         konfig->linker[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->linker[m]) goto ABBRUCH ; }

      if (set_other_predefinedIDs(genmake)) goto ABBRUCH ;
      {  const char * value = get_varvalue( genmake->index, g_varLFlags, konfig->modes[m], genmake->filename) ;
         konfig->linker_flags[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->linker_flags[m]) goto ABBRUCH ; }

      {  const char * value = get_varvalue( genmake->index, g_varObjectdir, konfig->modes[m], genmake->filename) ;
         size_t len = 0 ; if (value) len = strlen(value) ; while (len && value[len-1] == '/') --len ;
         konfig->objectfiles_directory[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, len, genmake->filename) ;
         if (!konfig->objectfiles_directory[m]) goto ABBRUCH ; }

      {  const char * value = get_varvalue( genmake->index, g_varSrc, konfig->modes[m], genmake->filename) ;
         konfig->src[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         if (!konfig->src[m]) goto ABBRUCH ; }

      {  const char * value = get_varvalue( genmake->index, g_varTarget, konfig->modes[m], genmake->filename) ;
         konfig->target_filename[m] = replace_vars(genmake->index, 0/*unknown linenr*/, value, strlen(value?value:""), genmake->filename) ;
         konfig->target_directory[m] = get_directory( konfig->target_filename[m] ) ;
         if (!konfig->target_filename[m]) goto ABBRUCH ;
         if (!konfig->target_directory[m]) goto ABBRUCH ; }

      // scan directories and build array of files
      {  stringarray_t * searchpatterns = 0 ;
         int errflag = 0 ;
         if (new_stringarray( konfig->src[m], " \t", &searchpatterns)) goto ABBRUCH ;
         glob_t foundfiles = { .gl_pathc = 0, .gl_pathv= 0 } ;
         for(size_t pi=0; !errflag && pi < searchpatterns->size; ++pi) {
            char * pattern = searchpatterns->strings[pi] ;

            errflag = glob( pattern, (pi? (GLOB_APPEND|GLOB_NOSORT):(GLOB_NOSORT)), print_read_warning, &foundfiles) ;
            if (errflag == GLOB_NOMATCH) {
               print_warn( "'Src_%s=%s' matched no files\n(defined in file '%s')\n", konfig->modes[m], pattern, genmake->filename) ;
               errflag = 0 ;
            } else if (errflag == GLOB_NOSPACE) {
               print_err( "Out of memory!\n" ) ;
               errflag = 1 ;
            } else {
               errflag = 0 ;
            }
         }
         free_stringarray(&searchpatterns) ;
         if (ccopy_stringarray( foundfiles.gl_pathc, foundfiles.gl_pathv, &konfig->src_files[m])) errflag = 1 ;
         if (ccopy_stringarray( foundfiles.gl_pathc, foundfiles.gl_pathv, &konfig->obj_files[m])) errflag = 1 ;
         if (foundfiles.gl_pathv) globfree(&foundfiles) ;
         if (errflag) goto ABBRUCH ;
      }

      {  assert(konfig->obj_files[m]) ;
         for(uint16_t f = 0; f < konfig->obj_files[m]->size; ++f) {
            char * unsupported_character ;
            if ( (unsupported_character=strpbrk(konfig->obj_files[m]->strings[f], " \t\n:$'\"")) ) {
               char ch[2] = { *unsupported_character, 0 } ;
               const char * str = ch ;
               if (*unsupported_character == '\n') str = "\n" ;
               else if (*unsupported_character == '\t') str = "\t" ;
               else if (*unsupported_character == '\'') str = "\'" ;
               print_err( "Filename '%s' contains unsupported character ('%s')!\n", konfig->obj_files[m]->strings[f], str ) ;
               goto ABBRUCH ;
            }
         }
      }

      {  assert(konfig->obj_files[m]) ;
         for(uint16_t f = 0; f < konfig->obj_files[m]->size; ++f) {
            for(size_t i = 0; konfig->obj_files[m]->strings[f][i]; ++i) {
               if (konfig->obj_files[m]->strings[f][i] == '/') konfig->obj_files[m]->strings[f][i] = '!' ;
            }
            if (konfig->obj_files[m]->strings[f][0] == '.') konfig->obj_files[m]->strings[f][0] = '_' ;
         }
      }

      {  // linktargets
         char ** linktargets  = (char**) malloc( sizeof(char*) * genmake->links_count ) ;
         char ** linkmodefrom = (char**) malloc( sizeof(char*) * genmake->links_count ) ;
         if (!linktargets || !linkmodefrom) {
            free(linktargets) ;
            free(linkmodefrom) ;
            print_err( "Out of memory!\n" ) ;
            goto ABBRUCH ;
         }
         // (1) find corresponding targets for every link command
         linkcommand_t * linkcmd = genmake->links ;
         for(size_t cmdi = 0; cmdi < genmake->links_count; ++cmdi) {
            assert( linkcmd && "internal error" ) ;
            linkcmd = linkcmd->next ;
            linktargets[cmdi]  = 0 ;
            linkmodefrom[cmdi] = 0 ;
            for( linktarget_t * lt = linkcmd->targets ; lt ; lt = lt->next ) {
               if (  0==strcmp(konfig->modes[m], lt->mode)
                  || (!lt->mode[0] && !linktargets[cmdi]) /*defaultmapping (mode[0]="")*/ ) {
                  linktargets[cmdi]  = lt->target ;
                  linkmodefrom[cmdi] = lt->mappedFromMode ;
               }
            }
            if (!linktargets[cmdi]) {
               print_err( "'link %s' defines no mapping for mode '%s' in file '%s')\n", linkcmd->filename, konfig->modes[m], genmake->filename) ;
               free(linktargets) ;
               free(linkmodefrom) ;
               goto ABBRUCH ;
            }
         }
         if (  ccopy_stringarray(genmake->links_count, linktargets, &konfig->linktargets[m])
            || ccopy_stringarray(genmake->links_count, linkmodefrom, &konfig->linkmodefrom[m])) {
            free(linktargets) ;
            free(linkmodefrom) ;
            goto ABBRUCH ;
         }
         free(linkmodefrom) ;
         free(linktargets) ;
      }

   }

   // ! generate errors for unused variables !
   hash_entry_t * hentry ;
   const char * ids[]   = { "cflags",    "lflags",    "includes",   "defines",     "libs",    0 } ;
   const char * vars[]  = { g_varCFlags, g_varLFlags, g_varIncludes, g_varDefines, g_varLibs, 0 } ;
   for(int i = 0; ids[i]; ++i) {
      if (  0 == search_hashtable(genmake->index, ids[i], &hentry)
            && ((hash_entry_subclass_t*)hentry)->isUsed) {
            if (  0 == search_hashtable(genmake->index, vars[i], &hentry)) {
               ((hash_entry_subclass_t*)hentry)->isUsed = 1 ;
            }
      }
   }
   if (HASH_OK == firstentry_hashtable(genmake->index, &hentry)) {
      int errflag = 0 ;
      do {
         if (  !((hash_entry_subclass_t*)hentry)->isUsed
            && !((hash_entry_subclass_t*)hentry)->isPredefinedID) {
            print_err("unused variable definiton '%s' in file '%s'\n", hentry->name, genmake->filename) ;
            errflag = 1 ;
         }
      } while(HASH_OK==nextentry_hashtable(genmake->index, hentry, &hentry)) ;
      if (errflag) goto ABBRUCH ;
   }


   free_konfigvalues(&genmake->konfig) ;
   genmake->konfig = konfig ;
   return 0 ;

ABBRUCH:
   free_konfigvalues(&konfig) ;
   return 1 ;
}

typedef struct project_file project_file_t ;
struct project_file
{
   FILE*            file ;
   int              lineNr ;
   char*            name ;
   project_file_t*  included_from ;
} ;

static int read_projectfile(genmakeproject_t * genmake)
{
   int              err = 0 /*no error*/ ;
   char             linebuffer[1000] ;
   project_file_t   prj_file = { .file = NULL, .lineNr = 0, .name = genmake->filename, .included_from = NULL } ;
   project_file_t*  prj_file_stack = &prj_file ;

   FILE * file ;
   while( prj_file_stack )
   {
      PROCESS_STACK:

      file = prj_file_stack[0].file ;
      if (err) goto STACK_UNWIND ;

      if (!file) {
         prj_file_stack[0].file = file = fopen(prj_file_stack[0].name, "r") ;
      }
      if (!file) {
         const char* included_from_filename = 0 ;
         if ( prj_file_stack->included_from ) {
            included_from_filename = prj_file_stack->included_from->name ;
         }
         if (included_from_filename)
            print_err( "Cannot open project file '%s'\n included from '%s'\n", prj_file_stack[0].name, included_from_filename) ;
         else
            print_err( "Cannot open project file '%s'\n", prj_file_stack[0].name ) ;
         err = 1 ;
      } else {
         int lineNr = prj_file_stack[0].lineNr ;
         while( !feof(file) && !ferror(file) ) {
            /* read one line */
            if (!fgets( linebuffer, sizeof(linebuffer), file)) continue ; /*read error or eof occured */
            size_t len = strlen(linebuffer) ;
            ++ lineNr ;
            if (len == sizeof(linebuffer)-1 && linebuffer[len-1] != '\n') {
               print_err( "line %d too long in file '%s'\n", lineNr, prj_file_stack[0].name ) ;
               err = 1 ;
               break ;
            }
            if (len && linebuffer[len-1] == '\n') linebuffer[--len] = 0 ;
            /* parse line content */
            parse_line_result_t  parsed ;
            err = parse_line(&parsed, lineNr, linebuffer, prj_file_stack[0].name) ;
            if (err) break ;
            if (CmdInclude == parsed.command) {
               /* include filename */
               project_file_t * include_file = (project_file_t*) malloc(sizeof(project_file_t)) ;
               if (!include_file) {
                  print_err( "Out of memory!\n" ) ;
                  err = 1 ;
                  break ;
               }
               char* incfilename = replace_vars(genmake->index, lineNr, &linebuffer[parsed.paramStart], parsed.paramLen, prj_file_stack[0].name) ;
               if (!incfilename) {
                  free(include_file) ;
                  err = 1 ;
                  break ;
               }
               prj_file_stack[0].lineNr = lineNr ; // remember position in file
               *include_file  = (project_file_t) { .file = NULL, .lineNr = 0, .name = incfilename, .included_from = prj_file_stack } ;
               prj_file_stack = include_file ;
               goto PROCESS_STACK ;
            } else if (CmdLink == parsed.command) {
               /* read filename  defaultmode=> mode1=>modeX mode2=>modeY mode3=>* ...*/
               char* readfilename = replace_vars(genmake->index, lineNr, &linebuffer[parsed.paramStart], parsed.paramLen, prj_file_stack[0].name) ;
               if (!readfilename) {
                  err = 1 ;
                  free_stringarray(&parsed.modemap) ;
                  break ;
               }
               genmakeproject_t * subgenmake = 0 ;
               err = new_genmakeproject(&subgenmake, readfilename) ;
               if (!err) err = read_projectfile(subgenmake) ;
               if (!err) err = build_konfiguration(subgenmake) ;
               linkcommand_t * lc = 0 ;
               if (!err) err = new_linkcommand( &lc, readfilename, subgenmake->name) ;
               if (!err) {
                  ++ genmake->links_count ;
                  if (genmake->links) {
                     lc->next = genmake->links->next ;
                     genmake->links->next = lc ;
                  } else {
                     lc->next = lc ;
                  }
                  genmake->links = lc ;
               }
               if (!err) {
                  // check modemap mapping
                  for(size_t i = 0; i < parsed.modemap->size; ++i) {
                     size_t submode_index ;
                     char* separator = strstr(parsed.modemap->strings[i], "=>") ;
                     assert(separator && "internal error") ;
                     *separator = 0 ;
                     bool found = false ;
                     for(submode_index = 0; submode_index < subgenmake->konfig->modecount; ++submode_index) {
                        found = (0==strcmp(parsed.modemap->strings[i], subgenmake->konfig->modes[submode_index])) ;
                        if (found) break ;
                     }
                     if (!found) {
                        err = 1 ;
                        print_err( "line %d unknown mode '%s=>' in file '%s'\n", lineNr, parsed.modemap->strings[i], prj_file_stack[0].name ) ;
                        break ;
                     }
                     size_t modelen = strlen(separator+2) ;
                     const char * foundmode = "" ;
                     hash_entry_t * hash_entry ;
                     if (modelen) {
                        if (  search_hashtable(genmake->index, g_varModes, &hash_entry)
                           || 0==hash_entry->data) {
                           print_err( "line %d 'Modes' must be defined beforehand in file '%s'\n", lineNr, prj_file_stack[0].name ) ;
                           err = 1 ;
                           break ;
                        }
                        foundmode = strstr( hash_entry->data, separator+2)  ;
                        if (modelen == 1 && separator[2] == '*') {
                           foundmode = strstr( hash_entry->data, parsed.modemap->strings[i]) ;
                           modelen   = strlen( parsed.modemap->strings[i]) ;
                        }
                        found = false ;
                        if (foundmode) {
                           found  =  (  (foundmode == hash_entry->data)
                                     || (foundmode[-1]==' ' || foundmode[-1]=='\t') )
                                 &&  (foundmode[modelen]==' ' || foundmode[modelen]=='\t' || foundmode[modelen]==0) ;
                        }
                        if (!found) {
                           print_err( "line %d unknown mode '%s' in file '%s'\n", lineNr, separator, prj_file_stack[0].name ) ;
                           err = 1 ;
                           break ;
                        }
                     }
                     linktarget_t * lt ;
                     char * target = subgenmake->konfig->target_filename[submode_index] ;
                     char * mappedFromMode = subgenmake->konfig->modes[submode_index] ;
                     err = new_linktarget(&lt, foundmode, modelen, target, mappedFromMode) ;
                     if (err) break ;
                     lt->next    = lc->targets ;
                     lc->targets = lt ;
                  }
               }
               free_genmakeproject(&subgenmake) ;
               free_stringarray(&parsed.modemap) ;
               if (err) break ;
            } else if (CmdAssign == parsed.command) {
               /* ID = value*/
               hash_entry_t * hash_entry ;
               linebuffer[parsed.idStart + parsed.idLen] = 0 ;
               if (HASH_OK != search_hashtable(genmake->index, &linebuffer[parsed.idStart], &hash_entry)) {
                  hash_entry = &(new_hashtableentry( genmake->index, &linebuffer[parsed.idStart], parsed.idLen)->hash) ;
                  if (!hash_entry) {
                     err = 1 ;
                     break ;
                  }
               }
               if ( ((hash_entry_subclass_t *)hash_entry)->isPredefinedID ) {
                  print_err( "line %d can not assign predefined '%s' in file '%s'\n", lineNr, hash_entry->name, prj_file_stack[0].name ) ;
                  err = 1 ;
                  break ;
               }
               char* new_value = replace_vars(genmake->index, lineNr, &linebuffer[parsed.paramStart], parsed.paramLen, prj_file_stack[0].name) ;
               if (!new_value) {
                  err = 1 ;
                  break ;
               }
               if ('+' == parsed.assigntype && 0 != hash_entry->data) {
                  char* new_value2 = (char*) malloc( 2 + strlen(new_value) + strlen((char*)hash_entry->data) ) ;
                  if (new_value2) {
                     strcpy(new_value2, (char*)hash_entry->data) ;
                     strcat(new_value2, " ") ; // separator
                     strcat(new_value2, new_value) ;
                  } else {
                     print_err( "Out of memory!\n" ) ;
                     err = 1 ;
                     break ;
                  }
                  free(new_value) ;
                  new_value = new_value2 ;
               }
               free(hash_entry->data) ;
               hash_entry->data = new_value ;
           }
         } /* while read next line */

         if (ferror(file)) {
            clearerr(file) ;
            print_err( "Cannot read file '%s'\n", prj_file_stack[0].name ) ;
            err = 1 ;
         }

      }

      STACK_UNWIND:

      if (file) fclose( file ) ;
      project_file_t*  includedFrom = prj_file_stack[0].included_from ;
      if (includedFrom) {
         free( prj_file_stack[0].name ) ;
         free( prj_file_stack ) ;
      }
      prj_file_stack = includedFrom ;

   }

   return err ;
}



int write_makefile(
   genmakeproject_t * genmake,
   const char*  makefilename, /*NULL => use stdout*/
   int          isDirectory
   )
{
   int err = 1 ;
   konfigvalues_t * konfig = genmake->konfig ;
   char * constructed_filename = 0 ;
   FILE * makefile = stdout ;
   struct stat makefilestat ;

   if (makefilename) {
      size_t cfnamelen = strlen(makefilename) ;
      if (0==stat( makefilename, &makefilestat)) {
         if (S_ISDIR(makefilestat.st_mode)) {
            cfnamelen += strlen("/Makefile.") + strlen(genmake->name) ;
            constructed_filename = malloc( 1 + cfnamelen ) ;
            if (!constructed_filename) {
               print_err( "Out of memory!\n" ) ;
               goto ABBRUCH ;
            }
            strcpy( constructed_filename, makefilename) ;
            if (constructed_filename[strlen(makefilename)-1] == '/') constructed_filename[strlen(makefilename)-1] = 0 ;
            strcat( constructed_filename, "/Makefile." ) ;
            strcat( constructed_filename, genmake->name ) ;
            if (0==stat( constructed_filename, &makefilestat)) {
               print_err("Won't overwrite existing file '%s' with generated Makefile.\n Delete old file manually.\n", constructed_filename) ;
               goto ABBRUCH ;
            }
         } else {
            print_err("Won't overwrite existing file '%s' with generated Makefile.\n Delete old file manually.\n", makefilename) ;
            goto ABBRUCH ;
         }
      } else if (!isDirectory) {
         constructed_filename = strdup(makefilename) ;
         if (!constructed_filename) {
            print_err( "Out of memory!\n" ) ;
            goto ABBRUCH ;
         }
      } else {
         print_err( "Directory '%s' does not exist (argument of -o)\n", makefilename ) ;
         goto ABBRUCH ;
      }
   }

   if (constructed_filename) {
      makefile = fopen(constructed_filename, "w") ;
      if (!makefile) {
         print_err("Could not open '%s' for writing.\n", constructed_filename) ;
         goto ABBRUCH ;
      }
   }

   fprintf(makefile, "##\n## ! Genmake generated Makefile !\n##\n") ;
   char datbuffer[100] ;
   time_t t = time(NULL) ;
   strftime( datbuffer, sizeof(datbuffer), "%Y.%b.%d  %H:%M:%S", localtime(&t)) ;
   fprintf(makefile, "ProjectName     := %s\n", genmake->name) ;
   fprintf(makefile, "ProjectModes    :=") ;
   for(uint16_t m = 0; m < konfig->modecount; ++m) fprintf(makefile, " %s", konfig->modes[m]) ;
   fprintf(makefile, "\nProjectFile     := %s", genmake->filename) ;
   fprintf(makefile, "\nProjectLinks    := ") ;
   uint16_t li = 0 ;
   for(linkcommand_t * l = genmake->links; li < genmake->links_count; ++li) {
      l = l->next ; if (li) fprintf(makefile, "\\\n                   ") ;
      fprintf(makefile, "%s (", l->filename) ;
      for(uint16_t m = 0; m < konfig->modecount; ++m) {
         fprintf(makefile, " %s", konfig->linkmodefrom[m]->strings[li]) ;
      }
      fprintf(makefile, " )") ;
   }
   fprintf(makefile, "\n\nGenerationDate  := %s\n", datbuffer) ;
   struct passwd * user = getpwuid(getuid()) ;
   size_t userlen = user ? strlen(user->pw_gecos) : 1 ;
   if (strchr(user->pw_gecos,',')) userlen = (size_t) (strchr(user->pw_gecos,',') - user->pw_gecos) ;
   fprintf(makefile, "GeneratedBy     := %.*s\n", userlen, user?user->pw_gecos:"?") ;

   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      fprintf(makefile, "\n## %s\n", konfig->modes[m]) ;
      fprintf(makefile, "DefineFlag_%s    := %s\n", konfig->modes[m], konfig->define_flag[m]) ;
      fprintf(makefile, "IncludeFlag_%s   := %s\n", konfig->modes[m], konfig->include_flag[m]) ;
      fprintf(makefile, "LibraryFlag_%s   := %s\n", konfig->modes[m], konfig->lib_flag[m]) ;
      fprintf(makefile, "LibPathFlag_%s   := %s\n", konfig->modes[m], konfig->libpath_flag[m]) ;
      fprintf(makefile, "CFlags_%s        := %s\n", konfig->modes[m], konfig->compiler_flags[m]) ;
      fprintf(makefile, "LFlags_%s        := %s\n", konfig->modes[m], konfig->linker_flags[m]) ;
      fprintf(makefile, "Defines_%s       :=%s\n", konfig->modes[m], konfig->defines[m]) ;
      fprintf(makefile, "Includes_%s      :=%s\n", konfig->modes[m], konfig->includes[m]) ;
      fprintf(makefile, "Libraries_%s     :=%s\n", konfig->modes[m], konfig->libs[m]) ;
      fprintf(makefile, "Libpath_%s       :=%s\n", konfig->modes[m], konfig->libpath[m]) ;
      fprintf(makefile, "ObjectDir_%s     := %s\n", konfig->modes[m], konfig->objectfiles_directory[m]) ;
      fprintf(makefile, "TargetDir_%s     := %s\n", konfig->modes[m], konfig->target_directory[m]) ;
      fprintf(makefile, "Target_%s        := %s\n", konfig->modes[m], konfig->target_filename[m]) ;
      fprintf(makefile, "Libs_%s          := $(Libpath_%s) $(Libraries_%s)\n", konfig->modes[m], konfig->modes[m], konfig->modes[m]) ;
      fprintf(makefile, "CC_%s            = %s\n", konfig->modes[m], konfig->compiler[m]) ;
      fprintf(makefile, "LD_%s            = %s\n", konfig->modes[m], konfig->linker[m]) ;
   }
   fprintf(makefile, "\n##\n## Targets\n##\n") ;
   fprintf(makefile, "all:   ") ;
   for(uint16_t m = 0; m < konfig->modecount; ++m) fprintf(makefile, "%s%c", konfig->modes[m], m==konfig->modecount-1?'\n':' ') ;
   fprintf(makefile, "clean: ") ;
   for(uint16_t m = 0; m < konfig->modecount; ++m) fprintf(makefile, "clean_%s%c", konfig->modes[m], m==konfig->modecount-1?'\n':' ') ;
   fprintf(makefile, "init: ") ;
   for(uint16_t m = 0; m < konfig->modecount; ++m) fprintf(makefile, " init_%s", konfig->modes[m] ) ;
   for(uint16_t m = 0; m < konfig->modecount; ++m) fprintf(makefile, "\ninit_%s: $(ObjectDir_%s) $(TargetDir_%s)", konfig->modes[m], konfig->modes[m], konfig->modes[m] ) ;
   fprintf(makefile, "\n") ;

   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      fprintf(makefile, "\n%s: init_%s $(Target_%s)\n", konfig->modes[m], konfig->modes[m], konfig->modes[m]) ;
   }
   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      fprintf(makefile, "\nclean_%s:\n", konfig->modes[m]) ;
      fprintf(makefile, "\t@rm -f \"$(ObjectDir_%s)/\"*.[od]\n", konfig->modes[m]) ;
      fprintf(makefile, "\t@rm -f \"$(Target_%s)\"\n", konfig->modes[m]) ;
      fprintf(makefile, "\t@if [ -d \"$(ObjectDir_%s)\" ]; then rmdir -p --ignore-fail-on-non-empty \"$(ObjectDir_%s)\"; fi\n", konfig->modes[m], konfig->modes[m]) ;
      fprintf(makefile, "\t@if [ -d \"$(TargetDir_%s)\" ]; then rmdir -p --ignore-fail-on-non-empty \"$(TargetDir_%s)\"; fi\n", konfig->modes[m], konfig->modes[m]) ;
   }
   fprintf(makefile, "\n$(sort") ;
   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      fprintf(makefile, " $(ObjectDir_%s) $(TargetDir_%s)", konfig->modes[m], konfig->modes[m]) ;
   }
   fprintf(makefile, "):\n\t@mkdir -p \"$@\"\n") ;

   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      fprintf(makefile, "\nObjects_%s :=", konfig->modes[m]) ;
      for(uint16_t f = 0; f < konfig->obj_files[m]->size; ++f) {
         fprintf(makefile, " \\\n $(ObjectDir_%s)/%s.o", konfig->modes[m], konfig->obj_files[m]->strings[f]) ;
      }
      for(uint16_t l = 0; l < konfig->linktargets[m]->size; ++l) {
         fprintf(makefile, " \\\n %s", konfig->linktargets[m]->strings[l]) ;
      }
      fprintf(makefile, "\n") ;
   }

   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      fprintf(makefile, "\n$(Target_%s): $(Objects_%s)", konfig->modes[m], konfig->modes[m]) ;
      fprintf(makefile, "\n\t@$(LD_%s)\n", konfig->modes[m]) ;
   }

   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      assert( konfig->src_files[m]->size == konfig->obj_files[m]->size ) ;
      for(uint16_t f = 0; f < konfig->src_files[m]->size; ++f) {
         fprintf(makefile, "\n$(ObjectDir_%s)/%s.o: %s\n", konfig->modes[m], konfig->obj_files[m]->strings[f], konfig->src_files[m]->strings[f]) ;
         fprintf(makefile, "\t@$(CC_%s)\n", konfig->modes[m]) ;
      }
   }

   for(uint16_t m = 0; m < konfig->modecount; ++m) {
      fprintf(makefile, "\n-include $(Objects_%s:.o=.d)\n", konfig->modes[m]) ;
   }

   err = 0 ;

ABBRUCH:
   if (makefile && makefile != stdout) fclose(makefile) ;
   free(constructed_filename) ;
   return err ;
}

int main(int argc, char* argv[])
{
   int isPrintHelp = 0 ;
   g_programname = argv[0] ;

   #ifdef KONFIG_UNITTEST
   assert( 0 == unittest_hashtable() ) ;
   #endif

   if (argc < 2) {
      goto PRINT_USAGE ;
   }

   if (0 == strcmp(argv[1], "-h")) {
      isPrintHelp = 1 ;
      goto PRINT_USAGE ;
   }

   const char * makefilename  = 0 ;
   int currentArgIndex = 1 ;
   if (0 == strcmp(argv[currentArgIndex], "-o")) {
      if (argc < currentArgIndex+3) goto PRINT_USAGE ;
      makefilename = argv[currentArgIndex+1] ;
      currentArgIndex += 2 ;
   }

   int isDirectory = (argc > currentArgIndex+1) ;

   int err ;
   for(err = 0; !err && currentArgIndex < argc; ++currentArgIndex) {
      genmakeproject_t * genmake = 0 ;

      if (new_genmakeproject(&genmake, argv[currentArgIndex])) {
         err = 1 ;
      } else if (read_projectfile(genmake)) {
         err = 1 ;
      } else if (build_konfiguration(genmake)) {
         err = 1 ;
      } else if (write_makefile(genmake, makefilename, isDirectory )) {
         err = 1 ;
      }

      free_genmakeproject(&genmake) ;
   }

   return err ;

PRINT_USAGE:
   fprintf(stderr, "Genmake version 0.1; Copyright (C) 2010 Joerg Seebohn\n" ) ;
   fprintf(stderr, "\nUsage(1): %s -o <makefile.name> [OPTS] <filename>\n", g_programname ) ;
   fprintf(stderr, "%s", "       -> generates a makefile from a project description\n" ) ;
   fprintf(stderr, "     (2): %s -o <directory> [OPTS] <filename1> ... <filenameN>\n", g_programname ) ;
   fprintf(stderr, "%s", "       -> generates N makefiles from N project descriptions in the specified directory.\n" ) ;
   fprintf(stderr, "%s", "          Makefile names follow the pattern 'Makefile.<projectname>'.\n" ) ;
   fprintf(stderr, "     (3): %s [OPTS] <filename1> ... <filenameN>\n", g_programname ) ;
   fprintf(stderr, "%s", "       -> generates N makefiles and prints them to stdout.\n" ) ;
   fprintf(stderr, "     (4): %s  -h\n", g_programname ) ;
   fprintf(stderr, "%s", "       -> prints additional help.\n" ) ;
   fprintf(stderr, "Optional paramters [OPTS]:\n" ) ;
   fprintf(stderr, "-md <dir> : Every path in a project description is considered to be\n" ) ;
   fprintf(stderr, "            relative to the working directory of genmake.\n" ) ;
   fprintf(stderr, "            Filenames stored in generated Makefiles rely on this too.\n" ) ;
   fprintf(stderr, "            To be able to run make in a different directory than genmake\n" ) ;
   fprintf(stderr, "            use -md <dir> to set the working directory of make.\n" ) ;

   if (isPrintHelp) {
      fprintf(stderr, "%s", "\nGenmake reads in a project textfile decribing the compiler&linker" ) ;
      fprintf(stderr, "%s", "\ncalling conventions and writes out a makefile. A subseqent 'make -f makefile'" ) ;
      fprintf(stderr, "%s", "\nwill then build a binary or shared object library for example." ) ;

      fprintf(stderr, "%s", "\n\nThe process of creating a project description may be as simple as to include" ) ;
      fprintf(stderr, "%s", "\na predefined setting. For example 'include binaray.gcc' loads some default") ;
      fprintf(stderr, "%s", "\nsettings for producing a binaray executable from C source code files") ;
      fprintf(stderr, "%s", "\nwith help of the GNU C compiler. Build modes are predefind as 'Debug' and 'Release'." ) ;
      fprintf(stderr, "%s", "\nTo make up a complete project you need to add the location of the source files" ) ;
      fprintf(stderr, "%s", "\nand the path where to find include files. The last step is to set some" ) ;
      fprintf(stderr, "%s", "\npredefined values. If you do not need any assign an empty value." ) ;
      fprintf(stderr, "%s", "\nThe complete project description should now look somehow like:" ) ;
      fprintf(stderr, "%s", "\n  include   binary.gcc\n  Src       = project/src/*.c" ) ;
      fprintf(stderr, "%s", "\n  Includes  = project/include\n  Defines   = CONFIG_UNITTEST" ) ;

      fprintf(stderr, "%s", "\n\nThe generated makefile handles source file dependencies from included" ) ;
      fprintf(stderr, "%s", "\nheader files automatically. For every object file with extension '.o'") ;
      fprintf(stderr, "%s", "\nGenmake expects the compiler to build a so called dependency file" ) ;
      fprintf(stderr, "%s", "\nwith extension '.d' which are included at the end of makefile." ) ;
      fprintf(stderr, "%s", "\nIf the source files include directives changes the new dependencies" ) ;
      fprintf(stderr, "%s", "\nare taken into account. But if new source files are added you have to rerun Genmake." ) ;

      fprintf(stderr, "%s", "\n\nTo be able to manage more than one project with Genmake the") ;
      fprintf(stderr, "%s", "\nproject build order has to be provided by you in form of") ;
      fprintf(stderr, "%s", "\na top level makefile which handles dependencies between projects." ) ;
      fprintf(stderr, "%s", "\nThat is a library should be built before an executable which uses it." ) ;
      fprintf(stderr, "%s", "\nWith help of the command 'link path_to_genmake_project' one project" ) ;
      fprintf(stderr, "%s", "\ncan link to a library defined in another one." ) ;

      fprintf(stderr, "%s", "\n\n\nGeneral project syntax\n" ) ;
      fprintf(stderr, "%s", "\nEvery project is described as a set of 'variable = value' pairs." ) ;
      fprintf(stderr, "%s", "\nLeading and trailing spaces are removed from a value." ) ;
      fprintf(stderr, "%s", "\nTo define a list of values seperate them by space." ) ;
      fprintf(stderr, "%s", "\nAppending a value to a previously defined variable is possible with:") ;
      fprintf(stderr, "%s", "\n variable += valueN\n # - or -\n variable = $(variable) valueN" ) ;
      fprintf(stderr, "%s", "\nText lines beginning with a '#' are ignored and have a commenting character." ) ;
//TODO
      fprintf(stderr, "%s", "\n\nBuild Modes\n" ) ;
      fprintf(stderr, "%s", "\n\nCause most projects have at least two different building modes more than one" ) ;
      fprintf(stderr, "%s", "\nbuilding mode is supported by assigning a list of names to 'Modes'.") ;
      fprintf(stderr, "%s", "\nFor example 'Modes=Debug Release'." ) ;
      fprintf(stderr, "%s", "\nValues of mode dependent variables can be accessed/assigned with" ) ;
      fprintf(stderr, "%s", "\n'$(<varname>_<modename>)' and '<varname>_<modename> = ...' respectively." ) ;
      fprintf(stderr, "%s", "\n'CFlags_Debug = -g $(CFlags)' therefore adds '-g' to the list of compiler flags" ) ;
      fprintf(stderr, "%s", "\nonly if the project is built in 'Debug' mode.") ;

      fprintf(stderr, "%s", "\n\n\nList of commands\n" ) ;
      fprintf(stderr, "%s", "\n# Include (generic) project description to preset default values" ) ;
      fprintf(stderr, "%s", "\ninclude  path_to_generic_project_description" ) ;
      fprintf(stderr, "%s", "\n# Reads in a project description to determine the location of the target file." ) ;
      fprintf(stderr, "%s", "\n# The target filename is added to the 'Libs' variable. " ) ;
      fprintf(stderr, "%s", "\nlink  path_to_library_building_project_description" ) ;

      fprintf(stderr, "%s", "\n\n\nList of variables ('Name' or 'Name_<mode>')\n" ) ;
      fprintf(stderr, "%s", "\n# Values defined before the current line can be included in an assignment" ) ;
      fprintf(stderr, "%s", "\n# with '$(Name)' or '$(Name_<mode>)'\n" ) ;
      fprintf(stderr, "%s", "\n# Sets list of names of build modes" ) ;
      fprintf(stderr, "%s", "\nModes = Debug Release" ) ;
      fprintf(stderr, "%s", "\n# Defines program and parameter for translating a source file into an object file") ;
      fprintf(stderr, "%s", "\n# Genmake assumes that the compiler produces also a dependency file suitable") ;
      fprintf(stderr, "%s", "\n# for processing by make (ext. '.o' of $(out) replaced with '.d'). ") ;
      fprintf(stderr, "%s", "\nCompiler = gcc $(defines) $(includes) $(cflags) -c -o $(out) $(in)" ) ;
      fprintf(stderr, "%s", "\n# Defines program for linking object files into a binary") ;
      fprintf(stderr, "%s", "\nLinker = gcc $(lflags) -o $(out) $(in) $(libs)" ) ;
      fprintf(stderr, "%s", "\n# Defines prefix for adding include path to preprocessor") ;
      fprintf(stderr, "%s", "\nCFlagInclude = -I" ) ;
      fprintf(stderr, "%s", "\n# Defines prefix for setting a defined value to the preprocessor") ;
      fprintf(stderr, "%s", "\nCFlagDefine = -D" ) ;
      fprintf(stderr, "%s", "\n# Defines flag to indicate that the following linker argment is a library") ;
      fprintf(stderr, "%s", "\nLFlagLib = -l" ) ;
      fprintf(stderr, "%s", "\n# Flag which marks following linker argment as a library path") ;
      fprintf(stderr, "%s", "\nLFlagLibpath = -L" ) ;
      fprintf(stderr, "%s", "\n# Additional compiler arguments. At least the following are needed for gcc:") ;
      fprintf(stderr, "%s", "\n# -MMD: write also a dependency file as a by product") ;
      fprintf(stderr, "%s", "\n# -c: produce only an object file without linking") ;
      fprintf(stderr, "%s", "\nCFlags = -MMD -std=gnu99 -Wall -c" ) ;
      fprintf(stderr, "%s", "\n# Additional linker arguments") ;
      fprintf(stderr, "%s", "\nLFlags = " ) ;
      fprintf(stderr, "%s", "\n# List of libraries to link object files with") ;
      fprintf(stderr, "%s", "\nLibs = GL X11" ) ;
      fprintf(stderr, "%s", "\n# List of additional paths where linker can search for libraries") ;
      fprintf(stderr, "%s", "\nLibpath = " ) ;
      fprintf(stderr, "%s", "\n# Name of directory where object files are stored. ") ;
      fprintf(stderr, "%s", "\n# Names of object files are generated by replacing '/' characters of ") ;
      fprintf(stderr, "%s", "\n# of their path names with '!' chars.") ;
      fprintf(stderr, "%s", "\nObjectdir = bin/$(mode)" ) ;
      fprintf(stderr, "%s", "\n# Full path name of linker generated output file") ;
      fprintf(stderr, "%s", "\nTarget = bin/$(projectname).$(mode)" ) ;

      fprintf(stderr, "%s", "\n\n\nList of predefined readonly variables ('$(name)')\n" ) ;
      fprintf(stderr, "%s", "\n# - Used to construct 'Compiler=' calling convention" ) ;
      fprintf(stderr, "%s", "\nCompiler = gcc $(defines) $(includes) $(cflags) -c -o $(out) $(in)\n" ) ;
      fprintf(stderr, "%s", "\n#$(cflags): The value of 'CFlags_<mode>' at the end of project description." ) ;
      fprintf(stderr, "%s", "\n#           Or 'CFlags' if no build mode specific value is defined." ) ;
      fprintf(stderr, "%s", "\n#$(defines): " ) ;
      fprintf(stderr, "%s", "\n#$(includes): " ) ;
      fprintf(stderr, "%s", "\n#$(in): " ) ;
      fprintf(stderr, "%s", "\n#$(out): " ) ;
      fprintf(stderr, "%s", "\n\n# - Used to construct 'Linker=' calling convention" ) ;
      fprintf(stderr, "%s", "\nLinker = gcc $(lflags) -o $(out) $(in) $(libs)\n" ) ;
      fprintf(stderr, "%s", "\n#$(lflags): The value of 'LFlags_<mode>' at the end of project description." ) ;
      fprintf(stderr, "%s", "\n#           Or 'LFlags' if no build mode specific value is defined." ) ;
      fprintf(stderr, "%s", "\n#$(libs): " ) ;
      fprintf(stderr, "%s", "\n#$(in): " ) ;
      fprintf(stderr, "%s", "\n#$(out): " ) ;
      fprintf(stderr, "%s", "\n\n# - Used to include later 'CFlags' additions into 'CFlags_<mode>'" ) ;
      fprintf(stderr, "%s", "\nCFlags_Debug = -g $(cflags)\n" ) ;
      fprintf(stderr, "%s", "\n#$(cflags): " ) ;
      fprintf(stderr, "%s", "\n\n# - Used to include later 'LFlags' additions into 'LFlags_<mode>'" ) ;
      fprintf(stderr, "%s", "\nLFlags_Debug = -g $(lflags)\n" ) ;
      fprintf(stderr, "%s", "\n#$(lflags): " ) ;
      fprintf(stderr, "%s", "\n\n# - Useful to construct 'Target=' and 'Objectdir=' file paths -" ) ;
      fprintf(stderr, "%s", "\nObjectdir = bin/$(mode)" ) ;
      fprintf(stderr, "%s", "\nTarget    = bin/$(projectname).$(mode)\n" ) ;
      fprintf(stderr, "%s", "\n#$(mode): Name of current building mode (list of possible values is set by 'Modes'): " ) ;
      fprintf(stderr, "%s", "\n#         'Modes = Debug Release' => 'bin/Debug' , 'bin/Release'n" ) ;
      fprintf(stderr, "%s", "\n#$(projectname): Filename of project description without path and extension: " ) ;
      fprintf(stderr, "%s", "\n#                'path/name.ext' => 'name' " ) ;
      fprintf(stderr, "%s", "\n") ;
   }
   return 1 ;
}
