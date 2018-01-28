/* title: TimeValue impl

   Implements <TimeValue>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2018 Jörg Seebohn

   file: C-kern/api/time/timevalue.h
    Header file <TimeValue>.

   file: C-kern/time/timevalue.c
    Implementation file <TimeValue impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   timevalue_t tv;

   // TEST timevalue_INIT
   tv = (timevalue_t) timevalue_INIT(10, 1000);
   TEST(tv.seconds == 10);
   TEST(tv.nanosec == 1000);

   tv = (timevalue_t) timevalue_INIT(0, 1);
   TEST(tv.seconds == 0);
   TEST(tv.nanosec == 1);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   timevalue_t tv;
   timevalue_t tv2;

   // TEST isvalid_timevalue
   tv = (timevalue_t) timevalue_INIT(0, 0);
   TEST(true == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(0, 999999999u);
   TEST(true == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(INT64_MAX, 999999999u);
   TEST(true == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(0, -1);
   TEST(false == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(-1, 0);
   TEST(false == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(INT64_MIN, 999999999u);
   TEST(false == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(0, 1 + 999999999u);
   TEST(false == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(INT64_MAX, 1 + 999999999u);
   TEST(false == isvalid_timevalue(&tv));
   tv = (timevalue_t) timevalue_INIT(0, -1);
   TEST(false == isvalid_timevalue(&tv));

   // TEST diffms_timevalue: seconds
   // TEST diffus_timevalue: seconds
   for (unsigned i=0; i<100; ++i) {
      tv  = (timevalue_t) timevalue_INIT((int)i, 0);
      tv2 = (timevalue_t) timevalue_INIT(0, 0);
      // test
      TEST( 1000*i == diffms_timevalue(&tv, &tv2));
      TEST( -(int64_t)(1000*i) == diffms_timevalue(&tv2, &tv));
      // test
      TEST( 1000000*i == diffus_timevalue(&tv, &tv2));
      TEST( -(int64_t)(1000000*i) == diffus_timevalue(&tv2, &tv));
   }

   // TEST diffms_timevalue: nanoseconds
   // TEST diffus_timevalue: nanoseconds
   for (unsigned i=0; i<100; ++i) {
      tv  = (timevalue_t) timevalue_INIT(0, (int32_t)(i*1000000));
      tv2 = (timevalue_t) timevalue_INIT(0, 0);
      // test
      TEST( i == diffms_timevalue(&tv, &tv2));
      TEST( -(int64_t)(i) == diffms_timevalue(&tv2, &tv));
      // test
      TEST( 1000*i == diffus_timevalue(&tv, &tv2));
      TEST( -(int64_t)(1000*i) == diffus_timevalue(&tv2, &tv));
   }

   // TEST diffms_timevalue
   tv  = (timevalue_t) timevalue_INIT(0, 0);
   tv2 = (timevalue_t) timevalue_INIT(3, 999999);
   TEST(3000 == diffms_timevalue(&tv2, &tv));
   tv2 = (timevalue_t) timevalue_INIT(3, 1000000);
   TEST(3001 == diffms_timevalue(&tv2, &tv));
   tv  = (timevalue_t) timevalue_INIT(1, 0);
   TEST(2001 == diffms_timevalue(&tv2, &tv));
   tv  = (timevalue_t) timevalue_INIT(1, 4000000);
   TEST(1997 == diffms_timevalue(&tv2, &tv));
   tv  = (timevalue_t) timevalue_INIT(2, 999999999);
   tv2 = (timevalue_t) timevalue_INIT(1, 1000000);
   TEST(-1998 == diffms_timevalue(&tv2, &tv));

   // TEST diffus_timevalue
   tv  = (timevalue_t) timevalue_INIT(0, 0);
   tv2 = (timevalue_t) timevalue_INIT(3, 999999999);
   TEST(3999999 == diffus_timevalue(&tv2, &tv));
   TEST(-3999999 == diffus_timevalue(&tv, &tv2));
   tv2 = (timevalue_t) timevalue_INIT(3, 1000);
   TEST(3000001 == diffus_timevalue(&tv2, &tv));
   TEST(-3000001 == diffus_timevalue(&tv, &tv2));
   tv  = (timevalue_t) timevalue_INIT(1, 0);
   TEST(2000001 == diffus_timevalue(&tv2, &tv));
   TEST(-2000001 == diffus_timevalue(&tv, &tv2));
   tv  = (timevalue_t) timevalue_INIT(1, 4000000);
   TEST(1996001 == diffus_timevalue(&tv2, &tv));
   TEST(-1996001 == diffus_timevalue(&tv, &tv2));

   return 0;
ONERR:
   return EINVAL;
}

static int test_generic(void)
{
   timevalue_t t0;
   struct {
      int64_t seconds;
      int32_t nanosec;
      uint8_t dummy;
   } t1;
   struct {
      uint8_t dummy;
      int64_t seconds;
      int32_t nanosec;
      uint8_t dummy2;
   } t2;

   // TEST cast_timevalue: same type
   TEST( &t0 == cast_timevalue(&t0));

   // TEST cast_timevalue: compatible type
   TEST( (timevalue_t*)&t1.seconds == cast_timevalue(&t1));

   // TEST cast_timevalue: compatible type with offset
   TEST( (timevalue_t*)&t2.seconds == cast_timevalue(&t2));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_time_timevalue(void)
{
   if (test_initfree())   goto ONERR;
   if (test_query())      goto ONERR;
   if (test_generic())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
