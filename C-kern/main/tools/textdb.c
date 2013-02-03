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
#include "C-kern/api/err.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/io/filesystem/mmfile.h"

typedef struct function_t              function_t ;
typedef struct iffunction_t            iffunction_t ;
typedef struct expression_t            expression_t ;
typedef struct select_parameter_t      select_parameter_t ;
typedef struct textdb_t                textdb_t ;
typedef struct textdb_column_t         textdb_column_t ;
typedef struct depfilename_written_t   depfilename_written_t ;

static int process_select_parameter(select_parameter_t * select_param, const size_t row, const textdb_t * dbfile, const size_t start_linenr) ;
static int prepare_select_parameter(select_parameter_t * select_param, const textdb_t * dbfile, const size_t start_linenr) ;
static int parse_select_parameter(/*out*/select_parameter_t ** param, /*out*/char ** end_param, char * start_param, char * end_macro, size_t start_linenr, const char * cmd ) ;
static int delete_select_parameter(select_parameter_t ** param) ;
static void print_err(const char* printf_format, ...) __attribute__ ((__format__ (__printf__, 1, 2))) ;

const char * g_programname = 0 ;
const char *  g_infilename = 0 ;
const char * g_outfilename = 0 ;
char *       g_depfilename = 0 ;
int          g_depencyfile = 0 ;
int          g_foverwrite  = 0 ;
int          g_outfd       = -1 ;
int          g_depfd       = -1 ;

static void print_err(const char* printf_format, ...)
{
   va_list args ;
   va_start( args, printf_format ) ;
   dprintf( STDERR_FILENO, "%s: error: ", g_programname) ;
   vdprintf( STDERR_FILENO, printf_format, args) ;
   dprintf( STDERR_FILENO, "\n") ;
   va_end( args ) ;
   if (g_infilename) {
      dprintf( STDERR_FILENO, "%s: error: processing '%s'\n", g_programname, g_infilename) ;
   }
}

static int process_arguments(int argc, const char * argv[])
{
   g_programname = argv[0] ;

   if (argc < 2) {
      return EINVAL ;
   }

   g_infilename = argv[argc-1] ;

   for (int i = 1; i < (argc-1); ++i) {
      if (     argv[i][0] != '-'
            || argv[i][1] == 0
            || argv[i][2] != 0 ) {
         return EINVAL ;
      }
      switch (argv[i][1]) {
      case 'd':
         g_depencyfile = 1 ;
         break ;
      case 'f':
         g_foverwrite = 1 ;
         break ;
      case 'o':
         if (i == (argc-2)) {
            return EINVAL ;
         }
         g_outfilename = argv[++i] ;
         break ;
      }
   }

   if (  g_depencyfile
      && !g_outfilename) {
      return EINVAL ;
   }

   return 0 ;
}

static int write_outfile(int outfd, const char * write_start, size_t write_size)
{
   if (write_size != (size_t) write(outfd, write_start, write_size)) {
      if (outfd == g_outfd && g_outfilename) {
         print_err( "Can not write file '%s': %s", g_outfilename, strerror(errno) ) ;
      } else if (outfd == g_depfd && g_depfilename) {
         print_err( "Can not write file '%s': %s", g_depfilename, strerror(errno) ) ;
      } else {
         print_err( "Can not write output: %s", strerror(errno) ) ;
      }
      return 1 ;
   }
   return 0 ;
}

static void skip_space( char ** current_pos, char * end_pos)
{
   char * next = *current_pos ;
   while (  next < end_pos
            && ' ' == next[0]) {
      ++ next ;
   }
   *current_pos = next ;
}

struct depfilename_written_t
{
   depfilename_written_t * next ;
   char                  * depfilename ;
} ;

depfilename_written_t  * s_depfilenamewritten_list = 0 ;

static int free_depfilenamewritten(void)
{
   while (s_depfilenamewritten_list) {
      depfilename_written_t * entry = s_depfilenamewritten_list ;
      s_depfilenamewritten_list = s_depfilenamewritten_list->next ;
      free(entry->depfilename) ;
      free(entry) ;
   }
   return 0 ;
}

static int find_depfilenamewritten(const char * filename)
{
   for (depfilename_written_t * next = s_depfilenamewritten_list; next; next = next->next) {
      if (0 == strcmp( filename, next->depfilename)) {
         return 0 ;
      }
   }

   return ESRCH ;
}

static int insert_depfilenamewritten(const char * filename)
{
   depfilename_written_t * new_entry = malloc(sizeof(depfilename_written_t)) ;

   if (new_entry) {
      new_entry->depfilename = strdup(filename) ;
   }

   if (  !new_entry
      || !new_entry->depfilename) {
      print_err("Out of memory") ;
      return ENOMEM ;
   }

   new_entry->next = s_depfilenamewritten_list ;
   s_depfilenamewritten_list = new_entry ;

   return 0 ;
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
      void * temp = realloc(column->value, column->allocated_memory_size) ;
      if (!temp) {
         print_err("Out of memory") ;
         goto ONABORT ;
      }
      column->value = (char*) temp ;
   }

   memcpy( column->value + column->length, str, str_len) ;
   column->length += str_len ;
   column->value[column->length] = 0 ;

   return 0 ;
ONABORT:
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

   while (input_len) {
      if ('\'' != *next_char && '"' != *next_char) {
         print_err( "Expected ' or \" as first character of value in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
         goto ONABORT ;
      }

      char end_of_string = *next_char ;

      for (--input_len, ++next_char; input_len ; --input_len, ++next_char) {
         if (end_of_string == *next_char) {
            break ;
         }
         if (  ! (   ('a' <= *next_char && *next_char <= 'z')
                  || ('A' <= *next_char && *next_char <= 'Z')
                  || ('0' <= *next_char && *next_char <= '9')
                  ||  '_' == *next_char
                  ||  '-' == *next_char)) {
            print_err( "Header name contains wrong character '%c' in textdb file '%s' in line: %d", *next_char, txtdb->filename, txtdb->line_number) ;
            goto ONABORT ;
         }
      }

      if (!input_len) {
         print_err( "Expected closing '%c' in textdb file '%s' in line: %d", end_of_string, txtdb->filename, txtdb->line_number) ;
         goto ONABORT ;
      }

      for (--input_len, ++next_char; input_len ; --input_len, ++next_char) {
         if (' ' != *next_char && '\t' != *next_char) {
            break ;
         }
      }

      if (  ! input_len
            || '\n' == *next_char) {
         // read end of line
         break ;
      }

      if (',' != *next_char) {
         print_err( "Expected ',' not '%c' in textdb file '%s' in line: %d", *next_char, txtdb->filename, txtdb->line_number) ;
         goto ONABORT ;
      }

      ++ nr_cols ;

      for (--input_len, ++next_char; input_len ; --input_len, ++next_char) {
         if (' ' != *next_char && '\t' != *next_char) {
            break ;
         }
      }

      if (  !input_len
            || '\n' == *next_char) {
         print_err( "No data after ',' in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
         goto ONABORT ;
      }
   }

   *number_of_cols = nr_cols ;

   return 0 ;
ONABORT:
   return err ;
}

static int tablesize_textdb( textdb_t * txtdb, /*out*/size_t * number_of_rows, /*out*/size_t * number_of_cols)
{
   int err ;
   char * next_char = (char*) addr_mmfile( &txtdb->input_file) ;
   size_t input_len = size_mmfile( &txtdb->input_file) ;

   size_t nr_rows = 0 ;
   size_t nr_cols = 0 ;

   txtdb->line_number = 1 ;

   while (input_len) {
      if (' ' == *next_char || '\t' == *next_char || '\n' == *next_char) {
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
            if (err) goto ONABORT ;
         }
         ++ nr_rows ;
      }
      // find end of line
      while (input_len) {
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
ONABORT:
   return err ;
}

static int readrows_textdb( textdb_t * txtdb, size_t start_column_index)
{
   int err ;
   char * next_char = (char*) addr_mmfile( &txtdb->input_file) ;
   size_t input_len = size_mmfile( &txtdb->input_file) ;
   size_t row_index = 0 ;

   txtdb->line_number = 1 ;

   while (input_len) {
      // find line containing data
      for (;;) {
         if (' ' == *next_char || '\t' == *next_char || '\n' == *next_char) {
            // ignore white space
            if ('\n' == *next_char) {
               ++ txtdb->line_number ;
            }
            -- input_len ;
            ++ next_char ;
         } else if ('#' == *next_char) {
            // ignore comment
            while (input_len) {
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

      while (input_len) {
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
               goto ONABORT ;
            }
            -- input_len ;
            ++ next_char ;
            continue ;
         }
         if ('\'' != *next_char && '\"' != *next_char) {
            err = EINVAL ;
            print_err( "Expected ' or \" as first character of value in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
            goto ONABORT ;
         }
         char end_of_string = *next_char ;
         isExpectData = false ;
         -- input_len ;
         ++ next_char ;
         char * str = next_char ;
         while (input_len) {
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
            goto ONABORT ;
         }
         size_t str_len = (size_t) (next_char - str) ;
         -- input_len ;
         ++ next_char ;
         err = appendvalue_textdbcolumn( &txtdb->rows[row_index * txtdb->column_count + column_index], str, str_len) ;
         if (err) goto ONABORT ;
      }

      if (     isExpectData
            || (column_index + 1) != txtdb->column_count) {
         err = ENODATA ;
         if ( isExpectData) {
            print_err( "Expected a value after ',' in textdb file '%s' in line: %d", txtdb->filename, txtdb->line_number) ;
         } else {
            print_err( "Expected %d columns in textdb file '%s' in line: %d", txtdb->column_count, txtdb->filename, txtdb->line_number) ;
         }
         goto ONABORT ;
      }

      ++ row_index ;
   }

   return 0 ;
ONABORT:
   return 1 ;
}

static int free_textdb(textdb_t * txtdb)
{
   int err = 0 ;
   int err2 ;
   size_t table_size = txtdb->row_count * txtdb->column_count ;

   for (size_t i = 0; i < table_size; ++i) {
      err2 = free_textdbcolumn(&txtdb->rows[i]) ;
      if (err2) err = err2 ;
   }

   free(txtdb->rows) ;
   txtdb->rows         = 0 ;
   txtdb->row_count    = 0 ;
   txtdb->column_count = 0 ;

   err2 = free_mmfile(&txtdb->input_file) ;
   if (err2) err = err2 ;

   if (err) goto ONABORT ;
   return 0 ;
ONABORT:
   return err ;
}

static int init_textdb(/*out*/textdb_t * result, const char * filename)
{
   int err ;
   textdb_t txtdb = textdb_INIT_FREEABLE ;

   txtdb.filename = strdup(filename) ;
   if (!txtdb.filename) {
      err = ENOMEM ;
      print_err( "Out of memory") ;
      goto ONABORT ;
   }

   err = init_mmfile( &txtdb.input_file, filename, 0, 0, accessmode_READ, 0) ;
   if (err) {
      print_err( "Can not open textdb file '%s' for reading: %s", filename, strerror(err)) ;
      goto ONABORT ;
   }

   err = tablesize_textdb( &txtdb, &txtdb.row_count, &txtdb.column_count) ;
   if (err) goto ONABORT ;

   if (! txtdb.row_count) {
      err = ENODATA ;
      print_err( "Expected text line with header names in textdb file '%s'", txtdb.filename) ;
      goto ONABORT ;
   }

   ++ txtdb.column_count /*special column row-id*/ ;
   size_t rows_size = sizeof(textdb_column_t) * txtdb.row_count * txtdb.column_count ;
   txtdb.rows       = (textdb_column_t*) malloc( rows_size) ;
   if (!txtdb.rows) {
      err = ENOMEM ;
      print_err( "Out of memory") ;
      goto ONABORT ;
   }
   memset( txtdb.rows, 0, rows_size) ;

   err = appendvalue_textdbcolumn( &txtdb.rows[0], "row-id", 6) ;
   if (err) goto ONABORT ;

   err = readrows_textdb( &txtdb, 1/*start after special columns*/) ;
   if (err) goto ONABORT ;

   memcpy( result, &txtdb, sizeof(txtdb)) ;

   return 0 ;
ONABORT:
   free_textdb( &txtdb) ;
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
      goto ONABORT ;
   }

   MEMSET0(new_expr) ;
   new_expr->type = type ;

   *expr = new_expr ;
   return 0 ;
ONABORT:
   return ENOMEM ;
}


// parse name=='value' or name!='value'
static int parse_exprcompare(/*out*/expression_t ** result, /*out*/char ** end_param, char * start_expr, char * end_macro, size_t start_linenr)
{
   int err ;
   expression_t    * name_node = 0 ;
   expression_t    * value_node = 0 ;
   expression_t    * compare_node = 0 ;

   skip_space( &start_expr, end_macro) ;
   char * start_name = start_expr ;
   while (  start_expr < end_macro
            && start_expr[0] != ' '
            && start_expr[0] != '\''
            && start_expr[0] != '"'
            && start_expr[0] != '!'
            && start_expr[0] != '=') {
      ++ start_expr ;
   }

   if (start_expr == start_name) {
      print_err( "Expected column-name in WHERE() in line: %d", start_linenr) ;
      goto ONABORT ;
   }

   err = new_expression( &name_node, exprtypeNAME) ;
   if (err) goto ONABORT ;
   name_node->value  = start_name ;
   name_node->length = (size_t) (start_expr - start_name) ;

   skip_space(&start_expr, end_macro) ;
   if (     start_expr + 1 >= end_macro
         || (     (     start_expr[0] != '!'
                     || start_expr[1] != '=')
               && (     start_expr[0] != '='
                     || start_expr[1] != '='))) {
      print_err( "Expected '==' or '!=' after column-name in WHERE() in line: %d", start_linenr) ;
      goto ONABORT ;
   }

   err = new_expression( &compare_node, start_expr[0] == '=' ? exprtypeCOMPARE_EQUAL : exprtypeCOMPARE_NOTEQUAL) ;
   if (err) goto ONABORT ;

   start_expr += 2 ;
   skip_space(&start_expr, end_macro) ;
   char * start_value = start_expr ;
   if (     start_value >= end_macro
         || (     start_value[0] != '\''
               && start_value[0] != '"')) {
      print_err( "Expected 'value' after compare in WHERE() in line: %d", start_linenr) ;
      goto ONABORT ;
   }

   ++ start_expr ;
   while (  start_expr < end_macro
            && start_expr[0] != start_value[0]) {
      ++ start_expr ;
   }

   if (start_expr >= end_macro) {
      print_err( "Expected closing %c in line: %d", start_value[0], start_linenr) ;
      goto ONABORT ;
   }

   err = new_expression( &value_node, exprtypeSTRING) ;
   if (err) goto ONABORT ;
   value_node->value  =  ( ++ start_value) ;
   value_node->length =  (size_t) (start_expr - start_value) ;

   compare_node->left  = name_node ;
   compare_node->right = value_node ;
   name_node->parent   = compare_node ;
   value_node->parent  = compare_node ;

   *result    = compare_node ;
   *end_param = start_expr ;
   return 0 ;
ONABORT:
   delete_expression(&name_node) ;
   delete_expression(&value_node) ;
   delete_expression(&compare_node) ;
   return 1 ;
}

// parse '(' ... ')'
static int parse_expression(/*out*/expression_t ** result, /*out*/char ** end_param, char * start_expr, char * end_macro, size_t start_linenr)
{
   int err ;
   expression_t  * andor_node = 0 ;
   expression_t   * expr_node = 0 ;

   skip_space( &start_expr, end_macro) ;

   if (  start_expr >= end_macro
         || '(' != start_expr[0]) {
      print_err( "Expected '(' after WHERE in line: %d", start_linenr) ;
      goto ONABORT ;
   }

   for (;;) {
      ++ start_expr ;
      skip_space( &start_expr, end_macro) ;

      if (     start_expr < end_macro
            && '(' == start_expr[0]) {
         err = parse_expression(&expr_node, &start_expr, start_expr, end_macro, start_linenr) ;
         if (err) goto ONABORT ;
      } else {
         err = parse_exprcompare(&expr_node, &start_expr, start_expr, end_macro, start_linenr) ;
         if (err) goto ONABORT ;
      }

      ++ start_expr ;
      skip_space( &start_expr, end_macro) ;

      if ( start_expr >= end_macro) {
         print_err( "Expected ')' after WHERE(... in line: %d", start_linenr) ;
         goto ONABORT ;
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
                              || '&' != start_expr[1])
                        && (     '|' != start_expr[0]
                              || '|' != start_expr[1]))) {
         print_err( "Expected ')' after WHERE(... in line: %d", start_linenr) ;
         goto ONABORT ;
      }

      err = new_expression( &andor_node, (start_expr[0] == '&') ? exprtypeAND : exprtypeOR) ;
      if (err) goto ONABORT ;
      andor_node->left  = expr_node ;
      expr_node->parent = andor_node ;
      expr_node = 0 ;
      ++ start_expr ;

   }

   *end_param = start_expr ;
   *result    = expr_node ;
   return 0 ;
ONABORT:
   delete_expression(&andor_node) ;
   delete_expression(&expr_node) ;
   return 1 ;
}

int matchnames_expression( expression_t * where_expr, const textdb_t * dbfile, const size_t start_linenr)
{
   int err ;

   if (!where_expr) {
      return 0 ;
   }

   err = matchnames_expression(where_expr->left, dbfile, start_linenr) ;
   if (err) goto ONABORT ;

   err = matchnames_expression(where_expr->right, dbfile, start_linenr) ;
   if (err) goto ONABORT ;

   if (where_expr->type == exprtypeNAME) {
      // search matching header
      bool isMatch = false ;
      for (size_t i = 0; i < dbfile->column_count ; ++i) {
         if (     dbfile->rows[i].length == where_expr->length
               && 0 == strncmp(dbfile->rows[i].value, where_expr->value, where_expr->length)) {
            where_expr->col_index = i ;
            isMatch = true ;
            break ;
         }
      }
      if (!isMatch) {
         print_err( "Unknown column name '%.*s' in WHERE() in line: %d", where_expr->length, where_expr->value, start_linenr) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   return 1 ;
}

bool ismatch_expression( expression_t * where_expr, size_t row, const textdb_t * dbfile, const size_t start_linenr)
{
   if (!where_expr) {
      // empty where always matches
      return true ;
   }

   textdb_column_t * data = &dbfile->rows[row * dbfile->column_count] ;

   switch (where_expr->type) {
   case exprtypeAND:
      return ismatch_expression(where_expr->left, row, dbfile, start_linenr) && ismatch_expression(where_expr->right, row, dbfile, start_linenr) ;
   case exprtypeOR:
      return ismatch_expression(where_expr->left, row, dbfile, start_linenr) || ismatch_expression(where_expr->right, row, dbfile, start_linenr) ;
   case exprtypeCOMPARE_EQUAL:
      return      data[where_expr->left->col_index].length == where_expr->right->length
               && 0 == strncmp(data[where_expr->left->col_index].value, where_expr->right->value, where_expr->right->length) ;
   case exprtypeCOMPARE_NOTEQUAL:
      return      data[where_expr->left->col_index].length != where_expr->right->length
               || 0 != strncmp(data[where_expr->left->col_index].value, where_expr->right->value, where_expr->right->length) ;
   default:
      print_err( "Internal error in WHERE() in line: %d", start_linenr) ;
      assert(0) ;
   }

   return false ;
}

struct function_t {
   int  (* free)     ( function_t ** funcobj) ;
   int  (* prepare)  ( function_t * funcobj, const textdb_t * dbfile, const size_t) ;
   int  (* process)  ( function_t * funcobj, const size_t row, const textdb_t * dbfile, const size_t start_linenr) ;
} ;

struct iffunction_t {
   function_t     funcobj ;
   expression_t * condition ;
   const char   * ifstring ;
   size_t         ifstring_len ;
   select_parameter_t * ifstring2 ;
   const char   * elsestring ;
   size_t         elsestring_len ;
} ;

static int delete_iffunction(iffunction_t ** iffunc) ;
static int process_iffunction(iffunction_t * iffunc, const size_t row, const textdb_t * dbfile, const size_t start_linenr) ;
static int prepare_iffunction(iffunction_t * iffunc, const textdb_t * dbfile, const size_t) ;

static int new_iffunction(/*out*/iffunction_t ** iffunc, expression_t * condition, const char * ifstring, size_t ifstring_len, select_parameter_t * ifstring2, const char * elsestring, size_t elsestring_len)
{
   iffunction_t * newiffunc = MALLOC(iffunction_t,malloc,) ;

   if (!newiffunc) {
      print_err("Out of memory") ;
      return ENOMEM ;
   }

   newiffunc->funcobj.free    = (int (*)(function_t**)) &delete_iffunction ;
   newiffunc->funcobj.prepare = (typeof(((function_t*)0)->prepare)) &prepare_iffunction ;
   newiffunc->funcobj.process = (typeof(((function_t*)0)->process)) &process_iffunction ;
   newiffunc->condition      = condition ;
   newiffunc->ifstring       = ifstring ;
   newiffunc->ifstring_len   = ifstring_len ;
   newiffunc->ifstring2      = ifstring2 ;
   newiffunc->elsestring     = elsestring;
   newiffunc->elsestring_len = elsestring_len ;

   *iffunc = newiffunc ;
   return 0 ;
}

int delete_iffunction(iffunction_t ** iffunc)
{
   int err ;

   if (*iffunc) {

      err = delete_expression(&(*iffunc)->condition) ;

      int err2 = delete_select_parameter(&(*iffunc)->ifstring2) ;
      if (err2) err = err2 ;

      free(*iffunc) ;

      *iffunc = 0 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return err ;
}

static int process_iffunction(iffunction_t * iffunc, const size_t row, const textdb_t * dbfile, const size_t start_linenr)
{
   int err = 0 ;

   if (ismatch_expression( iffunc->condition, row, dbfile, start_linenr)) {
      if (iffunc->ifstring2) {
         err = process_select_parameter(iffunc->ifstring2, row, dbfile, start_linenr) ;
      } else {
         err = write_outfile( g_outfd, iffunc->ifstring, iffunc->ifstring_len) ;
      }
   } else {
      err = write_outfile( g_outfd, iffunc->elsestring, iffunc->elsestring_len) ;
   }

   return err ;
}

static int prepare_iffunction(iffunction_t * iffunc, const textdb_t * dbfile, const size_t start_linenr)
{
   int err ;

   err = matchnames_expression( iffunc->condition, dbfile, start_linenr) ;

   if (  !err
      && iffunc->ifstring2) {
      err = prepare_select_parameter( iffunc->ifstring2, dbfile, start_linenr) ;
   }

   return err ;
}

static int parse_iffunction(/*out*/function_t ** funcobj, /*out*/char ** end_fct, char * start_fct, char * end_macro, size_t start_linenr)
{
   int err ;
   char         * next   = 0;
   expression_t * expr   = 0 ;
   iffunction_t * iffunc = 0 ;
   const char   * start_ifstr   = "" ;
   const char   * start_elsestr = "" ;
   select_parameter_t * ifstring2 = 0 ;
   size_t         len_ifstr     = 0 ;
   size_t         len_elsestr   = 0 ;

   // parse expression
   err = parse_expression( &expr, &next, start_fct, end_macro, start_linenr) ;
   if (err) goto ONABORT ;

   ++ next ;
   skip_space(&next, end_macro) ;

   start_ifstr = next ;
   if (  next < end_macro
         && next[0] == '(') {

      err = parse_select_parameter(&ifstring2, &next, next, end_macro, start_linenr, "if" ) ;
      if (err) goto ONABORT ;

   } else {

      if (  next >= end_macro
            || (  next[0] != '\''
                  && next[0] != '"')) {
         print_err( "Expected string after <(if () > in line: %d", start_linenr ) ;
         goto ONABORT ;
      }
      ++ next ;
      while (next < end_macro && next[0] != *start_ifstr)
         ++ next ;

      if (next >= end_macro) {
         print_err( "Expected closing %c after <(if () %c> in line: %d", *start_ifstr, *start_ifstr, start_linenr ) ;
         goto ONABORT ;
      }
      ++ start_ifstr ;
      len_ifstr = (size_t) ( next - start_ifstr ) ;

   }

   ++ next ;
   skip_space(&next, end_macro) ;

   if (0 == strncmp(next, "else", 4)) {
      next += 4 ;
      skip_space(&next, end_macro) ;

      start_elsestr = next ;
      if (     next >= end_macro
            || (     next[0] != '\''
                  && next[0] != '"' )) {
         print_err( "Expected string after <(if () '' else> in line: %d", start_linenr ) ;
         goto ONABORT ;
      }

      ++ next ;
      while (next < end_macro && next[0] != *start_elsestr)
         ++ next ;
      if (next >= end_macro) {
         print_err( "Expected closing %c after <(if () '' else %c> in line: %d", *start_elsestr, *start_elsestr, start_linenr ) ;
         goto ONABORT ;
      }
      ++ start_elsestr ;
      len_elsestr = (size_t) ( next - start_elsestr ) ;

      ++ next ;
      skip_space(&next, end_macro) ;
   }

   if (next >= end_macro || ')' != next[0]) {
      print_err( "Expected closing ) after <(if () '' else ''> in line: %d", start_linenr ) ;
      goto ONABORT ;
   }

   err = new_iffunction( &iffunc, expr, start_ifstr, len_ifstr, ifstring2, start_elsestr, len_elsestr) ;
   if (err) goto ONABORT ;

   *funcobj = &iffunc->funcobj ;
   *end_fct = next ;

   return 0 ;
ONABORT:
   delete_expression(&expr) ;
   delete_iffunction(&iffunc) ;
   return EINVAL ;
}

enum select_parameter_type_e {
    seltypeFIELD
   ,seltypeSTRING
   ,seltypeFUNCTION
} ;
typedef enum select_parameter_type_e select_parameter_type_e ;

struct select_parameter_t
{
   select_parameter_t * next ;
   const char *   value ;
   size_t         length ;
   function_t   * funcobj ;
   size_t         col_index ;
   select_parameter_type_e type ;
} ;

static int delete_select_parameter(select_parameter_t ** param)
{
   int err = 0 ;
   select_parameter_t * next = *param ;

   while (next) {
      select_parameter_t * prev = next ;
      next = next->next ;
      if (prev->funcobj) {
         int err2 = prev->funcobj->free( &prev->funcobj ) ;
         if (err2) err = err2 ;
      }
      free(prev) ;
   }

   *param = NULL ;
   return err ;
}

static int extend_select_parameter(/*inout*/select_parameter_t ** next_param)
{
   select_parameter_t * new_param = MALLOC(select_parameter_t,malloc,) ;

   if (!new_param) {
      print_err("Out of memory") ;
      goto ONABORT ;
   }

   if (!(*next_param)) {
      *next_param = new_param ;
   } else {
      (*next_param)->next = new_param ;
      *next_param = new_param ;
   }

   new_param->next      = 0 ;
   new_param->value     = "" ;
   new_param->length    = 0 ;
   new_param->funcobj   = 0 ;
   new_param->col_index = 0 ;
   new_param->type      = seltypeFIELD ;

   return 0 ;
ONABORT:
   return 1 ;
}

static int process_select_parameter(select_parameter_t * select_param, const size_t row, const textdb_t * dbfile, const size_t start_linenr)
{
   int err ;

   textdb_column_t * data = &dbfile->rows[row * dbfile->column_count] ;

   for (const select_parameter_t * p = select_param; p; p = p->next) {
      if (seltypeSTRING == p->type) {
         err = write_outfile( g_outfd, p->value, p->length ) ;
         if (err) goto ONABORT ;
      } else if (seltypeFIELD == p->type) {
         err = write_outfile( g_outfd, data[p->col_index].value, data[p->col_index].length ) ;
         if (err) goto ONABORT ;
      } else if (seltypeFUNCTION == p->type) {
         p->funcobj->process(p->funcobj, row, dbfile, start_linenr) ;
      }
   }

   return 0 ;
ONABORT:
   return err ;
}

static int prepare_select_parameter(select_parameter_t * select_param, const textdb_t * dbfile, const size_t start_linenr)
{
   int err ;

   // match select parameter to header of textdb
   for (select_parameter_t * p = select_param; p; p = p->next) {
      if (seltypeFIELD == p->type) {
         // search matching header
         bool isMatch = false ;
         for (size_t i = 0; i < dbfile->column_count ; ++i) {
            if (  p->length == dbfile->rows[i].length
                  && 0 == strncmp(dbfile->rows[i].value, p->value, p->length) ) {
               p->col_index = i ;
               isMatch = true ;
               break ;
            }
         }
         if (!isMatch) {
            err = EINVAL ;
            print_err( "Unknown column name '%.*s' in SELECT()FROM() in line: %d", p->length, p->value, start_linenr ) ;
            goto ONABORT ;
         }
      } else if (seltypeFUNCTION == p->type) {
         err = p->funcobj->prepare( p->funcobj, dbfile, start_linenr) ;
         if (err) goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   return err ;
}

static int parse_select_parameter(/*out*/select_parameter_t ** param, /*out*/char ** end_param, char * start_param, char * end_macro, size_t start_linenr, const char * cmd )
{
   int err ;
   select_parameter_t first_param = { .next = 0 } ;
   select_parameter_t  * next_param = &first_param ;

   assert(0 == *param) ;

   while (start_param < end_macro && ' ' == start_param[0]) {
      ++ start_param ;
   }

   if (  start_param >= end_macro
         || '(' != start_param[0] ) {
      print_err( "Expected '(' after %s in line: %d", cmd, start_linenr ) ;
      goto ONABORT ;
   }

   char * next = start_param ;

   while (next < end_macro) {
      ++ next ;
      while (  next < end_macro
               && ' ' == next[0]) {
         ++ next ;
      }
      const char * start_field = next ;
      while (  next < end_macro
               && ' ' != next[0]
               && '(' != next[0]
               && ')' != next[0]
               && '"' != next[0]
               && '\'' != next[0]
               && '\\' != next[0]) {
         ++ next ;
      }
      size_t field_len = (size_t) (next - start_field) ;
      if (field_len) {
         err = extend_select_parameter(&next_param) ;
         if (err) goto ONABORT ;
         next_param->value    = start_field ;
         next_param->length   = field_len ;
      }
      if (next < end_macro && '(' == next[0]) {
         char       * start_func = next + 1 ;
         function_t * funcobj    = 0 ;

         if (0 == strncmp( next, "(if ", strlen("(if "))) {
            err = parse_iffunction( &funcobj, &next, next+strlen("(if "), end_macro, start_linenr) ;
         } else {
            print_err( "Unknown function '%.4s' in line: %d", start_func, start_linenr ) ;
            goto ONABORT ;
         }

         err = extend_select_parameter(&next_param) ;
         if (err) {
            funcobj->free( &funcobj ) ;
            goto ONABORT ;
         }

         next_param->value    = start_func ;
         next_param->length   = (size_t) (next - start_func) ;
         next_param->funcobj  = funcobj ;
         next_param->type     = seltypeFUNCTION ;

      } else if (next < end_macro && ' ' != next[0]) {
         const char * start_string = next + 1 ;
         size_t       string_len ;
         if ('"' == next[0]) {
            do {
               ++ next ;
            } while (next < end_macro && '"' != next[0]) ;
            if (next >= end_macro) {
               print_err( "Expected \" after %s(...\" in line: %d", cmd, start_linenr ) ;
               goto ONABORT ;
            }
            string_len = (size_t) (next - start_string) ;
         } else if ('\'' == next[0]) {
            do {
               ++ next ;
            } while (next < end_macro && '\'' != next[0]) ;
            if (next >= end_macro) {
               print_err( "Expected ' after %s(...' in line: %d", cmd, start_linenr ) ;
               goto ONABORT ;
            }
            string_len = (size_t) (next - start_string) ;
         } else if ('\\' == next[0]) {
            ++ next ;
            if (next >= end_macro) {
               print_err( "Expected no endofline after \\ in line: %d", start_linenr ) ;
               goto ONABORT ;
            }
            switch (next[0]) {
            case 'n':   start_string = "\n" ;
                        break ;
            default:    print_err( "Unsupported escaped character \\%c in line: %d", next[0], start_linenr ) ;
                        goto ONABORT ;
            }
            string_len = strlen(start_string) ;
         } else {
            assert(')' == next[0]) ;
            *end_param = next ;
            break ;
         }

         err = extend_select_parameter(&next_param) ;
         if (err) goto ONABORT ;

         next_param->value    = start_string ;
         next_param->length   = string_len ;
         next_param->type     = seltypeSTRING ;
      }
   }

   if ( next >= end_macro ) {
      print_err( "Expected ')' after %s( in line: %d", cmd, start_linenr ) ;
      goto ONABORT ;
   }

   *param = first_param.next ;
   return 0 ;
ONABORT:
   delete_select_parameter(&first_param.next) ;
   return EINVAL ;
}

static int tostring_select_parameter(const select_parameter_t * param, /*out*/char ** string )
{
   char * buffer = 0 ;
   size_t length = 0 ;

   for (const select_parameter_t * p = param; p; p = p->next) {
      length += p->length ;
   }

   buffer= malloc(length+1) ;
   if (!buffer) {
      print_err("Out of memory") ;
      goto ONABORT ;
   }

   size_t offset = 0 ;
   for (const select_parameter_t * p = param; p; p = p->next) {
      memcpy( &buffer[offset], p->value, p->length ) ;
      offset += p->length ;
   }
   assert( offset == length ) ;

   buffer[length] = 0 ;
   *string = buffer ;
   return 0 ;
ONABORT:
   return 1 ;
}

static int process_selectcmd( char * start_macro, char * end_macro, size_t start_linenr )
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
   if (err) goto ONABORT ;

   char * start_from = end_param + 1 ;
   while (start_from < end_macro && ' ' == start_from[0]) {
      ++ start_from ;
   }

   if (     (end_macro - start_from) < 4
         || strncmp( start_from, "FROM", 4) ) {
      err = EINVAL ;
      print_err( "Expected 'FROM' after SELECT() in line: %d", start_linenr ) ;
      goto ONABORT ;
   }

   err = parse_select_parameter( &from_param, &end_param, start_from+4, end_macro, start_linenr, "FROM" ) ;
   if (err) goto ONABORT ;

   ++ end_param ;
   skip_space( &end_param, end_macro ) ;

   char * start_where = end_param ;

   if (     (end_macro - start_where) > 5
         && 0 == strncmp( start_where, "WHERE", 5) ) {
      err = parse_expression( &where_expr, &end_param, start_where+5, end_macro, start_linenr ) ;
      if (err) goto ONABORT ;
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
      goto ONABORT ;
   }

   err = tostring_select_parameter( from_param, &filename ) ;
   if (err) goto ONABORT ;

   // open text database and parse header (CSV format)
   err = init_textdb( &dbfile, filename ) ;
   if (err) goto ONABORT ;

   if (  g_depencyfile
      && 0 != find_depfilenamewritten(filename)) {
      err = insert_depfilenamewritten(filename) ;
      if (err) goto ONABORT ;
      err = write_outfile( g_depfd, " \\\n ", strlen(" \\\n ") ) ;
      if (err) goto ONABORT ;
      err = write_outfile( g_depfd, filename, strlen(filename) ) ;
      if (err) goto ONABORT ;
   }

   // match select parameter to header of textdb
   err = prepare_select_parameter(select_param, &dbfile, start_linenr) ;
   if (err) goto ONABORT ;

   err = matchnames_expression( where_expr, &dbfile, start_linenr ) ;
   if (err) goto ONABORT ;

   // determine row-id
   for (size_t row = 1/*skip header*/, rowid = 1; row < dbfile.row_count; ++row) {
      if (  ismatch_expression( where_expr, row, &dbfile, start_linenr ) ) {
         textdb_column_t * data  = &dbfile.rows[row * dbfile.column_count] ;
         char              buffer[10] ;

         snprintf(buffer, sizeof(buffer), "%u", (int)rowid) ;
         err = appendvalue_textdbcolumn( &data[0], buffer, strlen(buffer)) ;
         if (err) goto ONABORT ;

         ++ rowid ;
      }
   }

   // write for every read row the values of the concatenated select parameters as one line of output
   for (size_t i = 1/*skip header*/; i < dbfile.row_count; ++i) {

      size_t row = (ascending ? i : (dbfile.row_count-i)) ;

      if (  ismatch_expression( where_expr, row, &dbfile, start_linenr ) ) {

         err = process_select_parameter(select_param, row, &dbfile, start_linenr) ;
         if (err) goto ONABORT ;

         err = write_outfile( g_outfd, "\n", 1 ) ;
         if (err) goto ONABORT ;
      }
   }

   free_textdb( &dbfile ) ;
   free(filename) ;
   delete_select_parameter(&select_param) ;
   delete_select_parameter(&from_param) ;
   delete_expression(&where_expr) ;
   return 0 ;
ONABORT:
   free_textdb( &dbfile ) ;
   free(filename) ;
   delete_select_parameter(&select_param) ;
   delete_select_parameter(&from_param) ;
   delete_expression(&where_expr) ;
   return err ;
}

static int find_macro(/*out*/char ** start_pos, /*out*/char ** end_pos, /*inout*/size_t * line_number, char * next_input, size_t input_size)
{
   for (; input_size; --input_size, ++next_input) {
      if ('\n' == *next_input) {
         ++ (*line_number) ;
         if (     input_size > 10
               && 0 == strncmp(&next_input[1], "// TEXTDB:", 10) ) {
            *start_pos  = next_input + 1 ;
            input_size -= 11 ;
            next_input += 11 ;
            for (; input_size; --input_size, ++next_input) {
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

static int process_macro(const mmfile_t * input_file)
{
   int err ;
   char    * next_input = (char*) addr_mmfile(input_file) ;
   size_t    input_size = size_mmfile(input_file) ;
   size_t   line_number = 1 ;

   while (input_size) {
      // find start "// TEXTDB:..."
      size_t  start_linenr ;
      char   * start_macro ;
      char     * end_macro ;
      err = find_macro(&start_macro, &end_macro, &line_number, next_input, input_size) ;
      if (err) break ;
      size_t write_size = (size_t) (end_macro - next_input) ;
      err = write_outfile( g_outfd, next_input, write_size ) ;
      if (err) goto ONABORT ;
      err = write_outfile( g_outfd, "\n", 1) ;
      if (err) goto ONABORT ;
      start_linenr = line_number ;
      input_size  -= write_size ;
      next_input   = end_macro ;

      if (     13 == end_macro - start_macro
            && strncmp(start_macro, "// TEXTDB:END", 13)) {
         print_err( "Found end of macro '// TEXTDB:END' without beginning of macro in line: %d", line_number ) ;
         goto ONABORT ;
      }

      // find end "// TEXTDB:END"
      char   * start_macro2 ;
      char     * end_macro2 ;
      err = find_macro(&start_macro2, &end_macro2, &line_number, next_input, input_size) ;
      if (     err
            || strncmp( start_macro2, "// TEXTDB:END", (size_t) (end_macro2 - start_macro2)) ) {
         print_err( "Can not find end of macro '// TEXTDB:END' which starts at line: %d", start_linenr ) ;
         goto ONABORT ;
      }
      input_size -= (size_t) (end_macro2 - next_input) ;
      next_input  = end_macro2 ;

      // process TEXTDB:command
      size_t macro_size = (size_t) (end_macro - start_macro) ;
      if (  macro_size > sizeof("// TEXTDB:SELECT")
            && (0 == strncmp( start_macro, "// TEXTDB:SELECT", sizeof("// TEXTDB:SELECT")-1)) ) {
         err = process_selectcmd( start_macro, end_macro, start_linenr) ;
         if (err) goto ONABORT ;
      } else  {
         print_err( "Unknown macro '%.*s' in line: %"PRIuSIZE, macro_size > 16 ? 16 : macro_size, start_macro, start_linenr) ;
         goto ONABORT ;
      }

      err = write_outfile( g_outfd, "// TEXTDB:END", sizeof("// TEXTDB:END")-1) ;
      if (err) goto ONABORT ;
   }

   if ( input_size ) {
      err = write_outfile( g_outfd, next_input, input_size ) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   return 1 ;
}

int main(int argc, const char * argv[])
{
   int err ;
   mmfile_t input_file = mmfile_INIT_FREEABLE ;
   int      exclflag   = O_EXCL ;

   err = init_maincontext(maincontext_DEFAULT, argc, argv) ;
   if (err) goto ONABORT ;

   err = process_arguments(argc, argv) ;
   if (err) goto PRINT_USAGE ;

   if (g_foverwrite) exclflag = O_TRUNC ;

   // create out file if -o <filename> is specified
   if (g_outfilename) {
      g_outfd = open(g_outfilename, O_WRONLY|O_CREAT|exclflag|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
      if (g_outfd == -1) {
         print_err( "Can not create file '%s' for writing: %s", g_outfilename, strerror(errno) ) ;
         goto ONABORT ;
      }
   } else {
      g_outfd = STDOUT_FILENO ;
   }

   if (  g_outfilename
      && g_depencyfile) {
      g_depfilename = malloc( strlen(g_outfilename) + 3 ) ;
      if (!g_depfilename) {
         print_err("Out of memory") ;
         goto ONABORT ;
      }
      strcpy(g_depfilename, g_outfilename) ;
      if (     strrchr(g_depfilename, '.')
            && (     ! strrchr(g_depfilename, '/')
                  ||   strrchr(g_depfilename, '/') < strrchr(g_depfilename, '.'))) {
         strrchr(g_depfilename, '.')[1] = 'd' ;
         strrchr(g_depfilename, '.')[2] = 0 ;
      } else {
         strcat(g_depfilename, ".d") ;
      }

      g_depfd = open(g_depfilename, O_WRONLY|O_CREAT|exclflag|O_CLOEXEC, S_IRUSR|S_IWUSR) ;
      if (g_depfd == -1) {
         print_err( "Can not create file '%s' for writing: %s", g_depfilename, strerror(errno) ) ;
         goto ONABORT ;
      }
      err = write_outfile( g_depfd, g_outfilename, strlen(g_outfilename)) ;
      if (err) goto ONABORT ;
      err = write_outfile( g_depfd, ": ", strlen(": ")) ;
      if (err) goto ONABORT ;
      err = write_outfile( g_depfd, g_infilename, strlen(g_infilename)) ;
      if (err) goto ONABORT ;
   }

   // open input file for reading
   err = init_mmfile(&input_file, g_infilename, 0, 0, accessmode_READ, 0) ;
   if (err) {
      print_err( "Can not open file '%s' for reading: %s", g_infilename, strerror(err)) ;
      goto ONABORT ;
   }

   // parse input => expand macros => write output
   err = process_macro( &input_file ) ;
   if (err) goto ONABORT ;

   // flush output to disk
   if (g_outfilename) {
      err = free_file(&g_outfd) ;
      if (err) {
         print_err( "Can not write file '%s': %s", g_outfilename, strerror(errno) ) ;
         goto ONABORT ;
      }
   }

   if (-1 != g_depfd) {
      err = write_outfile( g_depfd, "\n", strlen("\n") ) ;
      if (err) goto ONABORT ;
      err = free_file(&g_depfd) ;
      if (err) {
         print_err( "Can not write file '%s': %s", g_depfilename, strerror(errno) ) ;
         goto ONABORT ;
      }
   }

   free(g_depfilename) ;
   g_depfilename = 0 ;

   free_mmfile(&input_file) ;
   free_maincontext() ;
   free_depfilenamewritten() ;
   return 0 ;
PRINT_USAGE:
   dprintf(STDERR_FILENO, "TextDB version 0.1 - Copyright (c) 2011 Joerg Seebohn\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* TextDB is a textdb macro preprocessor.\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* It reads a C source file and expands the contained textdb macros.\n" ) ;
   dprintf(STDERR_FILENO, "%s", "* The result is written to stdout or <out.c>.\n\n" ) ;
   dprintf(STDERR_FILENO, "Usage:   %s [[-f] [-d] -o <out.c>] <in.c>\n\n", g_programname ) ;
   dprintf(STDERR_FILENO, "Options: -d: Write makefile dependency rule to <out.d>\n") ;
   dprintf(STDERR_FILENO, "         -f: If output file exists force overwrite\n\n") ;
ONABORT:
   if (g_outfilename && g_outfd != -1) {
      free_file(&g_outfd) ;
      unlink(g_outfilename) ;
   }
   if (g_depfd != -1) {
      free_file(&g_depfd) ;
      unlink(g_depfilename) ;
      free(g_depfilename) ;
   }
   free_mmfile(&input_file) ;
   free_maincontext() ;
   free_depfilenamewritten() ;
   return 1 ;
}
