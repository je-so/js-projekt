/* title: Locale-support Linux
   Implements <Locale-support>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/string/clocale.h
    Header file of <Locale-support>.

   file: C-kern/string/clocale.c
    Linux specific implementation file <Locale-support Linux>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/string/clocale.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#endif

// section: locale_t

// group: lifetime

int init_clocale(/*out*/clocale_t* cl)
{
   int err;

   strncpy(cl->old_locale, current_clocale(), sizeof(cl->old_locale)-1);
   cl->old_locale[sizeof(cl->old_locale)-1] = 0;

   err = setuser_clocale();
   if (err) goto ONERR;

   return 0;
ONERR:
   cl->old_locale[0] = 0;
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_clocale(clocale_t* cl)
{
   int err;

   if (cl->old_locale[0]) {
      err = set_clocale(cl->old_locale);
      cl->old_locale[0] = 0;
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

const char* charencoding_clocale()
{
   return nl_langinfo(CODESET);
}

const char* current_clocale()
{
   int err;
   const char* lname = setlocale(LC_ALL, 0);

   if (!lname) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("setlocale", err);
      goto ONERR;
   }

   return lname;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return 0;
}

const char * currentmsg_clocale()
{
   int err;
   const char * lname = setlocale(LC_MESSAGES, 0);

   if (!lname) {
      err = EINVAL;
      TRACESYSCALL_ERRLOG("setlocale", err);
      goto ONERR;
   }

   return lname;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return 0;
}

// group: update

int set_clocale(const char* name)
{
   int err;

   if (!setlocale(LC_ALL, name)) {
      err = EINVAL;
      TRACE_NOARG_ERRLOG(log_flags_NONE, LOCALE_SETLOCALE);
      PRINTCSTR_ERRLOG(getenv("LC_ALL"));
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int setuser_clocale()
{
   int err;

   err = set_clocale(""/*read from env variables set by user*/);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: reset_clocale implementation
 * Calls C99 conforming function »setlocale«.
 * Set all subsystems of the C runtime environment to the standard locale "C"
 * which is active by default after process creation. */
int reset_clocale()
{
   int err;

   if (!setlocale(LC_ALL, "C")) {
      err = EINVAL;
      TRACE_NOARG_ERRLOG(log_flags_NONE, LOCALE_SETLOCALE);
      PRINTCSTR_ERRLOG("LC_ALL=C");
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int resetmsg_clocale()
{
   int err;

   if (!setlocale(LC_MESSAGES, "C")) {
      err = EINVAL;
      TRACE_NOARG_ERRLOG(log_flags_NONE, LOCALE_SETLOCALE);
      PRINTCSTR_ERRLOG("LC_MESSAGES=C");
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef char usrlocale_t[100];

static void get_usr_locale(/*ou*/usrlocale_t* usrlocale)
{
   if (getenv("LC_ALL")) {
      strncpy( *usrlocale, getenv("LC_ALL"), sizeof(usrlocale_t)-1);
   } else if (getenv("LANG")) {
      strncpy( *usrlocale, getenv("LANG"), sizeof(usrlocale_t)-1);
   } else {
      strcpy( *usrlocale, "");
   }
}

static int test_env(void)
{
   clocale_t cl;
   usrlocale_t usrlocale;

   // prepare
   get_usr_locale(&usrlocale);

   if (usrlocale[0]) {

      // TEST init_clocale: read from LANG / LC_ALL
      TEST(0 == init_clocale(&cl));
      TEST(0 == strcmp(usrlocale, current_clocale()));

      // TEST set_clocale: read from LANG / LC_ALL
      TEST(0 == set_clocale(""));
      TEST(0 == strcmp(usrlocale, current_clocale()));

      // TEST setuser_clocale: read from LANG / LC_ALL
      TEST(0 == setuser_clocale());
      TEST(0 == strcmp(usrlocale, current_clocale()));
   }

   // prepare
   TEST(0 == setenv("LC_ALL", "XXX@unknown", 1));

   // TEST init_clocale: error
   strcpy(cl.old_locale, "123");
   TEST(EINVAL == init_clocale(&cl));
   TEST(0 == cl.old_locale[0]); // set to 0 in case of error

   // TEST set_clocale: error
   TEST(EINVAL == set_clocale(""));

   // TEST setuser_clocale: error
   TEST(EINVAL == setuser_clocale());

   // prepare
   TEST(0 == setenv("LC_ALL", "C", 1));

   // TEST init_clocale: read from LC_ALL
   TEST(0 == init_clocale(&cl));
   TEST(0 == strcmp("C", current_clocale()));

   // TEST set_clocale: read from LC_ALL
   TEST(0 == set_clocale(""));
   TEST(0 == strcmp("C", current_clocale()));

   // TEST setuser_clocale: read from LC_ALL
   TEST(0 == setuser_clocale());
   TEST(0 == strcmp("C", current_clocale()));

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   char old[100] = { 0 };
   clocale_t  cl = clocale_FREE;

   // prepare
   strncpy(old, current_clocale(), sizeof(old)-1);

   // TEST clocale_FREE
   TEST(0 == cl.old_locale[0]);

   const char* test_names[] = { "C", "POSIX", "" };

   for (unsigned tc=0; tc<lengthof(test_names); ++tc) {
      char previous[100];
      TEST(0 == set_clocale(test_names[tc]));
      strncpy(previous, current_clocale(), sizeof(previous)-1);

      // TEST init_clocale: stores previous value / changes to user loale
      TEST(0 == init_clocale(&cl))
      TEST(0 != cl.old_locale);
      TEST(0 == strcmp(previous, cl.old_locale));
      TEST(0 != strcmp("C", current_clocale()));

      // TEST free_clocale: restores previous value
      TEST(0 == free_clocale(&cl));
      TEST(0 == cl.old_locale[0]);
      TEST(0 == strcmp(previous, current_clocale()));
   }

   // reset
   TEST(0 == set_clocale(old));
   TEST(0 == strcmp(old, current_clocale()));

   return 0;
ONERR:
   set_clocale(old);
   return EINVAL;
}

static int test_query(void)
{
   char old[100] = { 0 };
   usrlocale_t usrlocale;

   // prepare
   strncpy(old, current_clocale(), sizeof(old)-1);
   get_usr_locale(&usrlocale);

   // TEST current_clocale
   TEST(0 != setlocale(LC_ALL, "C"));
   TEST(0 == strcmp("C", current_clocale()));
   TEST(0 != setlocale(LC_ALL, "POSIX"));
   TEST(0 == strcmp("C", current_clocale()));
   if (usrlocale[0]) {
      TEST(0 != setlocale(LC_ALL, usrlocale));
      TEST(0 == strcmp(usrlocale, current_clocale()));
   }

   // reset
   TEST(0 == set_clocale(old));
   TEST(0 == strcmp(old, current_clocale()));

   return 0;
ONERR:
   set_clocale(old);
   return EINVAL;
}

static int test_update(void)
{
   char old[100] = { 0 };
   usrlocale_t usrlocale;

   // prepare
   strncpy(old, current_clocale(), sizeof(old)-1);
   get_usr_locale(&usrlocale);

   // TEST set_clocale
   set_clocale("POSIX");
   TEST(0 == strcmp("C", current_clocale()));
   set_clocale("C");
   TEST(0 == strcmp("C", current_clocale()));

   // TEST setuser_clocale
   if (usrlocale[0]) {
      TEST(0 == setuser_clocale());
      TEST(0 == strcmp(usrlocale, current_clocale()));
   }

   // TEST reset_clocale
   reset_clocale();
   TEST(0 == strcmp("C", current_clocale()));

   // reset
   TEST(0 == set_clocale(old));
   TEST(0 == strcmp(old, current_clocale()));

   return 0;
ONERR:
   set_clocale(old);
   return EINVAL;
}

int unittest_string_clocale()
{
   int err;

   TEST(0 == execasprocess_unittest(&test_env, &err));
   TEST(0 == err);
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}


#endif
