/* title: Exothread impl
   Implements <Exothread>.

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

   file: C-kern/api/os/task/exothread.h
    Header file <Exothread>.

   file: C-kern/os/shared/task/exothread.c
    Implementation file <Exothread impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/os/task/exothread.h"
#include "C-kern/api/errlog.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test.h"
#endif


/* define: setfreestate_exothread
 * Sets the instruction pointer to label
 * > exothread_FREE:
 * The macro JUMPSTATE_exothread knows how to interpret this.
 *
 * Precondition:
 * Call <setfreestate_exothread> only if <isholdingresource_exothread> returns true. */
#define setfreestate_exothread(_xthread) \
   (_xthread)->instr_ptr = 0

int init_exothread(/*out*/exothread_t * xthread, exothread_main_f main_fct)
{
   MEMSET0(xthread) ;
   xthread->main         = main_fct ;
   // This defines the initstate in case isholdingresource_exothread() returns false
   // xthread->instr_ptr = 0
   return 0 ;
}

int free_exothread(exothread_t * xthread)
{
   int err ;

   if (xthread->main) {
      xthread->main     = 0 ;
      if (exothread_flag_RUN == (xthread->flags & (exothread_flag_FINISH|exothread_flag_RUN))) {
         err = EBUSY ;
         goto ABBRUCH ;
      }
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT_FREE(err) ;
   return err ;
}

static void seterror_exothread(exothread_t * xthread, int err)
{
   if (!xthread->returncode) {
      xthread->returncode = err ;
   }

   if (!isfinish_exothread(xthread)) {
      if (isholdingresource_exothread(xthread)) {
         // free resources of exothread in next call
         setfreestate_exothread(xthread) ;
      } else {
         finish_exothread() ;
      }
   }

}

int run_exothread(exothread_t * xthread)
{
   int err ;

   CHECK_INARG(0 == isfinish_exothread(xthread),ABBRUCH,)

   xthread->flags = (typeof(xthread->flags)) (xthread->flags | exothread_flag_RUN) ;

   err = xthread->main(xthread) ;
   if (err) {
      seterror_exothread(xthread, err) ;
   }

   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

void abort_exothread(exothread_t * xthread)
{
   if (     !isfinish_exothread(xthread)
         && !xthread->returncode) {
      seterror_exothread(xthread, ECANCELED) ;
   }
}


#ifdef KONFIG_UNITTEST

#define TEST(CONDITION) TEST_ONERROR_GOTO(CONDITION,unittest_os_task_exothread,ABBRUCH)

static int testinit_xthread(exothread_t * xthread)
{
   int err = 0 ;

   jumpstate_exothread() ;

   exothread_INIT: ;
   CHECK_INARG(0 == err,ABBRUCH,) ;
   err = 1 ;

   exothread_FREE: ;
   CHECK_INARG(1 == err,ABBRUCH,) ;

   finish_exothread() ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int s_isfree_called = 0 ;

static int testerror_xthread(exothread_t * xthread)
{

   jumpstate_exothread() ;

   exothread_INIT: ;
   setholdingresource_exothread() ;
   return EINVAL ;

   exothread_FREE: ;
   s_isfree_called = 1 ;
   return ETIMEDOUT ;
}

static int s_loop_count    = 0 ;

static int testloop_xthread(exothread_t * xthread)
{

   jumpstate_exothread() ;

   exothread_INIT: ;
   -- s_loop_count ;
   if (s_loop_count <= 0) {
      finish_exothread() ;
   }
   return 0 ;

   exothread_FREE: ;
   s_isfree_called = 1 ;
   finish_exothread() ;
   return 0 ;
}

typedef struct counter_xthread_t             counter_xthread_t ;

struct counter_xthread_t {
   exothread_t xthread ;
   struct {
      int   limit ;
      int   errval ;
      int   errfreeresource ;
   } inarg ;
   struct {
      int   counter_value ;
   } outarg ;
   /* state */
   int         value ;
   int         limit ;
   void      * dummy ;
} ;

static int s_outparam_set  = 0 ;

static int counter_xthread(counter_xthread_t * xthread)
{
   int err ;

   jumpstate_exothread() ;

   exothread_INIT: ;
   {
      declare_inparam_exothread(inparam) ;

      CHECK_INARG(inparam->limit > 0,ABBRUCH,LOG_INT(inparam->limit)) ;

      xthread->value = 0 ;
      xthread->limit = inparam->limit ;
      xthread->dummy = malloc(12) ;
      setholdingresource_exothread() ;
      if (!xthread->dummy) {
         err = ENOMEM ;
         LOG_OUTOFMEMORY(12) ;
         goto ABBRUCH ;
      }
   }

   rememberstate_exothread() ;

   ++ xthread->value ;

   if (xthread->value == inarg_exothread()->errval) {
      err = ETIME ;
      goto ABBRUCH ;
   }

   if (xthread->value < xthread->limit) {
      return 0 ;
   }

   exothread_FREE: ;
   ++ s_isfree_called ;
   err = 0 ;
   free(xthread->dummy) ;
   xthread->dummy = 0 ;
   if (inarg_exothread()->errfreeresource) {
      err = inarg_exothread()->errfreeresource ;
   }

   if (err) goto ABBRUCH ;

   if (!iserror_exothread(&xthread->xthread)) {
      ++ s_outparam_set ;
      // write out parameter
      declare_outparam_exothread(result) ;
      result->counter_value = xthread->value ;
   }

   finish_exothread() ;
   return 0 ;
ABBRUCH:
   LOG_ABORT(err) ;
   return err ;
}

static int testloopyield_xthread(exothread_t * xthread)
{
   jumpstate_exothread() ;

   exothread_INIT: ;
   s_loop_count = 0 ;

   while( s_loop_count < 10 ) {
      ++ s_loop_count ;
      yield_exothread() ;
   }

   exothread_FREE: ;
   finish_exothread() ;
   return 0 ;
}

static int testloopwhile_xthread(exothread_t * xthread)
{
   jumpstate_exothread() ;

   exothread_INIT: ;
   s_loop_count = 0 ;

   while_exothread( s_loop_count < 10 ) {
      ++ s_loop_count ;
   }

   exothread_FREE: ;
   finish_exothread() ;
   return 0 ;
}

static int testloopfor_xthread(exothread_t * xthread)
{
   jumpstate_exothread() ;

   exothread_INIT: ;
   s_loop_count = 100 ;

   for_exothread( s_loop_count = 0, s_loop_count < 10, ++ s_loop_count ) {
      ;
   }

   exothread_FREE: ;
   finish_exothread() ;
   return 0 ;
}

static int test_initfree(void)
{
   exothread_t xthread = exothread_INIT_FREEABLE ;

   // TEST init, double free
   xthread.next       = (void*) 1 ;
   xthread.flags      = 1 ;
   xthread.instr_ptr  = (void*) 1 ;
   xthread.returncode = 1 ;
   TEST(0 == init_exothread(&xthread, (exothread_main_f)&test_initfree)) ;
   TEST(xthread.next      == 0) ;
   TEST(xthread.main      == (exothread_main_f)&test_initfree) ;
   TEST(xthread.instr_ptr == 0) ;
   TEST(xthread.returncode== 0) ;
   TEST(xthread.flags     == 0) ;
   TEST(0 == free_exothread(&xthread)) ;
   TEST(0 == xthread.main) ;
   TEST(0 == free_exothread(&xthread)) ;
   TEST(0 == xthread.main) ;

   // TEST query flags
   TEST(0 == init_exothread(&xthread, &testinit_xthread)) ;
   TEST(xthread.next      == 0) ;
   TEST(xthread.main      == &testinit_xthread) ;
   TEST(xthread.instr_ptr == 0) ;
   TEST(xthread.returncode== 0) ;
   TEST(xthread.flags     == 0) ;
   TEST(0 == iserror_exothread(&xthread)) ;
   TEST(0 == isfinish_exothread(&xthread)) ;
   TEST(0 == isholdingresource_exothread(&xthread)) ;
   // iserror_exothread
   xthread.returncode = -200 ;
   TEST(0 != iserror_exothread(&xthread)) ;
   xthread.returncode = +100 ;
   TEST(0 != iserror_exothread(&xthread)) ;
   xthread.returncode = 0 ;
   TEST(0 == iserror_exothread(&xthread)) ;
   // isfinish_exothread
   xthread.flags = exothread_flag_FINISH ;
   TEST(0 != isfinish_exothread(&xthread)) ;
   xthread.flags = (uint16_t) (255 | exothread_flag_FINISH) ;
   TEST(0 != isfinish_exothread(&xthread)) ;
   xthread.flags = (uint16_t) (255 & ~(exothread_flag_FINISH)) ;
   TEST(0 == isfinish_exothread(&xthread)) ;
   xthread.flags = 0 ;
   TEST(0 == isfinish_exothread(&xthread)) ;
   // isholdingresource_exothread
   xthread.flags = exothread_flag_HOLDINGRESOURCE ;
   TEST(0 != isholdingresource_exothread(&xthread)) ;
   xthread.flags = (uint16_t) (255 | exothread_flag_HOLDINGRESOURCE) ;
   TEST(0 != isholdingresource_exothread(&xthread)) ;
   xthread.flags = (uint16_t) (255 & ~(exothread_flag_HOLDINGRESOURCE)) ;
   TEST(0 == isholdingresource_exothread(&xthread)) ;
   xthread.flags = 0 ;
   TEST(0 == isholdingresource_exothread(&xthread)) ;
   TEST(0 == free_exothread(&xthread)) ;
   TEST(0 == xthread.main) ;

   // TEST run / xthread finishes in one step
   TEST(0 == init_exothread(&xthread, &testinit_xthread)) ;
   TEST(0 == xthread.flags) ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(xthread.next      == 0) ;
   TEST(xthread.main      == &testinit_xthread) ;
   TEST(xthread.instr_ptr == 0) ;
   TEST(xthread.returncode== 0) ;
   TEST(xthread.flags     == (exothread_flag_FINISH|exothread_flag_RUN)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST run / first errorcode => exothread_flag_HOLDINGRESOURCE cleared + exothread_flag_FINISH is set after two steps
   s_isfree_called = 0 ;
   TEST(0 == init_exothread(&xthread, &testerror_xthread)) ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(0 == s_isfree_called) ;
   TEST(xthread.returncode == EINVAL) ;
   TEST(xthread.flags      == (exothread_flag_HOLDINGRESOURCE|exothread_flag_RUN)) ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(1 == s_isfree_called) ;
   TEST(xthread.returncode == EINVAL) ;
   TEST(xthread.flags      == (exothread_flag_FINISH|exothread_flag_RUN)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST run / exothread_flag_HOLDINGRESOURCE is cleared => jumps directly to exothread_FREE
   s_isfree_called = 0 ;
   TEST(0 == init_exothread(&xthread, &testerror_xthread)) ;
   xthread.flags = exothread_flag_HOLDINGRESOURCE ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(1 == s_isfree_called) ;
   TEST(xthread.returncode == ETIMEDOUT ) ;
   TEST(xthread.flags      == (exothread_flag_FINISH|exothread_flag_RUN)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST run / xthread finishes after 100 loops
   s_isfree_called = 0 ;
   s_loop_count    = 100 ;
   TEST(0 == init_exothread(&xthread, &testloop_xthread)) ;
   for(int i = 0; i < 99; ++i) {
      TEST(100-i == s_loop_count) ;
      TEST(0 == run_exothread(&xthread)) ;
      TEST(xthread.flags == (exothread_flag_RUN)) ;
   }
   TEST(0 == run_exothread(&xthread)) ;
   TEST(0 == s_loop_count) ;
   TEST(0 == s_isfree_called) ;
   TEST(0 == xthread.returncode) ;
   TEST(xthread.flags == (exothread_flag_FINISH|exothread_flag_RUN)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST abort of exothread after initialization
   TEST(0 == init_exothread(&xthread, &testinit_xthread)) ;
   abort_exothread(&xthread) ;
   TEST(xthread.returncode == ECANCELED) ;
   TEST(xthread.flags      == (exothread_flag_FINISH)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST abort of exothread after init + isholdingresource_exothread()
   // => no finish, next run would call exothread_FREE:
   TEST(0 == init_exothread(&xthread, &testinit_xthread)) ;
   xthread.flags = exothread_flag_HOLDINGRESOURCE ;
   abort_exothread(&xthread) ;
   TEST(xthread.returncode == ECANCELED) ;
   TEST(xthread.flags      == exothread_flag_HOLDINGRESOURCE) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST abort of running xthread
   s_isfree_called = 0 ;
   s_loop_count    = 100 ;
   TEST(0 == init_exothread(&xthread, &testloop_xthread)) ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(99 == s_loop_count) ;
   TEST(xthread.flags == (exothread_flag_RUN)) ;
   abort_exothread(&xthread) ;
   TEST(xthread.returncode == ECANCELED) ;
   TEST(xthread.flags      == (exothread_flag_FINISH|exothread_flag_RUN)) ;
   TEST(0 == s_isfree_called) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST abort is freeing resources
   s_isfree_called = 0 ;
   s_loop_count    = 100 ;
   TEST(0 == init_exothread(&xthread, &testloop_xthread)) ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(99 == s_loop_count) ;
   TEST(xthread.flags == (exothread_flag_RUN)) ;
   xthread.flags = (uint16_t) (exothread_flag_HOLDINGRESOURCE|exothread_flag_RUN) ;
   abort_exothread(&xthread) ;
   TEST(xthread.returncode == ECANCELED) ;
   TEST(xthread.flags      == (exothread_flag_HOLDINGRESOURCE|exothread_flag_RUN)) ;
   TEST(0 == s_isfree_called) ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(1 == s_isfree_called) ;
   TEST(xthread.returncode == ECANCELED) ;
   TEST(xthread.flags      == (exothread_flag_FINISH|exothread_flag_RUN)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST EINVAL
   TEST(0 == init_exothread(&xthread, &testinit_xthread)) ;
   TEST(0 == run_exothread(&xthread)) ;
   TEST(xthread.returncode == 0) ;
   TEST(xthread.flags      == (exothread_flag_FINISH|exothread_flag_RUN)) ;
   TEST(EINVAL == run_exothread(&xthread)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST EBUSY
   TEST(0 == init_exothread(&xthread, &testinit_xthread)) ;
   xthread.flags = exothread_flag_RUN ;
   TEST(EBUSY == free_exothread(&xthread)) ;

   return 0 ;
ABBRUCH:
   (void) free_exothread(&xthread) ;
   return EINVAL ;
}

static int test_subtype_counter(void)
{
   counter_xthread_t       xthread = { .xthread = exothread_INIT_FREEABLE } ;

   // TEST init, free
   xthread.xthread.flags      = 1 ;
   xthread.xthread.instr_ptr  = (void*) 1 ;
   xthread.xthread.returncode = 1 ;
   TEST(0 == initsubtype_exothread(&xthread, &counter_xthread)) ;
   TEST(xthread.xthread.main      == (exothread_main_f)&counter_xthread) ;
   TEST(xthread.xthread.instr_ptr == 0) ;
   TEST(xthread.xthread.returncode== 0) ;
   TEST(0 == free_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.main) ;
   TEST(0 == free_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.main) ;

   // TEST EINVAL CHECK_INARG
   MEMSET0(&xthread.inarg) ;
   MEMSET0(&xthread.outarg) ;
   xthread.inarg.limit  = -1 ;
   s_isfree_called = 0 ;
   s_outparam_set  = 0 ;
   TEST(0 == initsubtype_exothread(&xthread, &counter_xthread)) ;
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(0 != iserror_exothread(&xthread.xthread)) ;
   TEST(0 != isfinish_exothread(&xthread.xthread)) ;
   TEST(0 == isholdingresource_exothread(&xthread.xthread)) ;
   TEST(xthread.xthread.returncode == EINVAL) ;
   TEST(0 == s_isfree_called) ;
   TEST(0 == s_outparam_set) ;
   TEST(0 == free_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.main) ;

   // TEST counting
   MEMSET0(&xthread.inarg) ;
   MEMSET0(&xthread.outarg) ;
   xthread.inarg.limit  = 33 ;
   s_isfree_called = 0 ;
   s_outparam_set  = 0 ;
   TEST(0 == initsubtype_exothread(&xthread, &counter_xthread)) ;
   for(int i = 1; i < xthread.inarg.limit; ++i) {
      TEST(0 == run_exothread(&xthread.xthread)) ;
      TEST(i == xthread.value) ;
   }
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(0 == iserror_exothread(&xthread.xthread)) ;
   TEST(0 != isfinish_exothread(&xthread.xthread)) ;
   TEST(0 != isholdingresource_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.returncode) ;
   TEST(1 == s_isfree_called) ;
   TEST(1 == s_outparam_set) ;
   TEST(0 == free_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.main) ;

   // TEST counting produces error => free resource => out param not changed
   MEMSET0(&xthread.inarg) ;
   MEMSET0(&xthread.outarg) ;
   xthread.inarg.limit  = 40 ;
   xthread.inarg.errval = 12 ;
   s_isfree_called = 0 ;
   s_outparam_set  = 0 ;
   TEST(0 == initsubtype_exothread(&xthread, &counter_xthread)) ;
   for(int i = 1; i < xthread.inarg.errval; ++i) {
      TEST(0 == run_exothread(&xthread.xthread)) ;
      TEST(i == xthread.value) ;
   }
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(0 == s_isfree_called) ;
   TEST(0 != iserror_exothread(&xthread.xthread)) ;
   TEST(0 == isfinish_exothread(&xthread.xthread)) ;
   TEST(0 != isholdingresource_exothread(&xthread.xthread)) ;
   TEST(ETIME == xthread.xthread.returncode) ;
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(1 == s_isfree_called) ;
   TEST(0 == s_outparam_set) ;
   TEST(0 != iserror_exothread(&xthread.xthread)) ;
   TEST(0 != isfinish_exothread(&xthread.xthread)) ;
   TEST(0 == isholdingresource_exothread(&xthread.xthread)) ;
   TEST(ETIME == xthread.xthread.returncode) ;
   TEST(0 == free_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.main) ;

   // TEST free resource produces error => call it a second time (cause flag is set) => out param not changed
   MEMSET0(&xthread.inarg) ;
   MEMSET0(&xthread.outarg) ;
   xthread.inarg.limit  = 11 ;
   xthread.inarg.errfreeresource = 2345 ;
   s_isfree_called = 0 ;
   s_outparam_set  = 0 ;
   TEST(0 == initsubtype_exothread(&xthread, &counter_xthread)) ;
   for(int i = 1; i <= xthread.inarg.limit; ++i) {
      TEST(0 == run_exothread(&xthread.xthread)) ;
      TEST(i == xthread.value) ;
   }
   TEST(1 == s_isfree_called) ;
   TEST(0 != iserror_exothread(&xthread.xthread)) ;
   TEST(0 == isfinish_exothread(&xthread.xthread)) ;
   TEST(0 != isholdingresource_exothread(&xthread.xthread)) ;
   TEST(2345 == xthread.xthread.returncode) ;
   xthread.inarg.errfreeresource = EINVAL ;
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(2 == s_isfree_called) ;
   TEST(0 == s_outparam_set) ;
   TEST(0 != iserror_exothread(&xthread.xthread)) ;
   TEST(0 != isfinish_exothread(&xthread.xthread)) ;
   TEST(0 == isholdingresource_exothread(&xthread.xthread)) ;
   TEST(2345 == xthread.xthread.returncode) ;
   TEST(0 == free_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.main) ;

   // TEST abort => free resource called => out param not changed !
   MEMSET0(&xthread.inarg) ;
   MEMSET0(&xthread.outarg) ;
   xthread.inarg.limit  = 3 ;
   s_isfree_called = 0 ;
   s_outparam_set  = 0 ;
   TEST(0 == initsubtype_exothread(&xthread, &counter_xthread)) ;
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(0 == iserror_exothread(&xthread.xthread)) ;
   abort_exothread(&xthread.xthread) ;
   TEST(2 == xthread.value) ;
   TEST(0 == s_isfree_called) ;
   TEST(0 != iserror_exothread(&xthread.xthread)) ;
   TEST(0 == isfinish_exothread(&xthread.xthread)) ;
   TEST(0 != isholdingresource_exothread(&xthread.xthread)) ;
   TEST(0 == s_isfree_called) ;
   abort_exothread(&xthread.xthread) ; // calling a second time should do nothing
   TEST(0 == run_exothread(&xthread.xthread)) ;
   TEST(2 == xthread.value) ;
   TEST(1 == s_isfree_called) ;
   TEST(0 == s_outparam_set) ;
   TEST(0 != iserror_exothread(&xthread.xthread)) ;
   TEST(0 != isfinish_exothread(&xthread.xthread)) ;
   TEST(0 == isholdingresource_exothread(&xthread.xthread)) ;
   TEST(ECANCELED == xthread.xthread.returncode) ;
   TEST(0 == free_exothread(&xthread.xthread)) ;
   TEST(0 == xthread.xthread.main) ;

   return 0 ;
ABBRUCH:
   (void) free_exothread(&xthread.xthread) ;
   return EINVAL ;
}

static int test_loops(void)
{
   exothread_t xthread = exothread_INIT_FREEABLE ;

   // TEST yield_exothread loop
   TEST(0 == init_exothread(&xthread, &testloopyield_xthread)) ;
   s_loop_count = -1 ;
   for(int i = 1; i <= 10; ++i) {
      TEST(0 == run_exothread(&xthread)) ;
      TEST(i == s_loop_count) ;
      TEST(0 == isfinish_exothread(&xthread)) ;
   }
   TEST(0 == run_exothread(&xthread)) ;
   TEST(10 == s_loop_count) ;
   TEST(0 != isfinish_exothread(&xthread)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST while_exothread loop
   TEST(0 == init_exothread(&xthread, &testloopwhile_xthread)) ;
   s_loop_count = -1 ;
   for(int i = 1; i <= 10; ++i) {
      TEST(0 == run_exothread(&xthread)) ;
      TEST(i == s_loop_count) ;
      TEST(0 == isfinish_exothread(&xthread)) ;
   }
   TEST(0 == run_exothread(&xthread)) ;
   TEST(10 == s_loop_count) ;
   TEST(0 != isfinish_exothread(&xthread)) ;
   TEST(0 == free_exothread(&xthread)) ;

   // TEST for_exothread loop
   TEST(0 == init_exothread(&xthread, &testloopfor_xthread)) ;
   s_loop_count = -1 ;
   for(int i = 1; i <= 10; ++i) {
      TEST(0 == run_exothread(&xthread)) ;
      TEST(i == s_loop_count) ;
      TEST(0 == isfinish_exothread(&xthread)) ;
   }
   TEST(0 == run_exothread(&xthread)) ;
   TEST(10 == s_loop_count) ;
   TEST(0 != isfinish_exothread(&xthread)) ;
   TEST(0 == free_exothread(&xthread)) ;

   return 0 ;
ABBRUCH:
   free_exothread(&xthread) ;
   return EINVAL ;
}

int unittest_os_task_exothread()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_initfree())          goto ABBRUCH ;
   if (test_subtype_counter())   goto ABBRUCH ;
   if (test_loops())             goto ABBRUCH ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ABBRUCH:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
