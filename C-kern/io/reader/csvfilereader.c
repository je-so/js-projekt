/* title: CSV-Filereader impl

   Implements <CSV-Filereader>.

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

#include "C-kern/konfig.h"
#include "C-kern/api/io/reader/csvfilereader.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/utf8.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/string/cstring.h"
#endif

/* typedef: csvfilereader_parsestate_t
 * Export <csvfilereader_parsestate_t> into global namespace. */
typedef struct csvfilereader_parsestate_t    csvfilereader_parsestate_t ;


/* struct: csvfilereader_parsestate_t
 * State held during parsing of input data. */
struct csvfilereader_parsestate_t {
   /* variable: data
    * Start address of text data. */
   const uint8_t  * data/*length*/ ;
   /* variable: length
    * Length of text data in bytes. */
   size_t         length ;
   /* variable: offset
    * Byte offset into text data. */
   size_t         offset ;
   /* variable: offset
    * Byte offset to current start of line. Used to calculate the column number. */
   size_t         startofline ;
   /* variable: linenr
    * Current text line nr. */
   size_t         linenr ;
} ;

/* define: csvfilereader_parsestate_INIT
 * Static initializer. */
#define csvfilereader_parsestate_INIT(length, data)      { data, length, 0, 0, 1 }

/* function: skipempty_csvfilereaderparsestate
 * Increments offset until next non empty data line is found.
 * Comments begins with a '#' and extends until end of line. */
static void skipempty_csvfilereaderparsestate(csvfilereader_parsestate_t * state)
{
   while (state->offset < state->length) {
      if (state->data[state->offset] == '\n') {
         state->startofline = ++state->offset ;
         ++state->linenr ;
      } else if ( state->data[state->offset] == ' '
                  || state->data[state->offset] == '\t'
                  || state->data[state->offset] == '\r') {
         ++state->offset ;
      } else if (state->data[state->offset] == '#') {
         while ((++state->offset) < state->length) {
            if (state->data[state->offset] == '\n') break ;
         }
      } else {
         break ;
      }
   }
}

static size_t colnr_csvfilereaderparsestate(csvfilereader_parsestate_t * state)
{
   return 1 + length_utf8(&state->data[state->startofline], &state->data[state->offset]) ;
}

/* function: parsechar_csvfilereaderparsestate
 * Expects the next character to be of value chr.
 * If there end of input is reached or the next charater does not match chr EINVAL is returned.
 * Else the character is consumed. */
static int parsechar_csvfilereaderparsestate(csvfilereader_parsestate_t * state, uint8_t chr)
{
   if (state->offset >= state->length || state->data[state->offset] != chr) {
      const char str[2] = { (char)chr, 0 } ;
      TRACEERR_LOG(PARSEERROR_EXPECTCHAR, EINVAL, state->linenr, colnr_csvfilereaderparsestate(state), str) ;
      return EINVAL ;
   }
   ++ state->offset ;
   return 0 ;
}

/* function: parsedatafield_csvfilereaderparsestate
 * Parses value between two double quotes. The function expects the current offset
 * at the first double quote character. */
static int parsedatafield_csvfilereaderparsestate(csvfilereader_parsestate_t * state, /*out*/string_t * value)
{
   int err ;

   err = parsechar_csvfilereaderparsestate(state, (uint8_t)'"') ;
   if (err) goto ONABORT ;

   size_t start = state->offset ;
   while (  state->offset < state->length
            && state->data[state->offset] != '"'
            && state->data[state->offset] != '\n') {
      ++ state->offset ;
   }
   size_t end = state->offset ;

   err = parsechar_csvfilereaderparsestate(state, (uint8_t)'"') ;
   if (err) goto ONABORT ;

   if (value) {
      *value = (string_t) string_INIT(end - start, state->data + start) ;
   }

   return 0 ;
ONABORT:
   return err ;
}

/* function: parsenrcolumns_csvfilereaderparsestate
 * Counts the number of data fields of the first line. */
static int parsenrcolumns_csvfilereaderparsestate(csvfilereader_parsestate_t * state, /*out*/size_t * nrcolumns)
{
   int err ;

   skipempty_csvfilereaderparsestate(state) ;

   csvfilereader_parsestate_t state2 = *state ;
   size_t                     nrc    = 0 ;

   if (state2.offset < state2.length) {
      for (;;) {
         ++ nrc ;
         err = parsedatafield_csvfilereaderparsestate(&state2, 0) ;
         if (err) goto ONABORT ;
         skipempty_csvfilereaderparsestate(&state2) ;
         if (state2.offset >= state2.length || state->linenr != state2.linenr) break ;
         err = parsechar_csvfilereaderparsestate(&state2, (uint8_t)',') ;
         if (err) goto ONABORT ;
         size_t erroffset = state2.offset ;
         skipempty_csvfilereaderparsestate(&state2) ;
         if (state2.offset >= state2.length || state->linenr != state2.linenr) {
            state2.startofline = state->startofline ;
            state2.offset      = erroffset ;
            err = EINVAL ;
            TRACEERR_LOG(PARSEERROR_EXPECTCHAR, err, state->linenr, colnr_csvfilereaderparsestate(&state2), "\"") ;
            goto ONABORT ;
         }
      }
   }

   *nrcolumns = nrc ;

   return 0 ;
ONABORT:
   return err ;
}


// section: csvfilereader_t

// group: helper

/* function: tablesize_csvfilereader
 * Returns size in bytes of <csvfilereader_t.tablevalues>. */
static inline size_t tablesize_csvfilereader(csvfilereader_t * csvfile)
{
   return csvfile->allocated_rows * csvfile->nrcolumns * sizeof(csvfile->tablevalues[0]) ;
}

static int resizetable_csvfilereader(csvfilereader_t * csvfile, size_t nrrows)
{
   int err ;

   if (csvfile->allocated_rows < nrrows) {
      size_t     oldalloc = csvfile->allocated_rows ;
      memblock_t mblock   = memblock_INIT(tablesize_csvfilereader(csvfile), (uint8_t*)csvfile->tablevalues) ;
      csvfile->allocated_rows = nrrows ;
      size_t     newtablesize = tablesize_csvfilereader(csvfile) ;
      // csvfile->nrcolumns * sizeof(csvfile->tablevalues[0]) does not overflow checked in init_csvfilereader
      if (newtablesize / (csvfile->nrcolumns * sizeof(csvfile->tablevalues[0])) != nrrows) {
         err = EOVERFLOW ;
      } else {
         err = RESIZE_MM(newtablesize, &mblock) ;
      }
      if (err) {
         csvfile->allocated_rows = oldalloc ;
         goto ONABORT ;
      }
      csvfile->tablevalues = (string_t*) addr_memblock(&mblock) ;
   }

   return 0 ;
ONABORT:
   return err ;
}

static int parsedata_csvfilereader(csvfilereader_t * csvfile, csvfilereader_parsestate_t * state)
{
   int err ;
   size_t nrrows = 0 ;
   size_t tableindex = 0 ;

   size_t oldlinenr = state->linenr - 1 ;

   do {
      ++ nrrows ;
      if (nrrows > csvfile->allocated_rows) {
         if (2*csvfile->allocated_rows > nrrows) {
            err = resizetable_csvfilereader(csvfile, 2*csvfile->allocated_rows) ;
         } else { // if (alloc...==0) || (2*alloc... overflows)
            err = resizetable_csvfilereader(csvfile, nrrows) ;
         }
         if (err) goto ONABORT ;
      }
      if (oldlinenr == state->linenr) {
         err = EINVAL ;
         TRACEERR_LOG(PARSEERROR_EXPECTNEWLINE, err, state->linenr, colnr_csvfilereaderparsestate(state)) ;
         goto ONABORT ;
      }
      size_t startofline = state->startofline ;
      oldlinenr = state->linenr ;
      err = parsedatafield_csvfilereaderparsestate(state, &csvfile->tablevalues[tableindex ++]) ;
      if (err) goto ONABORT ;
      for (size_t i = 1; i < csvfile->nrcolumns; ++i) {
         size_t erroffset = state->offset ;
         skipempty_csvfilereaderparsestate(state) ;
         if (oldlinenr != state->linenr) {
            state->startofline = startofline ;
            state->offset      = erroffset ;
            err = EINVAL ;
            TRACEERR_LOG(PARSEERROR_EXPECTCHAR, err, oldlinenr, colnr_csvfilereaderparsestate(state), ",") ;
            goto ONABORT ;
         }
         err = parsechar_csvfilereaderparsestate(state, (uint8_t)',') ;
         if (err) goto ONABORT ;
         erroffset = state->offset ;
         skipempty_csvfilereaderparsestate(state) ;
         if (oldlinenr != state->linenr) {
            state->startofline = startofline ;
            state->offset      = erroffset ;
            err = EINVAL ;
            TRACEERR_LOG(PARSEERROR_EXPECTCHAR, err, oldlinenr, colnr_csvfilereaderparsestate(state), "\"") ;
            goto ONABORT ;
         }
         err = parsedatafield_csvfilereaderparsestate(state, &csvfile->tablevalues[tableindex ++]) ;
         if (err) goto ONABORT ;
      }
      skipempty_csvfilereaderparsestate(state) ;
   } while (state->offset < state->length) ;

   csvfile->nrrows = nrrows ;

   return 0 ;
ONABORT:
   return err ;
}

// group: lifetime

int init_csvfilereader(/*out*/csvfilereader_t * csvfile, const char * filepath)
{
   int err ;

   *csvfile = (csvfilereader_t) csvfilereader_INIT_FREEABLE ;

   err = init_mmfile(&csvfile->file, filepath, 0, 0, accessmode_READ, 0) ;
   if (err) goto ONABORT ;

   csvfilereader_parsestate_t parsestate = csvfilereader_parsestate_INIT(size_mmfile(&csvfile->file), addr_mmfile(&csvfile->file)) ;

   err = parsenrcolumns_csvfilereaderparsestate(&parsestate, &csvfile->nrcolumns) ;
   if (err) goto ONABORT ;

   if (csvfile->nrcolumns) {
      csvfile->allocated_rows = 1 ;
      size_t tablesize = tablesize_csvfilereader(csvfile) ;
      csvfile->allocated_rows = 0 ;

      if (tablesize / csvfile->nrcolumns != sizeof(csvfile->tablevalues[0])) {
         err = EOVERFLOW ;
         goto ONABORT ;
      }

      err = parsedata_csvfilereader(csvfile, &parsestate) ;
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   free_csvfilereader(csvfile) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_csvfilereader(csvfilereader_t * csvfile)
{
   int err ;

   err = free_mmfile(&csvfile->file) ;

   if (csvfile->tablevalues) {
      memblock_t mblock = memblock_INIT(tablesize_csvfilereader(csvfile), (uint8_t*)csvfile->tablevalues) ;
      csvfile->tablevalues = 0 ;

      int err2 = FREE_MM(&mblock) ;
      if (err) err = err2 ;
   }

   csvfile->nrcolumns = 0 ;
   csvfile->nrrows    = 0 ;
   csvfile->allocated_rows = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int colvalue_csvfilereader(const csvfilereader_t * csvfile, size_t row/*1..nrrows-1*/, size_t column/*0..nrcolumns-1*/, /*out*/struct string_t * colvalue)
{
   int err ;

   VALIDATE_INPARAM_TEST(column < csvfile->nrcolumns, ONABORT, PRINTSIZE_LOG(column); PRINTSIZE_LOG(csvfile->nrcolumns)) ;
   VALIDATE_INPARAM_TEST(row < csvfile->nrrows, ONABORT, PRINTSIZE_LOG(row); PRINTSIZE_LOG(csvfile->nrrows)) ;

   const size_t offset = csvfile->nrcolumns * row + column ;
   *colvalue = (string_t) string_INIT(csvfile->tablevalues[offset].size, csvfile->tablevalues[offset].addr) ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   csvfilereader_t  csvfile = csvfilereader_INIT_FREEABLE ;
   mmfile_t         mmempty = mmfile_INIT_FREEABLE ;
   directory_t      * tmpdir = 0 ;
   cstring_t        tmppath  = cstring_INIT ;
   cstring_t        filepath = cstring_INIT ;
   file_t           file     = file_INIT_FREEABLE ;

   // prepare
   TEST(0 == newtemp_directory(&tmpdir, "test_initfree", &tmppath)) ;
   TEST(0 == makefile_directory(tmpdir, "single", 0)) ;
   TEST(0 == init_file(&file, "single", accessmode_WRITE, tmpdir)) ;
   TEST(0 == write_file(file, strlen("\"1\""), "\"1\"", 0)) ;
   TEST(0 == free_file(&file)) ;

   // TEST csvfilereader_INIT_FREEABLE
   TEST(0 == memcmp(&mmempty, &csvfile.file, sizeof(mmempty))) ;
   TEST(0 == csvfile.nrcolumns) ;
   TEST(0 == csvfile.nrrows) ;
   TEST(0 == csvfile.allocated_rows) ;
   TEST(0 == csvfile.tablevalues) ;

   // TEST init_csvfilereader, free_csvfilereader
   TEST(0 == printfappend_cstring(&filepath, "%s/single", str_cstring(&tmppath))) ;
   TEST(0 == init_csvfilereader(&csvfile, str_cstring(&filepath))) ;
   TEST(1 == csvfile.nrcolumns) ;
   TEST(1 == csvfile.nrrows) ;
   TEST(1 == csvfile.allocated_rows) ;
   TEST(0 != csvfile.tablevalues) ;
   TEST(0 == free_csvfilereader(&csvfile)) ;
   TEST(0 == memcmp(&mmempty, &csvfile.file, sizeof(mmempty))) ;
   TEST(0 == csvfile.nrcolumns) ;
   TEST(0 == csvfile.nrrows) ;
   TEST(0 == csvfile.allocated_rows) ;
   TEST(0 == csvfile.tablevalues) ;
   TEST(0 == free_csvfilereader(&csvfile)) ;
   TEST(0 == memcmp(&mmempty, &csvfile.file, sizeof(mmempty))) ;
   TEST(0 == csvfile.nrcolumns) ;
   TEST(0 == csvfile.nrrows) ;
   TEST(0 == csvfile.allocated_rows) ;
   TEST(0 == csvfile.tablevalues) ;

   // unprepare
   TEST(0 == removefile_directory(tmpdir, "single")) ;
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == delete_directory(&tmpdir)) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == free_cstring(&filepath)) ;

   return 0 ;
ONABORT:
   delete_directory(&tmpdir) ;
   free_cstring(&tmppath) ;
   free_cstring(&filepath) ;
   free_csvfilereader(&csvfile) ;
   return EINVAL ;
}

static int test_query(void)
{
   csvfilereader_t   csvfile  = csvfilereader_INIT_FREEABLE ;
   string_t          values[] = {
                                 string_INIT_CSTR("1"), string_INIT_CSTR("22"), string_INIT_CSTR("333"), string_INIT_CSTR("444"),
                                 string_INIT_CSTR("v1"), string_INIT_CSTR("v2"), string_INIT_CSTR("v3"), string_INIT_CSTR("v4"),
                                 string_INIT_CSTR("v5"), string_INIT_CSTR("v6"), string_INIT_CSTR("v7"), string_INIT_CSTR("v8"),
                              } ;

   // TEST nrcolumns_csvfilereader
   for (size_t i = 1; i; i <<= 1) {
      csvfile.nrcolumns = i ;
      TEST(i == nrcolumns_csvfilereader(&csvfile)) ;
   }
   csvfile.nrcolumns = 0 ;
   TEST(0 == nrcolumns_csvfilereader(&csvfile)) ;

   // TEST nrrows_csvfilereader
   for (size_t i = 1; i; i <<= 1) {
      csvfile.nrrows = i ;
      TEST(i == nrrows_csvfilereader(&csvfile)) ;
   }
   csvfile.nrrows = 0 ;
   TEST(0 == nrrows_csvfilereader(&csvfile)) ;

   // TEST colname_csvfilereader
   csvfile.nrcolumns = 4 ;
   csvfile.nrrows    = 3 ;
   csvfile.tablevalues = values ;
   for (unsigned i = 0; i < nrcolumns_csvfilereader(&csvfile); ++i) {
      string_t colname = string_INIT_FREEABLE ;
      TEST(0 == colname_csvfilereader(&csvfile, i, &colname)) ;
      TEST(values[i].addr == colname.addr) ;
      TEST(values[i].size == colname.size) ;
   }

   // TEST colvalue_csvfilereader
   for (unsigned ri = 0; ri < nrrows_csvfilereader(&csvfile); ++ri) {
      for (unsigned ci = 0; ci < nrcolumns_csvfilereader(&csvfile); ++ci) {
         string_t colvalue = string_INIT_FREEABLE ;
         TEST(0 == colvalue_csvfilereader(&csvfile, ri, ci, &colvalue)) ;
         TEST(values[ri * nrcolumns_csvfilereader(&csvfile) + ci].addr == colvalue.addr) ;
         TEST(values[ri * nrcolumns_csvfilereader(&csvfile) + ci].size == colvalue.size) ;
      }
   }

   // TEST EINVAL
   string_t colname ;
   string_t colvalue ;
   TEST(EINVAL == colname_csvfilereader(&csvfile, nrcolumns_csvfilereader(&csvfile), &colname)) ;
   TEST(EINVAL == colname_csvfilereader(&csvfile, (size_t)-1, &colname)) ;
   TEST(EINVAL == colvalue_csvfilereader(&csvfile, nrrows_csvfilereader(&csvfile), 0, &colvalue)) ;
   TEST(EINVAL == colvalue_csvfilereader(&csvfile, (size_t)-1, 0, &colvalue)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_reading(void)
{
   csvfilereader_t   csvfile  = csvfilereader_INIT_FREEABLE ;
   directory_t       * tmpdir = 0 ;
   mmfile_t          mmempty  = mmfile_INIT_FREEABLE ;
   cstring_t         tmppath  = cstring_INIT ;
   cstring_t         filepath = cstring_INIT ;
   file_t            file     = file_INIT_FREEABLE ;
   const char        * errdata[] = {
                        "\"h1\", \"h2\" x",    // extra data in header
                         "\"h1\", \"h2\", \n\"h3\"",  // header field on next line
                         "\"h1\", \"h2\",",     // missing field in header
                         "\"h1\", \"h2\", \"",  // missing field in header
                         "\"h1\", \"h2\"\n\"v1\", \"v2\" \"v3\", \"v4\"", // extra characters after second data field v2
                         "\"h1\", \"h2\"\n\"v1\"\n",     // unexpected end of input
                         "\"h1\", \"h2\"\n\"v1\",\n",    // unexpected end of input
                         "\"h1\", \"h2\"\n\"v1\", \"v2", // unexpected end of input
                         "\"v\n1\"\n"                    // data field contains '\n'
                     } ;
   unsigned          errcol[lengthof(errdata)] = {
                        12,
                        12,
                        12,
                        14,
                        12,
                        5,
                        6,
                        10,
                        3
                     } ;

   // prepare
   TEST(0 == newtemp_directory(&tmpdir, "test_reading", &tmppath)) ;

   // TEST init_csvfilereader: read filesize = 0
   TEST(0 == makefile_directory(tmpdir, "zero", 0)) ;
   TEST(0 == printfappend_cstring(&filepath, "%s/zero", str_cstring(&tmppath))) ;
   TEST(0 == init_csvfilereader(&csvfile, str_cstring(&filepath))) ;
   TEST(0 == nrcolumns_csvfilereader(&csvfile)) ;
   TEST(0 == nrrows_csvfilereader(&csvfile)) ;
   TEST(0 == csvfile.allocated_rows) ;
   TEST(0 == csvfile.tablevalues) ;
   TEST(0 == removefile_directory(tmpdir, "zero")) ;
   TEST(0 == free_csvfilereader(&csvfile)) ;

   // TEST init_csvfilereader: read empty file (only comments)
   const char * empty ="# gggg\n  \t  # fff\n\n\n# fojsfoj" ;
   TEST(0 == makefile_directory(tmpdir, "empty", strlen(empty))) ;
   TEST(0 == init_file(&file, "empty", accessmode_WRITE, tmpdir)) ;
   TEST(0 == write_file(file, strlen(empty), empty, 0)) ;
   TEST(0 == free_file(&file)) ;
   clear_cstring(&filepath) ;
   TEST(0 == printfappend_cstring(&filepath, "%s/empty", str_cstring(&tmppath))) ;
   TEST(0 == init_csvfilereader(&csvfile, str_cstring(&filepath))) ;
   TEST(0 == nrcolumns_csvfilereader(&csvfile)) ;
   TEST(0 == nrrows_csvfilereader(&csvfile)) ;
   TEST(0 == csvfile.allocated_rows) ;
   TEST(0 == csvfile.tablevalues) ;
   TEST(0 == removefile_directory(tmpdir, "empty")) ;
   TEST(0 == free_csvfilereader(&csvfile)) ;

   // TEST init_csvfilereader: read error
   for (unsigned i = 0; i < lengthof(errdata); ++i) {
      TEST(0 == makefile_directory(tmpdir, "error", strlen(errdata[i]))) ;
      TEST(0 == init_file(&file, "error", accessmode_WRITE, tmpdir)) ;
      TEST(0 == write_file(file, strlen(errdata[i]), errdata[i], 0)) ;
      TEST(0 == free_file(&file)) ;
      clear_cstring(&filepath) ;
      TEST(0 == printfappend_cstring(&filepath, "%s/error", str_cstring(&tmppath))) ;
      memset(&csvfile, 255, sizeof(csvfile)) ;
      char     * logbuffer ;
      size_t   logstart ;
      GETBUFFER_LOG(&logbuffer, &logstart) ;
      TEST(EINVAL == init_csvfilereader(&csvfile, str_cstring(&filepath))) ;
      TEST(0 == memcmp(&mmempty, &csvfile.file, sizeof(mmempty))) ;
      TEST(0 == nrcolumns_csvfilereader(&csvfile)) ;
      TEST(0 == nrrows_csvfilereader(&csvfile)) ;
      TEST(0 == csvfile.allocated_rows) ;
      TEST(0 == csvfile.tablevalues) ;
      TEST(0 == removefile_directory(tmpdir, "error")) ;
      // compare correct column number in error log
      size_t logend ;
      GETBUFFER_LOG(&logbuffer, &logend) ;
      TEST(0 != strstr(logbuffer + logstart, "column: ")) ;
      unsigned colnr ;
      sscanf(strstr(logbuffer + logstart, "column: ")+8, "%u", &colnr) ;
      TEST(errcol[i] == colnr) ;
   }

   // TEST init_csvfilereader: content is parsed correctly
   TEST(0 == makefile_directory(tmpdir, "content", 0)) ;
   TEST(0 == init_file(&file, "content", accessmode_WRITE, tmpdir)) ;
   const char * comment = "  # xxx \n" ;
   for (unsigned rows = 0; rows < 50; ++rows) {
      TEST(0 == write_file(file, strlen(comment), comment, 0)) ;
      for (unsigned cols = 0; cols < 20; ++cols) {
         char buffer[100] ;
         sprintf(buffer, "\"%s%u%u\"%s", rows ? "value" : "header", rows, cols, cols != 19 ? " \t, \t" : " \r # s1289e0u,\"\",\r\n") ;
         TEST(0 == write_file(file, strlen(buffer), buffer, 0)) ;
      }
   }
   TEST(0 == free_file(&file)) ;
   clear_cstring(&filepath) ;
   TEST(0 == printfappend_cstring(&filepath, "%s/content", str_cstring(&tmppath))) ;
   TEST(0 == init_csvfilereader(&csvfile, str_cstring(&filepath))) ;
   TEST(20 == nrcolumns_csvfilereader(&csvfile)) ;
   TEST(50 == nrrows_csvfilereader(&csvfile)) ;
   for (unsigned rows = 0; rows < 50; ++rows) {
      for (unsigned cols = 0; cols < 20; ++cols) {
         string_t colvalue = string_INIT_FREEABLE ;
         char     buffer[100] ;
         sprintf(buffer, "%s%u%u", rows ? "value" : "header", rows, cols) ;
         TEST(0 == colvalue_csvfilereader(&csvfile, rows, cols, &colvalue)) ;
         TEST(strlen(buffer) == colvalue.size) ;
         TEST(0 == strncmp(buffer, (const char*)colvalue.addr, colvalue.size)) ;
      }
   }
   TEST(0 == free_csvfilereader(&csvfile)) ;
   TEST(0 == removefile_directory(tmpdir, "content")) ;

   // unprepare
   TEST(0 == removedirectory_directory(0, str_cstring(&tmppath))) ;
   TEST(0 == delete_directory(&tmpdir)) ;
   TEST(0 == free_cstring(&tmppath)) ;
   TEST(0 == free_cstring(&filepath)) ;

   return 0 ;
ONABORT:
   free_cstring(&tmppath) ;
   free_cstring(&filepath) ;
   free_file(&file) ;
   delete_directory(&tmpdir) ;
   free_csvfilereader(&csvfile) ;
   return EINVAL ;
}

int unittest_io_reader_csvfilereader()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())       goto ONABORT ;
   if (test_query())          goto ONABORT ;
   if (test_reading())        goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
