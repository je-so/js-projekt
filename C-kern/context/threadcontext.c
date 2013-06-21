/* title: ThreadContext impl
   Implements the global (thread local) context of a running system thread.
   See <ThreadContext>.

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

   file: C-kern/api/context/threadcontext.h
    Header file <ThreadContext>.

   file: C-kern/context/threadcontext.c
    Implementation file <ThreadContext impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/context/threadcontext.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/writer/log/logmain.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/pagecache_macros.h"
#include "C-kern/api/memory/mm/mm.h"
#include "C-kern/api/test/errortimer.h"
// TEXTDB:SELECT('#include "'header-name'"')FROM("C-kern/resource/config/initthread")
#include "C-kern/api/memory/pagecache_impl.h"
#include "C-kern/api/memory/mm/mm_impl.h"
#include "C-kern/api/task/syncrun.h"
#include "C-kern/api/cache/objectcache_impl.h"
#include "C-kern/api/io/writer/log/logwriter.h"
// TEXTDB:END
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


// section: threadcontext_t

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_threadcontext_errtimer
 * Simulates an error in <init_threadcontext>. */
static test_errortimer_t   s_threadcontext_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: helper

/* define: INITIOBJ
 * Initializes iobj. Calls ALLOCSTATIC_PAGECACHE to allocate memory.
 * This memory is initialized with call to init_##module() and the address
 * is assigned to iobj.object. The return value of interfacethread_##module()
 * is assigned to iobj.iimpl. */
#define INITIOBJ(module, objtype_t, iobj)                   \
         int err ;                                          \
         memblock_t memobject = memblock_INIT_FREEABLE ;    \
                                                            \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = ALLOCSTATIC_PAGECACHE(                       \
                  sizeof(objtype_t), &memobject) ;          \
         if (err) goto ONABORT ;                            \
                                                            \
         objtype_t * newobj ;                               \
         newobj = (objtype_t*) memobject.addr ;             \
                                                            \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = init_##module(newobj) ;                      \
         if (err) goto ONABORT ;                            \
                                                            \
         init_iobj(&(iobj),                                 \
               (void*)newobj, interfacethread_##module()) ; \
                                                            \
         return 0 ;                                         \
      ONABORT:                                              \
         FREESTATIC_PAGECACHE(&memobject) ;                 \
         return err ;

/* define: FREEIOBJ
 * Frees iobj. Calls FREESTATIC_PAGECACHE to free memory as last operation.
 * Calls free_##module() to free the object iobj.object.
 * The return value of interfacethread_##module() is checked against iobj.iimpl.
 * iobj is cleared. Calling it twice has no effect. */
#define FREEIOBJ(module, objtype_t, iobj, staticiobj)       \
         int err ;                                          \
         int err2 ;                                         \
         objtype_t * delobj = (objtype_t*) (iobj).object ;  \
                                                            \
         if ((iobj).object != (staticiobj).object) {        \
            assert(  interfacethread_##module()             \
                     == (iobj).iimpl) ;                     \
                                                            \
            (iobj) = (staticiobj) ;                         \
                                                            \
            err = free_##module(delobj) ;                   \
                                                            \
            memblock_t memobject =                          \
                        memblock_INIT(                      \
                           sizeof(objtype_t),               \
                           (uint8_t*)delobj) ;              \
            err2 = FREESTATIC_PAGECACHE(&memobject) ;       \
            if (err2) err = err2 ;                          \
                                                            \
            return err ;                                    \
         }                                                  \
         return 0 ;

/* define: INITOBJECT
 * Initializes object. Calls ALLOCSTATIC_PAGECACHE to allocate memory.
 * This memory is initialized with call to init_##module() and the address
 * is assigned to object. */
#define INITOBJECT(module, objtype_t, object)               \
         int err ;                                          \
         memblock_t memobject = memblock_INIT_FREEABLE ;    \
                                                            \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = ALLOCSTATIC_PAGECACHE(                       \
                  sizeof(objtype_t), &memobject) ;          \
         if (err) goto ONABORT ;                            \
                                                            \
         objtype_t * newobj ;                               \
         newobj = (objtype_t*) memobject.addr ;             \
                                                            \
         ONERROR_testerrortimer(                            \
               &s_threadcontext_errtimer, ONABORT) ;        \
         err = init_##module(newobj) ;                      \
         if (err) goto ONABORT ;                            \
                                                            \
         (object) = newobj ;                                \
                                                            \
         return 0 ;                                         \
      ONABORT:                                              \
         FREESTATIC_PAGECACHE(&memobject) ;                 \
         return err ;

/* define: FREEOBJECT
 * Frees object. Calls FREESTATIC_PAGECACHE to free memory as last operation.
 * Calls free_##module() to free object and object is cleared.
 * Calling it twice has no effect. */
#define FREEOBJECT(module, objtype_t, object, staticobj)    \
         int err ;                                          \
         int err2 ;                                         \
         objtype_t * delobj = (object) ;                    \
                                                            \
         if (delobj != (staticobj)) {                       \
                                                            \
            (object) = (staticobj) ;                        \
                                                            \
            err = free_##module(delobj) ;                   \
                                                            \
            memblock_t memobject =                          \
                        memblock_INIT(                      \
                           sizeof(objtype_t),               \
                           (uint8_t*)delobj) ;              \
            err2 = FREESTATIC_PAGECACHE(&memobject) ;       \
            if (err2) err = err2 ;                          \
                                                            \
            return err ;                                    \
         }                                                  \
         return 0 ;

/* about: freehelperX_threadcontext
 * o Generated freehelper functions to free modules with calls to freethread_module.
 * o Generated freehelper functions to free interfaceable objects with calls free_module.
 * o Generated freehelper functions to free objects with calls free_module.
 * */

// TEXTDB:SELECT("static inline int freehelper"row-id"_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)"\n"{"\n"   FREEOBJECT("module", "objtype", tcontext->"parameter", statictcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="object")
static inline int freehelper3_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEOBJECT(syncrun, syncrun_t, tcontext->syncrun, statictcontext->syncrun) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static inline int freehelper"row-id"_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)"\n"{"\n"   FREEIOBJ("module", "objtype", tcontext->"parameter", statictcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface")
static inline int freehelper2_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEIOBJ(mmimpl, mm_impl_t, tcontext->mm, statictcontext->mm) ;
}

static inline int freehelper4_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEIOBJ(objectcacheimpl, objectcache_impl_t, tcontext->objectcache, statictcontext->objectcache) ;
}

static inline int freehelper5_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   FREEIOBJ(logwriter, logwriter_t, tcontext->log, statictcontext->log) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static inline int freehelper"row-id"_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)"\n"{"\n"   (void) tcontext;"\n"   (void) statictcontext;"\n"   return freethread_"module"("(if (parameter!="") "genericcast_iobj(&tcontext->" else "(")parameter(if (parameter!="") ", " else "")parameter")) ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="initthread")
static inline int freehelper1_threadcontext(threadcontext_t * tcontext, threadcontext_t * statictcontext)
{
   (void) tcontext;
   (void) statictcontext;
   return freethread_pagecacheimpl(genericcast_iobj(&tcontext->pagecache, pagecache)) ;
}

// TEXTDB:END

/* about: inithelperX_threadcontext
 * o Generated inithelper functions to init modules with calls to initthread_module.
 * o Generated inithelper functions to init interfaceable objects with calls init_module.
 * o Generated inithelper functions to init objects with calls init_module.
 * */

// TEXTDB:SELECT("static inline int inithelper"row-id"_threadcontext(threadcontext_t * tcontext)"\n"{"\n"   INITOBJECT("module", "objtype", tcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="object")
static inline int inithelper3_threadcontext(threadcontext_t * tcontext)
{
   INITOBJECT(syncrun, syncrun_t, tcontext->syncrun) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static inline int inithelper"row-id"_threadcontext(threadcontext_t * tcontext)"\n"{"\n"   INITIOBJ("module", "objtype", tcontext->"parameter") ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="interface")
static inline int inithelper2_threadcontext(threadcontext_t * tcontext)
{
   INITIOBJ(mmimpl, mm_impl_t, tcontext->mm) ;
}

static inline int inithelper4_threadcontext(threadcontext_t * tcontext)
{
   INITIOBJ(objectcacheimpl, objectcache_impl_t, tcontext->objectcache) ;
}

static inline int inithelper5_threadcontext(threadcontext_t * tcontext)
{
   INITIOBJ(logwriter, logwriter_t, tcontext->log) ;
}

// TEXTDB:END

// TEXTDB:SELECT("static inline int inithelper"row-id"_threadcontext(threadcontext_t * tcontext)"\n"{"\n"   (void) tcontext;"\n"   return initthread_"module"("(if (parameter!="") "genericcast_iobj(&tcontext->" else "(")parameter(if (parameter!="") ", " else "")parameter")) ;"\n"}"\n)FROM(C-kern/resource/config/initthread)WHERE(inittype=="initthread")
static inline int inithelper1_threadcontext(threadcontext_t * tcontext)
{
   (void) tcontext;
   return initthread_pagecacheimpl(genericcast_iobj(&tcontext->pagecache, pagecache)) ;
}

// TEXTDB:END


// group: lifetime

int free_threadcontext(threadcontext_t * tcontext)
{
   int err = 0 ;
   int err2 ;
   threadcontext_t statictcontext = threadcontext_INIT_STATIC ;

   size_t    initcount = tcontext->initcount ;
   tcontext->initcount = 0 ;

   switch (initcount) {
   default:    assert(false && "initcount out of bounds") ;
               break ;
// TEXTDB:SELECT("   case "row-id":     err2 = freehelper"row-id"_threadcontext(tcontext, &statictcontext) ;"\n"               if (err2) err = err2 ;")FROM(C-kern/resource/config/initthread)DESCENDING
   case 5:     err2 = freehelper5_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 4:     err2 = freehelper4_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 3:     err2 = freehelper3_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 2:     err2 = freehelper2_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
   case 1:     err2 = freehelper1_threadcontext(tcontext, &statictcontext) ;
               if (err2) err = err2 ;
// TEXTDB:END
   case 0:     break ;
   }

   tcontext->pcontext = statictcontext.pcontext ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

int init_threadcontext(/*out*/threadcontext_t * tcontext, processcontext_t * pcontext)
{
   int err ;

   *tcontext = (threadcontext_t) threadcontext_INIT_STATIC ;
   tcontext->pcontext = pcontext ;

   VALIDATE_STATE_TEST(maincontext_STATIC != type_maincontext(), ONABORT, ) ;

// TEXTDB:SELECT(\n"   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;"\n"   err = inithelper"row-id"_threadcontext(tcontext) ;"\n"   if (err) goto ONABORT ;"\n"   ++tcontext->initcount ;")FROM(C-kern/resource/config/initthread)

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper1_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper2_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper3_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper4_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;
   err = inithelper5_threadcontext(tcontext) ;
   if (err) goto ONABORT ;
   ++tcontext->initcount ;
// TEXTDB:END

   ONERROR_testerrortimer(&s_threadcontext_errtimer, ONABORT) ;

   return 0 ;
ONABORT:
   (void) free_threadcontext(tcontext) ;
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: query

bool isstatic_threadcontext(const threadcontext_t * tcontext)
{
   return   &g_maincontext.pcontext == tcontext->pcontext
            && 0 == tcontext->pagecache.object   && 0 == tcontext->pagecache.iimpl
            && 0 == tcontext->mm.object          && 0 == tcontext->mm.iimpl
            && 0 == tcontext->syncrun
            && 0 == tcontext->objectcache.object && 0 == tcontext->objectcache.iimpl
            && (struct log_t*)&g_logmain == tcontext->log.object && &g_logmain_interface == tcontext->log.iimpl
            && 0 == tcontext->initcount ;
}

// group: change

void setmm_threadcontext(threadcontext_t * tcontext, mm_t * new_mm)
{
   initcopy_iobj(&tcontext->mm, new_mm) ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct testmodule_impl_t    testmodule_impl_t ;

struct testmodule_impl_t {
   double   dummy[4] ;
} ;

iobj_DECLARE(testmodule_t, testmodule) ;

static struct testmodule_impl_t *  s_test_testmodule = 0 ;

static int init_testmoduleimpl(struct testmodule_impl_t * object)
{
   s_test_testmodule = object ;
   return 0 ;
}

static int free_testmoduleimpl(struct testmodule_impl_t * object)
{
   s_test_testmodule = object ;
   return 0 ;
}

static struct testmodule_it * interfacethread_testmoduleimpl(void)
{
   return (struct testmodule_it*) 33 ;
}

static int call_initiobj(struct testmodule_t * testiobj)
{
   INITIOBJ(testmoduleimpl, struct testmodule_impl_t, *testiobj)
}

static int call_freeiobj(struct testmodule_t * testiobj)
{
   struct testmodule_t staticiobj = { (void*)1, (void*)2 } ;
   FREEIOBJ(testmoduleimpl, struct testmodule_impl_t, *testiobj, staticiobj)
}

static int test_iobjhelper(void)
{
   struct testmodule_t  testiobj ;
   memblock_t           memblock = memblock_INIT_FREEABLE ;

   // TEST INITIOBJ
   size_t stsize = SIZESTATIC_PAGECACHE() ;
   s_test_testmodule = 0 ;
   free_iobj(&testiobj) ;
   TEST(0 == call_initiobj(&testiobj)) ;
   TEST(testiobj.object != 0) ;
   TEST(testiobj.iimpl  == (struct testmodule_it*)33) ;
   TEST(testiobj.object == (struct testmodule_t*)s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize + sizeof(struct testmodule_impl_t)) ;

   // TEST FREEIOBJ
   void * const oldobject = s_test_testmodule ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeiobj(&testiobj)) ;
   TEST(testiobj.object == (void*)1) ;
   TEST(testiobj.iimpl  == (void*)2) ;
   TEST(oldobject       == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   TEST(!ALLOCSTATIC_PAGECACHE(sizeof(struct testmodule_impl_t), &memblock)) ;
   TEST(oldobject       == addr_memblock(&memblock)) ;
   TEST(!FREESTATIC_PAGECACHE(&memblock)) ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeiobj(&testiobj)) ;
   TEST(testiobj.object == (void*)1) ;
   TEST(testiobj.iimpl  == (void*)2) ;
   TEST(0               == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;

   // TEST INITIOBJ: ENOMEM
   testiobj.object = 0 ;
   testiobj.iimpl  = 0 ;
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, ENOMEM) ;
      TEST(ENOMEM == call_initiobj(&testiobj)) ;
      TEST(testiobj.object == 0) ;
      TEST(testiobj.iimpl  == 0) ;
      TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   }

   return 0 ;
ONABORT:
   FREESTATIC_PAGECACHE(&memblock) ;
   return EINVAL ;
}

static int call_initobject(struct testmodule_impl_t ** testobj)
{
   INITOBJECT(testmoduleimpl, struct testmodule_impl_t, *testobj)
}

static int call_freeobject(struct testmodule_impl_t ** testobj)
{
   struct testmodule_impl_t * staticobj = (void*)3 ;
   FREEOBJECT(testmoduleimpl, struct testmodule_impl_t, *testobj, staticobj)
}

static int test_objhelper(void)
{
   testmodule_impl_t *  testobj  = 0 ;
   memblock_t           memblock = memblock_INIT_FREEABLE ;

   // TEST INITOBJECT
   size_t stsize = SIZESTATIC_PAGECACHE() ;
   s_test_testmodule = 0 ;
   TEST(0 == call_initobject(&testobj)) ;
   TEST(testobj != 0) ;
   TEST(testobj == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize + sizeof(struct testmodule_impl_t)) ;

   // TEST FREEOBJECT
   void * const oldobj = s_test_testmodule ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeobject(&testobj)) ;
   TEST(testobj == (void*)3) ;
   TEST(oldobj  == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   TEST(!ALLOCSTATIC_PAGECACHE(sizeof(struct testmodule_impl_t), &memblock)) ;
   TEST(oldobj  == addr_memblock(&memblock)) ;
   TEST(!FREESTATIC_PAGECACHE(&memblock)) ;
   s_test_testmodule = 0 ;
   TEST(0 == call_freeobject(&testobj)) ;
   TEST(testobj == (void*)3) ;
   TEST(0       == s_test_testmodule) ;
   TEST(SIZESTATIC_PAGECACHE() == stsize) ;

   // TEST INITOBJECT: ENOMEM
   testobj = 0 ;
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, ENOMEM) ;
      TEST(ENOMEM == call_initobject(&testobj)) ;
      TEST(testobj == 0) ;
      TEST(SIZESTATIC_PAGECACHE() == stsize) ;
   }

   return 0 ;
ONABORT:
   FREESTATIC_PAGECACHE(&memblock) ;
   return EINVAL ;
}

static int test_initfree(void)
{
   threadcontext_t      tcontext = threadcontext_INIT_FREEABLE ;
   processcontext_t *   p        = pcontext_maincontext() ;
   const size_t         nrsvc    = 5 ;
   size_t               sizestatic ;

   // prepare
   TEST(p != 0) ;

   // TEST threadcontext_INIT_FREEABLE
   TEST(0 == tcontext.pcontext) ;
   TEST(0 == tcontext.pagecache.object) ;
   TEST(0 == tcontext.pagecache.iimpl) ;
   TEST(0 == tcontext.mm.object) ;
   TEST(0 == tcontext.mm.iimpl) ;
   TEST(0 == tcontext.syncrun) ;
   TEST(0 == tcontext.objectcache.object) ;
   TEST(0 == tcontext.objectcache.iimpl) ;
   TEST(0 == (logmain_t*)tcontext.log.object) ;
   TEST(0 == tcontext.log.iimpl) ;
   TEST(0 == tcontext.initcount) ;

   // TEST threadcontext_INIT_STATIC
   tcontext = (threadcontext_t) threadcontext_INIT_STATIC ;
   TEST(1 == isstatic_threadcontext(&tcontext)) ;

   // TEST init_threadcontext
   sizestatic = SIZESTATIC_PAGECACHE() ;
   TEST(0 == init_threadcontext(&tcontext, p)) ;
   TEST(p == tcontext.pcontext) ;
   TEST(0 != tcontext.pagecache.object) ;
   TEST(0 != tcontext.pagecache.iimpl) ;
   TEST(0 != tcontext.mm.object) ;
   TEST(0 != tcontext.mm.iimpl) ;
   TEST(0 != tcontext.syncrun) ;
   TEST(0 != tcontext.objectcache.object) ;
   TEST(0 != tcontext.objectcache.iimpl) ;
   TEST(0 != tcontext.log.object) ;
   TEST(0 != tcontext.log.iimpl) ;
   TEST(&g_logmain           != (logmain_t*)tcontext.log.object) ;
   TEST(&g_logmain_interface != tcontext.log.iimpl) ;
   TEST(nrsvc == tcontext.initcount) ;
   TEST(SIZESTATIC_PAGECACHE() > sizestatic) ;

   // TEST free_threadcontext
   TEST(0 == free_threadcontext(&tcontext)) ;
   TEST(1 == isstatic_threadcontext(&tcontext)) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   TEST(0 == free_threadcontext(&tcontext)) ;
   TEST(1 == isstatic_threadcontext(&tcontext)) ;
   TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;

   // TEST init_threadcontext: ERROR
   for(unsigned i = 1; i; ++i) {
      init_testerrortimer(&s_threadcontext_errtimer, i, (int)i) ;
      memset(&tcontext, 0xff, sizeof(tcontext)) ;
      int err = init_threadcontext(&tcontext, p) ;
      if (err == 0) {
         TEST(0 == free_threadcontext(&tcontext)) ;
         TEST(i > nrsvc) ;
         break ;
      }
      TEST(i == (unsigned)err) ;
      TEST(1 == isstatic_threadcontext(&tcontext)) ;
      TEST(SIZESTATIC_PAGECACHE() == sizestatic) ;
   }

   // unprepare
   s_threadcontext_errtimer = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;

   return 0 ;
ONABORT:
   s_threadcontext_errtimer = (test_errortimer_t) test_errortimer_INIT_FREEABLE ;
   return EINVAL ;
}

static int test_query(void)
{
   threadcontext_t tcontext = threadcontext_INIT_STATIC ;

   // TEST isstatic_threadcontext
   TEST(1 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pcontext = 0 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pcontext = &g_maincontext.pcontext ;
   tcontext.pagecache.object = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pagecache.object = 0 ;
   tcontext.pagecache.iimpl = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.pagecache.iimpl = 0 ;
   tcontext.mm.object = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.mm.object = 0 ;
   tcontext.mm.iimpl = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.mm.iimpl = 0 ;
   tcontext.syncrun = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.syncrun = 0 ;
   tcontext.objectcache.object = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.objectcache.object = 0 ;
   tcontext.objectcache.iimpl = (void*)1 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.objectcache.iimpl = 0 ;
   tcontext.log.object = 0 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.log.object = (struct log_t*)&g_logmain;
   tcontext.log.iimpl = 0 ;
   TEST(0 == isstatic_threadcontext(&tcontext)) ;
   tcontext.log.iimpl = &g_logmain_interface ;
   TEST(1 == isstatic_threadcontext(&tcontext)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_change(void)
{
   threadcontext_t tcontext = threadcontext_INIT_FREEABLE ;

   // TEST setmm_threadcontext
   setmm_threadcontext(&tcontext, &(mm_t) iobj_INIT((void*)1, (void*)2)) ;
   TEST(tcontext.mm.object == (mm_t*)1) ;
   TEST(tcontext.mm.iimpl  == (mm_it*)2) ;
   setmm_threadcontext(&tcontext, &(mm_t) iobj_INIT(0, 0)) ;
   TEST(tcontext.mm.object == 0) ;
   TEST(tcontext.mm.iimpl  == 0) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_context_threadcontext()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_iobjhelper())  goto ONABORT ;
   if (test_objhelper())   goto ONABORT ;
   if (test_initfree())    goto ONABORT ;
   if (test_query())       goto ONABORT ;
   if (test_change())      goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
