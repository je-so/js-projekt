/* title: CSV-Filereader

   Supports reading a CSV text file.

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

   file: C-kern/api/io/reader/csvfilereader.h
    Header file <CSV-Filereader>.

   file: C-kern/io/reader/csvfilereader.c
    Implementation file <CSV-Filereader impl>.
*/
#ifndef CKERN_IO_READER_CSVFILEREADER_HEADER
#define CKERN_IO_READER_CSVFILEREADER_HEADER

#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/mmfile.h"

// forward
struct string_t ;

/* typedef: struct csvfilereader_t
 * Export <csvfilereader_t> into global namespace. */
typedef struct csvfilereader_t         csvfilereader_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_reader_csvfilereader
 * Test <csvfilereader_t> functionality. */
int unittest_io_reader_csvfilereader(void) ;
#endif


/* struct: csvfilereader_t
 * Reads a CSV file and allows to access the values.
 * The file is accessed with help of <mmfile_t>.
 *
 * CSV File Format:
 * See <CSV-File-Format>.
 * */
struct csvfilereader_t {
   /* variable: file
    * The read file content mapped to memory. */
   mmfile_t       file ;
   /* variable: nrcolumns
    * The number of columns (Data fields) per row. */
   size_t         nrcolumns ;
   /* variable: nrrows
    * The number of rows of data. */
   size_t         nrrows ;
   /* variable: allocated_rows
    * The number of allocated rows of <tablevalues>. */
   size_t         allocated_rows ;
   /* variable: tablevalues
    * Table of strings indexing memory mapped <file>.
    * The allocated table size is determined by <allocated_rows> and <nrcolumns>.
    * The valid values are determined by <nrrows> and <nrcolumns>. */
   struct string_t * tablevalues/*[nrrows][nrcolumns]*/ ;
} ;

// group: lifetime

/* define: csvfilereader_INIT_FREEABLE
 * Static initializer. */
#define csvfilereader_INIT_FREEABLE    { mmfile_INIT_FREEABLE, 0, 0, 0, 0 }

/* function: init_csvfilereader
 * Opens file and reads all contained values.
 * All rows must have the same number of columns.
 *
 * TODO: Return error information in extra struct ! Genral Parsing Error Framework ? */
int init_csvfilereader(/*out*/csvfilereader_t * csvfile, const char * filepath) ;

/* function: free_csvfilereader
 * Closes file and frees memory for parsed values. */
int free_csvfilereader(csvfilereader_t * csvfile) ;

// group: query

/* function: nrcolumns_csvfilereader
 * The number of columns (data fields) per row contained in the input data. */
size_t nrcolumns_csvfilereader(const csvfilereader_t * csvfile) ;

/* function: nrrows_csvfilereader
 * The number of rows (data records) contained in the input data. */
size_t nrrows_csvfilereader(const csvfilereader_t * csvfile) ;

/* function: colname_csvfilereader
 * The name of a column. This name is defined in the first row of data and is the same for all following rows. */
int colname_csvfilereader(const csvfilereader_t * csvfile, size_t column/*0..nrcolumns-1*/, /*out*/struct string_t * colname) ;

/* function: colvalue_csvfilereader
 * Returns the value of a single column in a certain row.
 * The index of the columns a value between 0 and <nrcolumns_csvfilereader>-1.
 * The index of the row is a value between 1 and <nrrows_csvfilereader>-1.
 * A row index of 0 is the same as calling <colname_csvfilereader>. */
int colvalue_csvfilereader(const csvfilereader_t * csvfile, size_t row/*1..nrrows-1*/, size_t column/*0..nrcolumns-1*/, /*out*/struct string_t * colvalue) ;

// group: description

/* about: CSV-File-Format
 *
 * It is a file format for storing tabular data as plain text.
 * A text line contains one data row (record) they are separated by new lines ('\n').
 * Any additional carriage return '\r' is ignored.
 * Every row contains the same number of columns (data fields).
 * A single value in a row is separated by comma from another value.
 * It must be enclosed in double quotes.
 *
 * Example:
 * > # The first line contains the column names
 * > "filename",        "proglang",    "authors"
 * > # The following lines defines the data
 * > "test_file_1.cpp", "C++",         "MR. X1, MR. X2"
 * > "test_file_2.c",   "C",           "MR. Y"
 * > "test_file_3.sh",  "shell",       "MR. Z"
 *
 * A double quote or new line character as part of a value is not supported.
 * It must be represented by an escape sequence of a higher level abstraction
 * like "\x22" or "\042" used in C-strings.
 *
 * First Row:
 * The values of the first row are considered to be the header names of the corresponding column.
 *
 * Comments:
 * Lines beginning with a '#' are recognized as comments and are ignored.
 *
 * Character Encoding:
 * The reader assumes that comma, white space and double quotes are encoded in ASCII. UTF-8 is supported or other code pages if
 * comma, white space and double quotes uses the same codes as in ASCII.
 */


// section: inline implementation

/* define: colname_csvfilereader
 * Implements <csvfilereader_t.colname_csvfilereader>. */
#define colname_csvfilereader(csvfile, column, colname) \
   (colvalue_csvfilereader((csvfile), 0, (column), (colname)))

/* define: nrcolumns_csvfilereader
 * Implements <csvfilereader_t.nrcolumns_csvfilereader>. */
#define nrcolumns_csvfilereader(csvfile)           ((csvfile)->nrcolumns)

/* define: nrrows_csvfilereader
 * Implements <csvfilereader_t.nrrows_csvfilereader>. */
#define nrrows_csvfilereader(csvfile)              ((csvfile)->nrrows)

#endif
