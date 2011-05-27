/* title: TextDB
   Implements main function and core logic of TextDB.
   TextDB is used to read from simple CSV files and
   patch the content into C source code files.

   Textfile syntax:
   The values must be enclosed in quotes. Use either " or '.
   It is possible to list more than one value for one column.
   They are concatenated and this is useful if a " or ' is in the value.
   > # TEXT.DB (1.0)
   > # Comment
   > "column1-name", "column2-name", ...
   > # The following line shows how to add a "
   > # to the value in the second column
   > "row1-col1-value", "row1-col2-value" '"', ...
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

typedef struct expression_t         expression_t ;
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

static void skip_space( char ** current_pos, char * end_pos )
{
   char * next = *current_pos ;
   while(   next < end_pos
         && ' ' == next[0]) {
      ++ next ;
   }
   *current_pos = next ;
}

struct textdb_column_t
{
   char *   value ;
   size_t   length ;
   size_t   allocated_memory_size ;
} ;

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
   return ENOMEM ;
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
   size_t          row_count ;
   size_t          column_count ;
   textdb_column_t * rows  /*[row_count][column_count]*/ ;
   char            * filename ;
   size_t          line_number ;
} ;

#define textdb_INIT_FREEABLE  { mmfile_INIT_FREEABLE, 0, 0, 0, 0, 0 }

static int scanheader_textdb( textdb_t * txtdb, char * next_char, size_t input_len, /*out*/size_t * number_of_cols)
{
   int err = EINVAL ;

   size_t nr_cols = 1 ;

   while( input_len ) {
      if ('\'' != *next_char && '"' != *next_char ) {
         print_err( "Expected ' or \" as first character of value in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
         goto ABBRUCH ;
      }

      char end_of_string = *next_char ;

      for( --input_len, ++next_char; input_len ; --input_len, ++next_char ) {
         if (end_of_string == *next_char) {
            break ;
         }
         if (  ! (   ('a' <= *next_char && *next_char <= 'z')
                  || ('A' <= *next_char && *next_char <= 'Z')
                  || ('0' <= *next_char && *next_char <= '9')
                  ||  '_' == *next_char
                  ||  '-' == *next_char)) {
            print_err( "Header name contains wrong character '%c' in textdb file '%s' in line: %d", *next_char, txtdb->filename, txtdb->line_number) ;
            goto ABBRUCH ;
         }
      }

      if (!input_len) {
         print_err( "Expected closing '%c' in textdb file '%s' in line: %d", end_of_string, txtdb->filename, txtdb->line_number) ;
         goto ABBRUCH ;
      }

      for( --input_len, ++next_char; input_len ; --input_len, ++next_char ) {
         if (' ' != *next_char && '\t' != *next_char) {
            break ;
         }
      }

      if (     ! input_len
          ||   '\n' == *next_char ) {
         // read end of line
         break ;
      }

      if (',' != *next_char) {
         print_err( "Expected ',' not '%c' in textdb file '%s' in line: %d", *next_char, txtdb->filename, txtdb->line_number) ;
         goto ABBRUCH ;
      }

      ++ nr_cols ;

      for( --input_len, ++next_char; input_len ; --input_len, ++next_char ) {
         if (' ' != *next_char && '\t' != *next_char) {
            break ;
         }
      }

      if (     !input_len
          ||   '\n' == *next_char ) {
         print_err( "No data after ',' in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
         goto ABBRUCH ;
      }
   }

   *number_of_cols = nr_cols ;

   return 0 ;
ABBRUCH:
   return err ;
}

static int tablesize_textdb( textdb_t * txtdb, /*out*/size_t * number_of_rows, /*out*/size_t * number_of_cols)
{
   int err ;
   char * next_char = (char*) addr_mmfile( &txtdb->input_file ) ;
   size_t input_len = size_mmfile( &txtdb->input_file ) ;

   size_t nr_rows = 0 ;
   size_t nr_cols = 0 ;

   txtdb->line_number = 1 ;

   while( input_len ) {
      if (' ' == *next_char || '\t' == *next_char || '\n' == *next_char ) {
         // ignore white space
         if ('\n' == *next_char) {
            ++ txtdb->line_number ;
         }
         -- input_len ;
         ++ next_char ;
         continue ;
      }
      if ('#' != *next_char) {
         // data line
         if (!nr_rows) {
            // found header
            err = scanheader_textdb(txtdb, next_char, input_len, &nr_cols) ;
            if (err) goto ABBRUCH ;
         }
         ++ nr_rows ;
      }
      // find end of line
      while( input_len ) {
         if ('\n' == *next_char) {
            break ;
         }
         -- input_len ;
         ++ next_char ;
      }
   }

   *number_of_rows = nr_rows ;
   *number_of_cols = nr_cols ;

   return 0 ;
ABBRUCH:
   return err ;
}

static int readrows_textdb( textdb_t * txtdb, size_t start_column_index )
{
   int err ;
   char * next_char = (char*) addr_mmfile( &txtdb->input_file ) ;
   size_t input_len = size_mmfile( &txtdb->input_file ) ;
   size_t row_index = 0 ;

   txtdb->line_number = 1 ;

   while( input_len ) {
      // find line containing data
      for(;;) {
         if (' ' == *next_char || '\t' == *next_char || '\n' == *next_char ) {
            // ignore white space
            if ('\n' == *next_char) {
               ++ txtdb->line_number ;
            }
            -- input_len ;
            ++ next_char ;
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
         if (!input_len) {
            // no more data
            return 0 ;
         }
      }

      // read line content
      size_t column_index = start_column_index ;
      bool   isExpectData = true ;

      while( input_len ) {
         if ('\n' == *next_char) {
            break ;
         } else if (' ' == *next_char || '\t' == *next_char) {
            -- input_len ;
            ++ next_char ;
            continue ;
         } else if (',' == *next_char) {
            isExpectData = true ;
            ++ column_index ;
            if (column_index >= txtdb->column_count) {
               err = E2BIG ;
               print_err( "Expected only %d columns in textdb file '%s' in line: %d", txtdb->column_count, txtdb->filename, txtdb->line_number) ;
               goto ABBRUCH ;
            }
            -- input_len ;
            ++ next_char ;
            continue ;
         }
         if ('\'' != *next_char && '\"' != *next_char) {
            err = EINVAL ;
            print_err( "Expected ' or \" as first character of value in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
            goto ABBRUCH ;
         }
         char end_of_string = *next_char ;
         isExpectData = false ;
         -- input_len ;
         ++ next_char ;
         char * str = next_char ;
         while( input_len ) {
            if (end_of_string == *next_char) {
               break ;
            }
            if ('\n' == *next_char) {
               break ;
            }
            -- input_len ;
            ++ next_char ;
         }
         if (!input_len || end_of_string != *next_char) {
            err = EINVAL ;
            print_err( "Expected closing %c in in textdb file '%s' in line: %d", end_of_string, txtdb->filename, txtdb->line_number) ;
            goto ABBRUCH ;
         }
         size_t str_len = (size_t) (next_char - str) ;
         -- input_len ;
         ++ next_char ;
         err = appendvalue_textdbcolumn( &txtdb->rows[row_index * txtdb->column_count + column_index], str, str_len) ;
         if (err) goto ABBRUCH ;
      }

      if (     isExpectData
            || (column_index + 1) != txtdb->column_count) {
         err = ENODATA ;
         if ( isExpectData ) {
            print_err( "Expected a value after ',' in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
         } else {
            print_err( "Expected %d columns in textdb file '%s' in line: %d", txtdb->column_count, txtdb->filename, txtdb->line_number) ;
         }
         goto ABBRUCH ;
      }

      ++ row_index ;
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

static int free_textdb(textdb_t * txtdb)
{
   int err = 0 ;
   int err2 ;
   size_t table_size = txtdb->row_count * txtdb->column_count ;

   for(size_t i = 0; i < table_size; ++i) {
      err2 = free_textdbcolumn(&txtdb->rows[i]) ;
      if (err2) err = err2 ;
   }

   free(txtdb->rows) ;
   txtdb->rows         = 0 ;
   txtdb->row_count    = 0 ;
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
   textdb_t txtdb = textdb_INIT_FREEABLE ;

   txtdb.filename = strdup(filename) ;
   if (!txtdb.filename) {
      err = ENOMEM ;
      print_err( "Out of memory" ) ;
      goto ABBRUCH ;
   }

   err = init_mmfile( &txtdb.input_file, filename, 0, 0, 0, mmfile_openmode_RDONLY) ;
   if (err) {
      print_err( "Can not open textdb file '%s' for reading: %s", filename, strerror(err) ) ;
      goto ABBRUCH ;
   }

   err = tablesize_textdb( &txtdb, &txtdb.row_count, &txtdb.column_count ) ;
   if (err) goto ABBRUCH ;

   if (! txtdb.row_count) {
      err = ENODATA ;
      print_err( "Expected text line with header names in textdb file '%s'", txtdb.filename ) ;
      goto ABBRUCH ;
   }

   ++ txtdb.column_count /*special column row-id*/ ;
   size_t rows_size = sizeof(textdb_column_t) * txtdb.row_count * txtdb.column_count ;
   txtdb.rows       = (textdb_column_t*) malloc( rows_size ) ;
   if (!txtdb.rows) {
      err = ENOMEM ;
      print_err( "Out of memory" ) ;
      goto ABBRUCH ;
   }
   memset( txtdb.rows, 0, rows_size ) ;

   err = appendvalue_textdbcolumn( &txtdb.rows[0], "row-id", 6) ;
   if (err) goto ABBRUCH ;

   err = readrows_textdb( &txtdb, 1/*start after special columns*/ ) ;
   if (err) goto ABBRUCH ;

   memcpy( result, &txtdb, sizeof(txtdb) ) ;

   return 0 ;
ABBRUCH:
   free_textdb( &txtdb ) ;
   return err ;
}

enum expression_type_e {
    exprtypeNAME
   ,exprtypeSTRING
   ,exprtypeCOMPARE_EQUAL
   ,exprtypeCOMPARE_NOTEQUAL
   ,exprtypeAND
   ,exprtypeOR
} ;
typedef enum expression_type_e expression_type_e ;

struct expression_t
{
   expression_t      * parent ;
   expression_t      * left ;
   expression_t      * right ;
   expression_type_e type ;
   const char *      value ;
   size_t            length ;
   size_t            col_index ;
} ;

static int delete_expression(expression_t ** expr)
{
   int err = 0 ;
   int err2 ;

   expression_t * delobj = *expr ;

   if (delobj) {
      *expr = 0 ;
      err2 = delete_expression(&delobj->left) ;
      if (err2) err = err2 ;
      err2 = delete_expression(&delobj->right) ;
      if (err2) err = err2 ;
      free(delobj) ;
   }

   return err ;
}

static int new_expression(expression_t ** expr, expression_type_e type)
{
   expression_t * new_expr ;

   new_expr = MALLOC(expression_t,malloc,) ;
   if (!new_expr) {
      print_err("Out of memory") ;
      goto ABBRUCH ;
   }

   MEMSET0(new_expr) ;
   new_expr->type = type ;

   *expr = new_expr ;
   return 0 ;
ABBRUCH:
   return ENOMEM ;
}


// parse name=='value' or name!='value'
static int parse_exprcompare(/*out*/expression_t ** result, /*out*/char ** end_param, char * start_expr, char * end_macro, size_t start_linenr )
{
   int err ;
   expression_t    * name_node = 0 ;
   expression_t    * value_node = 0 ;
   expression_t    * compare_node = 0 ;

   skip_space( &start_expr, end_macro ) ;
   char * start_name = start_expr ;
   while(   start_expr < end_macro
         && start_expr[0] != ' '
         && start_expr[0] != '\''
         && start_expr[0] != '"'
         && start_expr[0] != '!'
         && start_expr[0] != '=' ) {
      ++ start_expr ;
   }

   if (start_expr == start_name) {
      print_err( "Expected column-name in WHERE() in line: %d", start_linenr ) ;
      goto ABBRUCH ;
   }

   err = new_expression( &name_node, exprtypeNAME ) ;
   if (err) goto ABBRUCH ;
   name_node->value  = start_name ;
   name_node->length = (size_t) (start_expr - start_name) ;

   skip_space(&start_expr, end_macro) ;
   if (     start_expr + 1 >= end_macro
         || (     (     start_expr[0] != '!'
                     || start_expr[1] != '=' )
               && (     start_expr[0] != '='
                     || start_expr[1] != '=' ))) {
      print_err( "Expected '==' or '!=' after column-name in WHERE() in line: %d", start_linenr ) ;
      goto ABBRUCH ;
   }

   err = new_expression( &compare_node, start_expr[0] == '=' ? exprtypeCOMPARE_EQUAL : exprtypeCOMPARE_NOTEQUAL ) ;
   if (err) goto ABBRUCH ;

   start_expr += 2 ;
   skip_space(&start_expr, end_macro) ;
   char * start_value = start_expr ;
   if (     start_value >= end_macro
         || (     start_value[0] != '\''
               && start_value[0] != '"')) {
      print_err( "Expected 'value' after compare in WHERE() in line: %d", start_linenr ) ;
      goto ABBRUCH ;
   }

   ++ start_expr ;
   while(   start_expr < end_macro
         && start_expr[0] != start_value[0] ) {
      ++ start_expr ;
   }

   if (start_expr >= end_macro) {
      print_err( "Expected closing %c in line: %d", start_value[0], start_linenr ) ;
      goto ABBRUCH ;
   }

   err = new_expression( &value_node, exprtypeSTRING ) ;
   if (err) goto ABBRUCH ;
   value_node->value  =  ( ++ start_value ) ;
   value_node->length =  (size_t) (start_expr - start_value) ;

   compare_node->left  = name_node ;
   compare_node->right = value_node ;
   name_node->parent   = compare_node ;
   value_node->parent  = compare_node ;

   *result    = compare_node ;
   *end_param = start_expr ;
   return 0 ;
ABBRUCH:
   delete_expression(&name_node) ;
   delete_expression(&value_node) ;
   delete_expression(&compare_node) ;
   return 1 ;
}

// parse '(' ... ')'
static int parse_expression(/*out*/expression_t ** result, /*out*/char ** end_param, char * start_expr, char * end_macro, size_t start_linenr )
{
   int err ;
   expression_t  * andor_node = 0 ;
   expression_t   * expr_node = 0 ;

   skip_space( &start_expr, end_macro ) ;

   if (  start_expr >= end_macro
         || '(' != start_expr[0] ) {
      print_err( "Expected '(' after WHERE in line: %d", start_linenr ) ;
      goto ABBRUCH ;
   }

   for(;;) {
      ++ start_expr ;
      skip_space( &start_expr, end_macro ) ;

      if (     start_expr < end_macro
            && '(' == start_expr[0] ) {
         err = parse_expression(&expr_node, &start_expr, start_expr, end_macro, start_linenr ) ;
         if (err) goto ABBRUCH ;
      } else {
         err = parse_exprcompare(&expr_node, &start_expr, start_expr, end_macro, start_linenr ) ;
         if (err) goto ABBRUCH ;
      }

      ++ start_expr ;
      skip_space( &start_expr, end_macro ) ;

      if ( start_expr >= end_macro ) {
         print_err( "Expected ')' after WHERE(... in line: %d", start_linenr ) ;
         goto ABBRUCH ;
      }

      if (andor_node) {
         expr_node->parent = andor_node ;
         andor_node->right = expr_node ;
         expr_node  = andor_node ;
         andor_node = 0 ;
      }

      if (     start_expr < end_macro
            && ')' == start_expr[0]) {
         break ;
      } else if (    start_expr + 1 >= end_macro
                  || (     (     '&' != start_expr[0]
                              || '&' != start_expr[1] )
                        && (     '|' != start_expr[0]
                              || '|' != start_expr[1] ))) {
         print_err( "Expected ')' after WHERE(... in line: %d", start_linenr ) ;
         goto ABBRUCH ;
      }

      err = new_expression( &andor_node, (start_expr[0] == '&') ? exprtypeAND : exprtypeOR ) ;
      if (err) goto ABBRUCH ;
      andor_node->left  = expr_node ;
      expr_node->parent = andor_node ;
      expr_node = 0 ;
      ++ start_expr ;

   }

   *end_param = start_expr ;
   *result    = expr_node ;
   return 0 ;
ABBRUCH:
   delete_expression(&andor_node) ;
   delete_expression(&expr_node) ;
   return 1 ;
}

int matchnames_expression( expression_t * where_expr, const textdb_t * dbfile, const size_t start_linenr )
{
   int err ;

   if (!where_expr) {
      return 0 ;
   }

   err = matchnames_expression(where_expr->left, dbfile, start_linenr ) ;
   if (err) goto ABBRUCH ;

   err = matchnames_expression(where_expr->right, dbfile, start_linenr ) ;
   if (err) goto ABBRUCH ;

   if (where_expr->type == exprtypeNAME) {
      // search matching header
      bool isMatch = false ;
      for(size_t i = 0; i < dbfile->column_count ; ++i) {
         if (     dbfile->rows[i].length == where_expr->length
               && 0 == strncmp(dbfile->rows[i].value, where_expr->value, where_expr->length) ) {
            where_expr->col_index = i ;
            isMatch = true ;
            break ;
         }
      }
      if (!isMatch) {
         print_err( "Unknown column name '%.*s' in WHERE() in line: %d", where_expr->length, where_expr->value, start_linenr ) ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   return 1 ;
}

bool ismatch_expression( expression_t * where_expr, size_t row, const textdb_t * dbfile, const size_t start_linenr)
{
   if (!where_expr) {
      // empty where always matches
      return true ;
   }

   textdb_column_t * data = &dbfile->rows[row * dbfile->column_count] ;

   switch(where_expr->type) {
   case exprtypeAND:
      return ismatch_expression(where_expr->left, row, dbfile, start_linenr ) && ismatch_expression(where_expr->right, row, dbfile, start_linenr ) ;
   case exprtypeOR:
      return ismatch_expression(where_expr->left, row, dbfile, start_linenr ) || ismatch_expression(where_expr->right, row, dbfile, start_linenr ) ;
   case exprtypeCOMPARE_EQUAL:
      return      data[where_expr->left->col_index].length == where_expr->right->length
               && 0 == strncmp(data[where_expr->left->col_index].value, where_expr->right->value, where_expr->right->length) ;
   case exprtypeCOMPARE_NOTEQUAL:
      return      data[where_expr->left->col_index].length != where_expr->right->length
               || 0 != strncmp(data[where_expr->left->col_index].value, where_expr->right->value, where_expr->right->length) ;
   default:
      print_err( "Internal error in WHERE() in line: %d", start_linenr ) ;
      assert(0) ;
   }

   return false ;
}

struct select_parameter_t
{
   select_parameter_t * next ;
   const char *   value ;
   size_t         length ;
   size_t         col_index ;
   bool           isString ;
} ;

static int delete_select_parameter(select_parameter_t ** param)
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
            && '\\' != next[0]
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
         size_t       string_len ;
         if ('"' == next[0]) {
            do {
               ++ next ;
            } while( next < end_macro && '"' != next[0]) ;
            if (next >= end_macro) {
               print_err( "Expected \" after %s(...\" in line: %d", cmd, start_linenr ) ;
               goto ABBRUCH ;
            }
            string_len = (size_t) (next - start_string) ;
         } else if ('\'' == next[0]) {
            do {
               ++ next ;
            } while( next < end_macro && '\'' != next[0]) ;
            if (next >= end_macro) {
               print_err( "Expected ' after %s(...' in line: %d", cmd, start_linenr ) ;
               goto ABBRUCH ;
            }
            string_len = (size_t) (next - start_string) ;
         } else if ('\\' == next[0]) {
            ++ next ;
            if (next >= end_macro) {
               print_err( "Expected no endofline after \\ in line: %d", start_linenr ) ;
               goto ABBRUCH ;
            }
            switch(next[0]) {
            case 'n':   start_string = "\n" ;
                        break ;
            default:    print_err( "Unsupported escaped character \\%c in line: %d", next[0], start_linenr ) ;
                        goto ABBRUCH ;
            }
            string_len = strlen(start_string) ;
         } else {
            assert(')' == next[0]) ;
            *end_param = next ;
            break ;
         }

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
   delete_select_parameter(&first_param.next) ;
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
   select_parameter_t   * select_param = 0 ;
   select_parameter_t     * from_param = 0 ;
   expression_t           * where_expr = 0 ;
   textdb_t                    dbfile = textdb_INIT_FREEABLE ;
   bool                     ascending = true ;  // default ascending order
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
      err = EINVAL ;
      print_err( "Expected 'FROM' after SELECT() in line: %d", start_linenr ) ;
      goto ABBRUCH ;
   }

   err = parse_select_parameter( &from_param, &end_param, start_from+4, end_macro, start_linenr, "FROM" ) ;
   if (err) goto ABBRUCH ;

   ++ end_param ;
   skip_space( &end_param, end_macro ) ;

   char * start_where = end_param ;

   if (     (end_macro - start_where) > 5
         && 0 == strncmp( start_where, "WHERE", 5) ) {
      err = parse_expression( &where_expr, &end_param, start_where+5, end_macro, start_linenr ) ;
      if (err) goto ABBRUCH ;
      ++ end_param ;
      skip_space( &end_param, end_macro ) ;
   }

   char * sort_order = end_param ;

   if (     (end_macro - sort_order) >= 9
         && 0 == strncmp( sort_order, "ASCENDING", 9) ) {
      end_param = 9 + sort_order ;
      skip_space( &end_param, end_macro ) ;
      ascending = true ;
   } else if (    (end_macro - sort_order) >= 10
               && 0 == strncmp( sort_order, "DESCENDING", 10) ) {
      end_param = 10 + sort_order ;
      skip_space( &end_param, end_macro ) ;
      ascending = false ;
   }

   if (end_param < end_macro) {
      err = EINVAL ;
      print_err( "Expected nothing after SELECT()FROM()WHERE()\\(ASCENDING\\|DESCENDING\\) in line: %d", start_linenr ) ;
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
            if (     p->length == dbfile.rows[i].length
                  && 0 == strncmp(dbfile.rows[i].value, p->value, p->length) ) {
               p->col_index = i ;
               isMatch = true ;
               break ;
            }
         }
         if (!isMatch) {
            err = EINVAL ;
            print_err( "Unknown column name '%.*s' in SELECT()FROM() in line: %d", p->length, p->value, start_linenr ) ;
            goto ABBRUCH ;
         }
      }
   }

   err = matchnames_expression( where_expr, &dbfile, start_linenr ) ;
   if (err) goto ABBRUCH ;

   // determine row-id
   for(size_t row = 1/*skip header*/, rowid = 1; row < dbfile.row_count; ++row) {
      if (  ismatch_expression( where_expr, row, &dbfile, start_linenr ) ) {
         textdb_column_t * data  = &dbfile.rows[row * dbfile.column_count] ;
         char              buffer[10] ;

         snprintf(buffer, sizeof(buffer), "%u", (int)rowid) ;
         err = appendvalue_textdbcolumn( &data[0], buffer, strlen(buffer)) ;
         if (err) goto ABBRUCH ;

         ++ rowid ;
      }
   }

   // write for every read row the values of the concatenated select parameters as one line of output
   for(size_t i = 1/*skip header*/; i < dbfile.row_count; ++i) {

      size_t row = (ascending ? i : (dbfile.row_count-i)) ;

      if (  ismatch_expression( where_expr, row, &dbfile, start_linenr ) ) {

         textdb_column_t * data = &dbfile.rows[row * dbfile.column_count] ;
         for( const select_parameter_t * p = select_param; p; p = p->next) {
            if (p->isString) {
               err = write_file( outfd, p->value, p->length ) ;
               if (err) goto ABBRUCH ;
            } else {
               err = write_file( outfd, data[p->col_index].value, data[p->col_index].length ) ;
               if (err) goto ABBRUCH ;
            }
         }

         err = write_file( outfd, "\n", 1 ) ;
         if (err) goto ABBRUCH ;
      }
   }

   free_textdb( &dbfile ) ;
   free(filename) ;
   delete_select_parameter(&select_param) ;
   delete_select_parameter(&from_param) ;
   delete_expression(&where_expr) ;
   return 0 ;
ABBRUCH:
   free_textdb( &dbfile ) ;
   free(filename) ;
   delete_select_parameter(&select_param) ;
   delete_select_parameter(&from_param) ;
   delete_expression(&where_expr) ;
   return err ;
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
   char    * next_input = (char*) addr_mmfile(input_file) ;
   size_t    input_size = size_mmfile(input_file) ;
   size_t   line_number = 1 ;

   while(input_size) {
      // find start "// TEXTDB:..."
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
         print_err( "Found end of macro '// TEXTDB:END' without beginning of macro in line: %d", line_number ) ;
         goto ABBRUCH ;
      }

      // find end "// TEXTDB:END"
      char   * start_macro2 ;
      char     * end_macro2 ;
      err = find_macro(&start_macro2, &end_macro2, &line_number, next_input, input_size) ;
      if (     err
            || strncmp( start_macro2, "// TEXTDB:END", (size_t) (end_macro2 - start_macro2)) ) {
         print_err( "Can not find end of macro '// TEXTDB:END' which starts at line: %d", start_linenr ) ;
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

   err = initprocess_umgebung(umgebung_type_DEFAULT) ;
   if (err) goto ABBRUCH ;

   err = process_arguments(argc, argv) ;
   if (err) goto PRINT_USAGE ;

   // create out file if -o <filename> is specified
   if (g_outfilename) {
      outfd = open(g_outfilename, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
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

   freeprocess_umgebung() ;
   return 0 ;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "TextDB version 0.1 - Copyright (c) 2011 Joerg Seebohn\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* TextDB is a textdb macro preprocessor.\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* It reads a C source file and translates it into a C source file\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* whereupon textdb macros are expanded.\n" ) ;
   dprintf(STDERR_FILENO, "\nUsage: %s [-o <out.c>] <in.c>\n\n", g_programname ) ;
ABBRUCH:
   if (g_outfilename && outfd != -1) {
      close(outfd) ;
      unlink(g_outfilename) ;
   }
   free_mmfile(&input_file) ;
   freeprocess_umgebung() ;
   return 1 ;
}
