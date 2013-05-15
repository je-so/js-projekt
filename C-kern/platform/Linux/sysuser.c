/* title: SystemUser Linuximpl

   Implements <SystemUser>.

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
#include "C-kern/api/test.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/platform/task/process.h"
#endif



// section: sysuser_id_t

// group: query

bool isadmin_sysuserid(sysuser_id_t uid)
{
   return uid == 0 ;
}

bool isequal_sysuserid(sysuser_id_t luid, sysuser_id_t ruid)
{
   return luid == ruid ;
}


// section: sysuser_t

// group: static configuration

// TEXTDB:SELECT('#undef  sysuser_'name\n'#define sysuser_'name'      "'value'"')FROM("C-kern/resource/config/modulevalues")WHERE(module=="sysuser_t")
#undef  sysuser_SYS_SERVICE_NAME
#define sysuser_SYS_SERVICE_NAME      "passwd"
#undef  sysuser_UNITTEST_USERNAME
#define sysuser_UNITTEST_USERNAME      "guest"
#undef  sysuser_UNITTEST_PASSWORD
#define sysuser_UNITTEST_PASSWORD      "GUEST"
// TEXTDB:END

// group: initonce

int initonce_sysuser(/*out*/sysuser_t ** sysuser)
{
   int err ;
   sysuser_t * new_sysusr ;

   new_sysusr = allocstatic_maincontext(sizeof(sysuser_t)) ;
   if (!new_sysusr) {
      err = ENOMEM ;
      goto ONABORT ;
   }

   err = init_sysuser(new_sysusr) ;
   if (err) goto ONABORT ;

   *sysuser = new_sysusr ;

   return 0 ;
ONABORT:
   if (new_sysusr) {
      freestatic_maincontext(sizeof(sysuser_t)) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}

int freeonce_sysuser(sysuser_t ** sysuser)
{
   int err ;
   sysuser_t * delobj = *sysuser ;

   if (delobj) {
      *sysuser = 0 ;

      err = free_sysuser(delobj) ;

      int err2 = freestatic_maincontext(sizeof(sysuser_t)) ;
      if (err2) err = err2 ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: lifetime

int init_sysuser(sysuser_t * sysusr)
{
   int err ;
   sysuser_id_t uid ;
   sysuser_id_t euid ;

   if (0 != sysuser_maincontext()) {
      // already initialized (used in testing)
      uid  = sysuser_maincontext()->realuser ;
      euid = sysuser_maincontext()->privilegeduser ;
   } else {
      uid  = getuid() ;
      euid = geteuid() ;
   }

   err = setresuid(uid, uid, euid) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("setresuid", err) ;
      goto ONABORT ;
   }

   sysusr->current  = uid ;
   sysusr->realuser = uid ;
   sysusr->privilegeduser = euid ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int free_sysuser(sysuser_t * sysusr)
{
   int err ;

   if (sysuser_id_INIT_FREEABLE != sysusr->realuser) {
      err = setresuid(sysusr->realuser, sysusr->privilegeduser, sysusr->privilegeduser) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("setresuid", err) ;
         goto ONABORT ;
      }

      *sysusr = (sysuser_t) sysuser_INIT_FREEABLE ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}

// group: query

bool isequal_sysuser(const sysuser_t * lsysusr, const sysuser_t * rsysusr)
{
   return   lsysusr->current == rsysusr->current
            && lsysusr->realuser == rsysusr->realuser
            && lsysusr->privilegeduser == rsysusr->privilegeduser ;
}

// group: switch

int switchtoprivilege_sysuser(sysuser_t * sysusr)
{
   int err ;

   if (sysuser_id_INIT_FREEABLE != sysusr->privilegeduser) {
      err = setresuid(sysusr->privilegeduser, sysusr->privilegeduser, sysusr->realuser) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("setresuid", err) ;
         goto ONABORT ;
      }

      sysusr->current = sysusr->privilegeduser ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int switchtoreal_sysuser(sysuser_t * sysusr)
{
   int err ;

   if (sysuser_id_INIT_FREEABLE != sysusr->realuser) {
      err = setresuid(sysusr->realuser, sysusr->realuser, sysusr->privilegeduser) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("setresuid", err) ;
         goto ONABORT ;
      }

      sysusr->current = sysusr->realuser ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: set

int setusers_sysuser(sysuser_t * sysusr, sysuser_id_t realuser, sysuser_id_t privilegeduser)
{
   int err ;

   VALIDATE_INPARAM_TEST(realuser != sysuser_id_INIT_FREEABLE && privilegeduser != sysuser_id_INIT_FREEABLE, ONABORT, ) ;

   err = setresuid(realuser, realuser, privilegeduser) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("setresuid", err) ;
      goto ONABORT ;
   }

   sysusr->current  = realuser ;
   sysusr->realuser = realuser ;
   sysusr->privilegeduser = privilegeduser ;

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: authentication

/* function: pamerr2errno_sysuser
 * Converts error codes from pam lib to errno error codes. */
static int pamerr2errno_sysuser(int pamerr)
{
   switch (pamerr) {
   case PAM_SUCCESS:          // Transaction was successful created.
      return 0 ;
   case PAM_OPEN_ERR:         // shared service library loading error with dlopen
      return ELIBACC ;
   case PAM_ABORT:            // Caller should exit immediately after return
      return ENOTRECOVERABLE ;
   case PAM_USER_UNKNOWN:     // Unknown user name (no entry in authentication information found)
   case PAM_ACCT_EXPIRED:     // User account has expired
   case PAM_NEW_AUTHTOK_REQD: // Authentication token is expired
   case PAM_AUTH_ERR:         // The user was not authenticated
      return EACCES ;
   case PAM_AUTHINFO_UNAVAIL: // unable to access the authentication information
      return ENODATA ;
   case PAM_BUF_ERR:          // Memory buffer error
      return ENOMEM ;
   case PAM_MAXTRIES:         // (Do not try again) at least one authentication module has reached its limit of tries authenticating the user.
      return ERANGE ;
   case PAM_PERM_DENIED:
   case PAM_CRED_INSUFFICIENT:// process does not have sufficient credentials to authenticate the user
      return EPERM ;
   }

   return EINVAL ;
}

/* function: authenticatecallback_sysuser
 * Function called back from PAM library during authentication of user.
 * The single argument which is delivered back is the password. */
static int authenticatecallback_sysuser(int num_msg, const struct pam_message ** msg, struct pam_response ** resp, void * appdata_ptr)
{
   int err ;
   // appdata_ptr contains pasword
   char * password = strdup(appdata_ptr ? (const char*)appdata_ptr : "") ;

   if (!password) {
      return PAM_BUF_ERR ;
   }

   if (num_msg != 1 || msg[0]->msg_style != PAM_PROMPT_ECHO_OFF) {
      err = PAM_CONV_ERR ;
      goto ONABORT ;
   }

   *resp = (struct pam_response*) malloc(sizeof(struct pam_response)) ;
   if (0 == (*resp)) {
      err = PAM_BUF_ERR ;
      goto ONABORT ;
   }

   (*resp)[0].resp         = password ;
   (*resp)[0].resp_retcode = 0 ;

   return PAM_SUCCESS ;
ONABORT:
   free(password) ;
   return err ;
}

int authenticate_sysuser(const char * username, const char * password)
{
   int err ;
   pam_handle_t      * pamhandle = 0 ;
   struct pam_conv   pamconv     = { .conv = &authenticatecallback_sysuser, .appdata_ptr = CONST_CAST(char,password) } ;

   err = pam_start(sysuser_SYS_SERVICE_NAME, username, &pamconv, &pamhandle) ;
   err = pamerr2errno_sysuser(err) ;
   if (err) goto ONABORT ;

   err = pam_authenticate(pamhandle, PAM_DISALLOW_NULL_AUTHTOK|PAM_SILENT) ;
   err = pamerr2errno_sysuser(err) ;
   if (err) goto ONABORT ;

   err = pam_acct_mgmt(pamhandle, PAM_DISALLOW_NULL_AUTHTOK|PAM_SILENT) ;
   err = pamerr2errno_sysuser(err) ;
   if (err) goto ONABORT ;

   const void * username2 = 0 ;
   err = pam_get_item(pamhandle, PAM_USER, &username2) ;
   err = pamerr2errno_sysuser(err) ;
   if (err) goto ONABORT ;
   if (  username2
         && strcmp((const char*)username2, username)) {
      // username changed
      err = EACCES ;
      goto ONABORT ;
   }

   err = pam_end(pamhandle, 0) ;
   pamhandle = 0 ;
   err = pamerr2errno_sysuser(err) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   if (pamhandle) {
      (void) pam_end(pamhandle, 0) ;
   }
   TRACEABORT_LOG(err) ;
   return err ;
}


// section: sysuser_info_t

int new_sysuserinfo(/*out*/sysuser_info_t ** usrinfo, sysuser_id_t uid)
{
   int err ;
   struct passwd  info ;
   struct passwd  * result = 0 ;
   static_assert(sizeof(size_t) == sizeof(long), "long sysconf(...) converted to size_t" ) ;
   size_t         size   = sizeof(sysuser_info_t) + (size_t) sysconf(_SC_GETPW_R_SIZE_MAX) ;
   memblock_t     mblock = memblock_INIT_FREEABLE ;

   err = RESIZE_MM(size, &mblock) ;
   if (err) goto ONABORT ;

   sysuser_info_t *  newobj  = (sysuser_info_t*)(mblock.addr) ;
   char *            straddr = (char*)          (mblock.addr + sizeof(sysuser_info_t)) ;
   size_t            strsize = size - sizeof(sysuser_info_t) ;

   err = getpwuid_r(uid, &info, straddr, strsize, &result) ;
   if (err) {
      TRACESYSERR_LOG("getpwuid_r", err) ;
      goto ONABORT ;
   }

   if (!result) {
      err = ENOENT ;
      goto ONABORT ;
   }

   newobj->size  = size ;
   newobj->name  = result->pw_name ;

   *usrinfo = newobj ;

   return 0 ;
ONABORT:
   FREE_MM(&mblock) ;
   if (ENOENT != err) {
      TRACEABORT_LOG(err) ;
   }
   return err ;
}

int delete_sysuserinfo(sysuser_info_t ** usrinfo)
{
   int err ;
   sysuser_info_t * delobj = *usrinfo ;

   if (delobj) {
      *usrinfo = 0 ;

      memblock_t mblock = memblock_INIT(delobj->size, (uint8_t*)delobj) ;
      err = FREE_MM(&mblock) ;

      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORTFREE_LOG(err) ;
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

static int test_userid(void)
{
   sysuser_id_t   usrid  = sysuser_id_INIT_FREEABLE ;

   // TEST sysuser_id_INIT_FREEABLE
   TEST(usrid == sys_userid_INIT_FREEABLE) ;
   TEST(usrid == (uid_t)-1) ;

   // TEST isadmin_sysuserid
   TEST(true == isadmin_sysuserid(0)) ;
   TEST(false == isadmin_sysuserid(1)) ;
   TEST(false == isadmin_sysuserid(sysuser_id_INIT_FREEABLE)) ;

   // TEST isequal_sysuserid
   TEST( isequal_sysuserid((sysuser_id_t)0,    (sysuser_id_t)0)) ;
   TEST( isequal_sysuserid((sysuser_id_t)1,    (sysuser_id_t)1)) ;
   TEST( isequal_sysuserid((sysuser_id_t)1234, (sysuser_id_t)1234)) ;
   TEST( isequal_sysuserid(sysuser_id_INIT_FREEABLE, sysuser_id_INIT_FREEABLE)) ;
   TEST(!isequal_sysuserid((sysuser_id_t)1,    (sysuser_id_t)0)) ;
   TEST(!isequal_sysuserid(sysuser_id_INIT_FREEABLE, (sysuser_id_t)0)) ;
   TEST(!isequal_sysuserid((sysuser_id_t)1234, sysuser_id_INIT_FREEABLE)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_initfree(void)
{
   sysuser_t      sysusr  = sysuser_INIT_FREEABLE ;
   sysuser_t      freeusr = sysuser_INIT_FREEABLE ;

   // warning is printed in test_authenticate
   // if sysuser_maincontext()->realuser == sysuser_maincontext()->privilegeduser

   // TEST sysuser_INIT_FREEABLE
   TEST(sysusr.current  == sysuser_id_INIT_FREEABLE) ;
   TEST(sysusr.realuser == sysuser_id_INIT_FREEABLE) ;
   TEST(sysusr.privilegeduser == sysuser_id_INIT_FREEABLE) ;

   // TEST init_sysuser
   TEST(getuid()  == sysuser_maincontext()->current) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->realuser) ;
   TEST(0 == setresuid(sysuser_maincontext()->realuser, sysuser_maincontext()->privilegeduser, sysuser_maincontext()->privilegeduser)) ;
   TEST(0 == init_sysuser(&sysusr)) ;
   TEST(1 == isequal_sysuser(&sysusr, sysuser_maincontext())) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->realuser) ;

   // TEST free_sysuser
   TEST(0 == free_sysuser(&sysusr)) ;
   TEST(1 == isequal_sysuser(&sysusr, &freeusr)) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->privilegeduser) ;
   TEST(0 == setresuid(sysuser_maincontext()->realuser, sysuser_maincontext()->realuser, sysuser_maincontext()->privilegeduser)) ;
   // changes nothing
   TEST(0 == free_sysuser(&sysusr)) ;
   TEST(1 == isequal_sysuser(&sysusr, &freeusr)) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->realuser) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   sysuser_t   sysusr = sysuser_INIT_FREEABLE ;

   // TEST current_sysuser
   TEST(current_sysuser(&sysusr) == sysuser_id_INIT_FREEABLE) ;
   sysusr.current = 0 ;
   TEST(current_sysuser(&sysusr) == 0) ;
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      sysusr.current = i ;
      TEST(current_sysuser(&sysusr) == i) ;
   }

   // TEST real_sysuser
   TEST(real_sysuser(&sysusr) == sysuser_id_INIT_FREEABLE) ;
   sysusr.realuser = 0 ;
   TEST(real_sysuser(&sysusr) == 0) ;
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      sysusr.realuser = i ;
      TEST(real_sysuser(&sysusr) == i) ;
   }

   // TEST privileged_sysuser
   TEST(privileged_sysuser(&sysusr) == sysuser_id_INIT_FREEABLE) ;
   sysusr.privilegeduser = 0 ;
   TEST(privileged_sysuser(&sysusr) == 0) ;
   for (uid_t i = 1; i; i = (uid_t)(i << 1)) {
      sysusr.privilegeduser = i ;
      TEST(privileged_sysuser(&sysusr) == i) ;
   }

   // TEST isequal_sysuser
   sysuser_t   sysusr1 = sysuser_INIT_FREEABLE ;
   sysuser_t   sysusr2 = sysuser_INIT_FREEABLE ;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2)) ;
   sysusr1.current = 0 ;
   TEST(0 == isequal_sysuser(&sysusr1, &sysusr2)) ;
   sysusr2.current = 0 ;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2)) ;
   sysusr1.realuser = 0 ;
   TEST(0 == isequal_sysuser(&sysusr1, &sysusr2)) ;
   sysusr2.realuser = 0 ;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2)) ;
   sysusr1.privilegeduser = 0 ;
   TEST(0 == isequal_sysuser(&sysusr1, &sysusr2)) ;
   sysusr2.privilegeduser = 0 ;
   TEST(1 == isequal_sysuser(&sysusr1, &sysusr2)) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_switchandset(void)
{
   sysuser_t   oldusr = *sysuser_maincontext() ;

   // warning is printed in test_authenticate
   // if sysuser_maincontext()->realuser == sysuser_maincontext()->privilegeduser

   // TEST switchtoprivilege_sysuser
   TEST(getuid()  == sysuser_maincontext()->current) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->realuser) ;
   TEST(0 == switchtoprivilege_sysuser(sysuser_maincontext())) ;
   TEST(sysuser_maincontext()->privilegeduser == getuid()) ;
   TEST(sysuser_maincontext()->privilegeduser == geteuid()) ;
   TEST(sysuser_maincontext()->current        == getuid()) ;

   // TEST switchtoreal_sysuser
   TEST(0 == switchtoreal_sysuser(sysuser_maincontext())) ;
   TEST(sysuser_maincontext()->realuser == getuid()) ;
   TEST(sysuser_maincontext()->realuser == geteuid()) ;
   TEST(sysuser_maincontext()->current  == getuid()) ;

   // TEST setusers_sysuser
   TEST(0 == switchtoprivilege_sysuser(sysuser_maincontext())) ;
   TEST(0 == setusers_sysuser(sysuser_maincontext(), sysuser_maincontext()->privilegeduser, sysuser_maincontext()->realuser)) ;
   TEST(sysuser_maincontext()->current        == oldusr.privilegeduser) ;
   TEST(sysuser_maincontext()->realuser       == oldusr.privilegeduser) ;
   TEST(sysuser_maincontext()->privilegeduser == oldusr.realuser) ;
   TEST(sysuser_maincontext()->realuser       == getuid()) ;
   TEST(sysuser_maincontext()->realuser       == geteuid()) ;
   TEST(0 == setusers_sysuser(sysuser_maincontext(), sysuser_maincontext()->privilegeduser, sysuser_maincontext()->realuser)) ;
   TEST(1 == isequal_sysuser(sysuser_maincontext(), &oldusr)) ;
   TEST(sysuser_maincontext()->realuser       == getuid()) ;
   TEST(sysuser_maincontext()->realuser       == geteuid()) ;

   return 0 ;
ONABORT:
   if (!isequal_sysuser(sysuser_maincontext(), &oldusr)) {
      setusers_sysuser(sysuser_maincontext(), oldusr.realuser, oldusr.privilegeduser) ;
   }
   return EINVAL ;
}

static int test_userinfo(void)
{
   sysuser_info_t  * usrinfo = 0 ;

   // TEST new_sysuserinfo, delete_sysuserinfo: root
   TEST(0 == new_sysuserinfo(&usrinfo, 0/*root*/)) ;
   TEST(0 != usrinfo) ;
   TEST(name_sysuserinfo(usrinfo) == (char*)(&usrinfo[1])) ;
   TEST(0 == strcmp("root", name_sysuserinfo(usrinfo))) ;
   TEST(0 == delete_sysuserinfo(&usrinfo)) ;
   TEST(0 == usrinfo) ;
   TEST(0 == delete_sysuserinfo(&usrinfo)) ;
   TEST(0 == usrinfo) ;

   // TEST new_sysuserinfo: current user
   TEST(0 == new_sysuserinfo(&usrinfo, getuid())) ;
   TEST(0 != usrinfo) ;
   TEST(name_sysuserinfo(usrinfo) == (char*)(&usrinfo[1])) ;
   TEST(0 != strcmp("", name_sysuserinfo(usrinfo))) ;
   TEST(0 == delete_sysuserinfo(&usrinfo)) ;
   TEST(0 == usrinfo) ;

   // TEST new_sysuserinfo: ENOENT
   usrinfo = (void*)1 ;
   TEST(ENOENT == new_sysuserinfo(&usrinfo, (uid_t)-2)) ;
   TEST((void*)1 == usrinfo) ;
   usrinfo = 0 ;

   return 0 ;
ONABORT:
   delete_sysuserinfo(&usrinfo) ;
   return EINVAL ;
}


static int test_authenticate(bool iswarn)
{
   const char     * username   = sysuser_UNITTEST_USERNAME ;
   const char     * password   = sysuser_UNITTEST_PASSWORD ;
   sysuser_info_t * usrinfo[2] = { 0 } ;

   // TEST authenticate_sysuser
   TEST(0 == new_sysuserinfo(&usrinfo[0], real_sysuser(sysuser_maincontext()))) ;
   TEST(0 == new_sysuserinfo(&usrinfo[1], privileged_sysuser(sysuser_maincontext()))) ;
   if (  isadmin_sysuserid(real_sysuser(sysuser_maincontext()))
         || isadmin_sysuserid(privileged_sysuser(sysuser_maincontext()))
         || 0 == strcmp(name_sysuserinfo(usrinfo[0]), username)
         || 0 == strcmp(name_sysuserinfo(usrinfo[1]), username)) {
      bool isChangeUID = ( real_sysuser(sysuser_maincontext()) != privileged_sysuser(sysuser_maincontext())
                           && (  isadmin_sysuserid(privileged_sysuser(sysuser_maincontext()))
                                 || 0 == strcmp(name_sysuserinfo(usrinfo[1]), username))) ;
      if (isChangeUID) {
         TEST(0 == switchtoprivilege_sysuser(sysuser_maincontext()))
      }
      int err = authenticate_sysuser(username, password) ;
      if (isChangeUID) {
         TEST(0 == switchtoreal_sysuser(sysuser_maincontext()))
      }
      if (err && iswarn) {
         CPRINTF_LOG(TEST, "\n*** Need user account name=%s password=%s ***\n", username, password) ;
      }
      TEST(0 == err) ;
   } else if (iswarn) {
      CPRINTF_LOG(TEST, "** Need root or guest setuid ** ") ;
   }
   TEST(0 == delete_sysuserinfo(&usrinfo[0])) ;
   TEST(0 == delete_sysuserinfo(&usrinfo[1])) ;

   // TEST authenticate_sysuser: EACCES
   TEST(EACCES == authenticate_sysuser("unknown_user", 0)) ;

   // unprepare
   closelog() ;   // opened by libpam

   return 0 ;
ONABORT:
   closelog() ;
   return EINVAL ;
}

static int test_initonce(void)
{
   sysuser_t   *  sysusr = 0 ;

   // warning is printed in test_authenticate
   // if sysuser_maincontext()->realuser == sysuser_maincontext()->privilegeduser

   // TEST initonce_sysuser
   size_t oldsize = sizestatic_maincontext() ;
   TEST(0 == initonce_sysuser(&sysusr)) ;
   TEST(0 != sysusr) ;
   TEST(1 == isequal_sysuser(sysusr, sysuser_maincontext())) ;
   TEST(sizestatic_maincontext() == oldsize + sizeof(sysuser_t)) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->realuser) ;

   // TEST freeonce_sysuser
   TEST(0 == freeonce_sysuser(&sysusr)) ;
   TEST(0 == sysusr) ;
   TEST(sizestatic_maincontext() == oldsize) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->privilegeduser) ;
   TEST(0 == setresuid(sysuser_maincontext()->realuser, sysuser_maincontext()->realuser, sysuser_maincontext()->privilegeduser)) ;
   // changes nothing
   TEST(0 == freeonce_sysuser(&sysusr)) ;
   TEST(0 == sysusr) ;
   TEST(sizestatic_maincontext() == oldsize) ;
   TEST(getuid()  == sysuser_maincontext()->realuser) ;
   TEST(geteuid() == sysuser_maincontext()->realuser) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int exectest_childprocess(void * logfd)
{
   resourceusage_t   usage    = resourceusage_INIT_FREEABLE ;

   if (test_authenticate(true))     goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_userid())               goto ONABORT ;
   if (test_initfree())             goto ONABORT ;
   if (test_query())                goto ONABORT ;
   if (test_switchandset())         goto ONABORT ;
   if (test_userinfo())             goto ONABORT ;
   if (test_authenticate(false))    goto ONABORT ;
   if (test_initonce())             goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   // transfer log file
   char *   logbuffer ;
   size_t   logsize ;
   GETBUFFER_LOG(&logbuffer, &logsize) ;
   TEST((size_t)write((int)logfd, logbuffer, logsize) == logsize) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

int unittest_platform_sysuser()
{
   resourceusage_t      usage    = resourceusage_INIT_FREEABLE ;
   int                  logfd[2] = { -1, -1 } ;
   process_t            child    = process_INIT_FREEABLE ;
   process_stdfd_t      stdfd    = process_stdfd_INIT_INHERIT ;

   TEST(0 == init_resourceusage(&usage)) ;

   TEST(0 == pipe2(logfd, O_CLOEXEC)) ;
   // run unittest in child process to protect main process from loading additional shared libs !
   TEST(0 == init_process(&child, &exectest_childprocess, (void*)logfd[1], &stdfd)) ;
   process_result_t result ;
   TEST(0 == wait_process(&child, &result)) ;
   TEST(result.returncode == 0) ;
   TEST(result.state      == process_state_TERMINATED) ;

   // read log file from child
   char logbuffer[256] = {0} ;
   int  logsize ;
   logsize = read(logfd[0], logbuffer, sizeof(logbuffer)-1) ;
   if (logsize > 0) {
      PRINTF_LOG("%s", logbuffer) ;
   }
   TEST(0 == free_file(&logfd[0])) ;
   TEST(0 == free_file(&logfd[1])) ;
   TEST(0 == free_process(&child)) ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_process(&child) ;
   (void) free_file(&logfd[0]) ;
   (void) free_file(&logfd[1]) ;
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
