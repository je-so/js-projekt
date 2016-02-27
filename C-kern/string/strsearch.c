/* title: SearchString impl

   Implements <SearchString>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/string/strsearch.h
    Header file <SearchString>.

   file: C-kern/string/strsearch.c
    Implementation file <SearchString impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/strsearch.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/filesystem/fileutil.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#endif


// section: strsearch_t

// group: helper

/* function: buildtable_rsearchkmp
 * Builds helper table used by KMP string search algorithm.
 * The algorithm searches from end to begin (r == reverse).
 *
 * Content of sidx:
 * For all (0 <= i < findsize): sidx[i] gives the index into findstr
 * where you have to continue to compare if comparison failed for index i.
 * sidx[findsize-1] is set to findsize which marks the end of chain.
 *
 * >     FINDSTR
 * >           ^findsize-1: comparison fails
 * >    FINDSTR        <==> shift left
 * >           ^findsize: new index into findstr after shift
 *
 * Unchecked Precondition:
 * - findsize >= 1
 * */
static inline void buildtable_rsearchkmp(uint8_t findsize, const uint8_t findstr[findsize], /*out*/uint8_t sidx[findsize])
{
   sidx[findsize-1] = findsize; // end marker for 0 length match
   for (unsigned i = findsize-1u, i2 = findsize; i > 0; ) {
      while (i2 < findsize && findstr[i] != findstr[i2]) {
         i2 = sidx[i2];
      }
      sidx[--i] = (uint8_t) (--i2);
   }
}

/* function: optimizetable_rsearchkmp
 * Optimizes sidx so that unnecessary comparisons are avoided.
 * If comparison findstr[i] fails sidx[i] gives the index into findstr
 * from where to compare next (left shift). But if findstr[sidx[i]]
 * is equal to findstr[i] we already know that this compairson will
 * fail. Therefore set sidx[i] to sidx[sidx[i]] recursively.
 *
 * If we begin changing sidx from high i to low i no recursion is necessary.
 *
 * Unchecked Precondition:
 * - <buildtable_rsearchkmp> is called before with same arguments
 */
static inline void optimizetable_rsearchkmp(uint8_t findsize, const uint8_t findstr[findsize], /*inout*/uint8_t sidx[findsize])
{
   for (int i = findsize-1; i > 0; ) {
      int i2 = sidx[--i];
      // findstr[i] does not match, i2 is the next index into findstr
      if (findstr[i] == findstr[i2]) {
         // findstr[i2] equals findstr[i] ==> could not match either
         sidx[i] = sidx[i2]; // skip comparison
      }
   }
}

/* function: buildtable_strsearch
 * Builds helper table shift for Boyer-Moore string search algorithm.
 * The algorithm searches from start to end of text.
 * But the last character of findstr is compared first.
 *
 * Content of shift:
 * shift[i] gives the nr of positions findstr is to shift right.
 * Index i equals the nr of matched positions starting from end of findstr.
 *
 * > FINDSTR
 * >       ^findsize-1: comparison fails
 * >  FINDSTR   <==> shift right
 * >        ^findsize-1: new index into findstr after shift
 * >         (findstr += shift[i])
 *
 * How it works:
 * Consider findstr "ABCABC". If comparison fails for second C then findstr must be
 * shift 3 places to the right so that first "ABC" lies above the second "ABC" which
 * has been already matched.
 *
 * The algorithm to build the table uses the nr of matched chars from end of findstr
 * and searches to the left until a match is found. If no match is found then
 * shift[nrOfMatched] is set to findsize.
 *
 * Unchecked Precondition:
 * - findsize >= 1
 * */
static inline void buildtable_strsearch(uint8_t findsize, const uint8_t findstr[findsize], /*out*/uint8_t shift[findsize])
{
   // uses reverse kmp search (from end to begin)
   // which makes building of shift table O(findstr)
   uint8_t sidx[findsize];
   buildtable_rsearchkmp(findsize, findstr, sidx);

   shift[0] = 1;

   for (unsigned nrmatched = 1, lastoff = findsize-1u; nrmatched < findsize; ++nrmatched) {
      if (lastoff >= nrmatched) {
         unsigned dpos = lastoff-nrmatched+1;
         unsigned spos = findsize-nrmatched+1;
         while (dpos > 0) {
            --dpos;
            --spos;
            if (findstr[spos] != findstr[dpos]) {
               spos = sidx[spos];
               while (spos < findsize && findstr[spos] != findstr[dpos]) {
                  spos = sidx[spos];
               }
            }
            if (nrmatched == findsize-spos) break;
         }
         lastoff = (spos < findsize ? dpos+(findsize-spos) : 0);
      }
      shift[nrmatched] = (uint8_t) (findsize - lastoff);
   }
}


#ifdef KONFIG_UNITTEST
/* function: rsearch_kmp
 * Suche Teilstring findstr im Gesamtstring data von hinten nach vorn.
 * Search substring findstr in whole string data in *reverse* order.
 * If findstr is not found 0 is returned. If findstr is contained more than
 * once the occurrence with the highest address is reported (reverse order).
 *
 * Used algorithm:
 * Knuth-Morris-Pratt!
 *
 * Worst case:
 * O(size)
 *
 * Unchecked Precondition:
 * - findsize >= 1
 */
static const uint8_t* rsearch_kmp(size_t size, const uint8_t data[size], uint8_t findsize, const uint8_t findstr[findsize])
{
   if (!findsize || findsize > size) return 0;

   uint8_t sidx[findsize];
   buildtable_rsearchkmp(findsize, findstr, sidx);

   size_t dpos = size;
   int    spos = findsize;
   while (dpos > 0) {
      --dpos;
      --spos;
      if (findstr[spos] != data[dpos]) {
         spos = sidx[spos];
         while (spos < findsize && findstr[spos] != data[dpos]) {
            spos = sidx[spos];
         }
      }
      if (! spos) return data + dpos;
   }

   return 0;
}
#endif

// group: lifetime

void init_strsearch(/*out*/strsearch_t* strsrch, uint8_t findsize, const uint8_t findstr[findsize], uint8_t shift[findsize])
{
   if (findsize > 1) {
      buildtable_strsearch(findsize, findstr, shift);
   }

   // set out param
   strsrch->shift    = shift;
   strsrch->findstr  = findstr;
   strsrch->findsize = findsize;
}

// group: query

const uint8_t* find_strsearch(strsearch_t* strsrch, size_t size, const uint8_t data[size])
{
   const uint8_t* shift   = strsrch->shift;
   const uint8_t* findstr = strsrch->findstr;
   const int findsize  = strsrch->findsize;

   if ((size_t)findsize > size) {
      return 0;
   }

   if (findsize <= 1) {
      return findsize ? memchr(data, findstr[0], size) : 0;
   }

   const int findsize1 = findsize-1;

   int eoff = 0;
   int skip = 0;

   const uint8_t* const endpos = data + size - findsize1;
   const uint8_t* pos = data;

   do {
      int off = findsize1;
      while (findstr[off] == pos[off]) {
         if (off == eoff) {
            if (off <= skip) {
               return pos;
            }
            off -= skip; // make worst case O(size) instead O(size*substrlen)
            eoff = 0;
         }
         --off;
      }
      int nrmatched = findsize1 - off;
      int pincr = shift[nrmatched];
      skip = nrmatched;
      pos += pincr;
      eoff = findsize - pincr;
   } while (pos < endpos);

   return 0;
}

const uint8_t* strsearch(size_t size, const uint8_t data[size], uint8_t findsize, const uint8_t findstr[findsize])
{
   if (findsize > size) {
      return 0;
   }

   if (findsize <= 1) {
      return findsize ? memchr(data, findstr[0], size) : 0;
   }

   uint8_t shift[findsize];
   buildtable_strsearch(findsize, findstr, shift);

   const int findsize1 = findsize-1;

   int eoff = 0;
   int skip = 0;

   const uint8_t* const endpos = data + size - findsize1;
   const uint8_t* pos = data;

   do {
      int off = findsize1;
      while (findstr[off] == pos[off]) {
         if (off == eoff) {
            if (off <= skip) {
               return pos;
            }
            off -= skip; // make worst case O(size) instead O(size*substrlen)
            eoff = 0;
         }
         --off;
      }
      int nrmatched = findsize1 - off;
      int pincr = shift[nrmatched];
      skip = nrmatched;
      pos += pincr;
      eoff = findsize - pincr;
   } while (pos < endpos);

   return 0;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#if 0
void test_buildtable(int findsize, bool isoptimize, uint8_t findstr[findsize], uint8_t sidx[findsize])
{
   sidx[findsize-1] = (uint8_t) findsize;
   for (int si = findsize-2; si >= 0; --si) {
      int i;
      for (i = si+2; i <= findsize; ++i) {
         if (  (!isoptimize || (findstr[si] != findstr[i-1]))
               && (i == findsize || 0 == memcmp(findstr+si+1, findstr+i, (size_t) (findsize-i)))) {
            break;
         }
      }
      sidx[si] = (uint8_t) (i-1);
   }
}
#endif

static int test_helper(void)
{
   uint8_t findstr[255];
   uint8_t _sidx[255+2];
   uint8_t* sidx = _sidx + 1;

   memset(_sidx, 129, sizeof(_sidx));
   for (int findsize = 1; findsize <= (int)sizeof(findstr); findsize += ((int)sizeof(findstr)+2-findsize)/2) {

      // TEST buildtable_rsearchkmp: all chars different
      for (int chr = 0; chr < findsize; ++chr) {
         findstr[chr] = (uint8_t) (findsize + chr);
      }
      buildtable_rsearchkmp((uint8_t) findsize, findstr, sidx);
      TEST(findsize == sidx[findsize-1]);
      for (int i = findsize-2; i >= 0; --i) {
         TEST(findsize-1 == sidx[i]);
      }

      // TEST optimizetable_rsearchkmp: all chars different
      optimizetable_rsearchkmp((uint8_t) findsize, findstr, sidx);
      TEST(findsize == sidx[findsize-1]);
      for (int i = findsize-2; i >= 0; --i) {
         TEST(findsize-1 == sidx[i]);
      }

      // TEST buildtable_rsearchkmp: all chars equal
      memset(findstr, findsize, sizeof(findstr));
      buildtable_rsearchkmp((uint8_t) findsize, findstr, sidx);
      for (int i = findsize-1; i >= 0; --i) {
         TEST(i+1 == sidx[i]);
      }

      // TEST optimizetable_rsearchkmp: all chars different
      optimizetable_rsearchkmp((uint8_t) findsize, findstr, sidx);
      for (int i = findsize-1; i >= 0; --i) {
         TEST(findsize == sidx[i]);
      }

      TEST(129 == sidx[findsize] && 129 == sidx[-1]);
   }

   memset(_sidx, 129, sizeof(_sidx));
   for (int findsize = 30, replen = 1; findsize <= (int)sizeof(findstr); findsize += 45, ++replen) {
      uint8_t sidx2[255];
      int     pos[3];

      // TEST buildtable_rsearchkmp: repeat pattern
      for (pos[0] = findsize-10; pos[0] >= 0; --pos[0]) {
         if (pos[0] == findsize-18) pos[0] = 9;
         for (pos[1] = pos[0]; pos[1] >= 0; --pos[1]) {
            if (pos[1] == findsize-18) pos[1] = 9;
            for (pos[2] = pos[1]; pos[2] >= 0; --pos[2]) {
               if (pos[2] == findsize-18) pos[2] = 9;
               // init findstr with all chars different
               for (int chr = 0; chr < findsize; ++chr) {
                  findstr[chr] = (uint8_t) (findsize + chr);
               }
               // prepare findstr + result
               memset(sidx2, findsize-1, (size_t)findsize);
               sidx2[findsize-1] = (uint8_t)findsize;
               for (int pi = 0; pi < 3; ++pi) {
                  int len = pos[pi]+1 < replen ? pos[pi]+1 : replen;
                  memmove(findstr+pos[pi]+1-len, findstr+findsize-len, (size_t)len);
                  for (int i = 1; i <= len && i <= pos[pi]; ++i) {
                     sidx2[pos[pi]-i] = (uint8_t) (findsize-1-i);
                  }
               }
               // test
               // test_buildtable(findsize, false, findstr, sidx2);
               buildtable_rsearchkmp((uint8_t)findsize, findstr, sidx);
               TEST(0 == memcmp(sidx, sidx2, (size_t)findsize));

               // TEST optimizetable_rsearchkmp: repeat pattern
               optimizetable_rsearchkmp((uint8_t)findsize, findstr, sidx);
               for (int pi = 0; pi < 3; ++pi) {
                  int len = pos[pi]+1 < replen ? pos[pi]+1 : replen;
                  for (int i = 0; i < len && i <= pos[pi]; ++i) {
                     if (  findstr[pos[pi]-i] == findstr[findsize-1-i]
                           && sidx2[pos[pi]-i] == findsize-1-i) {
                        sidx2[pos[pi]-i] = (uint8_t) (findsize-(i != 0/*test last char with other*/));
                     }
                  }
               }
               // test_buildtable(findsize, true, findstr, sidx2);
               TEST(0 == memcmp(sidx, sidx2, (size_t)findsize));

               TEST(129 == sidx[findsize] && 129 == sidx[-1]);
            }
         }
      }
   }

   memset(_sidx, 129, sizeof(_sidx));
   for (int findsize = 1; findsize <= (int)sizeof(findstr); findsize += ((int)sizeof(findstr)+2-findsize)/2) {

      // TEST buildtable_strsearch: all chars different
      for (int chr = 0; chr < findsize; ++chr) {
         findstr[chr] = (uint8_t) (findsize + chr);
      }
      buildtable_strsearch((uint8_t) findsize, findstr, sidx);
      TEST(1 == sidx[0]);
      for (int i = 1; i < findsize; ++i) {
         TEST(findsize == sidx[i]);
      }

      // TEST buildtable_strsearch: all chars equal (worst case)
      memset(findstr, findsize, (size_t)findsize);
      buildtable_strsearch((uint8_t) findsize, findstr, sidx);
      for (int i = 0; i < findsize; ++i) {
         TEST(1 == sidx[i]);
      }

      // TEST buildtable_strsearch: all chars equal except one
      findstr[findsize-1] = (uint8_t) (findsize+1);
      buildtable_strsearch((uint8_t) findsize, findstr, sidx);
      for (int i = 0; i < findsize; ++i) {
         TEST((i ? findsize : 1) == sidx[i]);
      }
      findstr[findsize-1] = (uint8_t) findsize;
      findstr[findsize/2] = (uint8_t) (findsize+1);
      buildtable_strsearch((uint8_t) findsize, findstr, sidx);
      TEST(1 == sidx[0]);
      for (int i = 1; i < findsize; ++i) {
         TEST(sidx[i] == (findsize-1-i > findsize/2 ? 1 : 1+findsize/2));
      }

      TEST(129 == sidx[findsize] && 129 == sidx[-1]);
   }

   // TEST buildtable_strsearch: repeat pattern
   memset(_sidx, 129, sizeof(_sidx));
   for (int findsize = 10, replen = 1; findsize <= (int)sizeof(findstr); findsize += 35, ++replen) {
      uint8_t sidx2[255];

      for (int pos = findsize-1-replen; pos >= 0; --pos) {
         // init findstr with all chars equal
         memset(findstr, findsize, (size_t)(findsize-replen));
         for (int i = findsize-1; i >= findsize-replen; --i) {
            findstr[i] = (uint8_t) i;
         }
         // prepare findstr + result
         memset(sidx2, 1, (size_t)findsize);
         {
            int len = pos+1 < replen ? pos+1 : replen;
            memmove(findstr+pos+1-len, findstr+findsize-len, (size_t)len);
            for (int i = 1; i < findsize; ++i) {
               sidx2[i] = (uint8_t) (findsize-1-pos > pos || findsize-1-i >= pos ? findsize-1-pos : findsize);
            }
         }
         buildtable_strsearch((uint8_t)findsize, findstr, sidx);
         TEST(0 == memcmp(sidx, sidx2, (size_t)findsize));

         TEST(129 == sidx[findsize] && 129 == sidx[-1]);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   strsearch_t strsrch = strsearch_FREE;
   uint8_t     findstr[255];
   uint8_t     _shift[257];
   uint8_t*    shift = _shift+1;

   // TEST strsearch_FREE
   TEST(0 == strsrch.shift);
   TEST(0 == strsrch.findstr);
   TEST(0 == strsrch.findsize);

   // TEST init_strsearch: all char equal / test different parameter values
   memset(findstr, 0, sizeof(findstr));
   memset(_shift, 129, sizeof(_shift));
   for (int findsize = 0; findsize <= 255; ++findsize) {
      memset(&strsrch, 0, sizeof(strsrch));
      init_strsearch(&strsrch, (uint8_t)findsize, findstr+255-findsize, shift+255-findsize);
      TEST(strsrch.shift    == shift+255-findsize);
      TEST(strsrch.findstr  == findstr+255-findsize);
      TEST(strsrch.findsize == findsize);
      TEST(129 == shift[255] && 129 == shift[254-findsize]);
      for (int i = 1; i <= findsize; ++i) {
         TEST(shift[255-i] == (findsize < 2 ? 129 : 1));
      }
   }

   // TEST init_strsearch: all char different
   for (int i = 0; i < 255; ++i) {
      findstr[i] = (uint8_t) i;
   }
   memset(&strsrch, 0, sizeof(strsrch));
   init_strsearch(&strsrch, 255, findstr, shift);
   TEST(129 == shift[255] && 129 == shift[-1]);
   for (int i = 0; i < 255; ++i) {
      TEST(shift[i] == (i ? 255 : 1));
   }

   // TEST init_strsearch: repeating pattern
   memset(_shift, 129, sizeof(_shift));
   init_strsearch(&strsrch, 10, (const uint8_t*)"xyy12345xy", shift);
   TEST(129 == shift[10] && 129 == shift[-1]);
   for (int i = 0; i < 10; ++i) {
      TEST(shift[i] == (i == 0 ? 1 : i == 1 ? 7 : 8));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_find(void)
{
   memblock_t file_data = memblock_FREE;
   size_t   file_size;
   uint8_t  data[1024+255];
   uint8_t  findstr[255];
   uint8_t  shift[255];
   strsearch_t strsrch;

   // TEST find_strsearch, rsearch_kmp, strsearch: datasize < findsize
   for (size_t datasize = 0; datasize < 255; ++datasize) {
      init_strsearch(&strsrch, sizeof(findstr), findstr, shift);
      TEST(0 == find_strsearch(&strsrch, datasize, data));
      TEST(0 == rsearch_kmp(datasize, data, sizeof(findstr), findstr));
      TEST(0 == strsearch(datasize, data, sizeof(findstr), findstr));
   }

   // TEST find_strsearch, rsearch_kmp, strsearch: findsize == 0
   init_strsearch(&strsrch, 0, findstr, shift);
   TEST(0 == find_strsearch(&strsrch, sizeof(data), data));
   TEST(0 == rsearch_kmp(sizeof(data), data, 0, findstr));
   TEST(0 == strsearch(sizeof(data), data, 0, findstr));

   // TEST find_strsearch, rsearch_kmp, strsearch: findsize == datasize
   for (int equal = 0; equal <= 1; ++equal) {
      for (size_t findsize = 1; findsize <= 255; ++findsize) {
         uint8_t* d = data+findsize;
         if (equal) {
            memset(d, (int)findsize, findsize);
            memset(findstr, (int)findsize, findsize);
         } else {
            for (unsigned i = 0; i < findsize; ++i) {
               d[i] = (uint8_t) (findsize+i);
               findstr[i] = (uint8_t) (findsize+i);
            }
         }
         init_strsearch(&strsrch, (uint8_t)findsize, findstr, shift);
         TEST(d == find_strsearch(&strsrch, findsize, d));
         TEST(d == rsearch_kmp(findsize, d, (uint8_t)findsize, findstr));
         TEST(d == strsearch(findsize, d, (uint8_t)findsize, findstr));
      }
   }

   // TEST find_strsearch, rsearch_kmp, strsearch: find key with equal chars
   memset(data, 0, sizeof(data));
   for (int findsize = 1; findsize <= 255; findsize += (findsize & (findsize+1)) ? findsize-1 : 1) {
      memset(findstr, findsize, (size_t)findsize);
      init_strsearch(&strsrch, (uint8_t)findsize, findstr, shift);
      for (int pos = 0; pos <= (int)sizeof(data)-findsize; pos += (pos & (pos+1)) ? pos-1 : 1) {
         uint8_t* d = data+pos;
         if (pos) d[-1] = 0;
         memset(d, findsize, (size_t)findsize);
         TEST(d == find_strsearch(&strsrch, sizeof(data), data));
         TEST(d == rsearch_kmp(sizeof(data), data, (uint8_t)findsize, findstr));
         TEST(d == strsearch(sizeof(data), data, (uint8_t)findsize, findstr));
         d[0] = 0;
      }
   }

   #define FIND_STRSEARCH find_strsearch

   // TEST find_strsearch, rsearch_kmp, strsearch: search find_strsearch in this source file
   {
      // BUILD compare_pos[] with bash script (all in one line after the ">" prompt)
      /* > grep -ob find_strsearch C-kern/string/strsearch.c | while read; do echo -n "${REPLY%%:*},"; done; echo */
      size_t  compare_pos[] = {6232,17002,17202,17404,17519,17694,18309,18520,19064,19330,19357,19404,19549};
      {
         wbuffer_t wbuf = wbuffer_INIT_MEMBLOCK(&file_data);
         TEST(0 == load_file(__FILE__, &wbuf, 0));
         file_size = size_wbuffer(&wbuf);
      }
      uint8_t findsize = sizeof(STR(FIND_STRSEARCH))-1;
      memcpy(findstr, STR(FIND_STRSEARCH), (size_t)findsize);
      init_strsearch(&strsrch, (uint8_t)findsize, findstr, shift);
      for (unsigned pi = 0, off = 0; pi < lengthof(compare_pos); ++pi) {
         uint8_t* d = file_data.addr + compare_pos[pi];
         TEST(d == FIND_STRSEARCH(&strsrch, file_size-off, file_data.addr + off));
         TEST(d == strsearch(file_size-off, file_data.addr + off, findsize, findstr));
         off = compare_pos[pi]+1;
         if (pi == lengthof(compare_pos)-1) {
            TEST(0 == FIND_STRSEARCH(&strsrch, file_size-off, file_data.addr + off));
            TEST(0 == strsearch(file_size-off, file_data.addr + off, findsize, findstr));
         }
      }
      for (unsigned pi = lengthof(compare_pos)-1, s = file_size; pi < lengthof(compare_pos); --pi) {
         uint8_t* d = file_data.addr + compare_pos[pi];
         TEST(d == rsearch_kmp(s, file_data.addr, findsize, findstr));
         s = compare_pos[pi]+findsize-1u;
         if (pi == 0) {
            TEST(0 == rsearch_kmp(s, file_data.addr, findsize, findstr));
         }
      }
      TEST(0 == FREE_MM(&file_data));
   }

   return 0;
ONERR:
   FREE_MM(&file_data);
   return EINVAL;
}

int unittest_string_strsearch()
{
   if (test_helper())   goto ONERR;
   if (test_initfree()) goto ONERR;
   if (test_find())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
