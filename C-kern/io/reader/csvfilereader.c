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
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/string/string.h"
#include "C-kern/api/string/utf8.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#endif

// === internal types
struct csvparser_t;


/* struct: csvparser_t
 * State held during parsing of input data. */
typedef struct csvparser_t {
   /* variable: data
    * Start address of text data. */
   const uint8_t* data/*length*/;
   /* variable: length
    * Length of text data in bytes. */
   size_t         length;
   /* variable: offset
    * Byte offset into text data. */
   size_t         offset;
   /* variable: offset
    * Byte offset to current start of line. Used to calculate the column number. */
   size_t         startofline;
   /* variable: linenr
    * Current text line nr. */
   size_t         linenr;
   /* variable: nrcolumns
    * The number of columns (Data fields) per row. */
   size_t         nrcolumns;
   /* variable: nrrows
    * The number of rows of data. */
   size_t         nrrows;
   /* variable: allocated_rows
    * The number of allocated rows of <tablevalues>. */
   size_t         allocated_rows;
   /* variable: tablevalues
    * Table of strings indexing memory mapped <file>.
    * The allocated table size is determined by <allocated_rows> and <nrcolumns>.
    * The valid values are determined by <nrrows> and <nrcolumns>. */
   struct string_t * tablevalues/*[nrrows][nrcolumns]*/;
} csvparser_t;

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_csvparser_errtimer
 * Used to simulate errors in different functions. */
static test_errortimer_t s_csvparser_errtimer = test_errortimer_FREE;
#endif

// group: query

/* function: colnr_csvparser
 * Returns column number of current reading position. */
static size_t colnr_csvparser(csvparser_t * state)
{
   return 1 + length_utf8(&state->data[state->startofline], &state->data[state->offset]);
}

/* function: tablesize_csvparser
 * Returns size in bytes of <csvfilereader_t.tablevalues>. */
static inline size_t tablesize_csvparser(csvparser_t * state, size_t nr_allocated_rows)
{
   return nr_allocated_rows * state->nrcolumns * sizeof(state->tablevalues[0]);
}

// group: lifetime

/* define: csvparser_INIT
 * Static initializer. */
#define csvparser_INIT(length, data) \
         { data, length, 0, 0, 1, 0, 0, 0, 0 }

/* function: free_csvparser
 * Frees allocated table. Call this function only in case of
 * an error else the information is transfered into <csvfilereader_t>. */
static int free_csvparser(csvparser_t * state)
{
   int err = 0;

   if (state->tablevalues) {
      memblock_t  mblock = memblock_INIT(tablesize_csvparser(state, state->allocated_rows), (uint8_t*)state->tablevalues);
      state->allocated_rows = 0;
      state->tablevalues = 0;

      err = FREE_MM(&mblock);
      (void) PROCESS_testerrortimer(&s_csvparser_errtimer, &err);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

// group: parse

/* function: resizetable_csvparser
 * Returns size in bytes of <csvparser_t.tablevalues>. */
static int resizetable_csvparser(csvparser_t * state, size_t nrrows)
{
   int err;

   if (state->allocated_rows < nrrows) {
      memblock_t mblock  = memblock_INIT(tablesize_csvparser(state, state->allocated_rows), (uint8_t*)state->tablevalues);
      size_t     newsize = tablesize_csvparser(state, nrrows);
      // state->nrcolumns * sizeof(state->tablevalues[0]) does not overflow checked in init_csvfilereader
      if (newsize / tablesize_csvparser(state, 1) != nrrows) {
         err = EOVERFLOW;
      } else {
         if (! PROCESS_testerrortimer(&s_csvparser_errtimer, &err)) {
            err = RESIZE_MM(newsize, &mblock);
         }
      }
      if (err) goto ONERR;
      state->allocated_rows = nrrows;
      state->tablevalues = (string_t*) addr_memblock(&mblock);
   }

   return 0;
ONERR:
   return err;
}

/* function: skipempty_csvparser
 * Increments offset until next non empty data line is found.
 * Comments begins with a '#' and extends until end of line. */
static void skipempty_csvparser(csvparser_t * state)
{
   while (state->offset < state->length) {
      if (state->data[state->offset] == '\n') {
         state->startofline = ++state->offset;
         ++state->linenr;
      } else if ( state->data[state->offset] == ' '
                  || state->data[state->offset] == '\t'
                  || state->data[state->offset] == '\r') {
         ++state->offset;
      } else if (state->data[state->offset] == '#') {
         while ((++state->offset) < state->length) {
            if (state->data[state->offset] == '\n') break;
         }
      } else {
         break;
      }
   }
}

/* function: parsechar_csvparser
 * Expects the next character to be of value chr.
 * If there end of input is reached or the next charater does not match chr EINVAL is returned.
 * Else the character is consumed. */
static int parsechar_csvparser(csvparser_t * state, uint8_t chr)
{
   int err;
   if (state->offset >= state->length || state->data[state->offset] != chr) {
      err = EINVAL;
      const char str[2] = { (char)chr, 0 };
      TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, state->linenr, colnr_csvparser(state), str);
      goto ONERR;
   }
   ++ state->offset;
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: parsedatafield_csvparser
 * Parses value between two double quotes. The function expects the current offset
 * at the first double quote character. */
static int parsedatafield_csvparser(csvparser_t * state, /*out*/string_t * value)
{
   int err;

   err = parsechar_csvparser(state, (uint8_t)'"');
   if (err) goto ONERR;

   size_t start = state->offset;
   while (  state->offset < state->length
            && state->data[state->offset] != '"'
            && state->data[state->offset] != '\n') {
      ++ state->offset;
   }
   size_t end = state->offset;

   err = parsechar_csvparser(state, (uint8_t)'"');
   if (err) goto ONERR;

   if (value) {
      *value = (string_t) string_INIT(end - start, state->data + start);
   }

   return 0;
ONERR:
   return err;
}

/* function: parsenrcolumns_csvparser
 * Counts the number of data fields of the first line. */
static int parsenrcolumns_csvparser(csvparser_t * state)
{
   int err;

   skipempty_csvparser(state);

   csvparser_t state2 = *state;
   size_t      nrc    = 0;

   if (state2.offset < state2.length) {
      for (;;) {
         ++ nrc;
         err = parsedatafield_csvparser(&state2, 0);
         if (err) goto ONERR;
         skipempty_csvparser(&state2);
         if (state2.offset >= state2.length || state->linenr != state2.linenr) break;
         err = parsechar_csvparser(&state2, (uint8_t)',');
         if (err) goto ONERR;
         size_t erroffset = state2.offset;
         skipempty_csvparser(&state2);
         if (state2.offset >= state2.length || state->linenr != state2.linenr) {
            state2.startofline = state->startofline;
            state2.offset      = erroffset;
            err = EINVAL;
            TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, state->linenr, colnr_csvparser(&state2), "\"");
            goto ONERR;
         }
      }
   }

   state->nrcolumns = nrc;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int parsedata_csvparser(csvparser_t * state)
{
   int err;
   size_t nrrows = 0;
   size_t tableindex = 0;

   size_t oldlinenr = state->linenr - 1;

   do {
      ++ nrrows;
      if (nrrows > state->allocated_rows) {
         if (2*state->allocated_rows > nrrows) {
            err = resizetable_csvparser(state, 2*state->allocated_rows);
         } else { // if (alloc...==0) || (2*alloc... overflows)
            err = resizetable_csvparser(state, nrrows);
         }
         if (err) goto ONERR;
      }
      if (oldlinenr == state->linenr) {
         err = EINVAL;
         TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTNEWLINE, state->linenr, colnr_csvparser(state));
         goto ONERR;
      }
      size_t startofline = state->startofline;
      oldlinenr = state->linenr;
      err = parsedatafield_csvparser(state, &state->tablevalues[tableindex ++]);
      if (err) goto ONERR;
      for (size_t i = 1; i < state->nrcolumns; ++i) {
         size_t erroffset = state->offset;
         skipempty_csvparser(state);
         if (oldlinenr != state->linenr) {
            state->startofline = startofline;
            state->offset      = erroffset;
            err = EINVAL;
            TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, oldlinenr, colnr_csvparser(state), ",");
            goto ONERR;
         }
         err = parsechar_csvparser(state, (uint8_t)',');
         if (err) goto ONERR;
         erroffset = state->offset;
         skipempty_csvparser(state);
         if (oldlinenr != state->linenr) {
            state->startofline = startofline;
            state->offset      = erroffset;
            err = EINVAL;
            TRACE_ERRLOG(log_flags_NONE, PARSEERROR_EXPECTCHAR, oldlinenr, colnr_csvparser(state), "\"");
            goto ONERR;
         }
         err = parsedatafield_csvparser(state, &state->tablevalues[tableindex ++]);
         if (err) goto ONERR;
      }
      skipempty_csvparser(state);
   } while (state->offset < state->length);

   state->nrrows = nrrows;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: csvfilereader_t

// group: lifetime

int init_csvfilereader(/*out*/csvfilereader_t * csvfile, const char * filepath)
{
   int err;
   memblock_t  file_data = memblock_FREE;
   wbuffer_t   wbuf = wbuffer_INIT_MEMBLOCK(&file_data);
   csvparser_t state;

   if (PROCESS_testerrortimer(&s_csvparser_errtimer, &err)) goto ONERR_FREEDSTATE;

   err = load_file(filepath, &wbuf, 0);
   (void) PROCESS_testerrortimer(&s_csvparser_errtimer, &err);
   if (err) goto ONERR_FREEDSTATE;

   state = (csvparser_t) csvparser_INIT(size_wbuffer(&wbuf), file_data.addr);

   err = parsenrcolumns_csvparser(&state);
   (void) PROCESS_testerrortimer(&s_csvparser_errtimer, &err);
   if (err) goto ONERR;

   if (PROCESS_testerrortimer(&s_csvparser_errtimer, &err)) {
      state.nrcolumns = 1u + (size_t)-1 / sizeof(state.tablevalues[0]);
   }

   if (state.nrcolumns) {
      size_t tablesize = tablesize_csvparser(&state, 1);

      if (tablesize / state.nrcolumns != sizeof(state.tablevalues[0])) {
         err = EOVERFLOW;
         goto ONERR;
      }

      err = parsedata_csvparser(&state);
      (void) PROCESS_testerrortimer(&s_csvparser_errtimer, &err);
      if (err) goto ONERR;
   }

   // set out
   *cast_memblock(csvfile, file_) = file_data;
   csvfile->nrcolumns   = state.nrcolumns;
   csvfile->nrrows      = state.nrrows;
   csvfile->tablesize   = tablesize_csvparser(&state, state.allocated_rows);
   csvfile->tablevalues = state.tablevalues;

   return 0;
ONERR:
   free_csvparser(&state);
ONERR_FREEDSTATE:
   FREE_MM(&file_data);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_csvfilereader(csvfilereader_t * csvfile)
{
   int err;

   err = FREE_MM(cast_memblock(csvfile, file_));
   (void) PROCESS_testerrortimer(&s_csvparser_errtimer, &err);

   if (csvfile->tablevalues) {
      memblock_t mblock = memblock_INIT(csvfile->tablesize, (uint8_t*)csvfile->tablevalues);
      csvfile->tablevalues = 0;

      int err2 = FREE_MM(&mblock);
      (void) PROCESS_testerrortimer(&s_csvparser_errtimer, &err2);
      if (err2) err = err2;
   }

   csvfile->nrcolumns = 0;
   csvfile->nrrows    = 0;
   csvfile->tablesize = 0;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int colvalue_csvfilereader(const csvfilereader_t * csvfile, size_t row/*1..nrrows-1*/, size_t column/*0..nrcolumns-1*/, /*out*/struct string_t * colvalue)
{
   int err;

   VALIDATE_INPARAM_TEST(column < csvfile->nrcolumns, ONERR, PRINTSIZE_ERRLOG(column); PRINTSIZE_ERRLOG(csvfile->nrcolumns));
   VALIDATE_INPARAM_TEST(row < csvfile->nrrows, ONERR, PRINTSIZE_ERRLOG(row); PRINTSIZE_ERRLOG(csvfile->nrrows));

   const size_t offset = csvfile->nrcolumns * row + column;
   *colvalue = (string_t) string_INIT(csvfile->tablevalues[offset].size, csvfile->tablevalues[offset].addr);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}



// group: test

#ifdef KONFIG_UNITTEST

static int test_csvparser(void)
{
   uint8_t     data[8];
   csvparser_t state = csvparser_INIT(sizeof(data), data);

   // TEST csvparser_INIT
   TEST( data == state.data);
   TEST( 8 == state.length);
   TEST( 0 == state.offset);
   TEST( 0 == state.startofline);
   TEST( 1 == state.linenr);
   TEST( 0 == state.nrcolumns);
   TEST( 0 == state.nrrows);
   TEST( 0 == state.allocated_rows);
   TEST( 0 == state.tablevalues);

   // TEST free_csvparser
   state.nrcolumns = 13;
   TEST( 0 == resizetable_csvparser(&state, 9));
   TEST( 9 == state.allocated_rows);
   TEST( 0 != state.tablevalues);
   // test
   TEST( 0 == free_csvparser(&state));
   // check state
   TEST( 0 == state.allocated_rows);
   TEST( 0 == state.tablevalues);

   // TEST free_csvparser: ERROR
   state.nrcolumns = 13;
   TEST( 0 == resizetable_csvparser(&state, 9));
   init_testerrortimer(&s_csvparser_errtimer, 1, EINVAL);
   TEST( EINVAL == free_csvparser(&state));
   // check state
   TEST( 13 == state.nrcolumns);
   TEST( 0 == state.allocated_rows);
   TEST( 0 == state.tablevalues);

   // TEST resizetable_csvparser: parameter 0
   TEST( 0 == resizetable_csvparser(&state, 0));
   TEST( 13 == state.nrcolumns);
   TEST( 0 == state.allocated_rows);
   TEST( 0 == state.tablevalues);

   // TEST resizetable_csvparser
   for (size_t r = 1; r <= 5; ++r) {
      TEST( 0 == resizetable_csvparser(&state, r));
      // check state
      TEST( 13 == state.nrcolumns);
      TEST( 0 == state.nrrows);
      TEST( r == state.allocated_rows);
      TEST( 0 != state.tablevalues);
   }
   // reset
   TEST(0 == free_csvparser(&state));
   TEST(13 == state.nrcolumns);

   // TEST resizetable_csvparser: EOVERFLOW
   TEST( EOVERFLOW == resizetable_csvparser(&state, 1u + (size_t)-1 / (13*sizeof(string_t))));
   TEST( 13 == state.nrcolumns);
   TEST( 0 == state.nrrows);
   TEST( 0 == state.allocated_rows);
   TEST( 0 == state.tablevalues);

   // TEST resizetable_csvparser: ERROR
   init_testerrortimer(&s_csvparser_errtimer, 1, ENOMEM);
   TEST( ENOMEM == resizetable_csvparser(&state, 1));
   TEST( 13 == state.nrcolumns);
   TEST( 0 == state.nrrows);
   TEST( 0 == state.allocated_rows);
   TEST( 0 == state.tablevalues);

   // TEST resizetable_csvparser: nrrows <= allocated_rows
   state.allocated_rows = 5;
   for (size_t r = 0; r <= 5; ++r) {
      TEST( 0 == resizetable_csvparser(&state, r));
      // check state
      TEST( 13 == state.nrcolumns);
      TEST( 0 == state.nrrows);
      TEST( 5 == state.allocated_rows);
      TEST( 0 == state.tablevalues);
   }
   // reset
   state.allocated_rows = 0;

   // TEST tablesize_csvparser
   for (size_t r = 0; r <= 10; ++r) {
      for (size_t c = 0; c <= 10; ++c) {
         const size_t S = c * r * sizeof(string_t);
         state.nrcolumns = c;
         TEST( S == tablesize_csvparser(&state, r));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   csvfilereader_t csvfile  = csvfilereader_FREE;
   directory_t*    tmpdir   = 0;
   file_t          file     = file_FREE;
   char            filepath[256];
   char            tmppath[128];
   int             F;

   // prepare
   TEST(0 == newtemp_directory(&tmpdir, "test_initfree", &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(tmppath), (uint8_t*)tmppath)));
   TEST(0 == makefile_directory(tmpdir, "single", 0));
   TEST(0 == init_file(&file, "single", accessmode_WRITE, tmpdir));
   TEST(0 == write_file(file, strlen("\"1\""), "\"1\"", 0));
   TEST(0 == free_file(&file));

   // TEST csvfilereader_FREE
   TEST( 0 == csvfile.file_addr);
   TEST( 0 == csvfile.file_size);
   TEST( 0 == csvfile.nrcolumns);
   TEST( 0 == csvfile.nrrows);
   TEST( 0 == csvfile.tablesize);
   TEST( 0 == csvfile.tablevalues);

   // TEST init_csvfilereader
   F = (int) (strlen(tmppath) + strlen("/single"));
   TEST( F == snprintf(filepath, sizeof(filepath), "%s/single", tmppath));
   TEST( 0 == init_csvfilereader(&csvfile, filepath));
   TEST( 0 != csvfile.file_addr);
   TEST( 3 == csvfile.file_size);
   TEST( 1 == csvfile.nrcolumns);
   TEST( 1 == csvfile.nrrows);
   TEST( sizeof(csvfile.tablevalues[0]) == csvfile.tablesize);
   TEST( 0 != csvfile.tablevalues);

   // TEST free_csvfilereader
   for (int ti = 0; ti < 2; ++ti) {
      TEST( 0 == free_csvfilereader(&csvfile));
      TEST( 0 == csvfile.file_addr);
      TEST( 0 == csvfile.file_size);
      TEST( 0 == csvfile.nrcolumns);
      TEST( 0 == csvfile.nrrows);
      TEST( 0 == csvfile.tablesize);
      TEST( 0 == csvfile.tablevalues);
   }

   // TEST init_csvfilereader: ERROR
   for (unsigned e = 1;; ++e) {
      // test
      init_testerrortimer(&s_csvparser_errtimer, e, (int) e);
      const unsigned err = (unsigned) init_csvfilereader(&csvfile, filepath);
      if (! err) {
         free_testerrortimer(&s_csvparser_errtimer);
         TEST(7 == e);
         TEST(0 == free_csvfilereader(&csvfile));
         break;
      }
      TEST( err == (4 == e ? EOVERFLOW : e));
      // check csvfile
      TEST( 0 == csvfile.file_addr);
      TEST( 0 == csvfile.file_size);
      TEST( 0 == csvfile.nrcolumns);
      TEST( 0 == csvfile.nrrows);
      TEST( 0 == csvfile.tablesize);
      TEST( 0 == csvfile.tablevalues);
   }

   // TEST free_csvfilereader: ERROR
   for (unsigned e = 1;; ++e) {
      TEST(0 == init_csvfilereader(&csvfile, filepath));
      // test
      init_testerrortimer(&s_csvparser_errtimer, e, (int) e);
      const unsigned err = (unsigned) free_csvfilereader(&csvfile);
      if (! err) {
         free_testerrortimer(&s_csvparser_errtimer);
         TEST(3 == e);
         break;
      }
      TEST( e == err);
      // check csvfile
      TEST( 0 == csvfile.file_addr);
      TEST( 0 == csvfile.file_size);
      TEST( 0 == csvfile.nrcolumns);
      TEST( 0 == csvfile.nrrows);
      TEST( 0 == csvfile.tablesize);
      TEST( 0 == csvfile.tablevalues);
   }

   // unprepare
   TEST(0 == removefile_directory(tmpdir, "single"));
   TEST(0 == removedirectory_directory(0, tmppath));
   TEST(0 == delete_directory(&tmpdir));

   return 0;
ONERR:
   delete_directory(&tmpdir);
   free_csvfilereader(&csvfile);
   return EINVAL;
}

static int test_query(void)
{
   csvfilereader_t   csvfile  = csvfilereader_FREE;
   string_t          values[] = {
      string_INIT_CSTR("1"), string_INIT_CSTR("22"), string_INIT_CSTR("333"), string_INIT_CSTR("444"),
      string_INIT_CSTR("v1"), string_INIT_CSTR("v2"), string_INIT_CSTR("v3"), string_INIT_CSTR("v4"),
      string_INIT_CSTR("v5"), string_INIT_CSTR("v6"), string_INIT_CSTR("v7"), string_INIT_CSTR("v8"),
   };

   // TEST nrcolumns_csvfilereader
   for (size_t i = 1; i; i <<= 1) {
      csvfile.nrcolumns = i;
      TEST(i == nrcolumns_csvfilereader(&csvfile));
   }
   csvfile.nrcolumns = 0;
   TEST(0 == nrcolumns_csvfilereader(&csvfile));

   // TEST nrrows_csvfilereader
   for (size_t i = 1; i; i <<= 1) {
      csvfile.nrrows = i;
      TEST(i == nrrows_csvfilereader(&csvfile));
   }
   csvfile.nrrows = 0;
   TEST(0 == nrrows_csvfilereader(&csvfile));

   // TEST colname_csvfilereader
   csvfile.nrcolumns = 4;
   csvfile.nrrows    = 3;
   csvfile.tablevalues = values;
   for (unsigned i = 0; i < nrcolumns_csvfilereader(&csvfile); ++i) {
      string_t colname = string_FREE;
      TEST(0 == colname_csvfilereader(&csvfile, i, &colname));
      TEST(values[i].addr == colname.addr);
      TEST(values[i].size == colname.size);
   }

   // TEST colvalue_csvfilereader
   for (unsigned ri = 0; ri < nrrows_csvfilereader(&csvfile); ++ri) {
      for (unsigned ci = 0; ci < nrcolumns_csvfilereader(&csvfile); ++ci) {
         string_t colvalue = string_FREE;
         TEST(0 == colvalue_csvfilereader(&csvfile, ri, ci, &colvalue));
         TEST(values[ri * nrcolumns_csvfilereader(&csvfile) + ci].addr == colvalue.addr);
         TEST(values[ri * nrcolumns_csvfilereader(&csvfile) + ci].size == colvalue.size);
      }
   }

   // TEST EINVAL
   string_t colname;
   string_t colvalue;
   TEST(EINVAL == colname_csvfilereader(&csvfile, nrcolumns_csvfilereader(&csvfile), &colname));
   TEST(EINVAL == colname_csvfilereader(&csvfile, (size_t)-1, &colname));
   TEST(EINVAL == colvalue_csvfilereader(&csvfile, nrrows_csvfilereader(&csvfile), 0, &colvalue));
   TEST(EINVAL == colvalue_csvfilereader(&csvfile, (size_t)-1, 0, &colvalue));

   return 0;
ONERR:
   return EINVAL;
}

static int test_reading(void)
{
   csvfilereader_t   csvfile  = csvfilereader_FREE;
   directory_t     * tmpdir   = 0;
   file_t            file     = file_FREE;
   char              tmppath[128];
   char              filepath[256];
   int               F;
   const char      * errdata[] = {
         "\"h1\", \"h2\" x",    // extra data in header
          "\"h1\", \"h2\", \n\"h3\"",  // header field on next line
          "\"h1\", \"h2\",",     // missing field in header
          "\"h1\", \"h2\", \"",  // missing field in header
          "\"h1\", \"h2\"\n\"v1\", \"v2\" \"v3\", \"v4\"", // extra characters after second data field v2
          "\"h1\", \"h2\"\n\"v1\"\n",     // unexpected end of input
          "\"h1\", \"h2\"\n\"v1\",\n",    // unexpected end of input
          "\"h1\", \"h2\"\n\"v1\", \"v2", // unexpected end of input
          "\"v\n1\"\n"                    // data field contains '\n'
   };
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
   };

   // prepare
   TEST(0 == newtemp_directory(&tmpdir, "test_reading", &(wbuffer_t)wbuffer_INIT_STATIC(sizeof(tmppath), (uint8_t*)tmppath)));

   // TEST init_csvfilereader: read filesize = 0
   F = (int) (strlen(tmppath) + strlen("/zero"));
   TEST(F == snprintf(filepath, sizeof(filepath), "%s/zero", tmppath));
   TEST(0 == makefile_directory(tmpdir, "zero", 0));
   TEST(0 == init_csvfilereader(&csvfile, filepath));
   TEST(0 == nrcolumns_csvfilereader(&csvfile));
   TEST(0 == nrrows_csvfilereader(&csvfile));
   TEST(0 == csvfile.tablesize);
   TEST(0 == csvfile.tablevalues);
   TEST(0 == removefile_directory(tmpdir, "zero"));
   TEST(0 == free_csvfilereader(&csvfile));

   // TEST init_csvfilereader: read empty file (only comments)
   F = (int) (strlen(tmppath) + strlen("/empty"));
   TEST(F == snprintf(filepath, sizeof(filepath), "%s/empty", tmppath));
   const char * empty ="# gggg\n  \t  # fff\n\n\n# fojsfoj";
   TEST(0 == makefile_directory(tmpdir, "empty", strlen(empty)));
   TEST(0 == init_file(&file, "empty", accessmode_WRITE, tmpdir));
   TEST(0 == write_file(file, strlen(empty), empty, 0));
   TEST(0 == free_file(&file));
   TEST(0 == init_csvfilereader(&csvfile, filepath));
   TEST(0 == nrcolumns_csvfilereader(&csvfile));
   TEST(0 == nrrows_csvfilereader(&csvfile));
   TEST(0 == csvfile.tablesize);
   TEST(0 == csvfile.tablevalues);
   TEST(0 == removefile_directory(tmpdir, "empty"));
   TEST(0 == free_csvfilereader(&csvfile));

   // prepare
   F = (int) (strlen(tmppath) + strlen("/error"));
   TEST(F == snprintf(filepath, sizeof(filepath), "%s/error", tmppath));

   // TEST init_csvfilereader: read error
   for (unsigned i = 0; i < lengthof(errdata); ++i) {
      // prepare
      TEST(0 == makefile_directory(tmpdir, "error", strlen(errdata[i])));
      TEST(0 == init_file(&file, "error", accessmode_WRITE, tmpdir));
      TEST(0 == write_file(file, strlen(errdata[i]), errdata[i], 0));
      TEST(0 == free_file(&file));
      uint8_t* logbuffer;
      size_t   logstart;
      GETBUFFER_ERRLOG(&logbuffer, &logstart);
      // test
      csvfile = (csvfilereader_t) csvfilereader_FREE;
      TEST( EINVAL == init_csvfilereader(&csvfile, filepath));
      TEST( 0 == csvfile.file_addr);
      TEST( 0 == csvfile.file_size);
      TEST( 0 == csvfile.nrcolumns);
      TEST( 0 == csvfile.nrrows);
      TEST( 0 == csvfile.tablesize);
      TEST( 0 == csvfile.tablevalues);
      // compare correct column number in error log
      size_t logend;
      GETBUFFER_ERRLOG(&logbuffer, &logend);
      TEST(0 != strstr((char*)logbuffer + logstart, "column: "));
      unsigned colnr;
      sscanf(strstr((char*)logbuffer + logstart, "column: ")+8, "%u", &colnr);
      TEST(errcol[i] == colnr);
      // reset
      TEST(0 == removefile_directory(tmpdir, "error"));
   }

   // prepare
   F = (int) (strlen(tmppath) + strlen("/content"));
   TEST(F == snprintf(filepath, sizeof(filepath), "%s/content", tmppath));

   // TEST init_csvfilereader: content is parsed correctly
   TEST(0 == makefile_directory(tmpdir, "content", 0));
   TEST(0 == init_file(&file, "content", accessmode_WRITE, tmpdir));
   const char * comment = "  # xxx \n";
   for (unsigned rows = 0; rows < 50; ++rows) {
      TEST(0 == write_file(file, strlen(comment), comment, 0));
      for (unsigned cols = 0; cols < 20; ++cols) {
         char buffer[100];
         sprintf(buffer, "\"%s%u%u\"%s", rows ? "value" : "header", rows, cols, cols != 19 ? " \t, \t" : " \r # s1289e0u,\"\",\r\n");
         TEST(0 == write_file(file, strlen(buffer), buffer, 0));
      }
   }
   TEST(0 == free_file(&file));
   TEST( 0 == init_csvfilereader(&csvfile, filepath));
   TEST( 20 == nrcolumns_csvfilereader(&csvfile));
   TEST( 50 == nrrows_csvfilereader(&csvfile));
   for (unsigned rows = 0; rows < 50; ++rows) {
      for (unsigned cols = 0; cols < 20; ++cols) {
         string_t colvalue = string_FREE;
         char     buffer[100];
         sprintf(buffer, "%s%u%u", rows ? "value" : "header", rows, cols);
         TEST( 0 == colvalue_csvfilereader(&csvfile, rows, cols, &colvalue));
         TEST( strlen(buffer) == colvalue.size);
         TEST( 0 == strncmp(buffer, (const char*)colvalue.addr, colvalue.size));
      }
   }
   TEST(0 == free_csvfilereader(&csvfile));
   TEST(0 == removefile_directory(tmpdir, "content"));

   // reset
   TEST(0 == removedirectory_directory(0, tmppath));
   TEST(0 == delete_directory(&tmpdir));

   return 0;
ONERR:
   free_file(&file);
   delete_directory(&tmpdir);
   free_csvfilereader(&csvfile);
   return EINVAL;
}

int unittest_io_reader_csvfilereader()
{
   if (test_csvparser())   goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_reading())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
