/* title: SystemUser

   Offers interface for accessing current system user.

   Authentication of users is currently not supported.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
 * Export <sysuser_t> into global namespace. */
typedef struct sysuser_t sysuser_t;

/* typedef: struct sysuser_id_t
 * Make <sysuser_id_t> an alias of <sys_userid_t>. */
typedef sys_userid_t sysuser_id_t;

/* typedef: struct sysuser_info_t
 * Export <sysuser_info_t> into global namespace. */
typedef struct sysuser_info_t sysuser_info_t;


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
int unittest_platform_sysuser(void);
#endif


/* struct: sysuser_id_t
 * This type is an alias of <sys_userid_t>. */
typedef sys_userid_t sysuser_id_t;

// group: lifetime

/* define: sysuser_id_FREE
 * Static initializer. */
#define sysuser_id_FREE sys_userid_FREE

// group: query

/* function: isadmin_sysuserid
 * Returns true if this user is administrator (== root). */
bool isadmin_sysuserid(sysuser_id_t uid);

/* function: isequal_sysuserid
 * Compares two <sysuser_id_t> for equality. */
bool isequal_sysuserid(sysuser_id_t luid, sysuser_id_t ruid);


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
struct sysuser_t {
   /* variable: current
    * Contains the current user the process uses.
    * It is either set to the value of <realuser> or <privilegeduser>. */
   sysuser_id_t   current;
   /* variable: realuser
    * Contains user which started the process. */
   sysuser_id_t   realuser;
   /* variable: privilegeduser
    * Contains privileged user which is set at process creation from the system. */
   sysuser_id_t   privilegeduser;
};

// group: lifetime

/* define: sysuser_FREE
 * Static initializer. Sets user to invalid value. */
#define sysuser_FREE { sysuser_id_FREE, sysuser_id_FREE, sysuser_id_FREE }

/* function: init_sysuser
 * Initializes system user of process at process start.
 *
 * Posix (Linux):
 * On a Posix like system a process can have an effective user id which is different from the id of
 * the real user which started the process. The effectice user id is set from the system to the owner
 * of the program file if the setuid bit is set.
 *
 * This function sets the effective user id to the real user id but remembers it.
 * To get privileged rights call <switchtoprivilege_sysuser>. */
int init_sysuser(/*out*/sysuser_t* sysusr);

/* function: free_sysuser
 * Clears sysusr and resets system user ids.
 * The system user ids are set to the values before <init_sysuser> was called. */
int free_sysuser(sysuser_t* sysusr);

// group: query

/* function: isequal_sysuser
 * Returns true if lsysusr equals rsysusr. */
bool isequal_sysuser(const sysuser_t* lsysusr, const sysuser_t* rsysusr);

/* function: current_sysuser
 * Returns the current active system user. */
sysuser_id_t current_sysuser(sysuser_t* sysusr);

/* function: real_sysuser
 * Returns <sysuser_id_t> of the user which started the process. */
sysuser_id_t real_sysuser(sysuser_t* sysusr);

/* function: privileged_sysuser
 * Returns <sysuser_id_t> of the user which has other privileges.
 * If this user is equal to <real_sysuser> the process has no special privileges.
 * A privileged user is not necessarily an administrator but it can. */
sysuser_id_t privileged_sysuser(sysuser_t* sysusr);

// group: switch

/* function: switchtoprivilege_sysuser
 * Switches current user to <privileged_sysuser>.
 * See <current_sysuser>. */
int switchtoprivilege_sysuser(sysuser_t* sysusr);

/* function: switchtoreal_sysuser
 * Switches current user to <real_sysuser>.
 * See <current_sysuser>. */
int switchtoreal_sysuser(sysuser_t* sysusr);

// group: set

/* function: setusers_sysuser
 * Changes realuser and privileged user. The current user is switched to the real user.
 * See <real_sysuser> and <privileged_sysuser> and <current_sysuser>.
 * If you set privilegeduser to the same value as realuser you will give up your privileges.
 * If you want to change the user ids to arbitrary values other than realuser or privilegeduser
 * this call will only work if <current_sysuser> is admin. */
int setusers_sysuser(sysuser_t* sysusr, sysuser_id_t realuser, sysuser_id_t privilegeduser);


/* struct: sysuser_info_t
 * Stores information about a <sysuser_t>. */
struct sysuser_info_t {
   /* variable: size
    * Size in bytes of allocated memory this structure uses. */
   size_t      size;
   const char* name;
} ;

// group: lifetime

/* function: new_sysuserinfo
 * Returns information about <sysuser_id_t> given as parameter.
 * The system database is searched for an entry.
 *
 * If no one exists ENOENT is returned and no error log is written in this case. */
int new_sysuserinfo(/*out*/sysuser_info_t** usrinfo, sysuser_id_t uid);

/* function: delete_sysuserinfo
 * Frees memory holding system user information. */
int delete_sysuserinfo(sysuser_info_t** usrinfo);

// group: query

/* function: name_sysuserinfo
 * Returns user name stored in usrinfo. */
const char* name_sysuserinfo(sysuser_info_t* usrinfo);



// section: inline implementation

// group: sysuser_t

/* define: current_sysuser
 * Implement <sysuser_t.current_sysuser>. */
#define current_sysuser(sysusr)           ((sysusr)->current)

/* define: real_sysuser
 * Implement <sysuser_t.real_sysuser>. */
#define real_sysuser(sysusr)              ((sysusr)->realuser)

/* define: privileged_sysuser
 * Implement <sysuser_t.privileged_sysuser>. */
#define privileged_sysuser(sysusr)        ((sysusr)->privilegeduser)

#if !defined(KONFIG_SUBSYS_SYSUSER)
/* define: init_sysuser
 * Implement <sysuser_t.init_sysuser> as noop if !defined(KONFIG_SUBSYS_SYSUSER) */
#define init_sysuser(sysuser)             ((*(sysuser)) = (sysuser_t) sysuser_FREE, 0)
/* define: free_sysuser
 * Implement <sysuser_t.free_sysuser> as noop if !defined(KONFIG_SUBSYS_SYSUSER) */
#define free_sysuser(sysuser)             ((*(sysuser)) = (sysuser_t) sysuser_FREE, 0)
#endif

// group: sysuser_info_t

/* define: name_sysuserinfo
 * Implements <sysuser_info_t.name_sysuserinfo>. */
#define name_sysuserinfo(usrinfo)         ((usrinfo)->name)

#endif
