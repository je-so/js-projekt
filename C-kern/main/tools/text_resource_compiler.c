/*
   Makefile Generator: C-kern/main/tools/text_resource_compiler.c
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

const char * g_programname = 0 ;

#ifdef __GNUC__
static void print_err(const char* error_description_format, /*arguments*/ ...) __attribute__ ((__format__ (__printf__, 1, 2))) ;
#endif

static void print_err(const char* error_description_format, /*arguments*/ ...)
{
   va_list error_description_args ;
   va_start( error_description_args, error_description_format ) ;
   fprintf( stderr, "%s: error: ", g_programname) ;
   vfprintf( stderr, error_description_format, error_description_args) ;
   va_end( error_description_args ) ;
}

typedef struct textline textline_t ;
struct textline {
   textline_t * next ;
   size_t len ;
   char   text[] ;
} ;

static void free_textline( textline_t ** result)
{
   if (*result) {
      free(*result) ;
      *result = 0 ;
   }
}

static int new_textline( textline_t ** result, const char * text, size_t len)
{
   textline_t * textline = (textline_t*) malloc( sizeof(textline_t) + len + 1 ) ;
   if (!textline) {
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }

   textline->next = 0 ;
   textline->len  = len ;
   strncpy( textline->text, text, len) ;
   textline->text[len] = 0 ;

   *result = textline ;
   return 0 ;
}

typedef struct textresource textresource_t ;
struct textresource {
   char    * name ;
   size_t  nameLen ;
   textresource_t * next ;
   textresource_t * nextLang ;
   textline_t * text ;
   uint16_t     paramCount ;
   char         ** parameter ;
   char         langCode[2+1] ;
} ;

static void free_textresource(textresource_t ** result)
{
   if (*result) {
      while( (*result)->text ) {
         textline_t * text = (*result)->text ;
         (*result)->text   = text->next ;
         free_textline(&text) ;
      }
      free(*result) ;
      *result = 0 ;
   }
}

static int new_textresource(
   textresource_t ** result,
   const char * name, size_t len,
   const char * langCode,
   size_t paramCount, char ** parameter, size_t * parameterLen )
{
   size_t paramLen = 0 ;
   for(size_t i = 0; i < paramCount; ++i) {
      paramLen += parameterLen[i] ;
   }

   textresource_t * textentry = 0 ;
   if (paramCount < (uint16_t)-1) {
      textentry = (textresource_t*) malloc(  sizeof(textresource_t) + paramCount * (1+sizeof(char*))
                                             + paramLen + len + 3/*name extended with langcode*/) ;
   }
   if (!textentry) {
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }
   memset( textentry, 0, sizeof(*textentry)) ;
   char * string = (char*) (paramCount + ((char**) (&textentry[1]))) ;
   textentry->name = string ;
   textentry->nameLen = len ;
   textentry->paramCount = (uint16_t) paramCount ;
   textentry->parameter = (char**) (&textentry[1]) ;
   strncpy( textentry->langCode, langCode, 2 ) ;
   strncpy( string, name, len) ;
   strncpy( string+len, langCode, 2 ) ; // support same ID with different lang codes
   string[len+2] = 0 ;
   string += 3 + len ;
   for(size_t i = 0; i < paramCount; ++i) {
      textentry->parameter[i] = string ;
      strncpy( string, parameter[i], parameterLen[i]) ;
      string[parameterLen[i]] = 0 ;
      string += 1 + parameterLen[i] ;
   }
   assert( string == ((char*)(&textentry[1]) + paramCount*(1+sizeof(char*)) + paramLen + len + 3) ) ;

   *result = textentry ;
   return 0 ;
}

static void free_hashtable_textentry( hash_entry_t* entry )
{
   assert( entry->name  == ((textresource_t*)entry->data)->name ) ;
   free_textresource( (textresource_t**)&entry->data ) ;
   free( entry ) ;
}

static void free_hashtable_langentry( hash_entry_t* entry )
{
   free( entry ) ;
}

static int new_hash_entry(hash_table_t* table, char * name, void * data)
{
   hash_entry_t * entry = (hash_entry_t*) malloc(sizeof(hash_entry_t)) ;
   if (!entry) {
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }

   memset(entry, 0, sizeof(*entry)) ;
   entry->name = name ;
   entry->data = data ;

   if (HASH_OK != insert_hashtable(table, entry)) {
      free(entry) ;
      print_err( "Internal error(%d)!\n", __LINE__ ) ;
      return 1 ;
   }

   return 0 ;
}


typedef struct resourcefile resourcefile_t ;
struct resourcefile {
   char * filename ;
   textresource_t * first ;
   textresource_t * last ;
   textresource_t * lastLang ;
   hash_table_t   * textentries ;
   hash_table_t   * langentries ;
} ;

static void free_resourcefile(resourcefile_t ** resfile)
{
   resourcefile_t * ptr = resfile ? *resfile : (resourcefile_t*)0 ;
   if (ptr) {
      free_hashtable(&ptr->textentries) ;
      free_hashtable(&ptr->langentries) ;
      free(ptr) ;
      *resfile = 0 ;
   }
}

static int new_resourcefile(resourcefile_t ** result, const char * filename)
{
   resourcefile_t * resfile ;

   resfile = (resourcefile_t*) malloc(sizeof(resourcefile_t) + strlen(filename) + 1) ;
   if (!resfile) {
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }

   memset( resfile, 0, sizeof(*resfile)) ;
   resfile->filename = (char*) (&resfile[1]) ;
   strcpy(resfile->filename, filename) ;

   if (  new_hashtable(&resfile->textentries, 65535, &free_hashtable_textentry)
         || new_hashtable(&resfile->langentries, 1023, &free_hashtable_langentry)) {
      free_resourcefile(&resfile) ;
      print_err( "Out of memory!\n" ) ;
      return 1 ;
   }

   *result = resfile ;
   return 0 ;
}

static int add_textresource(
   resourcefile_t * resfile,
   textresource_t ** result,
   size_t lineNr,
   const char * name, size_t len,
   const char * langCode,
   size_t paramCount, char ** parameter, size_t * parameterLen )
{
   hash_entry_t * hashentry ;
   bool isNewLang = (HASH_OK!=search_hashtable2(resfile->langentries, langCode, 2, &hashentry)) ;

   textresource_t * textentry ;
   if (new_textresource(&textentry, name, len, langCode, paramCount, parameter, parameterLen)) {
      return 1 ;
   }

   if (HASH_OK==search_hashtable(resfile->textentries, textentry->name, &hashentry)) {
      free_textresource(&textentry) ;
      print_err( "%s:%d: ID '%.*s' already used\n", resfile->filename, lineNr, len, name) ;
      return 1 ;
   }

   if (new_hash_entry(resfile->textentries, textentry->name, textentry)) {
      free_textresource(&textentry) ;
      return 1 ;
   }

   if (isNewLang) {
      if (new_hash_entry(resfile->langentries, textentry->langCode, 0)) return 1 ;
      if (resfile->lastLang) resfile->lastLang->nextLang = textentry ;
      resfile->lastLang = textentry ;
   }

   if (!resfile->first) resfile->first = textentry ;
   if (resfile->last) resfile->last->next = textentry ;
   resfile->last = textentry ;

   *result = textentry ;
   return 0 ;
}

typedef struct resourceparser resourceparser_t ;
struct resourceparser
{
   char    *linebuffer ;
   size_t  lineNr ;
   char    *nextchar ;
   int     (*state) (resourceparser_t *) ;
   textresource_t * textentry ;
   resourcefile_t * resourcefile ;
} ;

static int parse_idandparameter(resourceparser_t * state) ;
static int parse_textstring(resourceparser_t * parser) ;

static void free_resourceparser(resourceparser_t * parser)
{
   if (parser->linebuffer)
   {
      parser->linebuffer = 0 ;
   }
}

int init_resourceparser(resourceparser_t * result, char * linebuffer, resourcefile_t * resourcefile)
{
   if (!result || !linebuffer || !resourcefile) {
      print_err( "Internal error(%d)!\n", __LINE__ ) ;
      return 1 ;
   }
   memset( result, 0, sizeof(*result)) ;
   result->linebuffer = linebuffer ;
   result->state = &parse_idandparameter ;
   result->resourcefile = resourcefile ;
   return 0 ;
}

static int parse_idandparameter(resourceparser_t * parser)
{
   assert(parser) ;
   char * nextchar = parser->nextchar ;
   char * beginID = nextchar ;

   bool isValidChar =
      (  *nextchar == '_'
         || ('A' <= *nextchar && *nextchar <= 'Z')
         || ('a' <= *nextchar && *nextchar <= 'z') ) ;

   while (isValidChar) {
      if (!*(++nextchar)) break ;
      if (':' == *nextchar || ' ' == *nextchar || '\t' == *nextchar) break ;
      isValidChar =
         (  *nextchar == '_'
            || ('0' <= *nextchar && *nextchar <= '9')
            || ('A' <= *nextchar && *nextchar <= 'Z')
            || ('a' <= *nextchar && *nextchar <= 'z') ) ;
   }

   if (!isValidChar) {
      print_err( "%s:%d: ID '%.*s' contains invalid character", parser->resourcefile->filename, parser->lineNr, 1+nextchar-beginID, beginID ) ;
      return 1 ;
   }

   assert(nextchar > beginID) ;
   size_t lenID = (size_t)(nextchar - beginID) ; // points one character after the ID => (endID - beginID) == ID-len
   while (' ' == *nextchar || '\t' == *nextchar) ++nextchar ;
   if (':' != *nextchar) {
      if (*nextchar)
         print_err( "%s:%d: expected ':' after '%.*s' and not '%c'\n", parser->resourcefile->filename, parser->lineNr, lenID, beginID, *nextchar ) ;
      else
         print_err( "%s:%d: expected ':' after '%.*s' and not 'end of line'\n", parser->resourcefile->filename, parser->lineNr, lenID, beginID ) ;
      return 1 ;
   }

   ++nextchar ; while (' ' == *nextchar || '\t' == *nextchar) ++nextchar ;
   char * langCode = nextchar ;
   if (! (  'a' <= *langCode && *langCode <= 'z'
         && 'a' <= langCode[1] && langCode[1] <= 'z') ) {
      print_err( "%s:%d: expected 'two characters language code (de,en,...)' after '%.*s:'\n", parser->resourcefile->filename, parser->lineNr, lenID, beginID) ;
      return 1 ;
   }
   nextchar += 2 ; while (' ' == *nextchar || '\t' == *nextchar) ++nextchar ;
   if (':' != *nextchar) {
      if (*nextchar)
         print_err( "%s:%d: expected ':' after ':%.2s' and not '%c'\n", parser->resourcefile->filename, parser->lineNr, langCode, *nextchar ) ;
      else
         print_err( "%s:%d: expected ':' after ':%.2s' and not 'end of line'\n", parser->resourcefile->filename, parser->lineNr, langCode ) ;
      return 1 ;
   }
   ++nextchar ; while (' ' == *nextchar || '\t' == *nextchar) ++nextchar ;

   unsigned paramCount = 0 ;
   char * parameter[10] ;
   size_t parameterLen[10] ;
   while(*nextchar) {
      if (paramCount == sizeof(parameter)/sizeof(parameter[0])) {
         print_err( "%s:%d: compiler supports no more than 10 parameters\n", parser->resourcefile->filename, parser->lineNr) ;
         return 1 ;
      }
      parameter[paramCount] = nextchar ;
      isValidChar =
         (  *nextchar == '_'
            || ('A' <= *nextchar && *nextchar <= 'Z')
            || ('a' <= *nextchar && *nextchar <= 'z') ) ;

      while (isValidChar) {
         if (!*(++nextchar)) break ;
         if (',' == *nextchar || ' ' == *nextchar || '\t' == *nextchar) break ;
         isValidChar =
            (  *nextchar == '_'
               || ('0' <= *nextchar && *nextchar <= '9')
               || ('A' <= *nextchar && *nextchar <= 'Z')
               || ('a' <= *nextchar && *nextchar <= 'z') ) ;
      }
      assert( nextchar >= parameter[paramCount] ) ;
      parameterLen[paramCount] = (size_t) ( nextchar - parameter[paramCount] ) ;
      if (!isValidChar) {
         print_err( "%s:%d: parameter '%.*s' contains invalid character", parser->resourcefile->filename, parser->lineNr, 1+parameterLen[paramCount], parameter[paramCount] ) ;
         return 1 ;
      }
      while (' ' == *nextchar || '\t' == *nextchar) ++nextchar ;
      if (*nextchar && ',' != *nextchar) {
         print_err( "%s:%d: expected ',' after '%.*s' and not '%c'\n", parser->resourcefile->filename, parser->lineNr, parameterLen[paramCount], parameter[paramCount], *nextchar ) ;
         return 1 ;
      }
      ++ nextchar ; while (' ' == *nextchar || '\t' == *nextchar) ++nextchar ;
      ++paramCount ;
   }

   // now build new textresource

   textresource_t * textentry ;
   if (add_textresource(parser->resourcefile, &textentry, parser->lineNr, beginID, lenID, langCode, paramCount, parameter, parameterLen)) {
      return 1 ;
   }

   parser->textentry = textentry ;
   parser->state = &parse_textstring ;
   return 0 ;
}

static int parse_textstring(resourceparser_t * parser)
{
   assert(parser) ;
   char * nextchar = parser->nextchar ;

   if (*nextchar != '"') {
      if (!parser->textentry->text) {
         print_err( "%s:%d: expected at least on line of text beginning with '\"'\n", parser->resourcefile->filename, parser->lineNr ) ;
         return 1 ;
      }
      parser->state = &parse_idandparameter ;
      return parse_idandparameter(parser) ;
   }

   char * beginText ;
   for( beginText = ++nextchar ;  nextchar[0] && ('"' != nextchar[0] || '\\' == nextchar[-1]) ; ++nextchar) { }
   while (     (0 == strncmp( nextchar, "\"PRI", 4))
            || (0 == strncmp( nextchar, "\"SCN", 4))) {
      ++ nextchar ;
      while (  ('A' <= nextchar[0] && nextchar[0] <= 'Z')
               || ('a' <= nextchar[0] && nextchar[0] <= 'z')
               || ('0' <= nextchar[0] && nextchar[0] <= '9')
               || '_' == nextchar[0] ) {
         ++nextchar ;
      }
      if (*nextchar == '"') {
         for( ++nextchar ;  nextchar[0] && ('"' != nextchar[0] || '\\' == nextchar[-1]) ; ++nextchar) { }
      }
   }
   if ('"' != *nextchar) {
      if (*nextchar)
         print_err( "%s:%d: expected '\"' at end of string and not '%c'\n", parser->resourcefile->filename, parser->lineNr, *nextchar ) ;
      else
         print_err( "%s:%d: expected '\"' at end of string and not 'end of line'\n", parser->resourcefile->filename, parser->lineNr ) ;
      return 1 ;
   }
   assert( nextchar >= beginText ) ;
   size_t lenText = (size_t) (nextchar - beginText) ;
   ++nextchar ; while( ' ' == *nextchar || '\t' == *nextchar ) ++nextchar ;
   if ( '#' != *nextchar && *nextchar) {
      print_err( "%s:%d: read unexpected character '%c' after string\n", parser->resourcefile->filename, parser->lineNr, *nextchar ) ;
      return 1 ;
   }

   textline_t * textline ;
   if (new_textline(&textline, beginText, lenText)) {
      return 1 ;
   }

   textline_t * prev = parser->textentry->text ;
   if (prev) {
      while( prev->next ) prev = prev->next ;
      prev->next = textline ;
   } else {
      parser->textentry->text = textline ;
   }
   return 0 ;
}

static int parse_nextline(resourceparser_t * parser)
{
   assert( parser && (&parse_idandparameter  == parser->state || &parse_textstring  == parser->state) ) ;

   ++ parser->lineNr ;

   char * nextchar = parser->linebuffer ;
   while( ' ' == *nextchar || '\t' == *nextchar) { ++nextchar ; }
   if ( !*nextchar || '#' == *nextchar ) {
      // ignoring empty lines or comments
      return 0 ;
   }

   parser->nextchar = nextchar ;
   return parser->state(parser) ;
}

static int read_textresourcefile(resourcefile_t * resfile)
{
   int    err = 1 ;
   resourceparser_t parser ;

   char linebuffer[1000] ;
   if (init_resourceparser(&parser, linebuffer, resfile)) {
      return err ;
   }

   FILE * file = fopen(resfile->filename, "r") ;
   if (!file) {
      print_err( "Could not open '%s'\n: %s\n", resfile->filename, strerror(errno)) ;
      goto ABBRUCH ;
   }

   size_t lineNr = 0 ;
   while( !feof(file) && !ferror(file) ) {

      if (!fgets( linebuffer, sizeof(linebuffer), file)) break ; /*read error or eof occured */
      size_t len = strlen(linebuffer) ;
      ++ lineNr ;
      if (len == sizeof(linebuffer)-1 && linebuffer[len-1] != '\n') {
         print_err( "%s:%d: line too long\n", resfile->filename, lineNr ) ;
         goto ABBRUCH ;
      }
      if (len && linebuffer[len-1] == '\n') linebuffer[--len] = 0 ;

      if (parse_nextline(&parser)) {
         goto ABBRUCH ;
      }

   }

   if (ferror(file)) {
      print_err( "Cannot read file '%s'\n: %s\n", resfile->filename, strerror(ferror(file)) ) ;
      goto ABBRUCH ;
   }

   err = 0 ;
ABBRUCH:
   free_resourceparser(&parser) ;
   if (file) fclose(file) ;
   return err ;
}

static int isPreprocessorToken(const char * string)
{
   if (  !strcmp("__FILE__", string)
         || !strcmp("__FUNCTION__", string)
         || !strcmp("__LINE__", string) )
      return true ;
   else
      return false ;
}

int write_cheaderfile(resourcefile_t * resfile, const char * C_HeaderFilename, const char * switchlabel)
{
   assert(resfile && C_HeaderFilename && switchlabel) ;
   struct stat filestat ;
   int  err = 1 ;
   FILE * outfile = 0 ;
   char * header_define = strdup( C_HeaderFilename ) ;

   if (!header_define) {
      print_err( "Out of memory!\n" ) ;
      goto ABBRUCH ;
   }

   if (0==stat( C_HeaderFilename, &filestat)) {
      print_err( "File '%s' already exists.\n", C_HeaderFilename ) ;
      goto ABBRUCH ;
   }

   outfile = fopen( C_HeaderFilename, "w" ) ;
   if (!outfile) {
      print_err( "Can not open '%s'.\n: %s", C_HeaderFilename, strerror(errno) ) ;
      goto ABBRUCH ;
   }

   if (strrchr( header_define, '/' ) && strchr( strrchr( header_define, '/' )+1, '.'))
      *strchr( strrchr( header_define, '/' )+1, '.') = 0 ;
   else if (strchr( '.' == header_define[0] ? header_define+1:header_define, '.' ))
      *strchr( '.' == header_define[0] ? header_define+1:header_define, '.' ) = 0 ;
   size_t o = 0 ;
   for(size_t i = 0; header_define[i]; ++i) {
      if ('a' <= header_define[i] && header_define[i] <= 'z') {
         header_define[o++] = (char) ('A' + (header_define[i]-'a')) ;
      } else if ('A' <= header_define[i] && header_define[i] <= 'Z') {
         header_define[o++] = header_define[i] ;
      } else if ('/' == header_define[i]) {
         header_define[o++] = '_' ;
      }
   }
   header_define[o] = 0 ;

   fprintf( outfile, "%s", "/*\n * Generated from R(esource)TextCompiler\n */\n" ) ;
   fprintf( outfile, "#ifndef %s_HEADER\n#define %s_HEADER\n\n", header_define, header_define ) ;
   textresource_t * text = resfile->first ;
   if (text && text->nextLang) {
      for(size_t langindex = 1; text ; text = text->nextLang, ++langindex) {
         fprintf( outfile, "#define %s %d\n", text->langCode, langindex) ;
      }
      for( textresource_t * langStart = resfile->first ; langStart; langStart = langStart->nextLang) {
         fprintf( outfile, "%sif (%s == %s)\n", (langStart == resfile->first)?"#":"\n#el", switchlabel, langStart->langCode) ;
         for( text = langStart; text; text = text->next) {
            if (strcmp(text->langCode, langStart->langCode)) continue ; // skip wrong language
            fprintf( outfile, "#define %.*s", text->nameLen, text->name) ;
            bool isParam = false ;
            for( size_t i = 0 ; i < text->paramCount; ++i) {
               if (isPreprocessorToken(text->parameter[i]))
                  continue ;
               fprintf( outfile, "%s%s", (isParam?", ":"("),text->parameter[i] ) ;
               isParam = true ;
            }
            fprintf( outfile, "%s \\\n        \"", (isParam?")":"")) ;
            for( textline_t * line = text->text ; line; line = line->next) {
               fprintf( outfile, "%s\\n", line->text ) ;
            }
            fprintf( outfile, "%s", "\"" ) ;
            for( size_t i = 0 ; i < text->paramCount; ++i) {
               fprintf( outfile, ", %s", text->parameter[i] ) ;
            }
            fprintf( outfile, "%s", "\n" ) ;
         }
      }
      fprintf( outfile, "\n#else\n#error unsupported language\n#endif\n") ;
      for(text = resfile->first ; text ; text = text->nextLang) {
         fprintf( outfile, "#undef %s\n", text->langCode) ;
      }
   } else if (text) {
      for( text = resfile->first ; text; text = text->next) {
         fprintf( outfile, "#define %.*s", text->nameLen, text->name) ;
         bool isParam = false ;
         for( size_t i = 0 ; i < text->paramCount; ++i) {
            if (isPreprocessorToken(text->parameter[i]))
               continue ;
            fprintf( outfile, "%s%s", (isParam?", ":"("),text->parameter[i] ) ;
            isParam = true ;
         }
         fprintf( outfile, "%s \\\n        \"", (isParam?")":"")) ;
         for( textline_t * line = text->text ; line; line = line->next) {
            fprintf( outfile, "%s\\n", line->text) ;
         }
         fprintf( outfile, "%s", "\"" ) ;
         for( size_t i = 0 ; i < text->paramCount; ++i) {
            fprintf( outfile, ", %s", text->parameter[i] ) ;
         }
         fprintf( outfile, "%s", "\n" ) ;
      }
   }
   fprintf( outfile, "%s", "\n#endif\n" ) ;

   if (ferror(outfile)) {
      print_err( "Write error in file '%s'\n", C_HeaderFilename ) ;
      goto ABBRUCH ;
   }

   err = 0 ;
ABBRUCH:
   if (outfile) fclose(outfile) ;
   free(header_define) ;
   return err ;
}


int main(int argc, char* argv[])
{
   g_programname = strrchr(argv[0],'/') ? strrchr(argv[0],'/') + 1 : argv[0] ;

   #ifdef KONFIG_UNITTEST
   assert( 0 == unittest_hashtable() ) ;
   #endif

   if (argc < 2) goto PRINT_USAGE ;

   int isPrintHelp = (0==strcmp(argv[1], "-h")) ;
   if (isPrintHelp) goto PRINT_USAGE ;

   int currentArgIndex = 1 ;
   const char * C_HeaderFilename = 0 ;
   const char * switchlabel = "LANGUAGE" ;
   if (0==strcmp(argv[currentArgIndex], "-c")) {
      if (argc < currentArgIndex+3) goto PRINT_USAGE ;
      C_HeaderFilename = argv[++ currentArgIndex] ;
      ++ currentArgIndex ;
   }

   if (  0==strcmp(argv[currentArgIndex], "-s")
         || 0==strcmp(argv[currentArgIndex], "--switch")) {
      if (argc < currentArgIndex+3) goto PRINT_USAGE ;
      switchlabel = argv[++ currentArgIndex] ;
      ++ currentArgIndex ;
   }

   if (!C_HeaderFilename) goto PRINT_USAGE ;
   if (currentArgIndex+1 != argc) goto PRINT_USAGE ;

   int err ;
   for(err = 0; !err && currentArgIndex < argc; ++currentArgIndex) {
      resourcefile_t * resfile = 0 ;

      if (new_resourcefile(&resfile, argv[currentArgIndex])) {
         err = 1 ;
      } else if (read_textresourcefile(resfile)) {
         err = 1 ;
      } else if (write_cheaderfile(resfile, C_HeaderFilename, switchlabel)) {
         err = 1 ;
      }

      free_resourcefile(&resfile) ;
   }

   return err ;

PRINT_USAGE:
   fprintf(stderr, "ResourceTextCompiler version 0.1; Copyright (C) 2010 Joerg Seebohn\n" ) ;
   fprintf(stderr, "\nUsage(1): %s -c <C-header.h> [-s <label>] <resource.text>\n\n", g_programname ) ;
   fprintf(stderr, "%s", "Generates a C header file whereas a text entry in <resource.text> is represented as a define.\n") ;
   fprintf(stderr, "%s", "-s is used to set the name of the define which is used to configure the language\n") ;
   fprintf(stderr, "%s", "during compile time (#if (<label> == en) ... #elif (<label> == de) ...).\n") ;
   fprintf(stderr, "%s", "LANGUAGE is used as default value if no value is set.\n") ;

   if (isPrintHelp) {
      // fprintf(stderr, "%s", "\nSyntax of text resource file\n" ) ;
      // fprintf(stderr, "%s", "TODO\n" ) ;
   }

   return 1;
}
