/* title: LinuxSystemContext impl

   Implements <LinuxSystemContext>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/platform/Linux/syscontext.h
    Header file <LinuxSystemContext>.

   file: C-kern/platform/Linux/syscontext.c
    Implementation file <LinuxSystemContext impl>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/platform/Linux/syscontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/log2.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/vm.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/io/pipe.h"
#include "C-kern/api/io/filesystem/file.h"
#endif

// struct: syscontext_t
struct syscontext_t;

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_syscontext_errtimer
 * Used to simulate an error in <initrun_syscontext>. */
static test_errortimer_t   s_syscontext_errtimer = test_errortimer_FREE;
#endif


// group: helper

static inline int validate_pagesize(size_t pagesize)
{
   return   pagesize < 256
            || ! ispowerof2_int(pagesize)
            ? EINVAL : 0;
}

// group: lifetime

int init_syscontext(/*out*/syscontext_t* scontext)
{
   int err;

   size_t pagesize = sys_pagesize_vm();

   if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      pagesize = 128;
   } else if (PROCESS_testerrortimer(&s_syscontext_errtimer, &err)) {
      pagesize = 1023;
   }

   err = validate_pagesize(pagesize);
   if (err) goto ONERR;

   // set out param
   scontext->pagesize_vm = pagesize;
   scontext->log2pagesize_vm = log2_int(pagesize);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: query

int isfree_syscontext(const syscontext_t* scontext)
{
         return   0 == scontext->pagesize_vm
                  && 0 == scontext->log2pagesize_vm;
}

int isvalid_syscontext(const syscontext_t* scontext)
{
   return   scontext->pagesize_vm >= 256
            && scontext->pagesize_vm == 1u << scontext->log2pagesize_vm
            && scontext->pagesize_vm == sys_pagesize_vm();
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_helper(void)
{
   // TEST validate_pagesize: EINVAL
   for (unsigned i=0; i<256; ++i) {
      TEST(EINVAL == validate_pagesize(i))
   }
   for (unsigned i=256; i; i<<=1) {
      TEST(EINVAL == validate_pagesize(i-1))
      TEST(EINVAL == validate_pagesize(i+1))
      TEST(EINVAL == validate_pagesize(i|(i-1)))
   }

   // TEST validate_pagesize
   for (unsigned i=256; i; i<<=1) {
      TEST(0 == validate_pagesize(i))
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   syscontext_t   sc = syscontext_FREE;
   uint8_t      * logbuf;
   size_t         logsize;
   size_t         logsize2;

   // prepare

   // TEST syscontext_FREE
   TEST(1 == isfree_syscontext(&sc));

   // TEST init_syscontext
   TEST(0 == init_syscontext(&sc));
   TEST(1 == isvalid_syscontext(&sc));

   // TEST init_syscontext: simulated ERROR
   PRINTF_ERRLOG("-- init_syscontext: simulated ERROR --\n");
   GETBUFFER_ERRLOG(&logbuf, &logsize);
   for (unsigned i=1; ; ++i) {
      init_testerrortimer(&s_syscontext_errtimer, i, 333);
      int err = init_syscontext(&sc);
      GETBUFFER_ERRLOG(&logbuf, &logsize2);
      if (!err) {
         TEST(3  == i);
         TEST(logsize == logsize2);
         free_testerrortimer(&s_syscontext_errtimer);
         break;
      }
      TESTP(EINVAL == err, "i:%d err:%d", i, err);
      TEST(logsize2 > logsize + 120);
      logsize = logsize2;
   }
   PRINTF_ERRLOG("-- \"\" --\n");

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   syscontext_t sc = syscontext_FREE;

   // TEST isfree_syscontext
   TEST(1 == isfree_syscontext(&sc));
   sc.pagesize_vm = 1;
   TEST(0 == isfree_syscontext(&sc));
   sc.pagesize_vm = 0;
   sc.log2pagesize_vm = 1;
   TEST(0 == isfree_syscontext(&sc));
   sc.log2pagesize_vm = 0;
   TEST(1 == isfree_syscontext(&sc));

   // TEST isvalid_syscontext
   TEST( 0 == isvalid_syscontext(&sc));
   sc.pagesize_vm = sys_pagesize_vm();
   for (sc.log2pagesize_vm = 1; sc.pagesize_vm != (1u << sc.log2pagesize_vm); ++sc.log2pagesize_vm) {
   }
   TEST( 1 == isvalid_syscontext(&sc));
   for (unsigned i=1; i<=3; ++i) {
      sc.pagesize_vm += i;
      TEST( 0 == isvalid_syscontext(&sc));
      sc.pagesize_vm -= 2*i;
      TEST( 0 == isvalid_syscontext(&sc));
      sc.pagesize_vm += i;
      TEST( 1 == isvalid_syscontext(&sc));
      sc.log2pagesize_vm = (uint8_t) (sc.log2pagesize_vm + i);
      TEST( 0 == isvalid_syscontext(&sc));
      sc.log2pagesize_vm = (uint8_t) (sc.log2pagesize_vm - 2*i);
      TEST( 0 == isvalid_syscontext(&sc));
      sc.log2pagesize_vm = (uint8_t) (sc.log2pagesize_vm + i);
      TEST( 1 == isvalid_syscontext(&sc));
   }

   // TEST context_syscontext
   TEST( context_syscontext() == (threadcontext_t*) (((uintptr_t)&sc) - ((uintptr_t)&sc) % stacksize_syscontext()));

   // TEST context2_syscontext
   for (size_t i = 0; i < 1000*stacksize_syscontext(); i += stacksize_syscontext()) {
      TEST((threadcontext_t*)i == context2_syscontext(i));
      TEST((threadcontext_t*)i == context2_syscontext(i+1));
      TEST((threadcontext_t*)i == context2_syscontext(i+stacksize_syscontext()-1));
   }

   // TEST stacksize_syscontext
   TEST( 0 == stacksize_syscontext() % pagesize_vm());
   TEST( 2 <  stacksize_syscontext() / pagesize_vm());

   return 0;
ONERR:
   return EINVAL;
}

int unittest_platform_syscontext()
{
   if (test_helper())      goto ONERR;
   if (test_query())       goto ONERR;
   if (test_initfree())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
