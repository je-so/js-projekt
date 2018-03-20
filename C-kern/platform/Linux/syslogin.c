/* title: SystemLogin Linuximpl

   Implements <SystemLogin>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/platform/syslogin.h
    Header file <SystemLogin>.

   file: C-kern/platform/Linux/syslogin.c
    Implementation file <SystemLogin Linuximpl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/err.h"
#include "C-kern/api/platform/syslogin.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/platform/sync/mutex.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/platform/task/thread.h"
#endif


// section: syslogin_t

// group: helper

static int switchuser(syslogin_t* syslogin, uid_t uid)
{
   int err;

   err = seteuid(uid);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("seteuid(uid)", err);
      PRINTUINT32_ERRLOG(uid);
   }

   syslogin->currentuser = uid;

   return err;
}

// group: lifetime

int init_syslogin(/*out*/syslogin_t* syslogin)
{
   int err;
   sys_userid_t uid;
   sys_userid_t euid;

   uid  = getuid();
   euid = geteuid();

   err = switchuser(syslogin, uid);
   if (err) goto ONERR;

   // syslogin->currentuser // already set in switchuser
   syslogin->realuser     = uid;
   syslogin->privilegeduser = euid;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_syslogin(syslogin_t* syslogin)
{
   int err;

   if (sys_userid_FREE != syslogin->realuser) {
      err = switchuser(syslogin, syslogin->privilegeduser);

      *syslogin = (syslogin_t) syslogin_FREE;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

bool issuperuser_syslogin(sys_userid_t uid)
{
   return 0 == uid;
}

bool isequal_syslogin(const syslogin_t* lsyslogin, const syslogin_t* rsyslogin)
{
   return   lsyslogin->currentuser == rsyslogin->currentuser
            && lsyslogin->realuser == rsyslogin->realuser
            && lsyslogin->privilegeduser == rsyslogin->privilegeduser;
}

int groups_syslogin(size_t capacity, /*out*/sys_groupid_t grouplist[capacity], /*out*/size_t* size)
{
   int err;

   if (capacity > INT_MAX) {
      err = EOVERFLOW;
      goto ONERR;
   }

   int len = getgroups((int)capacity, grouplist);
   if (len == -1) {
      err = (errno == EINVAL) ? ENOBUFS : errno;
      goto ONERR;
   }

   *size = (size_t) len;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: switch

int switchtoprivilegeduser_syslogin(syslogin_t* syslogin)
{
   int err;

   err = switchuser(syslogin, syslogin->privilegeduser);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int switchtorealuser_syslogin(syslogin_t* syslogin)
{
   int err;

   err = switchuser(syslogin, syslogin->realuser);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: set

int switchpermanent_syslogin(syslogin_t* syslogin, sys_userid_t uid)
{
   int err;

   VALIDATE_INPARAM_TEST(uid != sys_userid_FREE, ONERR, );

   err = switchuser(syslogin, uid);
   if (err) goto ONERR;

   err = setreuid(uid, uid);
   if (err) {
      err = errno;
      TRACESYSCALL_ERRLOG("setreuid(uid,uid)", err);
      PRINTUINT32_ERRLOG(uid);
      goto ONERR;
   }

   // syslogin->currentuser // set in switchuser
   syslogin->realuser     = uid;
   syslogin->privilegeduser = uid;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: syslogin_info_t

// group: static variables

mutex_t s_syslogininfo_lock = mutex_INIT_DEFAULT;

// group: lifetime

int new_syslogininfo(/*out*/syslogin_info_t** info, sys_userid_t uid)
{
   int err;
   memblock_t mblock = memblock_FREE;

   slock_mutex(&s_syslogininfo_lock);

   errno = 0;
   struct passwd* pwd = getpwuid(uid);
   if (!pwd) {
      if (errno) {
         err = errno;
         TRACESYSCALL_ERRLOG("getpwuid(uid)", err);
         PRINTUINT32_ERRLOG(uid);
      } else {
         err = ENOENT;
      }
      goto UNLOCK;
   }

   size_t namesize = strlen(pwd->pw_name) + 1; // size username
   size_t nrofgrp = 0;

   #define GELEMSIZE (sizeof(sys_groupid_t) + sizeof(char*))

   setgrent();

   for (;;) {
      errno = 0;
      struct group* grp = getgrent();
      if (!grp) {
         if (errno) {
            err = errno;
            TRACESYSCALL_ERRLOG("getgrent", err);
            goto UNLOCK;
         }
         break;
      }

      bool ismatch = (grp->gr_gid == pwd->pw_gid);
      for (int i = 0; !ismatch && grp->gr_mem[i]; ++i) {
         ismatch = (0 == strcmp(grp->gr_mem[i], pwd->pw_name));
      }

      if (ismatch) {
         size_t len = strlen(grp->gr_name);
         if (nrofgrp == (SIZE_MAX/GELEMSIZE) || namesize >= SIZE_MAX/2 || len >= SIZE_MAX/2) {
            err = ENOMEM;
            goto UNLOCK;
         }
         nrofgrp ++;
         namesize += len + 1;
      }
   }

   size_t size = sizeof(syslogin_info_t) + namesize;
   size_t arrsize = nrofgrp * GELEMSIZE;
   if (size <= namesize) size = 0;
   size += arrsize;
   if (size <= arrsize) {
      err = ENOMEM;
      goto UNLOCK;
   }

   err = ALLOC_MM(size, &mblock);
   if (err) goto UNLOCK;

   syslogin_info_t* newobj = (syslogin_info_t*) mblock.addr;
   uint8_t* data = mblock.addr + sizeof(syslogin_info_t);
   size_t datasize = size - sizeof(syslogin_info_t);
   size_t dataoff;

   newobj->size  = size;
   newobj->uid   = uid;
   newobj->nrgroups = nrofgrp;
   newobj->gmain = 0;
   newobj->gname = (const char**) data;
   dataoff = nrofgrp * sizeof(char*);
   newobj->gid   = (sys_groupid_t*) (data + dataoff);
   dataoff += nrofgrp * sizeof(sys_groupid_t);
   size_t fieldsize = strlen(pwd->pw_name) + 1;
   if (fieldsize > (datasize - dataoff)) {
      err = EAGAIN;
      goto UNLOCK;
   }
   memcpy(data + dataoff, pwd->pw_name, fieldsize);
   newobj->uname = (char*) (data + dataoff);
   dataoff += fieldsize;

   setgrent();
   for (size_t gi = 0; gi < nrofgrp; ) {
      errno = 0;
      struct group* grp = getgrent();
      if (!grp) {
         err = errno ? errno : ENOENT;
         TRACESYSCALL_ERRLOG("getgrent", err);
         goto UNLOCK;
      }

      bool ismatch = (grp->gr_gid == pwd->pw_gid);
      for (int i = 0; !ismatch && grp->gr_mem[i]; ++i) {
         ismatch = (0 == strcmp(grp->gr_mem[i], pwd->pw_name));
      }

      if (ismatch) {
         fieldsize = strlen(grp->gr_name) + 1;
         if (fieldsize > (datasize - dataoff)) {
            err = EAGAIN;
            goto UNLOCK;
         }

         char* gname = (char*) (data + dataoff);
         dataoff += fieldsize;
         memcpy(gname, grp->gr_name, fieldsize);

         if (grp->gr_gid == pwd->pw_gid) {
            newobj->gmain = gi;
         }

         newobj->gid[gi]   = grp->gr_gid;
         newobj->gname[gi] = gname;
         ++gi;
      }
   }

   *info = newobj;

UNLOCK:
   endgrent();
   endpwent();
   sunlock_mutex(&s_syslogininfo_lock);

   if (err) goto ONERR;

   return 0;
ONERR:
   FREE_MM(&mblock);
   if (ENOENT != err) {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}

int delete_syslogininfo(syslogin_info_t** info)
{
   int err;
   syslogin_info_t* delobj = *info;

   if (delobj) {
      *info = 0;

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

static int test_systypes(void)
{
   sys_userid_t  uid = sys_userid_FREE;
   sys_groupid_t gid = sys_groupid_FREE;

   // TEST sys_userid_FREE
   TEST((uid_t)-1 == uid);
   TEST(isidequal_syslogin(sys_userid_FREE, uid));

   // TEST sys_groupid_FREE
   TEST((uid_t)-1 == gid);
   TEST(isidequal_syslogin(sys_groupid_FREE, gid));

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   syslogin_t syslogin  = syslogin_FREE;
   syslogin_t freelogin = syslogin_FREE;

   // TEST syslogin_FREE
   TEST(syslogin.currentuser == sys_userid_FREE);
   TEST(syslogin.realuser    == sys_userid_FREE);
   TEST(syslogin.privilegeduser == sys_userid_FREE);

   // TEST init_syslogin
   TEST(getuid()  == syslogin_maincontext()->currentuser);
   TEST(getuid()  == syslogin_maincontext()->realuser);
   TEST(geteuid() == syslogin_maincontext()->realuser);
   TEST(0 == setresuid(syslogin_maincontext()->realuser, syslogin_maincontext()->privilegeduser, syslogin_maincontext()->privilegeduser));
   TEST(0 == init_syslogin(&syslogin));
   TEST(1 == isequal_syslogin(&syslogin, syslogin_maincontext()));
   TEST(getuid()  == syslogin_maincontext()->realuser);
   TEST(geteuid() == syslogin_maincontext()->realuser);

   // TEST free_syslogin
   TEST(0 == free_syslogin(&syslogin));
   TEST(1 == isequal_syslogin(&syslogin, &freelogin));
   TEST(getuid()  == syslogin_maincontext()->realuser);
   TEST(geteuid() == syslogin_maincontext()->privilegeduser);
   TEST(0 == setresuid(syslogin_maincontext()->realuser, syslogin_maincontext()->realuser, syslogin_maincontext()->privilegeduser));
   // changes nothing
   TEST(0 == free_syslogin(&syslogin));
   TEST(1 == isequal_syslogin(&syslogin, &freelogin));
   TEST(getuid()  == syslogin_maincontext()->realuser);
   TEST(geteuid() == syslogin_maincontext()->realuser);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   syslogin_t    syslogin = syslogin_FREE;
   sys_groupid_t grouplist[256];

   // TEST issuperuser_syslogin
   TEST(issuperuser_syslogin(0));
   for (sys_userid_t uid = 1; uid < 65536; ++uid) {
      TEST(!issuperuser_syslogin(uid));
   }
   for (sys_userid_t uid = sys_userid_FREE; uid; uid >>= 1) {
      TEST(!issuperuser_syslogin(uid));
   }

   // TEST isidequal_syslogin
   TEST(isidequal_syslogin(0, 0));
   TEST(isidequal_syslogin(sys_userid_FREE, sys_userid_FREE));
   TEST(!isidequal_syslogin(sys_userid_FREE, 0));
   TEST(!isidequal_syslogin(0, sys_userid_FREE));
   for (sys_userid_t uid = sys_userid_FREE; uid; uid >>= 1) {
      TEST(!isidequal_syslogin(0, uid));
      TEST(!isidequal_syslogin(uid, 0));
      TEST(!isidequal_syslogin(sys_userid_FREE, uid-1));
      TEST(!isidequal_syslogin(uid-1, sys_userid_FREE));
   }
   for (sys_userid_t uid = sys_userid_FREE; uid; uid >>= 1) {
      for (sys_userid_t uid2 = uid-1; uid2; uid2 >>= 1) {
         TEST(!isidequal_syslogin(uid, uid2));
         TEST(!isidequal_syslogin(uid2, uid));
      }
   }

   // TEST currentuser_syslogin
   TEST(currentuser_syslogin(&syslogin) == sys_userid_FREE);
   syslogin.currentuser = 0;
   TEST(currentuser_syslogin(&syslogin) == 0);
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      syslogin.currentuser = i;
      TEST(currentuser_syslogin(&syslogin) == i);
   }

   // TEST realuser_syslogin
   TEST(realuser_syslogin(&syslogin) == sys_userid_FREE);
   syslogin.realuser = 0;
   TEST(realuser_syslogin(&syslogin) == 0);
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      syslogin.realuser = i;
      TEST(realuser_syslogin(&syslogin) == i);
   }

   // TEST privilegeduser_syslogin
   TEST(privilegeduser_syslogin(&syslogin) == sys_userid_FREE);
   syslogin.privilegeduser = 0;
   TEST(privilegeduser_syslogin(&syslogin) == 0);
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      syslogin.privilegeduser = i;
      TEST(privilegeduser_syslogin(&syslogin) == i);
   }

   // TEST isequal_syslogin
   syslogin_t   syslogin1 = syslogin_FREE;
   syslogin_t   syslogin2 = syslogin_FREE;
   TEST(1 == isequal_syslogin(&syslogin1, &syslogin2));
   syslogin1.currentuser = 0;
   TEST(0 == isequal_syslogin(&syslogin1, &syslogin2));
   syslogin2.currentuser = 0;
   TEST(1 == isequal_syslogin(&syslogin1, &syslogin2));
   syslogin1.realuser = 0;
   TEST(0 == isequal_syslogin(&syslogin1, &syslogin2));
   syslogin2.realuser = 0;
   TEST(1 == isequal_syslogin(&syslogin1, &syslogin2));
   syslogin1.privilegeduser = 0;
   TEST(0 == isequal_syslogin(&syslogin1, &syslogin2));
   syslogin2.privilegeduser = 0;
   TEST(1 == isequal_syslogin(&syslogin1, &syslogin2));

   // TEST groups_syslogin: capacity == 0
   size_t size = 0;
   TEST(0 == groups_syslogin(0, grouplist, &size));
   TEST(0 < size);
   TEST(lengthof(grouplist) > size);

   // TEST groups_syslogin: capacity > 0
   size_t capacity = size;
   size = 0;
   for (unsigned i = 0; i < lengthof(grouplist); ++i) {
      grouplist[i] = sys_groupid_FREE;
   }
   TEST(0 == groups_syslogin(capacity, grouplist, &size));
   TEST(size == capacity);
   for (unsigned i = 0; i < lengthof(grouplist); ++i) {
      if (i < size) {
         TEST(grouplist[i] != sys_groupid_FREE);
      } else {
         TEST(grouplist[i] == sys_groupid_FREE);
      }
   }

   // TEST groups_syslogin: ENOBUFS
   if (capacity > 1) {
      TEST(ENOBUFS == groups_syslogin(capacity-1, grouplist, &size));
   }

   // TEST groups_syslogin: EOVERFLOW
   TEST(EOVERFLOW == groups_syslogin(1 + (size_t)INT_MAX, grouplist, &size));

   return 0;
ONERR:
   return EINVAL;
}

static int process_switchpermreal(void)
{
   uid_t uid = syslogin_maincontext()->realuser;
   TEST(0 == switchpermanent_syslogin(syslogin_maincontext(), uid));
   TEST(syslogin_maincontext()->currentuser    == uid);
   TEST(syslogin_maincontext()->realuser       == uid);
   TEST(syslogin_maincontext()->privilegeduser == uid);

   uid_t ruid, euid, suid;
   getresuid(&ruid, &euid, &suid);
   TEST(ruid == uid);
   TEST(euid == uid);
   TEST(suid == uid);

   return 0;
ONERR:
   return EINVAL;
}

static int process_switchpermpriv(void)
{
   uid_t uid = syslogin_maincontext()->privilegeduser;
   TEST(0 == switchpermanent_syslogin(syslogin_maincontext(), uid));
   TEST(syslogin_maincontext()->currentuser    == uid);
   TEST(syslogin_maincontext()->realuser       == uid);
   TEST(syslogin_maincontext()->privilegeduser == uid);

   uid_t ruid, euid, suid;
   getresuid(&ruid, &euid, &suid);
   TEST(ruid == uid);
   TEST(euid == uid);
   TEST(suid == uid);

   return 0;
ONERR:
   return EINVAL;
}

static int test_switchandset(void)
{
   int err;
   syslogin_t oldlogin = *syslogin_maincontext();

   if (  realuser_syslogin(syslogin_maincontext())
         == privilegeduser_syslogin(syslogin_maincontext())) {
      logwarning_unittest("Need set-user-ID bit to test switching user");
   }

   for (int tr = 0; tr <= 3; ++tr) {
      // TEST switchtoprivilegeduser_syslogin
      TEST(getuid()  == syslogin_maincontext()->currentuser);
      TEST(getuid()  == syslogin_maincontext()->realuser);
      TEST(geteuid() == syslogin_maincontext()->realuser);
      TEST(0 == switchtoprivilegeduser_syslogin(syslogin_maincontext()));
      TEST(getuid()  == syslogin_maincontext()->realuser);
      TEST(geteuid() == syslogin_maincontext()->privilegeduser);
      TEST(syslogin_maincontext()->currentuser    == oldlogin.privilegeduser);
      TEST(syslogin_maincontext()->realuser       == oldlogin.realuser);
      TEST(syslogin_maincontext()->privilegeduser == oldlogin.privilegeduser);

      // TEST switchtorealuser_syslogin
      TEST(0 == switchtorealuser_syslogin(syslogin_maincontext()));
      TEST(getuid()  == syslogin_maincontext()->realuser);
      TEST(geteuid() == syslogin_maincontext()->realuser);
      TEST(syslogin_maincontext()->currentuser    == oldlogin.currentuser);
      TEST(syslogin_maincontext()->realuser       == oldlogin.realuser);
      TEST(syslogin_maincontext()->privilegeduser == oldlogin.privilegeduser);
   }

   // TEST switchpermanent_syslogin: realuser
   TEST(0 == execasprocess_unittest(&process_switchpermreal, &err));
   TEST(0 == err);

   // TEST switchpermanent_syslogin: privilegeduser
   TEST(0 == execasprocess_unittest(&process_switchpermpriv, &err));
   TEST(0 == err);

   return 0;
ONERR:
   return EINVAL;
}

static int thread_initinfo(void* param)
{
   syslogin_info_t* info = 0;
   int fd = (int) (intptr_t) param;
   TEST(1 == write(fd, "1", 1));
   TEST(0 == new_syslogininfo(&info, 0/*root*/));
   TEST(1 == write(fd, "2", 1));
   TEST(0 == delete_syslogininfo(&info));
   return 0;
ONERR:
   return EINVAL;
}
static int test_logininfo(void)
{
   syslogin_info_t* info = 0;
   thread_t* thr = 0;
   uid_t   uid;
   gid_t   gid;
   int     fd[2] = { -1, -1 };
   uint8_t buffer[16];

   // prepare
   TEST(0 == pipe2(fd, O_CLOEXEC|O_NONBLOCK));

   // == lifetime ==

   for (unsigned entrypos = 0; true; ++entrypos) {
      setpwent();
      for (unsigned i = 0; i < entrypos; ++i) {
         TEST(0 != getpwent());
      }
      struct passwd* pwd = getpwent();
      if (pwd == 0) {
         TEST(entrypos > 2); // tested at least 2 entries
         break;
      }
      uid = pwd->pw_uid;
      gid = pwd->pw_gid;

      // TEST new_syslogininfo: read entries
      TEST(0 == new_syslogininfo(&info, uid));
      TEST(0 != info);
      TEST(0 <  info->size);
      TEST(uid == info->uid);
      TEST(1 <= info->nrgroups);
      TEST(info->nrgroups > info->gmain);
      TEST(gid == info->gid[info->gmain]);
      // check memory addr
      TEST(info->gname == (const char**)(&info[1]));
      TEST(info->gid   == (sys_groupid_t*)((uint8_t*)info->gname + info->nrgroups * sizeof(char*)));
      TEST(info->uname == (const char*)((uint8_t*)info->gid + info->nrgroups * sizeof(sys_groupid_t)));
      TEST(info->gname[0] == info->uname + strlen(info->uname) + 1);
      TEST((uint8_t*)info + info->size  == (const uint8_t*)info->gname[info->nrgroups-1] + strlen(info->gname[info->nrgroups-1]) + 1);
      for (size_t i = 1; i < info->nrgroups; ++i) {
         TEST(info->gname[i-1] + strlen(info->gname[i-1]) + 1 == info->gname[i]);
      }
      // DEBUG printf("user %s(%d) groups ", info->uname, info->uid);
      // DEBUG for (size_t i = 0; i < info->nrgroups; ++i) {
      // DEBUG    if (i == info->gmain) printf("*");
      // DEBUG    printf("%s(%d),", info->gname[i], info->gid[i]);
      // DEBUG }
      // DEBUG printf("\n");

      // TEST delete_syslogininfo
      TEST(0 != info);
      TEST(0 == delete_syslogininfo(&info));
      TEST(0 == info);
      TEST(0 == delete_syslogininfo(&info));
      TEST(0 == info);
   }

   // TEST new_syslogininfo: lock
   TEST(0 == new_thread(&thr, &thread_initinfo, (void*)(intptr_t)fd[1]));
   TEST(0 == lock_mutex(&s_syslogininfo_lock));
   struct pollfd pfd = { .fd = fd[0], .events = POLLIN };
   TEST(1 == poll(&pfd, 1, 10000));
   TEST(1 == read(fd[0], buffer, sizeof(buffer)));
   TEST(EBUSY == tryjoin_thread(thr));
   TEST(-1 == read(fd[0], buffer, sizeof(buffer)));
   TEST(EAGAIN == errno);
   TEST(0 == unlock_mutex(&s_syslogininfo_lock));
   TEST(0 == join_thread(thr));
   TEST(0 == returncode_thread(thr));
   TEST(0 == delete_thread(&thr));

   // TEST new_syslogininfo: ENOENT
   info = (void*)1;
   TEST(ENOENT == new_syslogininfo(&info, (uid_t)-2));
   TEST((void*)1 == info);
   info = 0;

   // == query ==

   // TEST username_syslogininfo
   syslogin_info_t info2;
   info2.uname = 0;
   TEST(0 == username_syslogininfo(&info2));
   for (uintptr_t i = 1; i; i <<= 1) {
      info2.uname = (const char*)i;
      TEST((const char*)i == username_syslogininfo(&info2));
   }

   // unprepare
   TEST(0 == close(fd[0]));
   TEST(0 == close(fd[1]));

   return 0;
ONERR:
   if (info != (void*)1) {
      delete_syslogininfo(&info);
   }
   close(fd[0]);
   close(fd[1]);
   return EINVAL;
}

static int childprocess_unittest(void)
{
   resourceusage_t usage = resourceusage_FREE;

   if (test_logininfo())            goto ONERR;
   CLEARBUFFER_ERRLOG();

   TEST(0 == init_resourceusage(&usage));

   if (test_systypes())             goto ONERR;
   if (test_initfree())             goto ONERR;
   if (test_query())                goto ONERR;
   if (test_switchandset())         goto ONERR;
   if (test_logininfo())            goto ONERR;

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

int unittest_platform_syslogin()
{
   int err;

   TEST(0 == execasprocess_unittest(&childprocess_unittest, &err));

   return err;
ONERR:
   return EINVAL;
}

#endif
