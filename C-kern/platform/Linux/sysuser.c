/* title: SystemUser Linuximpl

   Implements <SystemUser>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/platform/sysuser.h
    Header file <SystemUser>.

   file: C-kern/platform/Linux/sysuser.c
    Implementation file <SystemUser Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/sysuser.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/platform/task/process.h"
#endif



// section: sysuser_id_t

// group: query

bool isadmin_sysuserid(sysuser_id_t uid)
{
   return uid == 0;
}

bool isequal_sysuserid(sysuser_id_t luid, sysuser_id_t ruid)
{
   return luid == ruid;
}


// section: sysuser_t

// group: lifetime

int init_sysuser(/*out*/sysuser_t* sysusr)
{
   int err;
   sysuser_id_t uid;
   sysuser_id_t euid;

   if (0 != sysuser_maincontext()) {
      // already initialized (used in testing)
      uid  = sysuser_maincontext()->realuser;
      euid = sysuser_maincontext()->privilegeduser;
   } else {
      uid  = getuid();
      euid = geteuid();
   }

   err = setresuid(uid, uid, euid);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("setresuid", err);
      goto ONERR;
   }

   sysusr->current  = uid;
   sysusr->realuser = uid;
   sysusr->privilegeduser = euid;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_sysuser(sysuser_t * sysusr)
{
   int err;

   if (sysuser_id_FREE != sysusr->realuser) {
      err = setresuid(sysusr->realuser, sysusr->privilegeduser, sysusr->privilegeduser);
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("setresuid", err);
         goto ONERR;
      }

      *sysusr = (sysuser_t) sysuser_FREE;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

bool isequal_sysuser(const sysuser_t * lsysusr, const sysuser_t * rsysusr)
{
   return   lsysusr->current == rsysusr->current
            && lsysusr->realuser == rsysusr->realuser
            && lsysusr->privilegeduser == rsysusr->privilegeduser;
}

// group: switch

int switchtoprivilege_sysuser(sysuser_t * sysusr)
{
   int err;

   if (sysuser_id_FREE != sysusr->privilegeduser) {
      err = setresuid(sysusr->privilegeduser, sysusr->privilegeduser, sysusr->realuser);
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("setresuid", err);
         goto ONERR;
      }

      sysusr->current = sysusr->privilegeduser;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int switchtoreal_sysuser(sysuser_t * sysusr)
{
   int err;

   if (sysuser_id_FREE != sysusr->realuser) {
      err = setresuid(sysusr->realuser, sysusr->realuser, sysusr->privilegeduser);
      if (err) {
         err = errno;
         TRACESYSCALL_ERRLOG("setresuid", err);
         goto ONERR;
      }

      sysusr->current = sysusr->realuser;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: set

int setusers_sysuser(sysuser_t * sysusr, sysuser_id_t realuser, sysuser_id_t privilegeduser)
{
   int err;

   VALIDATE_INPARAM_TEST(realuser != sysuser_id_FREE && privilegeduser != sysuser_id_FREE, ONERR, );

   err = setresuid(realuser, realuser, privilegeduser);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("setresuid", err);
      goto ONERR;
   }

   sysusr->current  = realuser;
   sysusr->realuser = realuser;
   sysusr->privilegeduser = privilegeduser;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: sysuser_info_t

int new_sysuserinfo(/*out*/sysuser_info_t** usrinfo, sysuser_id_t uid)
{
   int err;
   struct passwd  info;
   struct passwd* result = 0;
   static_assert(sizeof(size_t) == sizeof(long), "long sysconf(...) converted to size_t" );
   size_t         size   = sizeof(sysuser_info_t) + (size_t) sysconf(_SC_GETPW_R_SIZE_MAX);
   memblock_t     mblock = memblock_FREE;

   err = RESIZE_MM(size, &mblock);
   if (err) goto ONERR;

   sysuser_info_t* newobj  = (sysuser_info_t*)(mblock.addr);
   char*           straddr = (char*)          (mblock.addr + sizeof(sysuser_info_t));
   size_t          strsize = size - sizeof(sysuser_info_t);

   err = getpwuid_r(uid, &info, straddr, strsize, &result);
   if (err) {
      TRACESYSCALL_ERRLOG("getpwuid_r", err);
      goto ONERR;
   }

   if (!result) {
      err = ENOENT;
      goto ONERR;
   }

   newobj->size  = size;
   newobj->name  = result->pw_name;

   *usrinfo = newobj;

   return 0;
ONERR:
   FREE_MM(&mblock);
   if (ENOENT != err) {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}

int delete_sysuserinfo(sysuser_info_t** usrinfo)
{
   int err;
   sysuser_info_t* delobj = *usrinfo;

   if (delobj) {
      *usrinfo = 0;

      memblock_t mblock = memblock_INIT(delobj->size, (uint8_t*)delobj);
      err = FREE_MM(&mblock);

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_userid(void)
{
   sysuser_id_t usrid = sysuser_id_FREE;

   // TEST sysuser_id_FREE
   TEST(usrid == sys_userid_FREE);
   TEST(usrid == (uid_t)-1);

   // TEST isadmin_sysuserid
   TEST(true == isadmin_sysuserid(0));
   TEST(false == isadmin_sysuserid(1));
   TEST(false == isadmin_sysuserid(sysuser_id_FREE));

   // TEST isequal_sysuserid
   TEST( isequal_sysuserid((sysuser_id_t)0,    (sysuser_id_t)0));
   TEST( isequal_sysuserid((sysuser_id_t)1,    (sysuser_id_t)1));
   TEST( isequal_sysuserid((sysuser_id_t)1234, (sysuser_id_t)1234));
   TEST( isequal_sysuserid(sysuser_id_FREE, sysuser_id_FREE));
   TEST(!isequal_sysuserid((sysuser_id_t)1,    (sysuser_id_t)0));
   TEST(!isequal_sysuserid(sysuser_id_FREE, (sysuser_id_t)0));
   TEST(!isequal_sysuserid((sysuser_id_t)1234, sysuser_id_FREE));

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   sysuser_t      sysusr  = sysuser_FREE;
   sysuser_t      freeusr = sysuser_FREE;

   // TEST sysuser_FREE
   TEST(sysusr.current  == sysuser_id_FREE);
   TEST(sysusr.realuser == sysuser_id_FREE);
   TEST(sysusr.privilegeduser == sysuser_id_FREE);

   // TEST init_sysuser
   TEST(getuid()  == sysuser_maincontext()->current);
   TEST(getuid()  == sysuser_maincontext()->realuser);
   TEST(geteuid() == sysuser_maincontext()->realuser);
   TEST(0 == setresuid(sysuser_maincontext()->realuser, sysuser_maincontext()->privilegeduser, sysuser_maincontext()->privilegeduser));
   TEST(0 == init_sysuser(&sysusr));
   TEST(1 == isequal_sysuser(&sysusr, sysuser_maincontext()));
   TEST(getuid()  == sysuser_maincontext()->realuser);
   TEST(geteuid() == sysuser_maincontext()->realuser);

   // TEST free_sysuser
   TEST(0 == free_sysuser(&sysusr));
   TEST(1 == isequal_sysuser(&sysusr, &freeusr));
   TEST(getuid()  == sysuser_maincontext()->realuser);
   TEST(geteuid() == sysuser_maincontext()->privilegeduser);
   TEST(0 == setresuid(sysuser_maincontext()->realuser, sysuser_maincontext()->realuser, sysuser_maincontext()->privilegeduser));
   // changes nothing
   TEST(0 == free_sysuser(&sysusr));
   TEST(1 == isequal_sysuser(&sysusr, &freeusr));
   TEST(getuid()  == sysuser_maincontext()->realuser);
   TEST(geteuid() == sysuser_maincontext()->realuser);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   sysuser_t sysusr = sysuser_FREE;

   // TEST current_sysuser
   TEST(current_sysuser(&sysusr) == sysuser_id_FREE);
   sysusr.current = 0;
   TEST(current_sysuser(&sysusr) == 0);
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      sysusr.current = i;
      TEST(current_sysuser(&sysusr) == i);
   }

   // TEST real_sysuser
   TEST(real_sysuser(&sysusr) == sysuser_id_FREE);
   sysusr.realuser = 0;
   TEST(real_sysuser(&sysusr) == 0);
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      sysusr.realuser = i;
      TEST(real_sysuser(&sysusr) == i);
   }

   // TEST privileged_sysuser
   TEST(privileged_sysuser(&sysusr) == sysuser_id_FREE);
   sysusr.privilegeduser = 0;
   TEST(privileged_sysuser(&sysusr) == 0);
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      sysusr.privilegeduser = i;
      TEST(privileged_sysuser(&sysusr) == i);
   }

   // TEST isequal_sysuser
   sysuser_t   sysusr1 = sysuser_FREE;
   sysuser_t   sysusr2 = sysuser_FREE;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2));
   sysusr1.current = 0;
   TEST(0 == isequal_sysuser(&sysusr1, &sysusr2));
   sysusr2.current = 0;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2));
   sysusr1.realuser = 0;
   TEST(0 == isequal_sysuser(&sysusr1, &sysusr2));
   sysusr2.realuser = 0;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2));
   sysusr1.privilegeduser = 0;
   TEST(0 == isequal_sysuser(&sysusr1, &sysusr2));
   sysusr2.privilegeduser = 0;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2));

   return 0;
ONERR:
   return EINVAL;
}

static int test_switchandset(void)
{
   sysuser_t oldusr = *sysuser_maincontext();

   if (  real_sysuser(sysuser_maincontext())
         == privileged_sysuser(sysuser_maincontext())) {
      logwarning_unittest("Need set-user-ID bit to test switching user");
   }

   // TEST switchtoprivilege_sysuser
   TEST(getuid()  == sysuser_maincontext()->current);
   TEST(getuid()  == sysuser_maincontext()->realuser);
   TEST(geteuid() == sysuser_maincontext()->realuser);
   TEST(0 == switchtoprivilege_sysuser(sysuser_maincontext()));
   TEST(sysuser_maincontext()->privilegeduser == getuid());
   TEST(sysuser_maincontext()->privilegeduser == geteuid());
   TEST(sysuser_maincontext()->current        == getuid());

   // TEST switchtoreal_sysuser
   TEST(0 == switchtoreal_sysuser(sysuser_maincontext()));
   TEST(sysuser_maincontext()->realuser == getuid());
   TEST(sysuser_maincontext()->realuser == geteuid());
   TEST(sysuser_maincontext()->current  == getuid());

   // TEST setusers_sysuser
   TEST(0 == switchtoprivilege_sysuser(sysuser_maincontext()));
   TEST(0 == setusers_sysuser(sysuser_maincontext(), sysuser_maincontext()->privilegeduser, sysuser_maincontext()->realuser));
   TEST(sysuser_maincontext()->current        == oldusr.privilegeduser);
   TEST(sysuser_maincontext()->realuser       == oldusr.privilegeduser);
   TEST(sysuser_maincontext()->privilegeduser == oldusr.realuser);
   TEST(sysuser_maincontext()->realuser       == getuid());
   TEST(sysuser_maincontext()->realuser       == geteuid());
   TEST(0 == setusers_sysuser(sysuser_maincontext(), sysuser_maincontext()->privilegeduser, sysuser_maincontext()->realuser));
   TEST(1 == isequal_sysuser(sysuser_maincontext(), &oldusr));
   TEST(sysuser_maincontext()->realuser       == getuid());
   TEST(sysuser_maincontext()->realuser       == geteuid());

   return 0;
ONERR:
   if (!isequal_sysuser(sysuser_maincontext(), &oldusr)) {
      setusers_sysuser(sysuser_maincontext(), oldusr.realuser, oldusr.privilegeduser);
   }
   return EINVAL;
}

static int test_userinfo(void)
{
   sysuser_info_t* usrinfo = 0;

   // TEST new_sysuserinfo, delete_sysuserinfo: root
   TEST(0 == new_sysuserinfo(&usrinfo, 0/*root*/));
   TEST(0 != usrinfo);
   TEST(name_sysuserinfo(usrinfo) == (char*)(&usrinfo[1]));
   TEST(0 == strcmp("root", name_sysuserinfo(usrinfo)));
   TEST(0 == delete_sysuserinfo(&usrinfo));
   TEST(0 == usrinfo);
   TEST(0 == delete_sysuserinfo(&usrinfo));
   TEST(0 == usrinfo);

   // TEST new_sysuserinfo: current user
   TEST(0 == new_sysuserinfo(&usrinfo, getuid()));
   TEST(0 != usrinfo);
   TEST(name_sysuserinfo(usrinfo) == (char*)(&usrinfo[1]));
   TEST(0 != strcmp("", name_sysuserinfo(usrinfo)));
   TEST(0 == delete_sysuserinfo(&usrinfo));
   TEST(0 == usrinfo);

   // TEST new_sysuserinfo: ENOENT
   usrinfo = (void*)1;
   TEST(ENOENT == new_sysuserinfo(&usrinfo, (uid_t)-2));
   TEST((void*)1 == usrinfo);
   usrinfo = 0;

   return 0;
ONERR:
   delete_sysuserinfo(&usrinfo);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE;

   if (test_userinfo())             goto ONERR;
   CLEARBUFFER_ERRLOG();

   TEST(0 == init_resourceusage(&usage));

   if (test_userid())               goto ONERR;
   if (test_initfree())             goto ONERR;
   if (test_query())                goto ONERR;
   if (test_switchandset())         goto ONERR;
   if (test_userinfo())             goto ONERR;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_platform_sysuser()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
