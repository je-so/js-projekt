/* title: RegularExpression impl

   Implements <RegularExpression>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/proglang/regexpr.h
    Header file <RegularExpression>.

   file: C-kern/proglang/regexpr.c
    Implementation file <RegularExpression impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/proglang/regexpr.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/inmem/slist.h"
#include "C-kern/api/ds/inmem/node/slist_node.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/memstream.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/string/utf8.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// === private types
struct buffer_t;

// forward

static int parse_regexpr(struct buffer_t* buffer);

#ifdef KONFIG_UNITTEST
static test_errortimer_t s_regex_errtimer;
#endif


// struct: regexpr_err_t

// group: log

void log_regexprerr(const regexpr_err_t *err, uint8_t channel)
{
   if (err->type <= 1) {
      PRINTTEXT_LOG(,channel, log_flags_NONE, 0, PARSEERROR_EXPECT_INSTEADOF_ERRLOG, err->expect, err->type ? 0 : err->unexpected);
   } else if (err->type == 2) {
      PRINTTEXT_LOG(,channel, log_flags_NONE, 0, PARSEERROR_UNEXPECTED_CHAR_ERRLOG, err->unexpected);
   } else if (err->type == 3) {
      PRINTTEXT_LOG(,channel, log_flags_NONE, 0, PARSEERROR_ILLEGALCHARACTERENCODING_ERRLOG, err->unexpected);
   }
}

/* struct: buffer_t
 * Siehe auch <memstream_ro_t> und <memstream_t>.
 *
 * Invariant:
 * Anzahl lesbare Bytes == (size_t) (end-next). */
typedef struct buffer_t {
   automat_t         mman;
   memstream_ro_t    input;
   /*== out ==*/
   automat_t         result;
   regexpr_err_t     err;
} buffer_t;

// group: lifetime

int init_buffer(/*out*/buffer_t* buffer, size_t len, const char str[len]) \
{
   int err;

   err = initempty_automat(&buffer->mman, 0);
   if (err) goto ONERR;
   buffer->input = (memstream_ro_t) memstream_INIT((const uint8_t*)str, (const uint8_t*)str + len);

   return 0;
ONERR:
   return err;
}

int free_buffer(buffer_t* buffer)
{
   int err;

   err = free_automat(&buffer->mman);
   if (err) goto ONERR;
   buffer->input = (memstream_ro_t) memstream_FREE;

   return 0;
ONERR:
   return err;
}

// group: parsing

/* function: read_next
 * Liest das nächste Byte, das nicht ein Leerzeichen ist und bewegt den Lesezeiger.
 * Leerzeichen werden überlesen. Falls ein Leerzeichen zurückgegeben
 * wird, wurde das Eingabeende erreicht. */
static inline uint8_t read_next(buffer_t* buffer)
{
   uint8_t next = ' ';
   while (isnext_memstream(&buffer->input)) {
      next = nextbyte_memstream(&buffer->input);
      if (next != ' ') break;
   }
   return next;
}

/* function: skip_next
 * Bewegt den Lesezeiger ein Byte weiter.
 *
 * Unchecked Precondition:
 * - Last call to <peek_next> returned value != ' '. */
static inline void skip_next(buffer_t* buffer)
{
   skip_memstream(&buffer->input, 1);
}

/* function: peek_next
 * Gibt das aktuelle Byte an der Leseposition zurück, ohne den Lesezeiger zu bewegen.
 * Falls jedoch ein Leerzeichen an der aktuellen Position steht, wird der Lesezeiger
 * solange weiterbewegt, bis die Position eines Nichtleerzeichens erreicht wird.
 * Falls ein Leerzeichen zurückgegeben wird, wurde das Eingabeende erreicht. */
static inline uint8_t peek_next(buffer_t* buffer)
{
   uint8_t next = ' ';
   while (isnext_memstream(&buffer->input)) {
      next = *buffer->input.next;
      if (next != ' ') break;
      skip_memstream(&buffer->input, 1);
   }
   return next;
}

static inline int parse_utf8(buffer_t* buffer, uint8_t next, char32_t* chr)
{
   unsigned nrbytes = sizePfirst_utf8(next);
   if (  nrbytes > size_memstream(&buffer->input)+1
         || nrbytes != decodechar_utf8(&next_memstream(&buffer->input)[-1], chr)) {
      buffer->err.type = 3;
      buffer->err.chr  = next;
      buffer->err.pos  = (const char*) buffer->input.next - 1;
      buffer->err.expect = 0;
      nrbytes = nrbytes > size_memstream(&buffer->input)+1 ? size_memstream(&buffer->input)+1 : nrbytes;
      buffer->err.unexpected[0] = (char) next;
      for (size_t i = 1; i < nrbytes; ++i) {
         buffer->err.unexpected[i] = (char) buffer->input.next[i-1];
      }
      buffer->err.unexpected[nrbytes] = 0;
      return EILSEQ;
   }
   skip_memstream(&buffer->input, nrbytes-1u);

   return 0;
}

/* function: parse_char
 * Falls next der Anfang eines utf-8 kodierten Zeichens ist, wird dieses dekodiert in chr
 * zurückgegeben. Dabei soviele Bytes aus buffer konsumiert wie nötig.
 * Besitzt next keine weiteren Bytes, wird dessen Wert direkt in chr übergeben,
 * ohne weitere Bytes zu konsumieren. */
static inline int parse_char(buffer_t* buffer, uint8_t next, /*out*/char32_t* chr)
{
   if (issinglebyte_utf8(next)) {
      if (next == '\\' && isnext_memstream(&buffer->input)) {
         next = nextbyte_memstream(&buffer->input);
         if (issinglebyte_utf8(next)) {
            if (next == 'n') next = '\n';
            else if (next == 'r') next = '\r';
            else if (next == 't') next = '\t';
            *chr = next;
            return 0;
         }
      } else {
         *chr = next;
         return 0;
      }
   }

   return parse_utf8(buffer, next, chr);
}

static inline int ERR_EXPECT_OR_UNMATCHED(buffer_t* buffer, const char* expect, const uint8_t next, bool isEndOfFile)
{
   buffer->err.type = expect ? isEndOfFile ? 1 : 0 : 2;
   buffer->err.chr  = next;
   buffer->err.pos  = (const char*) buffer->input.next - !isEndOfFile;
   buffer->err.expect = expect;

   if (! isEndOfFile && ! issinglebyte_utf8(next)) {
      int err = parse_utf8(buffer, next, &buffer->err.chr);
      if (err) return err;
   }

   static_assert( maxsize_utf8() < sizeof(buffer->err.unexpected), "check array overflow");
   if (isEndOfFile) {
      buffer->err.unexpected[0] = (char) next;
      buffer->err.unexpected[1] = 0;
   } else {
      uint8_t len = encodechar_utf8(buffer->err.chr, sizeof(buffer->err.unexpected), (uint8_t*)buffer->err.unexpected);
      buffer->err.unexpected[len] = 0;
   }

   return ESYNTAX;
}

static int operator_not(buffer_t* buffer)
{
   int err;
   automat_t notchar;

   err = initmatch_automat(&notchar, &buffer->mman, 1, (char32_t[]){ 0 }, (char32_t[]){ 0x7fffffff });
   if (err) return err;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = opandnot_automat(&notchar, &buffer->result);
   }
   if (err) {
      free_automat(&notchar);
   } else {
      initmove_automat(&buffer->result, &notchar);
   }

   return err;
}

static int operator_optional(buffer_t* buffer)
{
   int err;
   automat_t empty;

   err = initempty_automat(&empty, &buffer->mman);
   if (err) return err;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = opor_automat(&buffer->result, &empty);
   }
   if (err) free_automat(&empty);

   return err;
}


// section: regexpr_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_regex_errtimer
 * Simuliert Fehler in Funktionen für <buffer_t> und <regexpr_t>. */
static test_errortimer_t   s_regex_errtimer = test_errortimer_FREE;
#endif

// group: parsing

/* function: parse_atom
 * Erwartet wird mindestens ein Zeichen.
 * '.', '[' und '(' und '\\' werden gesondert behandelt. */
static int parse_atom(buffer_t* buffer)
{
   int err;
   uint8_t next = read_next(buffer);

   // recognize '.' | '[' | ']' | '(' | ')'

   if (next == ' ') {
      err = initempty_automat(&buffer->result, &buffer->mman);
      if (err) goto ONERR;
   } else if (next == '(') {
      err = parse_regexpr(buffer);
      if (err) goto ONERR;
      next = read_next(buffer);
      if (next != ')') {
         err = ERR_EXPECT_OR_UNMATCHED(buffer, ")", next, next == ' ');
         goto ONERR;
      }
   } else if (next == '[') {
      bool isFirst = true;
      bool isNot = (peek_next(buffer) == '^');
      if (isNot) skip_next(buffer);
      for (;;) {
         char32_t from, to;
         next = read_next(buffer);
         if (next == ' ') {
            err = ERR_EXPECT_OR_UNMATCHED(buffer, "]", next, true/*end of file*/);
            goto ONERR;
         }
         if (next == ']') break;
         err = parse_char(buffer, next, &from);
         if (err) goto ONERR;
         if (peek_next(buffer) == '-') {
            /* range */
            skip_next(buffer);
            err = parse_char(buffer, read_next(buffer), &to);
            if (err) goto ONERR;
            if (to == ']') {
               err = ERR_EXPECT_OR_UNMATCHED(buffer, "<char>", ']', false/*end of file*/);
               goto ONERR;
            }
         } else {
            /* single char */
            to = from;
         }
         if (isFirst) {
            isFirst = false;
            err = initmatch_automat(&buffer->result, &buffer->mman, 1, (char32_t[]){ from }, (char32_t[]){ to });
         } else {
            err = extendmatch_automat(&buffer->result, 1, (char32_t[]){ from }, (char32_t[]){ to });
         }
         if (err) goto ONERR;
      }
      if (isFirst) {
         err = initempty_automat(&buffer->result, &buffer->mman);
         if (err) goto ONERR;
      }
      if (isNot) {
         err = operator_not(buffer);
         if (err) goto ONERR;
      }
   } else {
      char32_t from;
      char32_t to;
      if (next == '.') {
         from = 0;
         to   = 0x7fffffff;
      } else {
         err = parse_char(buffer, next, &from);
         if (err) goto ONERR;
         to = from;
      }
      err = initmatch_automat(&buffer->result, &buffer->mman, 1, (char32_t[]){ from }, (char32_t[]){ to });
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

/* function: parse_sequence
 * Analysiert Syntax einer regulären Sequenz und baut einen enstprechenden Automaten vom Typ <automat_t>. */
static int parse_sequence(buffer_t* buffer)
{
   int err;
   int isResult = 0;
   uint8_t next = peek_next(buffer);
   automat_t seqresult;

   do {
      int isNot = 0;
      while (next == '!') {
         skip_next(buffer);
         isNot = !isNot;
         next = peek_next(buffer);
      }

      if (next == '*' || next == '+' || next == '|' || next == '&' || next == ')' || next == ']' || next == '?') {
         skip_next(buffer);
         err = ERR_EXPECT_OR_UNMATCHED(buffer, "<char>", next, false);
         goto ONERR;
      }

      err = parse_atom(buffer);
      if (err) goto ONERR;

      next = peek_next(buffer);
      if (next == '*' || next == '+') {
         skip_next(buffer);
         err = oprepeat_automat(&buffer->result, next == '+');
         if (err) goto ONERR;
         next = peek_next(buffer);
      } else if (next == '?') {
         skip_next(buffer);
         err = operator_optional(buffer);
         if (err) goto ONERR;
         next = peek_next(buffer);
      }

      if (isNot) {
         err = opnot_automat(&buffer->result);
         if (err) goto ONERR;
      }

      if (isResult) {
         err = opsequence_automat(&seqresult, &buffer->result);
         if (err) goto ONERR;
      } else {
         isResult = 1;
         initmove_automat(&seqresult, &buffer->result);
      }

   } while (next != ' ' && next != '|' && next != '&' && next != ')');

   initmove_automat(&buffer->result, &seqresult);

   return 0;
ONERR:
   if (isResult) {
      free_automat(&seqresult);
   }
   return err;
}

/* function: parse_regexpr
 * Analysiert Syntax und baut einen enstprechenden Automaten vom Typ <automat_t>. */
static int parse_regexpr(buffer_t* buffer)
{
   int err;
   uint8_t op = 0;
   uint8_t next = peek_next(buffer);
   automat_t regexresult;

   for (;;) {

      if (next == '|' || next == '&' || next == ')') {
         err = initempty_automat(&buffer->result, &buffer->mman);
         if (err) goto ONERR;
      } else {
         err = parse_sequence(buffer);
         if (err) goto ONERR;
         next = peek_next(buffer);
      }

      if (op) {
         if (op == '!') {
            err = opandnot_automat(&regexresult, &buffer->result);
            if (err) goto ONERR;
         } else if (op == '&') {
            err = opand_automat(&regexresult, &buffer->result);
            if (err) goto ONERR;
         } else {
            err = opor_automat(&regexresult, &buffer->result);
            if (err) goto ONERR;
         }
      } else {
         op = '|'; // mark regexresult as valid (free result in error case)
         initmove_automat(&regexresult, &buffer->result);
      }

      if (next == '|') {
         op = next;
         skip_next(buffer);
         next = peek_next(buffer);
      } else if (next == '&') {
         op = next;
         skip_next(buffer);
         if (isnext_memstream(&buffer->input) && *next_memstream(&buffer->input) == '!') {
            op = '!';
            skip_next(buffer);
         }
         next = peek_next(buffer);
      } else {
         break;
      }
   }

   initmove_automat(&buffer->result, &regexresult);

   return 0;
ONERR:
   if (op) {
      free_automat(&regexresult);
   }
   return err;
}

// group: lifetime

int free_regexpr(regexpr_t* regex)
{
   int err;

   err = free_automat(&regex->matcher);
   PROCESS_testerrortimer(&s_regex_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_regexpr(/*out*/regexpr_t* regex, size_t len, const char definition[len], /*err*/regexpr_err_t *errdescr)
{
   int err;
   int isBuffer = 0;
   buffer_t buffer;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = init_buffer(&buffer, len, definition);
   }
   if (err) goto ONERR;

   buffer.result = (automat_t) automat_FREE;
   isBuffer = 1;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = parse_regexpr(&buffer);
   }
   if (err) goto ONERR;

   uint8_t next = read_next(&buffer);
   if (next != ' ') {
      err = ERR_EXPECT_OR_UNMATCHED(&buffer, 0, next, false);
      goto ONERR;
   }

   err = free_buffer(&buffer);
   PROCESS_testerrortimer(&s_regex_errtimer, &err);
   if (err) goto ONERR;

   err = minimize_automat(&buffer.result);
   PROCESS_testerrortimer(&s_regex_errtimer, &err);
   if (err) goto ONERR;

   // set out
   regex->matcher = buffer.result;

   return 0;
ONERR:
   if (errdescr && (err == ESYNTAX || err == EILSEQ)) {
      *errdescr = buffer.err;
   }

   if (isBuffer) {
      (void) free_automat(&buffer.result);
      (void) free_buffer(&buffer);
   }
   if (err != ESYNTAX && err != EILSEQ) {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_buffer(void)
{
   buffer_t    buffer;
   const char  data[256];
   const char* input;
   char32_t    chr;

   // prepare
   memset(&buffer, 0, sizeof(buffer));
   TEST(0 == initmatch_automat(&buffer.result, 0, 1, (char32_t[]){'a'}, (char32_t[]){'z'}));

   for (size_t i = 0; i < lengthof(data); ++i) {
      automat_t old;
      memcpy(&old, &buffer.result, sizeof(buffer.result));

      // TEST init_buffer
      TEST( 0 == init_buffer(&buffer, i, data));
      // check buffer
      TEST( buffer.mman.nrstate == 2);
      TEST( buffer.input.next == (const uint8_t*)data);
      TEST( buffer.input.end  == i + buffer.input.next);
      TEST( 0 == memcmp(&buffer.result, &old, sizeof(buffer.result))); // not changed

      // TEST free_buffer
      TEST( 0 == free_buffer(&buffer));
      // check buffer
      TEST( isfree_automat(&buffer.mman));
      TEST( buffer.input.next == 0);
      TEST( buffer.input.end  == 0);
      TEST( 0 == memcmp(&buffer.result, &old, sizeof(buffer.result))); // not changed
   }

   // free resources
   free_automat(&buffer.result);

   // === group: parsing

   // TEST read_next: skip space
   input = "   0  1  2  3      4";
   buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
   for (unsigned i = 0; i < 5; ++i) {
      TEST( '0' + i == read_next(&buffer));
      // check buffer.input
      TEST( buffer.input.next == (const uint8_t*)strchr(input, (int)('0'+i))+1);
      TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
   }

   // TEST read_next: reads bytes even if they are start of utf8 seqeuences
   input = u8"abcde\u01ff\U00123456";
   buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
   for (unsigned i = 0; i < strlen(input); ++i) {
      TEST( (uint8_t)input[i] == read_next(&buffer));
      // check buffer.input
      TEST( buffer.input.next == (const uint8_t*)&input[i+1]);
      TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
   }

   // TEST read_next: returns ' ' at end of input
   for (unsigned tc = 0; tc <= 1; ++tc) {
      input = tc ? "    " : "";
      buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
      for (unsigned i = 0; i < 5; ++i) {
         TEST( ' ' == read_next(&buffer));
         // check buffer.input
         TEST( buffer.input.next == (const uint8_t*)input + strlen(input));
         TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
      }
   }

   // TEST skip_next
   input = "abcde";
   buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
   for (unsigned i = 0; i < strlen(input); ++i) {
      skip_next(&buffer);
      // check buffer.input
      TEST( buffer.input.next == (const uint8_t*)input + i + 1);
      TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
   }

   // TEST skip_next: ERROR CASE (never do that in the code)
   skip_next(&buffer);
   TEST( buffer.input.next == (const uint8_t*)input + strlen(input) + 1/*ERROR*/);
   TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));

   // TEST peek_next
   input = "   0  1  2  3      4";
   buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
   for (unsigned i = 0; i < 5; ++i) {
      TEST( '0' + i == peek_next(&buffer));
      // check buffer.input
      TEST( buffer.input.next == (const uint8_t*)strchr(input, (int)('0'+i)));
      TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
      skip_next(&buffer);
   }

   // TEST peek_next: returns ' ' at end of input
   for (unsigned i = 0; i < 5; ++i) {
      TEST( ' ' == peek_next(&buffer));
      // check buffer.input
      TEST( buffer.input.next == (const uint8_t*)input + strlen(input));
      TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
   }

   // TEST parse_utf8
   input = "\u0100\u0123\u7fff\U00012345";
   buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
   for (unsigned i = 0; i < 4; ++i) {
      char32_t expect[4] = { 0x0100, 0x0123, 0x7fff, 0x12345 };
      unsigned offset[4] = { 2, 2+2, 4+3, 7+4 };
      uint8_t  next = read_next(&buffer);
      TEST( 0 == parse_utf8(&buffer, next, &chr))
      // check chr
      TEST( chr == expect[i]);
      // check buffer.input
      TEST( buffer.input.next == (const uint8_t*)input + offset[i]);
      TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
   }

   // TEST parse_char: single char
   for (uint8_t c = 0; c <= 127; ++c) {
      if (c == 'n' || c == 'r' || c == 't' || c == '\\') continue/*ignore control-code character*/;
      buffer.input = (memstream_ro_t) memstream_INIT(&c, &c + 1);
      chr = 128;
      TEST( 0 == parse_char(&buffer, c, &chr))
      // check chr
      TEST( chr == (char32_t)c);
      // check buffer.input
      TEST( buffer.input.next == &c);
      TEST( buffer.input.end  == &c + 1);
   }

   // TEST parse_char: utf8 char
   input = "\u0100\u0123\u7fff\U00012345";
   buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
   for (unsigned i = 0; i < 4; ++i) {
      char32_t expect[4] = { 0x0100, 0x0123, 0x7fff, 0x12345 };
      unsigned offset[4] = { 2, 2+2, 4+3, 7+4 };
      uint8_t  next = read_next(&buffer);
      TEST( 0 == parse_char(&buffer, next, &chr))
      // check chr
      TEST( chr == expect[i]);
      // check buffer.input
      TEST( buffer.input.next == (const uint8_t*)input + offset[i]);
      TEST( buffer.input.end  == (const uint8_t*)input + strlen(input));
   }

   // TEST parse_char: '\\' at end of input
   input = "\\";
   buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)input, (const uint8_t*)input + strlen(input));
   TEST( 0 == parse_char(&buffer, read_next(&buffer), &chr))
   // check chr
   TEST( chr == '\\');
   // check buffer.input
   TEST( buffer.input.next == (const uint8_t*)input + 1);
   TEST( buffer.input.end  == (const uint8_t*)input + 1);

   // TEST parse_char: : "\\X" ...
   for (uint8_t c = 0; c <= 127; ++c) {
      buffer.input = (memstream_ro_t) memstream_INIT(&c, &c + 1);
      chr = 128;
      TEST( 0 == parse_char(&buffer, '\\', &chr));
      // check chr
      TEST( chr == (char32_t) (c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c));
      // check buffer.input
      TEST( buffer.input.next == &c + 1);
      TEST( buffer.input.end  == &c + 1);
   }

   // TEST ERR_EXPECT_OR_UNMATCHED: expect != 0
   for (unsigned isEndOfFile = 0; isEndOfFile <= 1; ++isEndOfFile) {
      for (unsigned i = 0; i < 3; ++i) {
         const char32_t _ch[3] = { U'a', U'\U0010FFFF', U'\u4abc' };
         const char   * str[3] = { u8"a", u8"\U0010FFFF", u8"\u4abc"};
         const char   * expect = ")";
         size_t len = isEndOfFile ? 1 : strlen(str[i]);
         buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)str[i], (const uint8_t*)str[i] + strlen(str[i]));
         TEST( ESYNTAX == ERR_EXPECT_OR_UNMATCHED(&buffer, expect, nextbyte_memstream(&buffer.input), isEndOfFile));
         // check buffer
         TEST( buffer.input.next == (isEndOfFile || i == 3 ? (const uint8_t*) &str[i][1] : buffer.input.end));
         TEST( buffer.input.end  == (const uint8_t*) str[i] + strlen(str[i]));
         TEST( buffer.err.type   == isEndOfFile);
         TEST( buffer.err.chr    == (isEndOfFile ? (uint8_t)str[i][0] : _ch[i]));
         TEST( buffer.err.pos    == (isEndOfFile ? &str[i][1] : &str[i][0]));
         TEST( buffer.err.expect == expect);
         TEST( buffer.err.unexpected[len] == 0);
         TEST( strncmp(buffer.err.unexpected, &str[i][0], len) == 0);
      }
   }

   // TEST ERR_EXPECT_OR_UNMATCHED: expect == 0
   for (unsigned i = 0; i < 3; ++i) {
      const char32_t _ch[3] = { U')', U'\U0010FFFF', U'\u4abc' };
      const char   * str[3] = { u8")", u8"\U0010FFFF", u8"\u4abc" };
      size_t len = strlen((const char*)str[i]);
      buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)str[i], (const uint8_t*)str[i] + len);
      TEST( ESYNTAX == ERR_EXPECT_OR_UNMATCHED(&buffer, 0, nextbyte_memstream(&buffer.input), false));
      // check buffer
      TEST( buffer.input.next == buffer.input.end); // last char consumed
      TEST( buffer.input.end  == (const uint8_t*)str[i] + strlen((const char*)str[i]));
      TEST( buffer.err.type   == 2);
      TEST( buffer.err.chr    == _ch[i]);
      TEST( buffer.err.pos    == &str[i][0]);
      TEST( buffer.err.expect == 0);
      TEST( strcmp(buffer.err.unexpected, &str[i][0]) == 0);
   }

   // TEST ERR_EXPECT_OR_UNMATCHED: test invalid utf8 sequence at end of input
   for (unsigned tc = 0; tc <= 1; ++tc) {
      for (unsigned i = 0; i <  2; ++i) {
         const char * str[2] = { u8"\U0010FFFF", u8"\u4abc" };
         size_t len = strlen((const char*)str[i]) -1;
         buffer.input = (memstream_ro_t) memstream_INIT((const uint8_t*)str[i], (const uint8_t*)str[i] + len);
         TEST( EILSEQ == ERR_EXPECT_OR_UNMATCHED(&buffer, tc ? "]" : 0, nextbyte_memstream(&buffer.input), false));
         // check buffer
         TEST( buffer.input.next == (const uint8_t*)&str[i][1]); // not changed
         TEST( buffer.input.end  == (const uint8_t*)str[i] + len);
         TEST( buffer.err.type   == 3);
         TEST( buffer.err.chr    == (uint8_t) str[i][0]);
         TEST( buffer.err.pos    == &str[i][0]);
         TEST( buffer.err.expect == 0);
         TEST( buffer.err.unexpected[len] == 0);
         TEST( strncmp(buffer.err.unexpected, &str[i][0], len) == 0);
      }
   }

   // prepare
   TEST( 0 == init_buffer(&buffer, 0, ""));

   // TEST operator_not: matches single char which is not in buffer.ndfa
   TEST(0 == initmatch_automat(&buffer.result, &buffer.mman, 3, (char32_t[]){ '0', 'a', 'A' }, (char32_t[]){ '9', 'z', 'Z' }));
   // test
   TEST( 0 == operator_not(&buffer));
   // check buffer.result
   for (char32_t c = 0; c <= 0x100; ++c) {
      int isMatch = !(('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
      TEST( (size_t)isMatch == matchchar32_automat(&buffer.result, 1, &c, false));
   }
   // reset
   TEST(0 == free_automat(&buffer.result));

   // TEST operator_not: simulated error
   TEST(0 == initmatch_automat(&buffer.result, &buffer.mman, 3, (char32_t[]){ '0', 'a', 'A' }, (char32_t[]){ '9', 'z', 'Z' }));
   // test
   init_testerrortimer(&s_regex_errtimer, 1, ENOMEM);
   TEST( ENOMEM == operator_not(&buffer));
   // check buffer.result not changed
   for (char32_t c = 0; c <= 0x100; ++c) {
      int isMatch = ('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
      TEST( (size_t)isMatch == matchchar32_automat(&buffer.result, 1, &c, false));
   }
   // reset
   TEST(0 == free_automat(&buffer.result));

   // TEST operator_optional: matches buffer->ndfa or empty
   TEST(0 == initmatch_automat(&buffer.result, &buffer.mman, 1, (char32_t[]){ '0' }, (char32_t[]){ '9' }));
   // test
   TEST( 0 == operator_optional(&buffer));
   // check buffer.result
   for (char32_t c = 0; c <= 0x100; ++c) {
      int isMatch = ('0' <= c && c <= '9');
      TEST( (size_t)isMatch == matchchar32_automat(&buffer.result, 1, &c, true));
      TEST( (size_t)0       == matchchar32_automat(&buffer.result, 1, &c, false)); // matches empty
   }
   // reset
   TEST(0 == free_automat(&buffer.result));

   // TEST operator_optional: simulated error
   TEST(0 == initmatch_automat(&buffer.result, &buffer.mman, 1, (char32_t[]){ '0' }, (char32_t[]){ '9' }));
   // test
   init_testerrortimer(&s_regex_errtimer, 1, ENOMEM);
   TEST( ENOMEM == operator_optional(&buffer));
   // check buffer.result
   for (char32_t c = 0; c <= 0x100; ++c) {
      int isMatch = ('0' <= c && c <= '9');
      TEST( (size_t)isMatch == matchchar32_automat(&buffer.result, 1, &c, true));
      TEST( (size_t)isMatch == matchchar32_automat(&buffer.result, 1, &c, false)); // not changed
   }
   // reset
   TEST(0 == free_automat(&buffer.result));

   // free resources
   TEST( 0 == free_buffer(&buffer));

   return 0;
ONERR:
   free_buffer(&buffer);
   free_automat(&buffer.result);
   return EINVAL;
}

static int test_initfree(void)
{
   regexpr_t regex = regexpr_FREE;

   // TEST regexpr_FREE
   TEST( 1 == isfree_automat(&regex.matcher));

   // TEST init_regexpr: empty expression
   TEST( 0 == init_regexpr(&regex, strlen(""), "", 0));
   // check regex
   TEST( 2 == nrstate_automat(&regex.matcher));
   TEST( isendstate_automat(&regex.matcher, 0/*start state reaches end state*/));

   // TEST free_regexpr
   TEST( 0 == free_regexpr(&regex));
   TEST( 1 == isfree_automat(&regex.matcher));

   // TEST free_regexpr: double free
   TEST( 0 == free_regexpr(&regex));
   TEST( 1 == isfree_automat(&regex.matcher));

   // TEST init_regexpr: simulated ERROR
   for (unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_regex_errtimer, i, (int)(ENOMEM-1+i));
      int err = init_regexpr(&regex, strlen(""), "", 0);
      if (!err) {
         TEST( i == 5);
         // reset
         free_testerrortimer(&s_regex_errtimer);
         TEST(0 == free_regexpr(&regex));
         break;
      }
      // check return code
      TEST( err == (int)(ENOMEM-1+i));
      // check regex
      TEST( 1 == isfree_automat(&regex.matcher));
   }

   // TEST free_regexpr: simulated ERROR
   for (unsigned i = 1; i; ++i) {
      TEST(0 == init_regexpr(&regex, strlen(""), "", 0));
      init_testerrortimer(&s_regex_errtimer, i, (int)(EINVAL-1+i));
      // test
      int err = free_regexpr(&regex);
      // check regex
      TEST( 1 == isfree_automat(&regex.matcher));
      if (!err) {
         TEST( i == 2);
         // reset
         free_testerrortimer(&s_regex_errtimer);
         break;
      }
      // check return code
      TEST( err == (int)(EINVAL-1+i));
   }

   return 0;
ONERR:
   free_regexpr(&regex);
   return EINVAL;
}

static int test_syntax(void)
{
   regexpr_t regex = regexpr_FREE;

   // TEST parse_regexpr: single utf8 character
   TEST( 0 == init_regexpr(&regex, strlen(u8"\u01ff"), u8"\u01ff", 0));
   TEST( 1 ==  matchchar32_automat(&regex.matcher, 1, U"\u01ff", false));
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: 2 utf8 characters
   for (char32_t c = 128; c <= maxchar_utf8()-9; c <<= 1, ++c) {
      uint8_t str[2*maxsize_utf8()];
      unsigned len = encodechar_utf8(c, sizeof(str), str);
      len += encodechar_utf8(c+9, sizeof(str)/2, str+len);
      TEST( 0 == init_regexpr(&regex, len, (char*)str, 0));
      TEST( 2 == matchchar32_automat(&regex.matcher, 2, (char32_t[]){c, c+9}, false));
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: \ at end of string
   TEST( 0 == init_regexpr(&regex, strlen(u8"\\"), u8"\\", 0));
   TEST( 1 == matchchar32_automat(&regex.matcher, 1, U"\\", false));
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: control-code
   for (unsigned tc = 0; tc <= 2; ++tc) {
      char ctrlcode[3][2] = { "\\n", "\\r", "\\t" };
      char32_t  expect[3] = { '\n', '\r', '\t' };
      TEST( 0 == init_regexpr(&regex, 2, ctrlcode[tc], 0));
      TEST( 1 == matchchar32_automat(&regex.matcher, 1, &expect[tc], false));
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: special-char (ignore space except if '\\' comes before)
   for (char32_t c = 1; c <= 127; ++c) {
      if (c == 'n' || c == 'r' || c == 't') continue/*ignore control-code character*/;
      char     str[4] = { ' ', '\\', (char)c, ' ' };
      TEST( 0 == init_regexpr(&regex, 4, str, 0));
      TEST( 1 == matchchar32_automat(&regex.matcher, 1, &c, false));
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: empty []
   TEST( 0 == init_regexpr(&regex, 2, "[]", 0));
   // check regex
   TEST( 2 == nrstate_automat(&regex.matcher));
   TEST( isendstate_automat(&regex.matcher, 0/*start state reaches end state*/));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: empty |, &
   for (unsigned o = 0; o <= 3; ++o) {
      for (unsigned i = 0; i <= 4-o; ++i) {
         TEST( 0 == init_regexpr(&regex, i, "||&&"+o, 0));
         // check regex
         TEST( 2 == nrstate_automat(&regex.matcher));
         TEST( isendstate_automat(&regex.matcher, 0/*start state reaches end state*/));
         // reset
         TEST(0 == free_regexpr(&regex));
      }
   }

   // TEST parse_regexpr: empty &!
   TEST( 0 == init_regexpr(&regex, 2, "&!", 0));
   // check regex
   TEST( 2 == nrstate_automat(&regex.matcher));
   TEST( iserrorstate_automat(&regex.matcher, 0));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: .
   TEST( 0 == init_regexpr(&regex, 1, ".", 0));
   // check regex
   TEST( 1 == matchchar32_automat(&regex.matcher, 2, U"ab", true));
   TEST( 1 == matchchar32_automat(&regex.matcher, 1, U"\U7fffffff", true));
   for (char32_t c = 1; c <= 0x7fffffff; c <<= 1) {
      TEST( 1 == matchchar32_automat(&regex.matcher, 1, &c, false));
   }
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: sequence
   TEST( 0 == init_regexpr(&regex, 6, "abcdef", 0));
   // check regex
   TEST( 6 == matchchar32_automat(&regex.matcher, 6, U"abcdef", false));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: *
   TEST( 0 == init_regexpr(&regex, 3, "ab*", 0));
   // check regex
   TEST( 0 == matchchar32_automat(&regex.matcher, 1, U"b", true));
   for (size_t i = 0; i <= 10; ++i) {
      TEST( i == matchchar32_automat(&regex.matcher, i, U"abbbbbbbbb", true));
   }
   TEST( 1 == matchchar32_automat(&regex.matcher, 10, U"abbbbbbbbb", false));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: +
   TEST( 0 == init_regexpr(&regex, 4, "xyz+", 0));
   // check regex
   TEST( 0 == matchchar32_automat(&regex.matcher, 2, U"xy", true));
   for (size_t i = 4; i <= 10; ++i) {
      TEST( i == matchchar32_automat(&regex.matcher, i, U"xyzzzzzzzz", true));
   }
   TEST( 3 == matchchar32_automat(&regex.matcher, 10, U"xyzzzzzzzz", false));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: ?
   for (unsigned i = 0; i < 2; ++i) {
      const char* def[2] = { "1(x9)?", "1(x9|)" }; // both are equal
      TEST( 0 == init_regexpr(&regex, strlen(def[i]), def[i], 0));
      // check regex
      TEST( 1 == matchchar32_automat(&regex.matcher, 3, U"1x9", false));
      TEST( 3 == matchchar32_automat(&regex.matcher, 3, U"1x9", true));
      // reset
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: []
   for (unsigned tc = 0; tc <= 1; ++tc) {
      if (tc) {
         TEST( 0 == init_regexpr(&regex, strlen(u8"[^a-z0-9_?\\nA-Z,.\u0100-\u0200]"), u8"[^a-z0-9_?\\nA-Z,.\u0100-\u0200]", 0));
      } else {
         TEST( 0 == init_regexpr(&regex, strlen(u8"[a-z0-9_?\\nA-Z,.\u0100-\u0200]"), u8"[a-z0-9_?\\nA-Z,.\u0100-\u0200]", 0));
      }
      for (char32_t c = 0; c <= 0x210; ++c) {
         int isMatch = (0x100 <= c && c <= 0x200) || ('a' <= c && c <= 'z')
                       || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9')
                       || c == ',' || c == '.' || c == '_' || c == '?' || c == '\n';
         isMatch ^= (int)tc;
         TEST( (isMatch?1u:0u) == matchchar32_automat(&regex.matcher, 1, &c, false));
      }
      // reset
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: ()
   TEST( 0 == init_regexpr(&regex, 6, "(xyz)+", 0));
   // check regex
   for (size_t i = 0; i <= 12; ++i) {
      TEST( (i-i%3) == matchchar32_automat(&regex.matcher, i, U"xyzxyzxyzxyz", true));
   }
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: |
   TEST( 0 == init_regexpr(&regex, 8, "(a|b|c)+", 0));
   // check regex
   for (size_t i = 0; i <= 18; ++i) {
      TEST( i == matchchar32_automat(&regex.matcher, i, U"abcbcacbaacbbacccc", true));
   }
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: &!
   TEST( 0 == init_regexpr(&regex, 10, "abc* &! ab", 0));
   // check regex
   TEST( 3 == matchchar32_automat(&regex.matcher, 3, U"abc", false));
   TEST( 0 == matchchar32_automat(&regex.matcher, 2, U"ab", true));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: &
   TEST( 0 == init_regexpr(&regex, 16, "[a-c]* & .*aaa.*", 0));
   // check regex
   TEST( 3  == matchchar32_automat(&regex.matcher, 3, U"aaa", false));
   TEST( 10 == matchchar32_automat(&regex.matcher, 14, U"accccccaaabbbb", false));
   TEST( 14 == matchchar32_automat(&regex.matcher, 14, U"accccccaaabbbb", true));
   TEST( 0  == matchchar32_automat(&regex.matcher, 5, U"caaba", true));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: !()
   TEST( 0 == init_regexpr(&regex, 9, "!(a*b*c*)", 0));
   // check regex
   TEST( ! isendstate_automat(&regex.matcher, 0/*does not match ""*/));
   TEST( 0 == matchchar32_automat(&regex.matcher, 1, U"a", false));
   TEST( 0 == matchchar32_automat(&regex.matcher, 1, U"b", false));
   TEST( 0 == matchchar32_automat(&regex.matcher, 1, U"c", false));
   TEST( 2 == matchchar32_automat(&regex.matcher, 2, U"ca", false));
   TEST( 0 == matchchar32_automat(&regex.matcher, 11, U"aabbbcccccc", false));
   TEST( 12 == matchchar32_automat(&regex.matcher, 12, U"aabbbccccccx", false));
   TEST( 1  == matchchar32_automat(&regex.matcher, 12, U"_aabbbcccccc", false));
   TEST( 12 == matchchar32_automat(&regex.matcher, 12, U"_aabbbcccccc", true));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: ! single-char
   TEST( 0 == init_regexpr(&regex, 2, "!b", 0));
   // check regex
   TEST( 1 == isendstate_automat(&regex.matcher, 0/*matches ""*/));
   TEST( 0 == matchchar32_automat(&regex.matcher, 1, U"b", false));
   TEST( 3 == matchchar32_automat(&regex.matcher, 3, U"abc", true));
   TEST( 3 == matchchar32_automat(&regex.matcher, 3, U"123", true));
   // reset
   TEST(0 == free_regexpr(&regex));

   // TEST parse_regexpr: precedence of ! *
   for (unsigned i = 0; i < 3; ++i) {
      const char* def[3] = { "(!b)*", "!(b*)", "!b*" /* == !(b*) */ };
      TEST( 0 == init_regexpr(&regex, strlen(def[i]), def[i], 0));
      // check regex
      TEST( (i?0:1) == isendstate_automat(&regex.matcher, 0/*does match "" ?*/));
      // reset
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: precedence of ! ?
   for (unsigned i = 0; i < 3; ++i) {
      const char* def[3] = { "(!b)?", "!(b?)", "!b?" /* == !(b?) */ };
      TEST( 0 == init_regexpr(&regex, strlen(def[i]), def[i], 0));
      // check regex
      TEST( (i?0:1) == isendstate_automat(&regex.matcher, 0/*does match "" ?*/));
      // reset
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: precedence of | & (same precedence from left to right)
   for (unsigned i = 0; i < 3; ++i) {
      const char* def[3] = { "a|(b &! [ab])", "(a|b) &! [ab]", "a|b &! [ab]" /* equals "(a|b) &! [ab]" */ };
      TEST( 0 == init_regexpr(&regex, strlen(def[i]), def[i], 0));
      // check regex
      TEST( (i?0:1) == matchchar32_automat(&regex.matcher, 1, U"a", false));
      // reset
      TEST(0 == free_regexpr(&regex));
   }

   // TEST parse_regexpr: \\ masks special characters
   TEST( 0 == init_regexpr(&regex, 4, "a\\(c", 0));
   // check error without masking
   TEST( 0 != init_regexpr(&regex, 3, "a(c", 0));
   // check regex
   TEST( 3 == matchchar32_automat(&regex.matcher, 3, U"a(c", false));
   // reset
   TEST(0 == free_regexpr(&regex));

   return 0;
ONERR:
   free_regexpr(&regex);
   return EINVAL;
}

static int test_syntax_err(void)
{
   regexpr_t      regex = regexpr_FREE;
   regexpr_err_t  errdescr;

   // TEST init_regexpr: expected ')', or ']' instead end of input
   for (unsigned i = 0; i < 2; ++i) {
      const char* str[2] = { "abc(abc", "abc[a-b0-9" };
      TEST( ESYNTAX == init_regexpr(&regex, strlen(str[i]), str[i], &errdescr));
      // check regex
      TEST( 1 == isfree_automat(&regex.matcher));
      // check errdescr
      TEST( errdescr.type == 1);
      TEST( errdescr.chr  == ' ');
      TEST( errdescr.pos  == str[i] + strlen(str[i]));
      TEST( strcmp(errdescr.expect, i ? "]" : ")") == 0);
      TEST( strcmp(errdescr.unexpected, " ") == 0);
   }

   // TEST init_regexpr: umatched ')' end of input
   for (unsigned i = 0; i < 2; ++i) {
      const char* str[2] = { ")", "(()))" };
      TEST( ESYNTAX == init_regexpr(&regex, strlen(str[i]), str[i], &errdescr));
      // check regex
      TEST( 1 == isfree_automat(&regex.matcher));
      // check errdescr
      TEST( errdescr.type == 2);
      TEST( errdescr.chr  == ')');
      TEST( errdescr.pos  == str[i] + strlen(str[i]) -1);
      TEST( errdescr.expect == 0);
      TEST( strcmp(errdescr.unexpected, ")") == 0);
      if (i == 0) {
         log_regexprerr(&errdescr, log_channel_ERR);
      }
   }

   // TEST init_regexpr: unexpected '+', '*', ']', ...
   for (unsigned i = 0; i < 15; ++i) {
      const char* str[15] = { "*", "+", "a]", "a!]", "a!)", "a!+", "a!*", "a|*", "a&+", "a!&", "a!|", "a!*", "a!+", "a++", "a+*" };
      TEST( ESYNTAX == init_regexpr(&regex, strlen(str[i]), str[i], &errdescr));
      // check regex
      TEST( 1 == isfree_automat(&regex.matcher));
      // check errdescr
      TEST( errdescr.type == 0);
      TEST( errdescr.chr  == (uint8_t)str[i][strlen(str[i])-1]);
      TEST( errdescr.pos  == str[i] + strlen(str[i]) -1);
      TEST( strcmp(errdescr.expect, "<char>") == 0);
      TEST( strcmp(errdescr.unexpected, &str[i][strlen(str[i])-1]) == 0);
      if (i == 0) {
         log_regexprerr(&errdescr, log_channel_ERR);
      }
   }

   // TEST init_regexpr: double '++', '**', '*?'
   for (unsigned i = 0; i < 9; ++i) {
      const char* str[9] = { "a**", "a*+", "a*?", "a+*", "a++", "a+?", "a??", "a?*", "a?+" };
      TEST( ESYNTAX == init_regexpr(&regex, strlen(str[i]), str[i], &errdescr));
      // check regex
      TEST( 1 == isfree_automat(&regex.matcher));
      // check errdescr
      TEST( errdescr.type == 0);
      TEST( errdescr.chr  == (uint8_t)str[i][strlen(str[i])-1]);
      TEST( errdescr.pos  == str[i] + strlen(str[i]) -1);
      TEST( strcmp(errdescr.expect, "<char>") == 0);
      TEST( strcmp(errdescr.unexpected, &str[i][strlen(str[i])-1]) == 0);
      if (i == 0) {
         log_regexprerr(&errdescr, log_channel_ERR);
      }
   }

   // TEST init_regexpr: illegal utf8 encoding
   for (unsigned i = 0; i < 3; ++i) {
      const char* str[3] = { "\xff\x81", "\xc0\x81", "\xff\x81\x82\x83" };
      TEST( EILSEQ == init_regexpr(&regex, strlen(str[i]), str[i], &errdescr));
      // check regex
      TEST( 1 == isfree_automat(&regex.matcher));
      // check errdescr
      TEST( errdescr.type == 3);
      TEST( errdescr.chr  == (uint8_t)str[i][0]);
      TEST( errdescr.pos  == str[i]);
      TEST( errdescr.expect == 0);
      TEST( strcmp(errdescr.unexpected, str[i]) == 0);
      if (i == 0) {
         log_regexprerr(&errdescr, log_channel_ERR);
      }
   }

   return 0;
ONERR:
   free_regexpr(&regex);
   return EINVAL;
}

int unittest_proglang_regexpr()
{
   if (test_buffer())      goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_syntax())      goto ONERR;
   if (test_syntax_err())  goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
