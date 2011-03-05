/* title: TextDB
   Implements main function and core logicof TextDB.
   TextDB is used to read from simple CSV files

   Textfile syntax:
   > # TEXT.DB (1.0)
   > # Comment
   > "column1-name", "column2-name", ...
   > "row1-col1-value", "row1-col2-value", ...
   > "row2-col1-value", "row2-col2-value", ...
   > ...

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
   (C) 2011 Jörg Seebohn

   file: C-kern/main/tools/textdb.c
    Main implementation file of <TextDB>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/filesystem/mmfile.h"

typedef struct select_parameter_t   select_parameter_t ;
typedef struct textdb_t             textdb_t ;
typedef struct textdb_column_t      textdb_column_t ;

static void print_err(const char* printf_format, ...) __attribute__ ((__format__ (__printf__, 1, 2))) ;

const char * g_programname = 0 ;
const char *  g_infilename = 0 ;
const char * g_outfilename = 0 ;

static void print_err(const char* printf_format, ...)
{
   va_list args ;
   va_start( args, printf_format ) ;
   dprintf( STDERR_FILENO, "%s: error: ", g_programname) ;
   vdprintf( STDERR_FILENO, printf_format, args) ;
   dprintf( STDERR_FILENO, "\n") ;
   va_end( args ) ;
}

static int process_arguments(int argc, const char * argv[])
{
   g_programname = argv[0] ;

   if (argc < 2) {
      return 1 ;
   }

   g_infilename = argv[argc-1] ;

   for(int i = 1; i < (argc-1); ++i) {
      if (     argv[i][0] != '-'
            || argv[i][1] == 0
            || argv[i][2] != 0 ) {
         return 1 ;
      }
      switch(argv[i][1]) {
      case 'o':
         if (i == (argc-2)) {
            return 1 ;
         }
         g_outfilename = argv[++i] ;
         break ;
      }
   }

   return 0 ;
}

static int write_file( int outfd, const char * write_start, size_t write_size )
{
   if (write_size != (size_t) write( outfd, write_start, write_size )) {
      if (g_outfilename) {
         print_err( "Can not write file '%s': %s", g_outfilename, strerror(errno) ) ;
      } else {
         print_err( "Can not write output: %s", strerror(errno) ) ;
      }
      return 1 ;
   }
   return 0 ;
}

struct textdb_column_t
{
   char *   value ;
   size_t   length ;
   size_t   allocated_memory_size ;
} ;

static int clearvalue_textdbcolumn(textdb_column_t * column)
{
   column->length = 0 ;
   return 0 ;
}

static int appendvalue_textdbcolumn(textdb_column_t * column, const char * str, size_t str_len)
{
   if (column->allocated_memory_size - column->length <= str_len) {
      column->allocated_memory_size += (column->allocated_memory_size > str_len) ? column->allocated_memory_size : str_len + 1 ;
      void * temp = realloc( column->value, column->allocated_memory_size ) ;
      if (!temp) {
         print_err("Out of memory") ;
         goto ABBRUCH ;
      }
      column->value = (char*) temp ;
   }
   memcpy( column->value + column->length, str, str_len) ;
   column->length += str_len ;
   column->value[column->length] = 0 ;
   return 0 ;
ABBRUCH:
   return 1 ;
}

static int free_textdbcolumn(textdb_column_t * column)
{
   free(column->value) ;
   column->value                 = 0 ;
   column->length                = 0 ;
   column->allocated_memory_size = 0 ;
   return 0 ;
}

struct textdb_t
{
   mmfile_t        input_file ;
   size_t          column_count ;
   textdb_column_t * header/*[column_count]*/ ;
   textdb_column_t * data/*[column_count]*/ ;
   char            * filename ;
   size_t          scanned_offset ;
   size_t          line_number ;
} ;

#define textdb_INIT_FREEABLE  { mmfile_INIT_FREEABLE, 0, 0, 0, 0, 0, 0 }

static int alloccolumns_textdb(textdb_t * txtdb)
{
   char * next_char = (char*) memstart_mmfile( &txtdb->input_file ) ;
   size_t input_len = sizeinbytes_mmfile( &txtdb->input_file ) ;

   while( input_len ) {
      if ('#' == *next_char) {
         // ignore comments
         while( input_len ) {
            if ('\n' == *next_char) {
               break ;
            }
            -- input_len ;
            ++ next_char ;
         }
      } else if (' ' != *next_char && '\t' != *next_char && '\n' != *next_char ) {
         // found non white space
         break ;
      }
      -- input_len ;
      ++ next_char ;
   }

   if (!input_len) {
      print_err( "Expected text line with header names in textdb file '%s'", txtdb->filename ) ;
      goto ABBRUCH ;
   }

   size_t column_count = 1 ;
   bool   isExpectData = true ;
   while( input_len ) {
      const char quotesign = *next_char ;
      -- input_len ;
      ++ next_char ;
      if ('\n' == quotesign && !isExpectData) {
         break ;
      }
      if (' ' == quotesign || '\t' == quotesign) {
         continue ;
      }
      if (',' == quotesign) {
         isExpectData = true ;
         ++ column_count ;
         continue ;
      }
      if ('\'' != quotesign && '\"' != quotesign) {
         print_err( "Expected ' or \" as first data character in textdb file '%s'", txtdb->filename ) ;
         goto ABBRUCH ;
      }
      isExpectData = false ;

      while( input_len ) {
         if (quotesign == *next_char) {
            break ;
         }
         if ('\n' == *next_char) {
            break ;
         }
         -- input_len ;
         ++ next_char ;
      }
      if (!input_len || quotesign != *next_char) {
         print_err( "Expected closing %c in first data line in textdb file '%s'", quotesign, txtdb->filename ) ;
         goto ABBRUCH ;
      }
      -- input_len ;
      ++ next_char ;
   }

   assert(!txtdb->header) ;
   assert(!txtdb->data) ;
   size_t memsize = sizeof(textdb_column_t) * column_count ;
   txtdb->header  = (textdb_column_t*) malloc( memsize ) ;
   txtdb->data    = (textdb_column_t*) malloc( memsize ) ;
   if (!txtdb->header || !txtdb->data) {
      print_err("Out of memory") ;
      goto ABBRUCH ;
   }
   memset( txtdb->header, 0, memsize ) ;
   memset( txtdb->data, 0, memsize ) ;
   txtdb->column_count = column_count ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

static int readrow_textdb( textdb_t * txtdb, textdb_column_t * columns )
{
   int err ;
   char * next_char = (char*) memstart_mmfile( &txtdb->input_file ) ;
   size_t input_len = sizeinbytes_mmfile( &txtdb->input_file ) ;

   input_len -= txtdb->scanned_offset ;
   next_char += txtdb->scanned_offset ;

   for(size_t i = 0; i < txtdb->column_count; ++i) {
      err = clearvalue_textdbcolumn( &columns[i] ) ;
      if (err) goto ABBRUCH ;
   }

   while( input_len ) {
      if (' ' == *next_char || '\t' == *next_char || '\n' == *next_char ) {
         // ignore white space
         -- input_len ;
         ++ next_char ;
         if ('\n' == *next_char) {
            ++ txtdb->line_number ;
         }
      } else if ('#' == *next_char) {
         // ignore comment
         while( input_len ) {
            if ('\n' == *next_char) {
               break ;
            }
            -- input_len ;
            ++ next_char ;
         }
      } else {
         break ;
      }
   }

   if (!input_len) {
      txtdb->scanned_offset = sizeinbytes_mmfile( &txtdb->input_file ) ;
      return ENODATA ;
   }

   size_t column_index = 0 ;
   bool   isExpectData = true ;

   while( input_len ) {
      const char quotesign = *next_char ;
      -- input_len ;
      ++ next_char ;
      if ('\n' == quotesign) {
         if ( isExpectData || (column_index + 1) != txtdb->column_count) {
            print_err( "Read only %d columns but expected %d in textdb file '%s' in line: %d", column_index, txtdb->column_count, txtdb->filename, txtdb->line_number) ;
            goto ABBRUCH ;
         }
         ++ txtdb->line_number ;
         break ;
      }
      if (' ' == quotesign || '\t' == quotesign) {
         continue ;
      }
      if (',' == quotesign) {
         isExpectData = true ;
         ++ column_index ;
         if (column_index >= txtdb->column_count) {
            print_err( "Read more than %d columns in textdb file '%s' in line: %d", txtdb->column_count, txtdb->filename, txtdb->line_number) ;
            goto ABBRUCH ;
         }
         continue ;
      }
      if ('\'' != quotesign && '\"' != quotesign) {
         print_err( "Expected ' or \" as quote sign in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
         goto ABBRUCH ;
      }
      isExpectData = false ;
      char * str = next_char ;
      while( input_len ) {
         if (quotesign == *next_char) {
            break ;
         }
         if ('\n' == *next_char) {
            break ;
         }
         -- input_len ;
         ++ next_char ;
      }
      if (!input_len || quotesign != *next_char) {
         print_err( "Expected closing %c in in textdb file '%s' in line: %d", quotesign, txtdb->filename, txtdb->line_number) ;
         goto ABBRUCH ;
      }
      size_t str_len = (size_t) (next_char - str) ;
      -- input_len ;
      ++ next_char ;
      err = appendvalue_textdbcolumn( &columns[column_index], str, str_len) ;
      if (err) goto ABBRUCH ;
   }

   if ( isExpectData || (column_index + 1) != txtdb->column_count) {
      print_err( "Read only %d columns but expected %d in textdb file '%s' in line: %d", column_index, txtdb->column_count, txtdb->filename, txtdb->line_number) ;
      goto ABBRUCH ;
   }

   txtdb->scanned_offset = sizeinbytes_mmfile( &txtdb->input_file ) - input_len ;
   return 0 ;
ABBRUCH:
   return 1 ;
}

static int free_textdb(textdb_t * txtdb)
{
   int err = 0 ;
   int err2 ;

   for(size_t i = 0 ; i < txtdb->column_count; ++i) {
      err2 = free_textdbcolumn(&txtdb->header[i]) ;
      if (err2) err = err2 ;
      err2 = free_textdbcolumn(&txtdb->data[i]) ;
      if (err2) err = err2 ;
   }

   free(txtdb->header) ;
   free(txtdb->data) ;
   txtdb->header = 0 ;
   txtdb->data   = 0 ;
   txtdb->column_count = 0 ;

   err2 = free_mmfile(&txtdb->input_file) ;
   if (err2) err = err2 ;

   if (err) goto ABBRUCH ;
   return 0 ;
ABBRUCH:
   return err ;
}

static int init_textdb(/*out*/textdb_t * result, const char * filename)
{
   int err ;
   textdb_t  txtdb = textdb_INIT_FREEABLE ;

   txtdb.line_number = 1 ;
   txtdb.filename    = strdup(filename) ;
   if (!txtdb.filename) {
      print_err( "Out of memory" ) ;
      goto ABBRUCH ;
   }

   err = init_mmfile( &txtdb.input_file, filename, 0, 0, 0, mmfile_openmode_RDONLY) ;
   if (err) {
      print_err( "Can not open textdb file '%s' for reading: %s", filename, strerror(err) ) ;
      goto ABBRUCH ;
   }

   err = alloccolumns_textdb( &txtdb ) ;
   if (err) goto ABBRUCH ;

   err = readrow_textdb( &txtdb, txtdb.header ) ;
   if (err) {
      if (ENODATA == err) {
         print_err( "Expected text line with header names in textdb file '%s'", txtdb.filename ) ;
      }
      goto ABBRUCH ;
   }

   memcpy( result, &txtdb, sizeof(txtdb) ) ;
   return 0 ;
ABBRUCH:
   free_textdb( &txtdb ) ;
   return 1 ;
}

struct select_parameter_t
{
   select_parameter_t * next ;
   const char *   value ;
   size_t         length ;
   size_t         col_index ;
   bool           isString ;
} ;

static int free_select_parameter(select_parameter_t ** param)
{
   select_parameter_t * next = *param ;
   *param = NULL ;
   while( next ) {
      select_parameter_t * prev = next ;
      next = next->next ;
      free(prev) ;
   }
   return 0 ;
}

static int extend_select_parameter(/*inout*/select_parameter_t ** next_param)
{
   select_parameter_t * new_param = MALLOC(select_parameter_t,malloc,) ;

   if (!new_param) {
      print_err("Out of memory") ;
      goto ABBRUCH ;
   }

   if (!(*next_param)) {
      *next_param = new_param ;
   } else {
      (*next_param)->next = new_param ;
      *next_param = new_param ;
   }

   new_param->next   = 0 ;
   new_param->value  = "" ;
   new_param->length = 0 ;
   new_param->col_index = 0 ;
   new_param->isString  = false ;

   return 0 ;
ABBRUCH:
   return 1 ;
}

static int parse_select_parameter(/*out*/select_parameter_t ** param, /*out*/char ** end_param, char * start_param, char * end_macro, size_t start_linenr, const char * cmd )
{
   int err ;
   select_parameter_t first_param = { .next = 0 } ;
   select_parameter_t  * next_param = &first_param ;

   assert(0 == *param) ;

   while( start_param < end_macro && ' ' == start_param[0] ) {
      ++ start_param ;
   }

   if (  start_param >= end_macro
         || '(' != start_param[0] ) {
      print_err( "Expected '(' after %s in line: %d", cmd, start_linenr ) ;
      goto ABBRUCH ;
   }

   char * next = start_param ;

   while( next < end_macro ) {
      ++ next ;
      while(   next < end_macro
            && ' ' == next[0]) {
         ++ next ;
      }
      const char * start_field = next ;
      while(   next < end_macro
            && ' ' != next[0]
            && ')' != next[0]
            && '"' != next[0]
            && '\'' != next[0]
            ) {
         ++ next ;
      }
      size_t field_len = (size_t) (next - start_field) ;
      if (field_len) {
         err = extend_select_parameter(&next_param) ;
         if (err) goto ABBRUCH ;
         next_param->value    = start_field ;
         next_param->length   = field_len ;
      }
      if (next < end_macro && ' ' != next[0]) {
         const char * start_string = next + 1 ;
         if ('"' == next[0]) {
            do {
               ++ next ;
            } while( next < end_macro && '"' != next[0]) ;
            if (next >= end_macro) {
               print_err( "Expected '\"' after %s(...\" in line: %d", cmd, start_linenr ) ;
               goto ABBRUCH ;
            }
         } else if ('\'' == next[0]) {
            do {
               ++ next ;
            } while( next < end_macro && '\'' != next[0]) ;
            if (next >= end_macro) {
               print_err( "Expected \"'\" after %s(...' in line: %d", cmd, start_linenr ) ;
               goto ABBRUCH ;
            }
         } else {
            *end_param = next ;
            break ;
         }

         size_t string_len = (size_t) (next - start_string) ;

         err = extend_select_parameter(&next_param) ;
         if (err) goto ABBRUCH ;

         next_param->value    = start_string ;
         next_param->length   = string_len ;
         next_param->isString = true ;
      }
   }

   if ( next >= end_macro ) {
      print_err( "Expected ')' after %s( in line: %d", cmd, start_linenr ) ;
      goto ABBRUCH ;
   }

   *param = first_param.next ;
   return 0 ;
ABBRUCH:
   free_select_parameter(&first_param.next) ;
   return 1 ;
}

static int tostring_select_parameter(const select_parameter_t * param, /*out*/char ** string )
{
   char * buffer = 0 ;
   size_t length = 0 ;

   for( const select_parameter_t * p = param; p; p = p->next) {
      length += p->length ;
   }

   buffer= malloc(length+1) ;
   if (!buffer) {
      print_err("Out of memory") ;
      goto ABBRUCH ;
   }

   size_t offset = 0 ;
   for( const select_parameter_t * p = param; p; p = p->next) {
      memcpy( &buffer[offset], p->value, p->length ) ;
      offset += p->length ;
   }
   assert( offset == length ) ;

   buffer[length] = 0 ;
   *string = buffer ;
   return 0 ;
ABBRUCH:
   return 1 ;
}

static int process_selectcmd( char * start_macro, char * end_macro, int outfd, size_t start_linenr )
{
   int err ;
   select_parameter_t  * select_param = 0 ;
   select_parameter_t    * from_param = 0 ;
   textdb_t                    dbfile = textdb_INIT_FREEABLE ;
   char    * filename = 0 ;
   char * start_param = start_macro + sizeof("// TEXTDB:SELECT")-1 ;
   char   * end_param = start_param ;

   err = parse_select_parameter( &select_param, &end_param, start_param, end_macro, start_linenr, "SELECT" ) ;
   if (err) goto ABBRUCH ;

   char * start_from = end_param + 1 ;
   while( start_from < end_macro && ' ' == start_from[0]) {
      ++ start_from ;
   }

   if (     (end_macro - start_from) < 4
         || strncmp( start_from, "FROM", 4) ) {
      print_err( "Expected 'FROM' after SELECT() in line: %d", start_linenr ) ;
      goto ABBRUCH ;
   }

   err = parse_select_parameter( &from_param, &end_param, start_from+4, end_macro, start_linenr, "FROM" ) ;
   if (err) goto ABBRUCH ;

   ++ end_param ;
   while( end_param < end_macro && ' ' == end_param[0] ) {
      ++ end_param ;
   }

   if (end_param < end_macro) {
      print_err( "Expected nothing after SELECT()FROM() in line: %d", start_linenr ) ;
      goto ABBRUCH ;
   }

   err = tostring_select_parameter( from_param, &filename ) ;
   if (err) goto ABBRUCH ;

   // open text database and parse header (CSV format)
   err = init_textdb( &dbfile, filename ) ;
   if (err) goto ABBRUCH ;

   // match select parameter to header of textdb
   for( select_parameter_t * p = select_param; p; p = p->next) {
      if (!p->isString) {
         // search matching header
         bool isMatch = false ;
         for(size_t i = 0; i < dbfile.column_count ; ++i) {
            if (     p->length == dbfile.header[i].length
                  && 0 == strncmp(dbfile.header[i].value, p->value, p->length) ) {
               p->col_index = i ;
               isMatch = true ;
               break ;
            }
         }
         if (!isMatch) {
            print_err( "Unknown column name '%.*s' in SELECT()FROM() in line: %d", p->length, p->value, start_linenr ) ;
            goto ABBRUCH ;
         }
      }
   }

   // write for every read row the values of the concatenated select parameters as one line of output
   for(;;) {
      err = readrow_textdb( &dbfile, dbfile.data ) ;
      if (err == ENODATA) break ;
      if (err) goto ABBRUCH ;
      for( const select_parameter_t * p = select_param; p; p = p->next) {
         if (p->isString) {
            err = write_file( outfd, p->value, p->length ) ;
            if (err) goto ABBRUCH ;
         } else {
            err = write_file( outfd, dbfile.data[p->col_index].value, dbfile.data[p->col_index].length ) ;
            if (err) goto ABBRUCH ;
         }
      }
      err = write_file( outfd, "\n", 1 ) ;
      if (err) goto ABBRUCH ;
   }


   free_textdb( &dbfile ) ;
   free(filename) ;
   free_select_parameter(&select_param) ;
   free_select_parameter(&from_param) ;
   return 0 ;
ABBRUCH:
   free_textdb( &dbfile ) ;
   free(filename) ;
   free_select_parameter(&select_param) ;
   free_select_parameter(&from_param) ;
   return 1 ;
}

static int find_macro(/*out*/char ** start_pos, /*out*/char ** end_pos, /*inout*/size_t * line_number, char * next_input, size_t input_size)
{
   for( ; input_size; --input_size, ++next_input) {
      if ('\n' == *next_input) {
         ++ (*line_number) ;
         if (     input_size > 10
               && 0 == strncmp(&next_input[1], "// TEXTDB:", 10) ) {
            *start_pos  = next_input + 1 ;
            input_size -= 11 ;
            next_input += 11 ;
            for( ; input_size; --input_size, ++next_input) {
               if ('\n' == *next_input) {
                  break ;
               }
            }
            *end_pos = next_input ;
            return 0 ;
         }
      }
   }

   return 1 ;
}

static int process_macro(const mmfile_t * input_file, int outfd)
{
   int err ;
   char    * next_input = (char*) memstart_mmfile(input_file) ;
   size_t    input_size = sizeinbytes_mmfile(input_file) ;
   size_t   line_number = 1 ;

   while(input_size) {
      // find start "// TEXTDB:SELECT..."
      size_t  start_linenr ;
      char   * start_macro ;
      char     * end_macro ;
      err = find_macro(&start_macro, &end_macro, &line_number, next_input, input_size) ;
      if (err) break ;
      size_t write_size = (size_t) (end_macro - next_input) ;
      err = write_file( outfd, next_input, write_size ) ;
      if (err) goto ABBRUCH ;
      err = write_file( outfd, "\n", 1) ;
      if (err) goto ABBRUCH ;
      start_linenr = line_number ;
      input_size  -= write_size ;
      next_input   = end_macro ;

      if (     13 == end_macro - start_macro
            && strncmp(start_macro, "// TEXTDB:END", 13)) {
         print_err( "Found end of macro '// TEXTDB:END' with a beginning macro" ) ;
         goto ABBRUCH ;
      }

      // find end "// TEXTDB:END"
      char   * start_macro2 ;
      char     * end_macro2 ;
      err = find_macro(&start_macro2, &end_macro2, &line_number, next_input, input_size) ;
      if (     err
            || strncmp( start_macro2, "// TEXTDB:END", (size_t) (end_macro2 - start_macro2)) ) {
         print_err( "Can not find end of macro '// TEXTDB:END'" ) ;
         goto ABBRUCH ;
      }
      input_size -= (size_t) (end_macro2 - next_input) ;
      next_input  = end_macro2 ;

      // process TEXTDB:command
      size_t macro_size = (size_t) (end_macro - start_macro) ;
      if (  macro_size > sizeof("// TEXTDB:SELECT")
            && (0 == strncmp( start_macro, "// TEXTDB:SELECT", sizeof("// TEXTDB:SELECT")-1)) ) {
         err = process_selectcmd( start_macro, end_macro, outfd, start_linenr) ;
         if (err) goto ABBRUCH ;
      } else  {
         print_err( "Unknown macro '%.*s' in line: %"PRIuSIZE, macro_size > 16 ? 16 : macro_size, start_macro, start_linenr) ;
         goto ABBRUCH ;
      }

      err = write_file( outfd, "// TEXTDB:END", sizeof("// TEXTDB:END")-1) ;
      if (err) goto ABBRUCH ;
   }

   if ( input_size ) {
      err = write_file( outfd, next_input, input_size ) ;
      if (err) goto ABBRUCH ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

int main(int argc, const char * argv[])
{
   int err ;
   int           outfd = -1 ;
   mmfile_t input_file = mmfile_INIT_FREEABLE ;

   err = init_process_umgebung(umgebung_type_DEFAULT) ;
   if (err) goto ABBRUCH ;

   err = process_arguments(argc, argv) ;
   if (err) goto PRINT_USAGE ;

   // create out file if -o <filename> is specified
   if (g_outfilename) {
      outfd = open(g_outfilename, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR) ;
      if (outfd == -1) {
         print_err( "Can not create file '%s' for writing: %s", g_outfilename, strerror(errno) ) ;
         goto ABBRUCH ;
      }
   } else {
      outfd = STDOUT_FILENO ;
   }

   // open input file for reading
   err = init_mmfile(&input_file, g_infilename, 0, 0, 0, mmfile_openmode_RDONLY) ;
   if (err) {
      print_err( "Can not open file '%s' for reading: %s", g_infilename, strerror(err)) ;
      goto ABBRUCH ;
   }

   // parse input => expand macros => write output
   err = process_macro( &input_file, outfd ) ;
   if (err) goto ABBRUCH ;

   // flush output to disk
   if (g_outfilename) {
      err = close(outfd) ;
      if (err) {
         print_err( "Can not write file '%s': %s", g_outfilename, strerror(errno) ) ;
         goto ABBRUCH ;
      }
   }

   free_process_umgebung() ;
   return 0 ;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "TextDB version 0.1 - Copyright (c) 2011 Joerg Seebohn\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* TextDB is a textdb macro preprocessor.\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* It reads a C source file and translates it into a C source file\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* whereupon textdb macros are expanded.\n" ) ;
   dprintf(STDERR_FILENO, "\nUsage: %s -o <out.c> [OPTIONS] <in.c>\n\n", g_programname ) ;
ABBRUCH:
   if (g_outfilename && outfd != -1) {
      close(outfd) ;
      unlink(g_outfilename) ;
   }
   free_mmfile(&input_file) ;
   free_process_umgebung() ;
   return 1 ;
}
