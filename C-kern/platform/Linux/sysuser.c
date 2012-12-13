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
#endif


// section: sysusercontext_t

// group: query

bool isequal_sysusercontext(sysusercontext_t * left, sysusercontext_t * right)
{
   return   left->realuser == right->realuser
            && left->privilegeduser == right->privilegeduser ;
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

int initonce_sysuser(/*out*/sysusercontext_t * sysuser)
{
   int err ;

   if (sys_userid_INIT_FREEABLE != real_sysuser()) {
      // already initialized
      *sysuser = (sysusercontext_t) sysusercontext_INIT(real_sysuser(), privileged_sysuser()) ;
   } else {
      *sysuser = (sysusercontext_t) sysusercontext_INIT(getuid(), geteuid()) ;
   }

   err = setresuid(sysuser->realuser, sysuser->realuser, sysuser->privilegeduser) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("setresuid", err) ;
      goto ONABORT ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int freeonce_sysuser(sysusercontext_t * sysuser)
{
   int err ;

   if (sys_userid_INIT_FREEABLE != sysuser->realuser) {
      err = setresuid(sysuser->realuser, sysuser->privilegeduser, sysuser->privilegeduser) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("setresuid", err) ;
         goto ONABORT ;
      }

      *sysuser = (sysusercontext_t) sysusercontext_INIT_FREEABLE ;
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: query

sysuser_t current_sysuser(void)
{
   return getuid() ;
}

bool isadmin_sysuser(sysuser_t sysusr)
{
   return 0 == sysusr ;
}

// group: switch

int switchtoprivilege_sysuser(void)
{
   int err ;

   if (sys_userid_INIT_FREEABLE != privileged_sysuser()) {
      err = setresuid(privileged_sysuser(), privileged_sysuser(), real_sysuser()) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("setresuid", err) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

int switchtoreal_sysuser(void)
{
   int err ;

   if (sys_userid_INIT_FREEABLE != real_sysuser()) {
      err = setresuid(real_sysuser(), real_sysuser(), privileged_sysuser()) ;
      if (err) {
         err = errno ;
         TRACESYSERR_LOG("setresuid", err) ;
         goto ONABORT ;
      }
   }

   return 0 ;
ONABORT:
   TRACEABORT_LOG(err) ;
   return err ;
}

// group: set

int setusers_sysuser(sysuser_t realuser, sysuser_t privilegeduser)
{
   int err ;

   VALIDATE_INPARAM_TEST(realuser != sys_userid_INIT_FREEABLE && privilegeduser != sys_userid_INIT_FREEABLE, ONABORT, ) ;

   err = setresuid(realuser, realuser, privilegeduser) ;
   if (err) {
      err = errno ;
      TRACESYSERR_LOG("setresuid", err) ;
      goto ONABORT ;
   }

   sysuser_maincontext() = (sysusercontext_t) sysusercontext_INIT(realuser, privilegeduser) ;

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


// section: sysuserinfo_t

int new_sysuserinfo(/*out*/sysuserinfo_t ** usrinfo, sysuser_t sysusr)
{
   int err ;
   struct passwd  info ;
   struct passwd  * result = 0 ;
   size_t         strsize  = (size_t) sysconf(_SC_GETPW_R_SIZE_MAX) ;
   memblock_t     mblock   = memblock_INIT_FREEABLE ;

   err = RESIZE_MM(strsize + sizeof(sysuserinfo_t), &mblock) ;
   if (err) goto ONABORT ;

   sysuserinfo_t * newobj = (sysuserinfo_t *)(mblock.addr) ;
   char          * strbuf = (char*)          (mblock.addr + sizeof(sysuserinfo_t)) ;

   err = getpwuid_r(sysusr, &info, strbuf, strsize, &result) ;
   if (err) {
      TRACESYSERR_LOG("getpwuid_r", err) ;
      goto ONABORT ;
   }

   if (!result) {
      err = ENOENT ;
      goto ONABORT ;
   }

   newobj->strsize  = strsize ;
   newobj->name     = result->pw_name ;

   *usrinfo = newobj ;

   return 0 ;
ONABORT:
   FREE_MM(&mblock) ;
   if (ENOENT != err) {
      TRACEABORT_LOG(err) ;
   }
   return err ;
}

int delete_sysuserinfo(sysuserinfo_t ** usrinfo)
{
   int err ;

   if (*usrinfo) {
      memblock_t mblock = memblock_INIT((*usrinfo)->strsize + sizeof(sysuserinfo_t), (uint8_t*)(*usrinfo)) ;

      *usrinfo = 0 ;

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

static int test_sysusercontext(void)
{
   sysusercontext_t  sysusrctxt  = sysusercontext_INIT_FREEABLE ;
   sysusercontext_t  sysusrctxt2 = sysusercontext_INIT_FREEABLE ;

   // TEST sysusercontext_INIT_FREEABLE
   TEST(sysusrctxt.realuser       == sys_userid_INIT_FREEABLE) ;
   TEST(sysusrctxt.privilegeduser == sys_userid_INIT_FREEABLE) ;

   // TEST sysusercontext_INIT
   sysusrctxt = (sysusercontext_t) sysusercontext_INIT((sys_userid_t)5, (sys_userid_t)23) ;
   TEST(sysusrctxt.realuser       == (sys_userid_t)5) ;
   TEST(sysusrctxt.privilegeduser == (sys_userid_t)23) ;

   // TEST isequal_sysusercontext
   sysusrctxt = (sysusercontext_t) sysusercontext_INIT_FREEABLE ;
   TEST(1 == isequal_sysusercontext(&sysusrctxt, &sysusrctxt2)) ;
   for (unsigned i = 0; i < 65536; i += 255) {
      sysusrctxt.realuser = i ;
      TEST(0 == isequal_sysusercontext(&sysusrctxt, &sysusrctxt2)) ;
      sysusrctxt2.realuser = i ;
      TEST(1 == isequal_sysusercontext(&sysusrctxt, &sysusrctxt2)) ;
      sysusrctxt.privilegeduser = i + 1 ;
      TEST(0 == isequal_sysusercontext(&sysusrctxt2, &sysusrctxt)) ;
      sysusrctxt2.privilegeduser = i + 1 ;
      TEST(1 == isequal_sysusercontext(&sysusrctxt2, &sysusrctxt)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_sysuser(void)
{
   sysuser_t   sysusr = sysuser_INIT_FREEABLE ;
   sysuser_t   ruid   = sysuser_INIT_FREEABLE ;
   sysuser_t   euid   = sysuser_INIT_FREEABLE ;
   sysuser_t   suid   = sysuser_INIT_FREEABLE ;
   sysusercontext_t oldsysuser = sysuser_maincontext() ;

   // TEST sysuser_INIT_FREEABLE
   TEST((uid_t)-1 == sysusr) ;

   // TEST current_sysuser
   TEST(sysuser_INIT_FREEABLE != current_sysuser()) ;
   TEST(getuid()       == current_sysuser()) ;
   TEST(real_sysuser() == current_sysuser()) ;

   // TEST isadmin_sysuser
   TEST(true == isadmin_sysuser(0)) ;
   TEST(false == isadmin_sysuser(1)) ;
   TEST(false == isadmin_sysuser(sysuser_INIT_FREEABLE)) ;

   // TEST isequal_sysuser
   TEST(isequal_sysuser((sysuser_t)0, (sysuser_t)0)) ;
   TEST(isequal_sysuser((sysuser_t)1234, (sysuser_t)1234)) ;
   TEST(isequal_sysuser(sysuser_INIT_FREEABLE, sysuser_INIT_FREEABLE)) ;
   TEST(!isequal_sysuser(sysuser_INIT_FREEABLE, (sysuser_t)0)) ;
   TEST(!isequal_sysuser((sysuser_t)1234, sysuser_INIT_FREEABLE)) ;

   TEST(0 == getresuid(&ruid, &euid, &suid)) ;
   TEST(ruid == euid) ;
   if (suid != ruid) {  // warning is printed in test_authenticate
      // TEST freeonce_sysuser
      sysusercontext_t emptyusrctxt = sysusercontext_INIT_FREEABLE ;
      sysusercontext_t sysuserctxt  = sysuser_maincontext() ;
      TEST(0 == freeonce_sysuser(&sysuser_maincontext())) ;
      TEST(1 == isequal_sysusercontext(&sysuser_maincontext(), &emptyusrctxt)) ;
      TEST(suid == geteuid()) ;
      TEST(ruid == getuid()) ;
      TEST(0 == freeonce_sysuser(&sysuser_maincontext())) ;
      TEST(1 == isequal_sysusercontext(&sysuser_maincontext(), &emptyusrctxt)) ;
      TEST(suid == geteuid()) ;
      TEST(ruid == getuid()) ;

      // TEST initonce_sysuser
      TEST(0 == initonce_sysuser(&sysuser_maincontext())) ;
      TEST(1 == isequal_sysusercontext(&sysuserctxt, &sysuser_maincontext())) ;
      TEST(0 == getresuid(&ruid, &euid, &suid)) ;
      TEST(ruid == sysuserctxt.realuser) ;
      TEST(euid == sysuserctxt.realuser) ;
      TEST(suid == sysuserctxt.privilegeduser) ;

      // TEST initonce_sysuser: twice
      TEST(0 == initonce_sysuser(&emptyusrctxt)) ;
      TEST(1 == isequal_sysusercontext(&sysuserctxt, &emptyusrctxt)) ;
      TEST(0 == getresuid(&ruid, &euid, &suid)) ;
      TEST(ruid == sysuserctxt.realuser) ;
      TEST(euid == sysuserctxt.realuser) ;
      TEST(suid == sysuserctxt.privilegeduser) ;

      // TEST real_sysuser, privileged_sysuser
      sysuserctxt = sysuser_maincontext() ;
      for (unsigned i = 0; i < 256; ++i) {
         sysuser_maincontext().realuser       = (sys_userid_t)i ;
         sysuser_maincontext().privilegeduser = (sys_userid_t)(i+1) ;
         TEST(real_sysuser()       == (sys_userid_t)i) ;
         TEST(privileged_sysuser() == (sys_userid_t)(i+1)) ;
      }
      sysuser_maincontext() = sysuserctxt ;

      // TEST switchtoprivilege_sysuser
      TEST(real_sysuser() == current_sysuser()) ;
      TEST(0 == switchtoprivilege_sysuser()) ;
      TEST(privileged_sysuser() == getuid()) ;
      TEST(privileged_sysuser() == geteuid()) ;
      TEST(privileged_sysuser() == current_sysuser()) ;

      // TEST switchtoreal_sysuser
      TEST(privileged_sysuser() == current_sysuser()) ;
      TEST(0 == switchtoreal_sysuser()) ;
      TEST(real_sysuser() == getuid()) ;
      TEST(real_sysuser() == geteuid()) ;
      TEST(real_sysuser() == current_sysuser()) ;

      // TEST setusers_sysuser
      sysuserctxt = sysuser_maincontext() ;
      TEST(0 == switchtoprivilege_sysuser()) ;
      TEST(0 == setusers_sysuser(sysuserctxt.privilegeduser, sysuserctxt.realuser)) ;
      TEST(sysuserctxt.privilegeduser == getuid()) ;
      TEST(sysuserctxt.privilegeduser == geteuid()) ;
      TEST(false == isequal_sysusercontext(&sysuserctxt, &sysuser_maincontext())) ;
      TEST(sysuserctxt.realuser       == privileged_sysuser()) ;
      TEST(sysuserctxt.privilegeduser == real_sysuser()) ;
      TEST(0 == setusers_sysuser(sysuserctxt.realuser, sysuserctxt.privilegeduser)) ;
      TEST(sysuserctxt.realuser == getuid()) ;
      TEST(sysuserctxt.realuser == geteuid()) ;
      TEST(true == isequal_sysusercontext(&sysuserctxt, &sysuser_maincontext())) ;
      TEST(sysuserctxt.realuser       == real_sysuser()) ;
      TEST(sysuserctxt.privilegeduser == privileged_sysuser()) ;
      sysuser_maincontext() = sysuserctxt ;
   }

   return 0 ;
ONABORT:
   sysuser_maincontext() = oldsysuser ;
   return EINVAL ;
}

static int test_sysuserinfo(void)
{
   sysuserinfo_t  * usrinfo = 0 ;

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
   TEST(0 == new_sysuserinfo(&usrinfo, current_sysuser())) ;
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
   const char     * username = sysuser_UNITTEST_USERNAME ;
   const char     * password = sysuser_UNITTEST_PASSWORD ;
   sysuserinfo_t  * usrinfo[2] = { 0 } ;

   // TEST authenticate_sysuser
   TEST(0 == new_sysuserinfo(&usrinfo[0], real_sysuser())) ;
   TEST(0 == new_sysuserinfo(&usrinfo[1], privileged_sysuser())) ;
   if (  isadmin_sysuser(real_sysuser())
         || isadmin_sysuser(privileged_sysuser())
         || 0 == strcmp(name_sysuserinfo(usrinfo[0]), username)
         || 0 == strcmp(name_sysuserinfo(usrinfo[1]), username)) {
      bool isChangeUID = ( real_sysuser() != privileged_sysuser()
                           && (  isadmin_sysuser(privileged_sysuser())
                                 || 0 == strcmp(name_sysuserinfo(usrinfo[1]), username))) ;
      if (isChangeUID) {
         TEST(0 == switchtoprivilege_sysuser())
      }
      int err = authenticate_sysuser(username, password) ;
      if (isChangeUID) {
         TEST(0 == switchtoreal_sysuser())
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

int unittest_platform_sysuser()
{
   resourceusage_t   usage = resourceusage_INIT_FREEABLE ;

   if (test_authenticate(true))     goto ONABORT ;

   TEST(0 == init_resourceusage(&usage)) ;

   if (test_sysusercontext())       goto ONABORT ;
   if (test_sysuser())              goto ONABORT ;
   if (test_sysuserinfo())          goto ONABORT ;
   if (test_authenticate(false))    goto ONABORT ;

   TEST(0 == same_resourceusage(&usage)) ;
   TEST(0 == free_resourceusage(&usage)) ;

   return 0 ;
ONABORT:
   (void) free_resourceusage(&usage) ;
   return EINVAL ;
}

#endif
