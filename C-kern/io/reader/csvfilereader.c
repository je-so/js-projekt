/* title: CSV-Filereader impl

   Implements <CSV-Filereader>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/memory/wbuffer.h"
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
   int err;
   if (state->offset >= state->length || state->data[state->offset] != chr) {
      err = EINVAL;
      const char str[2] = { (char)chr, 0 };
      TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, state->linenr, colnr_csvfilereaderparsestate(state), str);
      goto ONERR;
   }
   ++ state->offset;
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: parsedatafield_csvfilereaderparsestate
 * Parses value between two double quotes. The function expects the current offset
 * at the first double quote character. */
static int parsedatafield_csvfilereaderparsestate(csvfilereader_parsestate_t * state, /*out*/string_t * value)
{
   int err ;

   err = parsechar_csvfilereaderparsestate(state, (uint8_t)'"') ;
   if (err) goto ONERR;

   size_t start = state->offset ;
   while (  state->offset < state->length
            && state->data[state->offset] != '"'
            && state->data[state->offset] != '\n') {
      ++ state->offset ;
   }
   size_t end = state->offset ;

   err = parsechar_csvfilereaderparsestate(state, (uint8_t)'"') ;
   if (err) goto ONERR;

   if (value) {
      *value = (string_t) string_INIT(end - start, state->data + start) ;
   }

   return 0 ;
ONERR:
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
         if (err) goto ONERR;
         skipempty_csvfilereaderparsestate(&state2) ;
         if (state2.offset >= state2.length || state->linenr != state2.linenr) break ;
         err = parsechar_csvfilereaderparsestate(&state2, (uint8_t)',') ;
         if (err) goto ONERR;
         size_t erroffset = state2.offset ;
         skipempty_csvfilereaderparsestate(&state2) ;
         if (state2.offset >= state2.length || state->linenr != state2.linenr) {
            state2.startofline = state->startofline ;
            state2.offset      = erroffset ;
            err = EINVAL;
            TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, state->linenr, colnr_csvfilereaderparsestate(&state2), "\"");
            goto ONERR;
         }
      }
   }

   *nrcolumns = nrc;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
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
         goto ONERR;
      }
      csvfile->tablevalues = (string_t*) addr_memblock(&mblock) ;
   }

   return 0 ;
ONERR:
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
         if (err) goto ONERR;
      }
      if (oldlinenr == state->linenr) {
         err = EINVAL;
         TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTNEWLINE, state->linenr, colnr_csvfilereaderparsestate(state)) ;
         goto ONERR;
      }
      size_t startofline = state->startofline ;
      oldlinenr = state->linenr ;
      err = parsedatafield_csvfilereaderparsestate(state, &csvfile->tablevalues[tableindex ++]) ;
      if (err) goto ONERR;
      for (size_t i = 1; i < csvfile->nrcolumns; ++i) {
         size_t erroffset = state->offset ;
         skipempty_csvfilereaderparsestate(state) ;
         if (oldlinenr != state->linenr) {
            state->startofline = startofline ;
            state->offset      = erroffset ;
            err = EINVAL;
            TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, oldlinenr, colnr_csvfilereaderparsestate(state), ",") ;
            goto ONERR;
         }
         err = parsechar_csvfilereaderparsestate(state, (uint8_t)',') ;
         if (err) goto ONERR;
         erroffset = state->offset;
         skipempty_csvfilereaderparsestate(state);
         if (oldlinenr != state->linenr) {
            state->startofline = startofline;
            state->offset      = erroffset;
            err = EINVAL;
            TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, oldlinenr, colnr_csvfilereaderparsestate(state), "\"") ;
            goto ONERR;
         }
         err = parsedatafield_csvfilereaderparsestate(state, &csvfile->tablevalues[tableindex ++]);
         if (err) goto ONERR;
      }
      skipempty_csvfilereaderparsestate(state);
   } while (state->offset < state->length);

   csvfile->nrrows = nrrows;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: lifetime

int init_csvfilereader(/*out*/csvfilereader_t * csvfile, const char * filepath)
{
   int err ;

   *csvfile = (csvfilereader_t) csvfilereader_FREE ;

   err = init_mmfile(&csvfile->file, filepath, 0, 0, accessmode_READ, 0) ;
   if (err) goto ONERR;

   csvfilereader_parsestate_t parsestate = csvfilereader_parsestate_INIT(size_mmfile(&csvfile->file), addr_mmfile(&csvfile->file)) ;

   err = parsenrcolumns_csvfilereaderparsestate(&parsestate, &csvfile->nrcolumns) ;
   if (err) goto ONERR;

   if (csvfile->nrcolumns) {
      csvfile->allocated_rows = 1 ;
      size_t tablesize = tablesize_csvfilereader(csvfile) ;
      csvfile->allocated_rows = 0 ;

      if (tablesize / csvfile->nrcolumns != sizeof(csvfile->tablevalues[0])) {
         err = EOVERFLOW ;
         goto ONERR;
      }

      err = parsedata_csvfilereader(csvfile, &parsestate) ;
      if (err) goto ONERR;
   }

   return 0 ;
ONERR:
   free_csvfilereader(csvfile) ;
   TRACEEXIT_ERRLOG(err);
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

   if (err) goto ONERR;

   return 0 ;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err ;
}

int colvalue_csvfilereader(const csvfilereader_t * csvfile, size_t row/*1..nrrows-1*/, size_t column/*0..nrcolumns-1*/, /*out*/struct string_t * colvalue)
{
   int err ;

   VALIDATE_INPARAM_TEST(column < csvfile->nrcolumns, ONERR, PRINTSIZE_ERRLOG(column); PRINTSIZE_ERRLOG(csvfile->nrcolumns)) ;
   VALIDATE_INPARAM_TEST(row < csvfile->nrrows, ONERR, PRINTSIZE_ERRLOG(row); PRINTSIZE_ERRLOG(csvfile->nrrows)) ;

   const size_t offset = csvfile->nrcolumns * row + column ;
   *colvalue = (string_t) string_INIT(csvfile->tablevalues[offset].size, csvfile->tablevalues[offset].addr) ;

   return 0 ;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err ;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   csvfilereader_t csvfile  = csvfilereader_FREE;
   mmfile_t        mmempty  = mmfile_FREE;
   directory_t*    tmpdir   = 0;
   cstring_t       filepath = cstring_INIT;
   file_t          file     = file_FREE;
   char            tmppath[128];

   // prepare
   TEST(0 == newtemp_directory(&tmpdir, "test_initfree", &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(tmppath), (uint8_t*)tmppath)));
   TEST(0 == makefile_directory(tmpdir, "single", 0));
   TEST(0 == init_file(&file, "single", accessmode_WRITE, tmpdir));
   TEST(0 == write_file(file, strlen("\"1\""), "\"1\"", 0));
   TEST(0 == free_file(&file));

   // TEST csvfilereader_FREE
   TEST(0 == memcmp(&mmempty, &csvfile.file, sizeof(mmempty)));
   TEST(0 == csvfile.nrcolumns);
   TEST(0 == csvfile.nrrows);
   TEST(0 == csvfile.allocated_rows);
   TEST(0 == csvfile.tablevalues);

   // TEST init_csvfilereader, free_csvfilereader
   TEST(0 == printfappend_cstring(&filepath, "%s/single", tmppath)) ;
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
   TEST(0 == removefile_directory(tmpdir, "single"));
   TEST(0 == removedirectory_directory(0, tmppath));
   TEST(0 == delete_directory(&tmpdir));
   TEST(0 == free_cstring(&filepath));

   return 0;
ONERR:
   delete_directory(&tmpdir);
   free_cstring(&filepath);
   free_csvfilereader(&csvfile);
   return EINVAL;
}

static int test_query(void)
{
   csvfilereader_t   csvfile  = csvfilereader_FREE ;
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
      string_t colname = string_FREE ;
      TEST(0 == colname_csvfilereader(&csvfile, i, &colname)) ;
      TEST(values[i].addr == colname.addr) ;
      TEST(values[i].size == colname.size) ;
   }

   // TEST colvalue_csvfilereader
   for (unsigned ri = 0; ri < nrrows_csvfilereader(&csvfile); ++ri) {
      for (unsigned ci = 0; ci < nrcolumns_csvfilereader(&csvfile); ++ci) {
         string_t colvalue = string_FREE ;
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
ONERR:
   return EINVAL ;
}

static int test_reading(void)
{
   csvfilereader_t   csvfile  = csvfilereader_FREE;
   directory_t     * tmpdir   = 0;
   mmfile_t          mmempty  = mmfile_FREE;
   cstring_t         filepath = cstring_INIT;
   file_t            file     = file_FREE;
   char              tmppath[256];
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
   TEST(0 == newtemp_directory(&tmpdir, "test_reading", &(wbuffer_t)wbuffer_INIT_STATIC(sizeof(tmppath), (uint8_t*)tmppath)));

   // TEST init_csvfilereader: read filesize = 0
   TEST(0 == makefile_directory(tmpdir, "zero", 0));
   TEST(0 == printfappend_cstring(&filepath, "%s/zero", tmppath));
   TEST(0 == init_csvfilereader(&csvfile, str_cstring(&filepath)));
   TEST(0 == nrcolumns_csvfilereader(&csvfile));
   TEST(0 == nrrows_csvfilereader(&csvfile));
   TEST(0 == csvfile.allocated_rows);
   TEST(0 == csvfile.tablevalues);
   TEST(0 == removefile_directory(tmpdir, "zero"));
   TEST(0 == free_csvfilereader(&csvfile)) ;

   // TEST init_csvfilereader: read empty file (only comments)
   const char * empty ="# gggg\n  \t  # fff\n\n\n# fojsfoj" ;
   TEST(0 == makefile_directory(tmpdir, "empty", strlen(empty))) ;
   TEST(0 == init_file(&file, "empty", accessmode_WRITE, tmpdir)) ;
   TEST(0 == write_file(file, strlen(empty), empty, 0)) ;
   TEST(0 == free_file(&file)) ;
   clear_cstring(&filepath) ;
   TEST(0 == printfappend_cstring(&filepath, "%s/empty", tmppath));
   TEST(0 == init_csvfilereader(&csvfile, str_cstring(&filepath)));
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
      TEST(0 == printfappend_cstring(&filepath, "%s/error", tmppath));
      memset(&csvfile, 255, sizeof(csvfile));
      uint8_t* logbuffer;
      size_t   logstart;
      GETBUFFER_ERRLOG(&logbuffer, &logstart) ;
      TEST(EINVAL == init_csvfilereader(&csvfile, str_cstring(&filepath))) ;
      TEST(0 == memcmp(&mmempty, &csvfile.file, sizeof(mmempty))) ;
      TEST(0 == nrcolumns_csvfilereader(&csvfile)) ;
      TEST(0 == nrrows_csvfilereader(&csvfile)) ;
      TEST(0 == csvfile.allocated_rows) ;
      TEST(0 == csvfile.tablevalues) ;
      TEST(0 == removefile_directory(tmpdir, "error")) ;
      // compare correct column number in error log
      size_t logend ;
      GETBUFFER_ERRLOG(&logbuffer, &logend) ;
      TEST(0 != strstr((char*)logbuffer + logstart, "column: ")) ;
      unsigned colnr ;
      sscanf(strstr((char*)logbuffer + logstart, "column: ")+8, "%u", &colnr) ;
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
   TEST(0 == printfappend_cstring(&filepath, "%s/content", tmppath)) ;
   TEST(0 == init_csvfilereader(&csvfile, str_cstring(&filepath))) ;
   TEST(20 == nrcolumns_csvfilereader(&csvfile)) ;
   TEST(50 == nrrows_csvfilereader(&csvfile)) ;
   for (unsigned rows = 0; rows < 50; ++rows) {
      for (unsigned cols = 0; cols < 20; ++cols) {
         string_t colvalue = string_FREE ;
         char     buffer[100] ;
         sprintf(buffer, "%s%u%u", rows ? "value" : "header", rows, cols) ;
         TEST(0 == colvalue_csvfilereader(&csvfile, rows, cols, &colvalue)) ;
         TEST(strlen(buffer) == colvalue.size) ;
         TEST(0 == strncmp(buffer, (const char*)colvalue.addr, colvalue.size)) ;
      }
   }
   TEST(0 == free_csvfilereader(&csvfile)) ;
   TEST(0 == removefile_directory(tmpdir, "content")) ;

   // reset
   TEST(0 == removedirectory_directory(0, tmppath));
   TEST(0 == delete_directory(&tmpdir));
   TEST(0 == free_cstring(&filepath));

   return 0;
ONERR:
   free_cstring(&filepath);
   free_file(&file);
   delete_directory(&tmpdir);
   free_csvfilereader(&csvfile);
   return EINVAL;
}

int unittest_io_reader_csvfilereader()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_reading())        goto ONERR;

   return 0 ;
ONERR:
   return EINVAL ;
}

#endif
