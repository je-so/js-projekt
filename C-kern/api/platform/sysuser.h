/* title: SystemUser

   Offers interface for accessing current system user
   and authentication of users.

   On Linux the implementation uses PAM so the
   authentication is not restricted to a local password file
   instead it can also be configured to use LDAP.

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
#ifndef CKERN_PLATFORM_SYSUSER_HEADER
#define CKERN_PLATFORM_SYSUSER_HEADER

/* typedef: struct sysuser_t
 * Make <sysuser_t> an alias of <sys_userid_t>. */
typedef sys_userid_t                   sysuser_t ;

/* typedef: struct sysuserinfo_t
 * Export <sysuserinfo_t> into global namespace. */
typedef struct sysuserinfo_t           sysuserinfo_t ;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sysuser
 * Test <sysuser_t> functionality.
 *
 * Configuration:
 * The unit test uses username="guest" and password="GUEST" to test <authenticate_sysuser> for success.
 * Configure your system either with this test account or adapt the unit test with a valid username/password combination.
 * Also this test works only if <unittest_platform_sysuser> is started as user "root" or as "guest". */
int unittest_platform_sysuser(void) ;
#endif


/* struct: sysuser_t
 * Offers functionality to manage system user.
 *
 * The current implementation supports the idea (see <initonce_sysuser>) of two users.
 *
 * * Use <real_sysuser> to get the user which started the process.
 * * Use <privileged_sysuser> to get the user which is set at process creation from the system.
 *
 * The user returned from <privileged_sysuser> has higher or special privileges so the process
 * can accomplish system administration tasks for which the real user has not enough rights.
 * */

// group: static configuration

/* define: sysuser_SYS_SERVICE_NAME
 * The name of service used during authentication. The module registers itself to the
 * underlying authentication service provider with this name.
 * The current value uses the same name as the "passwd" service.
 * This value can be overwritten in C-kern/resource/config/modulevalues. */
#define sysuser_SYS_SERVICE_NAME       "passwd"

/* define: sysuser_UNITTEST_USERNAME
 * Used in <unittest_platform_sysuser> to test <switchtoprivilege_sysuser>. */
#define sysuser_UNITTEST_USERNAME      "guest"

/* define: sysuser_UNITTEST_PASSWORD
 * Used in <unittest_platform_sysuser> to test <switchtoprivilege_sysuser>. */
#define sysuser_UNITTEST_PASSWORD      "GUEST"

// group: initonce

/* function: initonce_sysuser
 * Initializes system user of process at process start.
 *
 * Posix (Linux):
 * On Posix like system a process can have an effective user id which is different from the real
 * user id which started the process. The effectice user id is set from the system to the owner
 * of the program file if the setuid bit is set.
 *
 * This function sets the effective user id to the real user id but remembers it.
 * You can later switch between the two.
 * */
int initonce_sysuser(/*out*/sysusercontext_t * sysuser) ;

/* function: freeonce_sysuser
 * Does nothing. Called from <free_maincontext>. */
int freeonce_sysuser(sysusercontext_t * sysuser) ;

// group: lifetime

/* define: sysuser_INIT_FREEABLE
 * Static initializer. Sets user to invalid value. */
#define sysuser_INIT_FREEABLE          sys_userid_INIT_FREEABLE

// group: query

/* function: current_sysuser
 * Returns the current active system user. */
sysuser_t current_sysuser(void) ;

/* function: isadmin_sysuser
 * Returns true if this user is administrator (root). */
bool isadmin_sysuser(sysuser_t sysusr) ;

/* function: isequal_sysuser
 * Compares two <sysuser_t> for equality. */
bool isequal_sysuser(sysuser_t left, sysuser_t right) ;

/* function: real_sysuser
 * Returns <sysuser_t> which started the process. */
sysuser_t real_sysuser(void) ;

/* function: privileged_sysuser
 * Returns <sysuser_t> which has special system privileges.
 * If this user is equal to <real_sysuser> the process has no special privileges.
 * A privileged user is not necessarily an administrator but it can. */
sysuser_t privileged_sysuser(void) ;

// group: switch

/* function: switchtoprivilege_sysuser
 * Switches current user <current_sysuser> to <privileged_sysuser>. */
int switchtoprivilege_sysuser(void) ;

/* function: switchtoreal_sysuser
 * Switches current user <current_sysuser> to <real_sysuser>. */
int switchtoreal_sysuser(void) ;

// group: set

/* function: setusers_sysuser
 * Changes realuser and privileged user. See <real_sysuser> and <privileged_sysuser>.
 * Also <switchtoreal_sysuser> is called. If you set privilegeduser to the same value as realuser
 * you will give up your privileges.
 * If you want to change to an arbitrarily user this call will work only if <current_sysuser> is admin. */
int setusers_sysuser(sysuser_t realuser, sysuser_t privilegeduser) ;

// group: authentication

/* function: authenticate_sysuser
 * Checks that a certain user / password combination is valid.
 *
 * Return codes:
 * 0               - Success
 * EACCES          - User unknown or password wrong.
 * ENOMEM          - Buffer allocation failed.
 * ERANGE          - Number of tries of wrong authentication reqeusts reached.
 * EPERM           - Not enough rights to authenticate user.
 * ENOTRECOVERABLE - Caller should exit process after this value has been returned.
 * ELIBACC         - Shared system library not found.
 *
 * Some underlying implementations are returns EACCES in case of EPERM or other error codes.
 *
 * Uses authentication service of the operating system to authenticate system users.
 * This means loading shated libraries into the running process. It is best to shield the caller by
 * spawing a child process which does the authentication (see <process_t>).
 * Only username / password combinations can be authenticated which are known to the underlying operating system.
 * The user of the running process is not changed. To authenticate other users
 * than the calling user the running process needs to have spedial rights.
 * On Linux it should be running as root.
 *
 * Linux specific:
 * The configuration file for PAM is stored in /etc/pam.d/service_name alternatively in /etc/pam.conf.
 * The fallback to /etc/pam.d/other is made if no service with this name exists. Currently the service name
 * "passwd" (see <sysuser_SYS_SERVICE_NAME>) is used therefore the configuration file /etc/pam.d/passwd
 * is used which is the configuration of the passwd command to change the own password.
 * This configuration should always exist. */
int authenticate_sysuser(const char * username, const char * password) ;


/* struct: sysuserinfo_t
 * Stores information about a <sysuser_t>. */
struct sysuserinfo_t {
   size_t      strsize ;
   const char  * name ;
} ;

// group: lifetime

/* function: new_sysuserinfo
 * Returns information about <sysuser_t> given as parameter.
 * The system database is searched for an entry.
 *
 * If no one exists ENOENT is returned and no error log is written in this case. */
int new_sysuserinfo(/*out*/sysuserinfo_t ** usrinfo, sysuser_t sysusr) ;

/* function: delete_sysuserinfo
 * Frees memory holding system user information. */
int delete_sysuserinfo(sysuserinfo_t ** usrinfo) ;

// group: query

/* function: name_sysuserinfo
 * Returns name of a <sysuser_t>. */
const char * name_sysuserinfo(sysuserinfo_t * usrinfo) ;


// section: inline implementation

/* define: isequal_sysuser
 * Implements <sysuser_t.isequal_sysuser>. */
#define isequal_sysuser(left, right)      ( __extension__ ({ sysuser_t _left = (left), _right = (right) ; _left == _right ; }) )

/* define: name_sysuserinfo
 * Implements <sysuser_t.name_sysuserinfo>. */
#define name_sysuserinfo(usrinfo)         ((usrinfo)->name)

/* define: real_sysuser
 * Implements <sysuser_t.real_sysuser>. */
#define real_sysuser()                    (sysuser_maincontext().realuser)

/* define: privileged_sysuser
 * Implements <sysuser_t.privileged_sysuser>. */
#define privileged_sysuser()              (sysuser_maincontext().privilegeduser)

#define SYSUSER 1
#if (!((KONFIG_SUBSYS)&SYSUSER))
/* define: initonce_sysuser
 * Implement <thread_t.initonce_sysuser> as noop if !((KONFIG_SUBSYS)&SYSUSER) */
#define initonce_sysuser(sysuser)         (0)
/* define: freeonce_sysuser
 * Implement <thread_t.freeonce_sysuser> as noop if !((KONFIG_SUBSYS)&SYSUSER) */
#define freeonce_sysuser(sysuser)         (0)
#endif
#undef SYSUSER

#endif
