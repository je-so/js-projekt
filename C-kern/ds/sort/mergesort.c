/* title: MergeSort impl

   Implements <MergeSort>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/ds/sort/mergesort.h
    Header file <MergeSort>.

   file: C-kern/ds/sort/mergesort.c
    Implementation file <MergeSort impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/sort/mergesort.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#endif


// section: mergesort_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_mergesort_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_mergesort_errtimer = test_errortimer_FREE;
#endif

// group: constants

/* define: MIN_BLK_LEN
 * The minimum nr of elements moved as one block in a merge operation.
 * The merge operation is implemented in <merge_adjacent_slices> and
 * <rmerge_adjacent_slices>. */
#define MIN_BLK_LEN 7

/* define: MIN_SLICE_LEN
 * The minimum number of elements stored in a sorted slice.
 * The actual minimum length is computed by <compute_minslicelen>
 * but is always bigger or equal than this value except if the
 * whole array has a size less than MIN_SLICE_LEN.
 * Every slice is described with <mergesort_sortedslice_t>. */
#define MIN_SLICE_LEN 32

// group: memory-helper

/* function: alloctemp_mergesort
 * Reallocates <mergesort_t.temp> so it can store tempsize bytes.
 * The array is always reallocated.
 * If tempsize is set to 0 any allocated memory is freed and
 * sort->tempsize and sort->temp are initialized with sort->tempmem. */
static int alloctemp_mergesort(mergesort_t * sort, size_t tempsize)
{
   int err;
   vmpage_t mblock;

   if (sort->temp != sort->tempmem) {
      mblock = (vmpage_t) vmpage_INIT(sort->tempsize, sort->temp);
      err = free_vmpage(&mblock);
      (void) PROCESS_testerrortimer(&s_mergesort_errtimer, &err);

      sort->temp     = sort->tempmem;
      sort->tempsize = sizeof(sort->tempmem);

      if (err) goto ONERR;
   }

   // TODO: replace call to init_vmpage with call to temporary stack memory manager
   //       == implement temporary stack memory manager ==
   if (tempsize) {
      if (! PROCESS_testerrortimer(&s_mergesort_errtimer, &err)) {
         err = init_vmpage(&mblock, tempsize);
      }
      if (err) goto ONERR;
      sort->temp     = mblock.addr;
      sort->tempsize = mblock.size;
   }

   return 0;
ONERR:
   return err;
}

/* function: ensuretempsize
 * Ensures <mergesort_t.temp> can store tempsize bytes.
 * If the temporary array has less capacity it is reallocated.
 * */
static inline int ensuretempsize(mergesort_t * sort, size_t tempsize)
{
   return tempsize <= sort->tempsize ? 0 : alloctemp_mergesort(sort, tempsize);
}

// group: lifetime

void init_mergesort(/*out*/mergesort_t * sort)
{
    sort->compare  = 0;
    sort->cmpstate = 0;
    sort->elemsize = 0;
    sort->temp     = sort->tempmem;
    sort->tempsize = sizeof(sort->tempmem);
    sort->stacksize = 0;
}

int free_mergesort(mergesort_t * sort)
{
   int err;

   if (sort->temp) {
      err = alloctemp_mergesort(sort, 0);

      sort->temp      = 0;
      sort->tempsize  = 0;
      sort->stacksize = 0;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

/* function: compute_minslicelen
 * Compute a good value for the minimum sub-array length;
 * presorted sub-arrays shorter than this are extended via <insertsort>.
 *
 * If n < 64, return n (it's too small to bother with fancy stuff).
 * Else if n is an exact power of 2, return 32.
 * Else return an int k, 32 <= k <= 64, such that n/k is close to, but
 * strictly less than, an exact power of 2.
 *
 * The implementation chooses the 6 most sign. bits of n as minsize.
 * And it increments minsize by one if n/minsize is not an exact power
 * of two.
 */
static uint8_t compute_minslicelen(size_t n)
{
   // becomes 1 if any 1 bits are shifted off
   unsigned r = 0;

   while (n >= 64) {
      r |= (unsigned)n & 1;
      n >>= 1;
   }

   return (uint8_t) ((unsigned)n + r);
}

// group: set

/* function: setsortstate
 * Prepares <mergesort_t> with additional information before sorting can commence.
 *
 * 1. Sets the compare function.
 * 2. Sets elemsize
 *
 * An error is returned if cmp is invalid or if elemsize*array_len overflows size_t type. */
static int setsortstate(
   mergesort_t  * sort,
   sort_compare_f cmp,
   void         * cmpstate,
   const uint8_t  elemsize,
   const size_t   array_len)
{
   if (cmp == 0 || elemsize == 0 || array_len > ((size_t)-1 / elemsize)) {
      return EINVAL;
   }

   sort->compare  = cmp;
   sort->cmpstate = cmpstate;
   sort->elemsize = elemsize;
   sort->stacksize = 0;
   return 0;
}

// group: generic

#define mergesort_TYPE_POINTER   1
#define mergesort_TYPE_LONG      2
#define mergesort_TYPE_BYTES     4

//////////////////

#define mergesort_IMPL_TYPE      mergesort_TYPE_BYTES
#include "mergesort_generic_impl.h"

#undef  mergesort_IMPL_TYPE
#define mergesort_IMPL_TYPE      mergesort_TYPE_LONG
#include "mergesort_generic_impl.h"

#undef  mergesort_IMPL_TYPE
#define mergesort_IMPL_TYPE      mergesort_TYPE_POINTER
#include "mergesort_generic_impl.h"

//////////////////

int sortblob_mergesort(mergesort_t * sort, uint8_t elemsize, size_t len, void * a/*uint8_t[len*elemsize]*/, sort_compare_f cmp, void * cmpstate)
{
   // use long copy if element size is multiple of long and array is aligned to long
   // on x86 the alignment of the array is not necessary ; long copy would work without it
   if (0 == (uintptr_t)a % sizeof(long) && 0 == elemsize % sizeof(long)) {
      return sortlong_mergesort(sort, elemsize, len, a, cmp, cmpstate);

   } else {
      // adds at least 50% runtime overhead (byte copy is really slow)
      return sortbytes_mergesort(sort, elemsize, len, a, cmp, cmpstate);
   }
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_stacksize(void)
{
   double   phi = (1+sqrt(5))/2; // golden ratio
   unsigned stacksize;

   // stack contains set whose length form a sequence of Fibonnacci numbers
   // stack[0].len == MIN_SLICE_LEN, stack[1].len == MIN_SLICE_LEN, stack[2].len >= stack[0].len + stack[1].len
   // Fibonacci F1 = 1, F2 = 1, F3 = 2, ..., Fn = F(n-1) + F(n-2)
   // The sum of the first Fn is: F1 + F2 + ... + Fn = F(n+2) -1
   // A stack of depth n describes MIN_SLICE_LEN * (F1 + ... + F(n)) = MIN_SLICE_LEN * (F(n+2) -1) entries.

   // A stack which could describe up to (size_t)-1 entries must have a depth n
   // where MIN_SLICE_LEN*F(n+2) > (size_t)-1. That means the nth Fibonacci number must overflow
   // the type size_t.

   // loga(x) == log(x) / log(a)
   // 64 == log2(2**64) == log(2**64) / log(2) ; ==> log(2**64) == 64 * log(2)
   // pow(2, bitsof(size_t)) overflows size_t and pow(2, bitsof(size_t))/MIN_SLICE_LEN overflows (size_t)-1 / MIN_SLICE_LEN
   // ==> F(n+2) > (size_t)-1 / MIN_SLICE_LEN

   //  The value of the nth Fibonnaci Fn for large numbers of n can be calculated as
   //  Fn ~ 1/sqrt(5) * golden_ratio**n;
   //  (pow(golden_ratio, n) == golden_ratio**n)
   //  ==> n = logphi(Fn*sqrt(5)) == (log(Fn) + log(sqrt(5))) / log(phi)

   // TEST lengthof(stack): size_t
   {
      unsigned ln_phi_SIZEMAX = (unsigned) ((bitsof(size_t)*log(2) - log(MIN_SLICE_LEN) + log(sqrt(5))) / log(phi) + 0.5);
      size_t size1 = 1*MIN_SLICE_LEN; // size of previous set F1
      size_t size2 = 1*MIN_SLICE_LEN; // size of next set     F2
      size_t next;

      for (stacksize = 0; size2 >= size1; ++stacksize, size1=size2, size2=next) {
         next = size1 + size2; // next == MIN_SLICE_LEN*F(n+2) && n-1 == stacksize
      }

      TEST(stacksize == ln_phi_SIZEMAX-2);
      TEST(stacksize <= lengthof(((mergesort_t*)0)->stack));
   }

   // TEST lengthof(stack): big enough to sort array of size uint64_t
   {
      unsigned ln_phi_SIZEMAX = (unsigned) ((64*log(2) - log(MIN_SLICE_LEN) +  log(sqrt(5))) / log(phi) + 0.5);
      uint64_t size1 = 1*MIN_SLICE_LEN; // size of previous set F1
      uint64_t size2 = 1*MIN_SLICE_LEN; // size of next set     F2
      uint64_t next;

      for (stacksize = 0; size2 >= size1; ++stacksize, size1=size2, size2=next) {
         next = size1 + size2; // next == MIN_SLICE_LEN*F(n+2) && n-1 == stacksize
      }

      static_assert(85 == lengthof(((mergesort_t*)0)->stack), "value is 85 for uint64_t");
      TEST(stacksize == lengthof(((mergesort_t*)0)->stack));
      TEST(stacksize == ln_phi_SIZEMAX-2);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_memhelper(void)
{
   mergesort_t sort;
   vmpage_t    vmpage;

   // TEST alloctemp_mergesort: size == 0
   memset(&sort, 0, sizeof(sort));
   for (unsigned i = 0; i < 2; ++i) {
      TEST(0 == alloctemp_mergesort(&sort, 0));
      TEST(sort.tempmem == sort.temp);
      TEST(sizeof(sort.tempmem) == sort.tempsize);
   }

   // TEST alloctemp_mergesort: size > 0
   TEST(0 == alloctemp_mergesort(&sort, 1));
   TEST(0 != sort.temp);
   TEST(pagesize_vm() == sort.tempsize);
   vmpage = (vmpage_t) vmpage_INIT(sort.tempsize, sort.temp);
   TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR));

   // TEST alloctemp_mergesort: size == 0 frees memory
   TEST(0 == alloctemp_mergesort(&sort, 0));
   TEST(sort.tempmem == sort.temp);
   TEST(sizeof(sort.tempmem) == sort.tempsize);
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR));

   // alloctemp_mergesort: different sizes
   for (unsigned i = 1; i <= 10; ++i) {
      TEST(0 == alloctemp_mergesort(&sort, i * pagesize_vm()));
      TEST(0 != sort.temp);
      TEST(i*pagesize_vm() == sort.tempsize);
      vmpage = (vmpage_t) vmpage_INIT(sort.tempsize, sort.temp);
      TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR));
   }

   // TEST alloctemp_mergesort: ERROR
   init_testerrortimer(&s_mergesort_errtimer, 1, ENOMEM);
   TEST(ENOMEM == alloctemp_mergesort(&sort, 1));
   // freed (reset to tempmem)
   TEST(sort.tempmem == sort.temp);
   TEST(sizeof(sort.tempmem) == sort.tempsize);
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR));

   // TEST ensuretempsize: no reallocation
   for (unsigned i = 0; i <= sort.tempsize; ++i) {
      TEST(0 == ensuretempsize(&sort, i));
      TEST(sort.tempmem == sort.temp);
      TEST(sizeof(sort.tempmem) == sort.tempsize);
   }

   // TEST ensuretempsize: reallocation
   for (unsigned i = 10; i <= 11; ++i) {
      // reallocation
      TEST(0 == ensuretempsize(&sort, i * pagesize_vm()));
      TEST(0 != sort.temp);
      TEST(i*pagesize_vm() == sort.tempsize);
      vmpage = (vmpage_t) vmpage_INIT(sort.tempsize, sort.temp);
      TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR));
      // no reallocation
      TEST(0 == ensuretempsize(&sort, 0));
      TEST(0 == ensuretempsize(&sort, i * pagesize_vm()-1));
      TEST(vmpage.addr == sort.temp);
      TEST(vmpage.size == sort.tempsize);
      TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR));
   }

   // TEST ensuretempsize: ERROR
   for (unsigned i = 1; i <= 2; ++i) {
      if (sort.tempmem == sort.temp) {
         TEST(0 == ensuretempsize(&sort, sort.tempsize+1));
      }
      init_testerrortimer(&s_mergesort_errtimer, i, ENOMEM);
      TEST(ENOMEM == ensuretempsize(&sort, sort.tempsize+1));
      // freed (reset to tempmem)
      TEST(sort.tempmem == sort.temp);
      TEST(sizeof(sort.tempmem) == sort.tempsize);
      TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   mergesort_t sort = mergesort_FREE;

   // TEST mergesort_FREE
   TEST(0 == sort.compare);
   TEST(0 == sort.cmpstate);
   TEST(0 == sort.elemsize);
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST init_mergesort
   memset(&sort, 255, sizeof(sort));
   init_mergesort(&sort);
   TEST(0 == sort.compare);
   TEST(0 == sort.cmpstate);
   TEST(0 == sort.elemsize);
   TEST(sort.tempmem == sort.temp);
   TEST(sizeof(sort.tempmem) == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST free_mergesort: sort.tempmem == sort.temp
   sort.stacksize = 1;
   TEST(0 == free_mergesort(&sort));
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST free_mergesort: sort.tempmem != sort.temp
   alloctemp_mergesort(&sort, pagesize_vm());
   sort.stacksize = 1;
   TEST(sort.temp != 0);
   TEST(sort.temp != sort.tempmem);
   TEST(sort.tempsize == pagesize_vm());
   TEST(0 == free_mergesort(&sort));
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST free_mergesort: ERROR
   alloctemp_mergesort(&sort, pagesize_vm());
   sort.stacksize = 1;
   init_testerrortimer(&s_mergesort_errtimer, 1, EINVAL);
   TEST(EINVAL == free_mergesort(&sort));
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   return 0;
ONERR:
   return EINVAL;
}

static uint64_t s_compare_count = 0;

static int test_compare_ptr(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   ++s_compare_count;
   if ((uintptr_t)left < (uintptr_t)right)
      return -1;
   else if ((uintptr_t)left > (uintptr_t)right)
      return +1;
   return 0;
}

static int test_compare_long(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   ++s_compare_count;
   if (*(const long*)left < *(const long*)right)
      return -1;
   else if (*(const long*)left > *(const long*)right)
      return +1;
   return 0;
}

static int test_compare_bytes(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   ++s_compare_count;
   int lk = ((const uint8_t*)left)[0] * 256 + ((const uint8_t*)left)[1];
   int rk = ((const uint8_t*)right)[0] * 256 + ((const uint8_t*)right)[1];
   return lk - rk;
}

static int test_comparehalf_ptr(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   uintptr_t lk = (uintptr_t)left / 2;
   uintptr_t rk = (uintptr_t)right / 2;
   if (lk < rk)
      return -1;
   else if (lk > rk)
      return +1;
   return 0;
}

static int test_comparehalf_long(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   long lk = *(const long*)left / 2;
   long rk = *(const long*)right / 2;
   if (lk < rk)
      return -1;
   else if (lk > rk)
      return +1;
   return 0;
}

static int test_comparehalf_bytes(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   int lk = ((const uint8_t*)left)[0] * 256 + ((const uint8_t*)left)[1];
   int rk = ((const uint8_t*)right)[0] * 256 + ((const uint8_t*)right)[1];
   return lk/2 - rk/2;
}


static int test_compare_qsort(const void * left, const void * right)
{
   ++s_compare_count;
   if (*(const long*)left < *(const long*)right)
      return -1;
   else if (*(const long*)left > *(const long*)right)
      return +1;
   return 0;
}


static int test_query(void)
{
   // TEST compute_minslicelen: >= MIN_SLICE_LEN except if arraysize < MIN_SLICE_LEN
   TEST(MIN_SLICE_LEN == compute_minslicelen(64));
   for (size_t i = 0; i < 64; ++i) {
      TEST(i == compute_minslicelen(i));
   }

   // TEST compute_minslicelen: minlen * pow(2, shift)
   for (size_t i = 32; i < 64; ++i) {
      for (size_t shift = 1; shift <= bitsof(size_t)-6; ++shift) {
         TEST(i == compute_minslicelen(i << shift));
      }
   }

   // TEST compute_minslicelen: minlen * pow(2, shift) + DELTA < minlen * pow(2, shift+1)
   for (size_t i = 32; i < 64; ++i) {
      for (size_t shift = 1; shift <= bitsof(size_t)-6; ++shift) {
         for (size_t delta = 0; delta < shift; ++delta) {
            TEST(i+1 == compute_minslicelen((i << shift) + ((size_t)1 << delta)/*set single bit to 1*/));
            if (delta) {
               TEST(i+1 == compute_minslicelen((i << shift) + ((size_t)1 << delta)-1/*set all lower bits to 1*/));
            }
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_set(void)
{
   mergesort_t sort = mergesort_FREE;

   // TEST setsortstate
   sort.stacksize = 1;
   TEST(0 == setsortstate(&sort, &test_compare_ptr, (void*)3, 5, 15/*only used to check for EINVAL*/));
   TEST(sort.compare  == &test_compare_ptr);
   TEST(sort.cmpstate == (void*)3);
   TEST(sort.elemsize == 5);
   TEST(sort.stacksize == 0);
   sort.stacksize = 100;
   TEST(0 == setsortstate(&sort, &test_compare_long, (void*)0, 16, SIZE_MAX/16/*only used to check for EINVAL*/));
   TEST(sort.compare  == &test_compare_long);
   TEST(sort.cmpstate == 0);
   TEST(sort.elemsize == 16);
   TEST(sort.stacksize == 0);

   // TEST setsortstate: EINVAL (cmp == 0)
   TEST(EINVAL == setsortstate(&sort, 0, (void*)1, 1, 1));

   // TEST setsortstate: EINVAL (elemsize == 0)
   TEST(EINVAL == setsortstate(&sort, &test_compare_ptr, (void*)1, 0, 1));

   // TEST setsortstate: EINVAL (elemsize * array_len > (size_t)-1)
   TEST(EINVAL == setsortstate(&sort, &test_compare_ptr, (void*)1, 8, SIZE_MAX/8 + 1u));

   return 0;
ONERR:
   return EINVAL;
}

static void * s_compstate = 0;
static const void * s_left  = 0;
static const void * s_right = 0;

static int test_compare_save(void * cmpstate, const void * left, const void * right)
{
   s_compstate = cmpstate;
   s_left      = left;
   s_right     = right;
   return -1;
}

static int test_compare_save2(void * cmpstate, const void * left, const void * right)
{
   s_compstate = cmpstate;
   s_left      = left;
   s_right     = right;
   return +1;
}

static int test_searchgrequal(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (uintptr_t i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long) (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST search_greatequal: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, 3, alen));
                        TEST(alen == search_greatequal_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[3*(alen-1)]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == search_greatequal_long(&sort, (void*)key, alen, (uint8_t*)larray));
                        TEST(s_left == (void*)&larray[alen-1]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == search_greatequal_ptr(&sort, (void*)key, alen, (uint8_t*)parray));
                        TEST(s_left == (void*)parray[alen-1]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST search_greatequal: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // alen is returned in case no value in array >= key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*i + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i == search_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i == search_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*)larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i == search_greatequal_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*)parray)); break;
               }
            }
         }
      }
   }

   // TEST search_greatequal: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*multi*i);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == search_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == search_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*)larray)); break;
               }
            }
         }
      }
   }

   // TEST search_greatequal: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == search_greatequal_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == search_greatequal_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_rsearchgrequal(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (uintptr_t i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long) (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST rsearch_greatequal: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, 3, alen));
                        TEST(alen == rsearch_greatequal_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[0]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == rsearch_greatequal_long(&sort, (void*)key, alen, (uint8_t*) larray));
                        TEST(s_left == (void*)&larray[0]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == rsearch_greatequal_ptr(&sort, (void*)key, alen, (uint8_t*) parray));
                        TEST(s_left == (void*)parray[0]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST rsearch_greatequal: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // 0 is returned in case no value in array >= key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*(alen-i) + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i == rsearch_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i == rsearch_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i == rsearch_greatequal_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*) parray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greatequal: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*(alen-i)*multi);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == rsearch_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == rsearch_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greatequal: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save2, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == rsearch_greatequal_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == rsearch_greatequal_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_searchgreater(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (uintptr_t i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long)  (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST search_greater: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, 3, alen));
                        TEST(alen == search_greater_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[3*(alen-1)]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == search_greater_long(&sort, (void*)key, alen, (uint8_t*) larray));
                        TEST(s_left == (void*)&larray[alen-1]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == search_greater_ptr(&sort, (void*)key, alen, (uint8_t*) parray));
                        TEST(s_left == (void*)parray[alen-1]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST search_greater: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // alen is returned in case no value in array > key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*i + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               unsigned i2 = i + (i == alen ? 0 : kadd);
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i2 == search_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i2 == search_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i2 == search_greater_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*) parray)); break;
               }
            }
         }
      }
   }

   // TEST search_greater: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*multi*i);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == search_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == search_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               }
            }
         }
      }
   }

   // TEST search_greater: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == search_greater_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == search_greater_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }


   return 0;
ONERR:
   return EINVAL;
}

static int test_rsearchgreater(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (uintptr_t i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long) (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST rsearch_greater: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, 3, alen));
                        TEST(alen == rsearch_greater_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[0]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == rsearch_greater_long(&sort, (void*)key, alen, (uint8_t*) larray));
                        TEST(s_left == (void*)&larray[0]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == rsearch_greater_ptr(&sort, (void*)key, alen, (uint8_t*) parray));
                        TEST(s_left == (void*)parray[0]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST rsearch_greater: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // 0 is returned in case no value in array > key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*(alen-i) + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               unsigned i2 = i - (i == 0 ? 0 : kadd);
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i2 == rsearch_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i2 == rsearch_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i2 == rsearch_greater_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*) parray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greater: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*(alen-i)*multi);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == rsearch_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == rsearch_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greater: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save2, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == rsearch_greater_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == rsearch_greater_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static void set_value(uintptr_t value, uint8_t * barray, long * larray, void ** parray)
{
   parray[0] = (void*) value;
   larray[0] = (long) value;
   barray[0] = (uint8_t) (value / 256);
   barray[1] = (uint8_t) value;
   barray[2] = 0;
}

static int compare_value(int type, uintptr_t value, unsigned i, uint8_t * barray, long * larray, void ** parray)
{
   switch (type) {
   case 0: TEST(barray[3*i]   == (uint8_t) (value / 256));
           TEST(barray[3*i+1] == (uint8_t) value );
           TEST(barray[3*i+2] == 0); break;
   case 1: TEST(larray[i] == (long) value);  break;
   case 2: TEST(parray[i] == (void*) value); break;
   }

   return 0;
ONERR:
   return EINVAL;
}

static int compare_content(int type, uint8_t * barray, long * larray, void ** parray, size_t len)
{
   for (size_t i = 0; i < len; ++i) {
      uintptr_t key = 5*i;
      switch (type) {
      case 0: TEST(barray[3*i]   == (uint8_t) (key / 256));
              TEST(barray[3*i+1] == (uint8_t) key);
              TEST(barray[3*i+2] == 0); break;
      case 1: TEST(larray[i] == (long) key);  break;
      case 2: TEST(parray[i] == (void*) key); break;
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_merge(void)
{
   mergesort_t sort;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];
   typedef int (*merge_slices_f) (mergesort_t * sort, uint8_t * left, size_t llen, uint8_t * right, size_t rlen);
   merge_slices_f merge_slices [3][2] = {
      { &merge_adjacent_slices_bytes, &rmerge_adjacent_slices_bytes },
      { &merge_adjacent_slices_long,  &rmerge_adjacent_slices_long },
      { &merge_adjacent_slices_ptr,   &rmerge_adjacent_slices_ptr },
   };

   // prepare
   init_mergesort(&sort);
   for (unsigned i = 0; i < lengthof(larray); ++i) {
      uintptr_t val = 5*i;
      set_value(val, barray+3*i, larray+i, parray+i);
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: allocate enough temporary memory
   for (unsigned nrpage = 2; nrpage <= 10; nrpage += 2) {
      vmpage_t vmpage;
      TEST(0 == init_vmpage(&vmpage, pagesize_vm()*nrpage));
      for (int type = 0; type < 3; ++type) {
         for (int reverse = 0; reverse <= 1; ++reverse) {
            for (size_t lsize = pagesize_vm(); lsize < vmpage.size; lsize += pagesize_vm()) {
               switch (type) {
               case 0: setsortstate(&sort, &test_compare_bytes, 0, 2, 1); break;
               case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); break;
               case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); break;
               }
               memset(vmpage.addr, 1, vmpage.size);
               memset(vmpage.addr + lsize, 0, vmpage.size - lsize);
               TEST(0 == merge_slices[type][reverse](&sort,
                                 vmpage.addr, lsize / sort.elemsize,
                                 vmpage.addr+lsize, (vmpage.size - lsize) / sort.elemsize));
               TEST(0 != sort.temp);
               TEST(sort.tempmem  != sort.temp);
               TEST(sort.tempsize == (size_t) (reverse ? vmpage.size - lsize : lsize));
               TEST(0 == alloctemp_mergesort(&sort, 0));
               TEST(sort.tempmem  == sort.temp);
            }
         }
      }
      TEST(0 == free_vmpage(&vmpage));
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: all elements are already in place
   for (int type = 0; type < 3; ++type) {
      for (int reverse = 0; reverse <= 1; ++reverse) {
         for (unsigned llen = 1; llen < lengthof(larray); ++llen) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, lengthof(barray)); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
            }
            uint8_t * right = left + llen * sort.elemsize;
            TEST(0 == merge_slices[type][reverse](&sort, left, llen, right, lengthof(larray)-llen));
            // nothing changed
            sort.tempsize = 0;
            TEST(0 == compare_content(type, barray, larray, parray, lengthof(larray)));
            // no tempmem used
            TEST(sort.temp == sort.tempmem);
         }
      }
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: alternating left right
   for (int type = 0; type < 3; ++type) {
      for (int reverse = 0; reverse <= 1; ++reverse) {
         for (unsigned off = 0; off <= 1; ++off) {
            for (unsigned i = 0, ki = 0; i < lengthof(larray); ++i, ki += 2) {
               uintptr_t val = 5u*((ki%lengthof(larray)) + (ki>=lengthof(larray) ? off : (unsigned)!off));
               set_value(val, barray+3*i, larray+i, parray+i);
            }
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, lengthof(barray)); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
            }
            uint8_t * right = left + lengthof(larray)/2 * sort.elemsize;
            TEST(0 == merge_slices[type][reverse](&sort, left, lengthof(larray)/2, right, lengthof(larray)/2));
            TEST(0 == compare_content(type, barray, larray, parray, lengthof(larray)));
         }
      }
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: block modes
   size_t blocksize[][3][2] = {
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 1 }, { 0, 0 } }, // triggers first if (llen == 0) goto DONE;
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 1 }, { 2*MIN_BLK_LEN, 0 } }, // triggers second if (rlen == 0) goto DONE;
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 2*MIN_BLK_LEN, 0 } }, // triggers third if (rlen == 0) goto DONE;
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, MIN_BLK_LEN } }, // triggers fourth if (llen == 0) goto DONE;
      // reverse
      { { 0, 2*MIN_BLK_LEN}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, 1 } }, // triggers reverse first if (llen == 0) goto DONE;
      { { 2*MIN_BLK_LEN, 1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, 1 } }, // triggers reverse second if (rlen == 0) goto DONE;
      { { 2*MIN_BLK_LEN, MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, 1 } }, // triggers reverse third if (rlen == 0) goto DONE;
      { { 0, 1 }, { 1, 2*MIN_BLK_LEN }, { 2*MIN_BLK_LEN+1, 1 } }, // triggers reverse fourth if (llen == 0) goto DONE;
   };
   for (unsigned ti = 0; ti < lengthof(blocksize); ++ti) {
      size_t llen = 0;
      size_t rlen = 0;
      for (unsigned bi = 0; bi < lengthof(blocksize[ti]); ++bi) {
         llen += blocksize[ti][bi][0];
         rlen += blocksize[ti][bi][1];
      }
      for (int type = 0; type < 3; ++type) {
         for (int reverse = 0; reverse <= 1; ++reverse) {
            for (unsigned isswap = 0; isswap <= 1; ++isswap) {
               size_t ki = 0, li = 0, ri = llen;
               for (unsigned bi = 0; bi < lengthof(blocksize[ti]); ++bi) {
                  size_t lk = ki + (!isswap ? 0 : blocksize[ti][bi][1]);
                  size_t rk = ki + (isswap  ? 0 : blocksize[ti][bi][0]);
                  ki += blocksize[ti][bi][0] + blocksize[ti][bi][1];
                  for (size_t i = 0; i < blocksize[ti][bi][0]; ++i, ++lk, ++li) {
                     set_value(5*lk, barray+3*li, larray+li, parray+li);
                  }
                  for (size_t i = 0; i < blocksize[ti][bi][1]; ++i, ++rk, ++ri) {
                     set_value(5*rk, barray+3*ri, larray+ri, parray+ri);
                  }
               }
               uint8_t * left;
               switch (type) {
               case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, lengthof(barray)); left = barray; break;
               case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
               case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
               }
               uint8_t * right = left + llen * sort.elemsize;
               TEST(0 == merge_slices[type][reverse](&sort, left, llen, right, rlen));
               TEST(0 == compare_content(type, barray, larray, parray, llen+rlen));
            }
         }
      }
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: stable 1
   for (int type = 0; type < 3; ++type) {
      for (int reverse = 0; reverse <= 1; ++reverse) {
         for (unsigned isswap = 0; isswap <= 1; ++isswap) {
            for (unsigned i = 0; i < lengthof(larray)/2; ++i) {
               set_value(2*i, barray+3*i, larray+i, parray+i);
            }
            for (unsigned i = lengthof(larray)/2; i < lengthof(larray); ++i) {
               set_value(2*i-lengthof(larray)+1, barray+3*i, larray+i, parray+i);
            }
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_comparehalf_bytes, 0, 3, lengthof(barray)); left = barray; break;
            case 1: setsortstate(&sort, &test_comparehalf_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
            case 2: setsortstate(&sort, &test_comparehalf_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
            }
            uint8_t * right = left + lengthof(larray)/2 * sort.elemsize;
            TEST(0 == merge_slices[type][reverse](&sort, left, lengthof(larray)/2, right, lengthof(larray)/2));
            for (unsigned i = 0; i < lengthof(larray); ++i) {
               TEST(0 == compare_value(type, i, i, barray, larray, parray));
            }
         }
      }
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: stable 2
   for (int type = 0; type < 3; ++type) {
      for (int reverse = 0; reverse <= 1; ++reverse) {
         for (unsigned isswap = 0; isswap <= 1; ++isswap) {
            for (unsigned i = 0; i < lengthof(larray)/2; ++i) {
               set_value(2*i+1, barray+3*i, larray+i, parray+i);
            }
            for (unsigned i = lengthof(larray)/2; i < lengthof(larray); ++i) {
               set_value(2*i-lengthof(larray), barray+3*i, larray+i, parray+i);
            }
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_comparehalf_bytes, 0, 3, lengthof(barray)); left = barray; break;
            case 1: setsortstate(&sort, &test_comparehalf_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
            case 2: setsortstate(&sort, &test_comparehalf_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
            }
            uint8_t * right = left + lengthof(larray)/2 * sort.elemsize;
            TEST(0 == merge_slices[type][reverse](&sort, left, lengthof(larray)/2, right, lengthof(larray)/2));
            for (unsigned i = 0; i < lengthof(larray); i += 2) {
               TEST(0 == compare_value(type, i+1, i, barray, larray, parray));
               TEST(0 == compare_value(type, i, i+1, barray, larray, parray));
            }
         }
      }
   }

   // TEST merge_topofstack: stacksize + isSecondTop
   memset(barray, 0, sizeof(barray));
   memset(larray, 0, sizeof(larray));
   memset(parray, 0, sizeof(parray));
   for (unsigned stacksize = 2; stacksize <= 10; ++stacksize) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned isSecondTop = 0; isSecondTop <= 1; ++isSecondTop) {
            if (isSecondTop && stacksize < 3) continue;
            for (unsigned s = 1; s <= 3; ++s) {
               unsigned top = stacksize-1u-isSecondTop;
               memset(sort.stack, 0, sizeof(sort.stack));
               sort.stack[top-1].base = left;
               sort.stack[top-1].len  = s*lengthof(larray)/4;
               sort.stack[top].base = left + s*lengthof(larray)/4 * sort.elemsize;
               sort.stack[top].len  = lengthof(larray) - s*lengthof(larray)/4;
               sort.stack[top+1].base = (void*)(uintptr_t)s;
               sort.stack[top+1].len  = SIZE_MAX/s;
               sort.stacksize = stacksize;
               switch (type) {
               case 0: TEST(0 == merge_topofstack_bytes(&sort, isSecondTop)); break;
               case 1: TEST(0 == merge_topofstack_long(&sort, isSecondTop)); break;
               case 2: TEST(0 == merge_topofstack_ptr(&sort, isSecondTop)); break;
               }
               TEST(sort.stacksize == stacksize-1);
               TEST(sort.stack[top-1].base == left);
               TEST(sort.stack[top-1].len  == lengthof(larray));
               TEST(sort.stack[top+1].base == (void*)(uintptr_t)s);
               TEST(sort.stack[top+1].len  == SIZE_MAX/s);
               if (isSecondTop) {
                  TEST(sort.stack[top].base == (void*)(uintptr_t)s);
                  TEST(sort.stack[top].len  == SIZE_MAX/s);
               } else {
                  TEST(sort.stack[top].base == left + s*lengthof(larray)/4 * sort.elemsize);
                  TEST(sort.stack[top].len  == lengthof(larray) - s*lengthof(larray)/4);
               }
               for (unsigned i = 0; i < lengthof(larray); ++i) {
                  TEST(0 == barray[3*i] && 0 == barray[3*i+1] && 0 == barray[3*i+2]);
                  TEST(0 == larray[i]);
                  TEST(0 == parray[i]);
               }
            }
         }
      }
   }

   // TEST merge_topofstack: tempsize has size of smaller slice
   for (unsigned nrpage = 5; nrpage <= 10; nrpage += 5) {
      vmpage_t vmpage;
      TEST(0 == init_vmpage(&vmpage, pagesize_vm()*nrpage));
      for (int type = 0; type < 3; ++type) {
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); break;
         }
         for (unsigned lsize = 1; lsize < nrpage; lsize += nrpage/2) {
            memset(vmpage.addr, 1, vmpage.size);
            memset(vmpage.addr + lsize*pagesize_vm(), 0, vmpage.size - lsize*pagesize_vm());
            sort.stack[0].base = vmpage.addr;
            sort.stack[0].len  = lsize*pagesize_vm() / sort.elemsize;
            sort.stack[1].base = vmpage.addr + lsize*pagesize_vm();
            sort.stack[1].len  = vmpage.size / sort.elemsize - sort.stack[0].len;
            sort.stacksize = 2;
            switch (type) {
            case 0: TEST(0 == merge_topofstack_bytes(&sort, false)); break;
            case 1: TEST(0 == merge_topofstack_long(&sort, false)); break;
            case 2: TEST(0 == merge_topofstack_ptr(&sort, false)); break;
            }
            TEST(sort.stacksize == 1);
            TEST(sort.stack[0].base == vmpage.addr);
            TEST(sort.stack[0].len  == vmpage.size / sort.elemsize);
            TEST(sort.tempsize == pagesize_vm() * (lsize == 1 ? lsize : nrpage - lsize));
            TEST(0 == alloctemp_mergesort(&sort, 0));
         }
      }
      TEST(0 == free_vmpage(&vmpage));
   }

   // TEST establish_stack_invariant: does nothing if invariant ok
   memset(barray, 0, sizeof(barray));
   memset(larray, 0, sizeof(larray));
   memset(parray, 0, sizeof(parray));
   for (unsigned stackoffset = 0; stackoffset <= lengthof(sort.stack)/2; stackoffset += (unsigned)lengthof(sort.stack)/2) {
      for (unsigned size = 1; size <= 10; ++size) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            // 3 entries
            if (stackoffset) {
               sort.stack[stackoffset-2].base = left;
               sort.stack[stackoffset-2].len  = (3*size+4);
               sort.stack[stackoffset-1].base = left + (3*size+4)*sort.elemsize;
               sort.stack[stackoffset-1].len  = (2*size+2);
            }
            sort.stack[stackoffset+0].base = left + (5*size+6)*sort.elemsize;
            sort.stack[stackoffset+0].len  = size+1;
            sort.stack[stackoffset+1].base = left + (6*size+7)*sort.elemsize;
            sort.stack[stackoffset+1].len  = size;
            sort.stacksize = stackoffset+2;
            switch (type) {
            case 0: TEST(0 == establish_stack_invariant_bytes(&sort)); break;
            case 1: TEST(0 == establish_stack_invariant_long(&sort)); break;
            case 2: TEST(0 == establish_stack_invariant_ptr(&sort)); break;
            }
            TEST(sort.stacksize == stackoffset+2);
            if (stackoffset) {
               TEST(sort.stack[stackoffset-2].base == left);
               TEST(sort.stack[stackoffset-2].len  == (3*size+4));
               TEST(sort.stack[stackoffset-1].base == left + (3*size+4)*sort.elemsize);
               TEST(sort.stack[stackoffset-1].len  == (2*size+2));
            }
            TEST(sort.stack[stackoffset+0].base == left + (5*size+6)*sort.elemsize);
            TEST(sort.stack[stackoffset+0].len  == size+1);
            TEST(sort.stack[stackoffset+1].base == left + (6*size+7)*sort.elemsize);
            TEST(sort.stack[stackoffset+1].len  == size);
         }
      }
   }

   // TEST establish_stack_invariant: merge if top[-2].len <= top[-1].len
   for (unsigned stackoffset = 0; stackoffset <= lengthof(sort.stack)/3; stackoffset += (unsigned)lengthof(sort.stack)/3) {
      for (unsigned size = 1; size <= 10; ++size) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            if (stackoffset) {
               sort.stack[stackoffset-3].base = 0;
               sort.stack[stackoffset-3].len  = SIZE_MAX;
               sort.stack[stackoffset-2].base = 0;
               sort.stack[stackoffset-2].len  = SIZE_MAX/2;
               sort.stack[stackoffset-1].base = 0;
               sort.stack[stackoffset-1].len  = SIZE_MAX/4;
            }
            sort.stack[stackoffset+0].base = left;
            sort.stack[stackoffset+0].len  = size;
            sort.stack[stackoffset+1].base = left + size*sort.elemsize;
            sort.stack[stackoffset+1].len  = 9;
            sort.stacksize = stackoffset+2;
            switch (type) {
            case 0: TEST(0 == establish_stack_invariant_bytes(&sort)); break;
            case 1: TEST(0 == establish_stack_invariant_long(&sort)); break;
            case 2: TEST(0 == establish_stack_invariant_ptr(&sort)); break;
            }
            if (size == 10) {
               TEST(sort.stacksize == stackoffset+2);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size);
               TEST(sort.stack[stackoffset+1].base == left + size*sort.elemsize);
               TEST(sort.stack[stackoffset+1].len  == 9);
            } else {
               TEST(sort.stacksize == stackoffset+1);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size+9);
            }
         }
      }
   }

   // TEST establish_stack_invariant: merge if top[-3].len <= top[-2].len + top[-1].len
   for (unsigned stackoffset = 0; stackoffset <= lengthof(sort.stack)/2; stackoffset += (unsigned)lengthof(sort.stack)/2) {
      for (unsigned size = 1; size <= 10; ++size) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            if (stackoffset) {
               sort.stack[stackoffset-3].base = 0;
               sort.stack[stackoffset-3].len  = SIZE_MAX;
               sort.stack[stackoffset-2].base = 0;
               sort.stack[stackoffset-2].len  = SIZE_MAX/2;
               sort.stack[stackoffset-1].base = 0;
               sort.stack[stackoffset-1].len  = SIZE_MAX/4;
            }
            sort.stack[stackoffset+0].base = left;
            sort.stack[stackoffset+0].len  = size;
            sort.stack[stackoffset+1].base = left + size*sort.elemsize;
            sort.stack[stackoffset+1].len  = 5;
            sort.stack[stackoffset+2].base = left + (5+size)*sort.elemsize;
            sort.stack[stackoffset+2].len  = 4;
            sort.stacksize = stackoffset+3;
            switch (type) {
            case 0: TEST(0 == establish_stack_invariant_bytes(&sort)); break;
            case 1: TEST(0 == establish_stack_invariant_long(&sort)); break;
            case 2: TEST(0 == establish_stack_invariant_ptr(&sort)); break;
            }
            if (size == 10) {
               TEST(sort.stacksize == stackoffset+3);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size);
               TEST(sort.stack[stackoffset+1].base == left + size*sort.elemsize);
               TEST(sort.stack[stackoffset+1].len  == 5);
               TEST(sort.stack[stackoffset+2].base == left + (5+size)*sort.elemsize);
               TEST(sort.stack[stackoffset+2].len  == 4);
            } else if (size <= 4) {
               // first merge [-2] and [-1] which then satisfy invariant ([-3].len <= [-1].len)
               TEST(sort.stacksize == stackoffset+2);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size+5);
               TEST(sort.stack[stackoffset+1].base == left + (5+size)*sort.elemsize);
               TEST(sort.stack[stackoffset+1].len  == 4);

            } else {
               // first merge [-1] and [-2] and then with [-3]
               TEST(sort.stacksize == stackoffset+1);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size+9);
            }
         }
      }
   }

   // TEST establish_stack_invariant: merge if top[-4].len <= top[-3].len + top[-2].len after merge
   // uses example: 120, 80, 25, 20, 30 ==> 120, 80, 45, 30 ==> 120, 80, 75 ==> 120, 155 ==> 275
   for (unsigned stackoffset = 0; stackoffset <= lengthof(sort.stack)/2; stackoffset += (unsigned)lengthof(sort.stack)/2) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         if (stackoffset) {
            sort.stack[stackoffset-3].base = 0;
            sort.stack[stackoffset-3].len  = SIZE_MAX;
            sort.stack[stackoffset-2].base = 0;
            sort.stack[stackoffset-2].len  = SIZE_MAX/2;
            sort.stack[stackoffset-1].base = 0;
            sort.stack[stackoffset-1].len  = SIZE_MAX/4;
         }
         sort.stack[stackoffset+0].base = left;
         sort.stack[stackoffset+0].len  = 120;
         sort.stack[stackoffset+1].base = left + 120*sort.elemsize;
         sort.stack[stackoffset+1].len  = 80;
         sort.stack[stackoffset+2].base = left + 200*sort.elemsize;
         sort.stack[stackoffset+2].len  = 25;
         sort.stack[stackoffset+3].base = left + 225*sort.elemsize;
         sort.stack[stackoffset+3].len  = 20;
         sort.stack[stackoffset+4].base = left + 245*sort.elemsize;
         sort.stack[stackoffset+4].len  = 30;
         sort.stacksize = stackoffset+5;
         switch (type) {
         case 0: TEST(0 == establish_stack_invariant_bytes(&sort)); break;
         case 1: TEST(0 == establish_stack_invariant_long(&sort)); break;
         case 2: TEST(0 == establish_stack_invariant_ptr(&sort)); break;
         }
         TEST(sort.stacksize == stackoffset+1);
         TEST(sort.stack[stackoffset+0].base == left);
         TEST(sort.stack[stackoffset+0].len  == 275);
      }
   }

   // TEST merge_all
   for (unsigned stacksize = 0; stacksize <= 5 && stacksize < lengthof(sort.stack); ++stacksize) {
      for (unsigned incr = 0; incr <= 1; ++incr) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            unsigned total = 0;
            for (unsigned i = 0, s = 1 + (1-incr)*(stacksize-1); i < stacksize; ++i, total += s, s += (unsigned)-1 + 2*incr) {
               sort.stack[i].base = left + total * sort.elemsize;
               sort.stack[i].len  = s;
            }
            sort.stacksize = stacksize;
            switch (type) {
            case 0: TEST(0 == merge_all_bytes(&sort)); break;
            case 1: TEST(0 == merge_all_long(&sort)); break;
            case 2: TEST(0 == merge_all_ptr(&sort)); break;
            }
            if (stacksize == 0) {
               TEST(sort.stacksize == 0);
            } else {
               TEST(sort.stacksize == 1);
               TEST(sort.stack[0].base == left);
               TEST(sort.stack[0].len  == total);
            }
         }
      }
   }

   // unprepare
   TEST(0 == free_mergesort(&sort));

   return 0;
ONERR:
   free_mergesort(&sort);
   return EINVAL;
}

static int test_presort(void)
{
   mergesort_t sort;
   void *      parray[64];
   long        larray[64];
   uint8_t     barray[3*64];

   // prepare
   init_mergesort(&sort);

   // TEST insertsort: stable (equal elements keep position relative to themselves)
   for (unsigned i = 0; i < lengthof(larray); ++i) {
      set_value(lengthof(larray)-1-i, barray+3*i, larray+i, parray+i);
   }
   for (int type = 0; type < 3; ++type) {
      uint8_t * left;
      switch (type) {
      case 0: setsortstate(&sort, &test_comparehalf_bytes, 0, 3, 1); left = barray; break;
      case 1: setsortstate(&sort, &test_comparehalf_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
      case 2: setsortstate(&sort, &test_comparehalf_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
      }
      switch (type) {
      case 0: insertsort_bytes(&sort, 1, lengthof(larray), left); break;
      case 1: insertsort_long(&sort, 1, lengthof(larray), left); break;
      case 2: insertsort_ptr(&sort, 1, lengthof(larray), left); break;
      }
      for (unsigned i = 0; i < lengthof(larray); i += 2) { // sorted
         TEST(0 == compare_value(type, i+1, i, barray, larray, parray));
         TEST(0 == compare_value(type, i, i+1, barray, larray, parray));
      }
   }

   // TEST insertsort: elements before start are not sorted
   for (uint8_t len = 1; len < lengthof(larray); ++len) {
      for (uint8_t start = 1; start <= len; ++start) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            for (unsigned i = 0; i < start; ++i) {
               set_value(start-i, barray+3*i, larray+i, parray+i);
            }
            for (unsigned i = start; i < len; ++i) {
               set_value(len-i+start, barray+3*i, larray+i, parray+i);
            }
            switch (type) {
            case 0: insertsort_bytes(&sort, start, len, left); break;
            case 1: insertsort_long(&sort, start, len, left); break;
            case 2: insertsort_ptr(&sort, start, len, left); break;
            }
            for (unsigned i = 0; i < start; ++i) { // not sorted
               TEST(0 == compare_value(type, start-i, i, barray, larray, parray));
            }
            for (unsigned i = start; i < len; ++i) { // sorted
               TEST(0 == compare_value(type, i+1, i, barray, larray, parray));
            }
         }
      }
   }

   // TEST insertsort: sort ascending & descending
   for (unsigned startval = 1; startval <= lengthof(larray); ++startval) {
      for (unsigned step = 1; step < lengthof(larray); step += 2) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            for (unsigned i = 0; i < lengthof(larray); ++i) {
               unsigned val = (startval + i * step) % lengthof(larray);
               set_value(val, barray+3*i, larray+i, parray+i);
            }
            switch (type) {
            case 0: insertsort_bytes(&sort, 1, lengthof(larray), left); break;
            case 1: insertsort_long(&sort, 1, lengthof(larray), left); break;
            case 2: insertsort_ptr(&sort, 1, lengthof(larray), left); break;
            }
            for (unsigned i = 0; i < lengthof(larray); ++i) {
               TEST(0 == compare_value(type, i, i, barray, larray, parray));
            }
         }
      }
   }

   // TEST reverse_elements
   for (unsigned len = 1; len <= lengthof(larray); ++len) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned i = 0; i < len; ++i) {
            set_value(i, barray+3*i, larray+i, parray+i);
         }
         switch (type) {
         case 0: reverse_elements_bytes(&sort, left, left+(len-1)*sort.elemsize); break;
         case 1: reverse_elements_long(&sort, left, left+(len-1)*sort.elemsize); break;
         case 2: reverse_elements_ptr(&sort, left, left+(len-1)*sort.elemsize); break;
         }
         for (unsigned i = 0; i < len; ++i) {
            TEST(0 == compare_value(type, i, len-1-i, barray, larray, parray));
         }
      }
   }

   // TEST count_presorted: ascending (or equal)
   for (unsigned len = 2; len <= lengthof(larray); ++len) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned i = 0; i < lengthof(larray); ++i) {
            set_value(1+i/2, barray+3*i, larray+i, parray+i);
         }
         if (len < lengthof(larray)) {
            set_value(len/2, barray+3*len, larray+len, parray+len);
         }
         switch (type) {
         case 0: TEST(len == count_presorted_bytes(&sort, len, left)); break;
         case 1: TEST(len == count_presorted_long(&sort, len, left)); break;
         case 2: TEST(len == count_presorted_ptr(&sort, len, left)); break;
         }
      }
   }

   // TEST count_presorted: descending (order is reversed before return)
   for (unsigned len = 2; len <= lengthof(larray); ++len) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned i = 0; i < lengthof(larray); ++i) {
            set_value(SIZE_MAX-i, barray+3*i, larray+i, parray+i);
         }
         if (len < lengthof(larray)) {
            set_value(SIZE_MAX, barray+3*len, larray+len, parray+len);
         }
         switch (type) {
         case 0: TEST(len == count_presorted_bytes(&sort, len, left)); break;
         case 1: TEST(len == count_presorted_long(&sort, len, left)); break;
         case 2: TEST(len == count_presorted_ptr(&sort, len, left)); break;
         }
         for (unsigned i = 0; i < len; ++i) {
            TEST(0 == compare_value(type, SIZE_MAX-i, len-1-i, barray, larray, parray));
         }
      }
   }

   // unprepare
   TEST(0 == free_mergesort(&sort));

   return 0;
ONERR:
   free_mergesort(&sort);
   return EINVAL;
}

static void shuffle(uint8_t elemsize, size_t len, uint8_t a[len*elemsize])
{
   assert(elemsize <= 32);
   uint8_t temp[32];
   for (unsigned i = 0; i < len; ++i) {
      size_t r = (size_t)random() % len;
      memcpy(temp, &a[elemsize*r], elemsize);
      memcpy(&a[r*elemsize], &a[i*elemsize], elemsize);
      memcpy(&a[i*elemsize], temp, elemsize);
   }
}

static int test_sort(mergesort_t * sort, const unsigned len, memblock_t * mblock)
{
   uint8_t * const a = mblock->addr;

   // prepare
   s_compare_count = 0;

   // TEST sortblob_mergesort: bytes stable
   TEST(mblock->size > 3*65536)
   for (uintptr_t i = 0; i < 65536; ++i) {
      a[3*i]   = (uint8_t) (i);
      a[3*i+1] = 0;
      a[3*i+2] = (uint8_t) (i/256);
   }
   TEST(0 == sortblob_mergesort(sort, 3, 65536, a, &test_compare_bytes, 0));
   for (uintptr_t i = 0; i < 65536; ++i) {
      TEST(a[3*i]   == (uint8_t) (i/256));
      TEST(a[3*i+1] == 0);
      TEST(a[3*i+2] == (uint8_t)i);
   }

   // TEST sortblob_mergesort: long stable
   TEST(mblock->size > 2*sizeof(long)*65536);
   for (uintptr_t i = 0; i < 65536; ++i) {
      ((long*)a)[2*i]   = (long) (i & 255);
      ((long*)a)[2*i+1] = (long) (i / 256);
   }
   TEST(0 == sortblob_mergesort(sort, 2*sizeof(long), 65536, a, &test_compare_long, 0));
   for (uintptr_t i = 0; i < 65536; ++i) {
      TEST(((long*)a)[2*i]   == (long) (i / 256));
      TEST(((long*)a)[2*i+1] == (long) (i & 255));
   }

   // TEST sortptr_mergesort: stable
   for (uintptr_t i = 0; i < len; ++i) {
      ((void**)a)[i] = (void*) i;
   }
   shuffle(2*sizeof(void*), len/2, (uint8_t*)a);
   TEST(0 == sortptr_mergesort(sort, len, (void**)a, &test_comparehalf_ptr, 0));
   for (uintptr_t i = 0; i < len; ++i) {
      TEST(((void**)a)[i] == (void*) i);
   }

   // TEST sortblob_mergesort: bytes ascending
   for (uintptr_t i = 0; i < 65536; ++i) {
      a[2*i]   = (uint8_t) (i/256);
      a[2*i+1] = (uint8_t) (i);
   }
   TEST(0 == sortblob_mergesort(sort, 2, 65536, a, &test_compare_bytes, 0));
   for (uintptr_t i = 0; i < 65536; ++i) {
      TEST(a[2*i]   == (uint8_t) (i/256));
      TEST(a[2*i+1] == (uint8_t) (i));
   }

   // TEST sortblob_mergesort: long ascending
   TEST(mblock->size > 3*sizeof(long)*len);
   for (uintptr_t i = 0; i < len; ++i) {
      ((long*)a)[3*i]   = (long) i;
      ((long*)a)[3*i+1] = (long) i;
      ((long*)a)[3*i+2] = (long) i;
   }
   TEST(0 == sortblob_mergesort(sort, 3*sizeof(long), len, a, &test_compare_long, 0));
   for (uintptr_t i = 0; i < len; ++i) {
      TEST(((long*)a)[3*i]   == (long) i);
      TEST(((long*)a)[3*i+1] == (long) i);
      TEST(((long*)a)[3*i+2] == (long) i);
   }

   // TEST sortptr_mergesort: ascending
   for (uintptr_t i = 0; i < len; ++i) {
      ((void**)a)[i] = (void*) i;
   }
   TEST(0 == sortptr_mergesort(sort, len, (void**)a, &test_compare_ptr, 0));
   for (uintptr_t i = 0; i < len; ++i) {
      TEST(((void**)a)[i] == (void*) i);
   }

   // TEST sortblob_mergesort: bytes descending
   for (uintptr_t i = 0; i < 65536; ++i) {
      a[2*i]   = (uint8_t) ((65535-i)/256);
      a[2*i+1] = (uint8_t)  (65535-i);
   }
   TEST(0 == sortblob_mergesort(sort, 2, 65536, a, &test_compare_bytes, 0));
   for (uintptr_t i = 0; i < 65536; ++i) {
      TEST(a[2*i]   == (uint8_t) (i/256));
      TEST(a[2*i+1] == (uint8_t) (i));
   }

   // TEST sortblob_mergesort: long descending
   for (uintptr_t i = 0; i < len; ++i) {
      ((long*)a)[i] = (long) (len-1-i);
   }
   TEST(0 == sortblob_mergesort(sort, sizeof(long), len, a, &test_compare_long, 0));
   for (uintptr_t i = 0; i < len; ++i) {
      TEST(((long*)a)[i] == (long) i);
   }

   // TEST sortptr_mergesort: descending
   for (uintptr_t i = 0; i < len; ++i) {
      ((void**)a)[i] = (void*) (len-1-i);
   }
   TEST(0 == sortptr_mergesort(sort, len, (void**)a, &test_compare_ptr, 0));
   for (uintptr_t i = 0; i < len; ++i) {
      TEST(((void**)a)[i] == (void*) i);
   }

   // TEST sortblob_mergesort: bytes random
   for (unsigned t = 0; t < 1; ++t) {
      if (t == 0) {
         for (uintptr_t i = 0; i < 65536; ++i) {
            a[2*i]   = (uint8_t) (i/256);
            a[2*i+1] = (uint8_t) (i);
         }
      }
      shuffle(2, 65536, a);
      TEST(0 == sortblob_mergesort(sort, 2, 65536, a, &test_compare_bytes, 0));
      for (uintptr_t i = 0; i < 65536; ++i) {
         TEST(a[2*i]   == (uint8_t) (i/256));
         TEST(a[2*i+1] == (uint8_t) (i));
      }
   }

   // TEST sortblob_mergesort: long random
   for (unsigned t = 0; t < 2; ++t) {
      if (t == 0) {
         for (uintptr_t i = 0; i < len; ++i) {
            ((long*)a)[2*i]   = (long) i;
            ((long*)a)[2*i+1] = (long) i;
         }
      }
      shuffle(2*sizeof(long), len, a);
      TEST(0 == sortblob_mergesort(sort, 2*sizeof(long), len, a, &test_compare_long, 0));
      for (uintptr_t i = 0; i < len; ++i) {
         TEST(((long*)a)[2*i]   == (long) i);
         TEST(((long*)a)[2*i+1] == (long) i);
      }
   }

   // TEST sortptr_mergesort: random
   for (unsigned t = 0; t < 2; ++t) {
      if (t == 0) {
         for (uintptr_t i = 0; i < len; ++i) {
            ((void**)a)[i] = (void*) i;
         }
      }
      shuffle(sizeof(void*), len, a);
      TEST(0 == sortptr_mergesort(sort, len, (void**)a, &test_compare_ptr, 0));
      for (uintptr_t i = 0; i < len; ++i) {
         TEST(((void**)a)[i] == (void*) i);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_measuretime(mergesort_t * sort, const unsigned len, memblock_t * mblock)
{
   uint8_t * const a     = mblock->addr;
   systimer_t      timer = systimer_FREE;

   // prepare
   s_compare_count = 0;

   // TEST sortptr_mergesort: compare against quicksort
   for (uintptr_t i = 0; i < len; ++i) {
      ((long*)a)[i] = (long) i;
   }
   srandom(312854);
   shuffle(sizeof(long), len, a);
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   TEST(0 == startinterval_systimer(timer, &(struct timevalue_t){.nanosec = 1000000}));
   s_compare_count = 0;
   TEST(0 == sortblob_mergesort(sort, sizeof(long), len, a, &test_compare_long, 0));
   uint64_t mergetime_ms;
   uint64_t mergecount = s_compare_count;
   TEST(0 == expirationcount_systimer(timer, &mergetime_ms));
   for (uintptr_t i = 0; i < len; ++i) {
      TEST(((long*)a)[i] == (long) i);
   }
   srandom(312854);
   shuffle(sizeof(long), len, a);
   TEST(0 == startinterval_systimer(timer, &(struct timevalue_t){.nanosec = 1000000}));
   s_compare_count = 0;
   qsort(a, len, sizeof(long), test_compare_qsort);
   uint64_t qsorttime_ms;
   TEST(0 == expirationcount_systimer(timer, &qsorttime_ms));
   // mergesort needs less compares !!
   if (mergecount > s_compare_count) {
      logwarning_unittest("quicksort uses less compares") ;
   }
   // mergesort beats quicksort sometimes
   if (qsorttime_ms < mergetime_ms && !isrepeat_unittest()) {
      logwarning_unittest("quicksort is faster") ;
   }

   // unprepare
   TEST(0 == free_systimer(&timer));

   return 0;
ONERR:
   free_systimer(&timer);
   return EINVAL;
}

#if 0
/*
 * Algorithm to build slices from top to down
 * with minsize = 1 (or 2 in case of upper half).
 *
 * This could be used to get rid of stack in some future
 * implementation.
 *
 * It is also possible to double slice_size (see algorithm)
 * until count_presorted is reached, so at least half of the
 * elements of presorted slices could be used in this top
 * down approach.
 *
 * */
static int build_top_down_slices()
{
   size_t size = 63; // test algorithm with all sizes up to 1024 if it works !
   size_t offset = 0;
   struct {
      size_t offset;
      size_t size;
   } stack[256]; // use stack to validate algorithm
   size_t stacksize = 0;

   /*
    * This part calculates slice_size, upperbits, and sizebits
    * from any offset. (could be optimized for offset == 0)
    *
    * */
   size_t slice_size = size;
   size_t slice_off  = offset; // nr of sorted elements (offset of next unsorted)
   size_t upperbits  = 0;
   size_t sizebits   = 0;
   for (;;) {
      size_t old = slice_size;
      slice_size >>= 1;
      const bool isupper = (slice_off >= slice_size);
      slice_off  -= isupper ? slice_size : 0;
      const bool isend = (slice_size == 1); // use slice_size < 64 for minsizes >= 32
      slice_size += isupper ? (old&1) : 0;
      sizebits  = (sizebits << 1) | (old&1);
      upperbits = (upperbits << 1) | isupper;
      if (isend) break;
   }

   for (;;) {

      if (slice_size + offset > size) slice_size = size - offset;
      printf("sort (off: %d sz: %d)\n", offset, slice_size);
      stack[stacksize].offset = offset;
      stack[stacksize].size   = slice_size;
      ++ stacksize;
      offset += slice_size;
      // merge
      while (upperbits & 1) {
         size_t lower_size = slice_size - (sizebits&1);
         upperbits >>= 1;
         sizebits  >>= 1;
         slice_size += lower_size;
         printf("merge (off: %d sz: %d)\n", offset-slice_size, slice_size);
         assert(stacksize >= 2);
         -- stacksize;
         assert(stack[stacksize-1].offset == offset-slice_size);
         assert(stack[stacksize-1].size   == lower_size);
         stack[stacksize-1].size = slice_size;
      }

      if (offset == size) break;

      slice_off = 0;
      bool isend = (slice_size == 1); // use slice_size < 64 for minsizes >= 32
      upperbits |= 1;
      slice_size += (sizebits&1);
      while (!isend) {
         size_t old = slice_size;
         slice_size >>= 1;
         isend = (slice_size == 1); // use slice_size < 64 for minsizes >= 32
         sizebits  = (sizebits << 1) | (old&1);
         upperbits = (upperbits << 1) /*| false*/ ;
      }

   }

   return 0;
}
#endif

int unittest_ds_sort_mergesort()
{
   const unsigned len = 300000;
   memblock_t     mblock = memblock_FREE;
   mergesort_t    sort;

   init_mergesort(&sort);
   TEST(0 == ALLOC_MM(len * (4*sizeof(void*)), &mblock));

   if (test_stacksize())      goto ONERR;
   if (test_memhelper())      goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_set())            goto ONERR;
   if (test_searchgrequal())  goto ONERR;
   if (test_rsearchgrequal()) goto ONERR;
   if (test_searchgreater())  goto ONERR;
   if (test_rsearchgreater()) goto ONERR;
   if (test_merge())          goto ONERR;
   if (test_presort())        goto ONERR;
   if (test_sort(&sort, len/10, &mblock))       goto ONERR;
   if (test_measuretime(&sort, len, &mblock))   goto ONERR;

   TEST(0 == free_mergesort(&sort));
   TEST(0 == FREE_MM(&mblock));

   return 0;
ONERR:
   FREE_MM(&mblock);
   free_mergesort(&sort);
   return EINVAL;
}

#endif
