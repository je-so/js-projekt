/* title: SystemLogin

   Offers interface for accessing current system user
   and switching privilehge level if setuid (set-user-id)
   bit or program is set.

   Authentication of users is currently not supported.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/platform/syslogin.h
    Header file <SystemLogin>.

   file: C-kern/platform/Linux/syslogin.c
    Implementation file <SystemLogin Linuximpl>.
*/
#ifndef CKERN_PLATFORM_SYSLOGIN_HEADER
#define CKERN_PLATFORM_SYSLOGIN_HEADER

/* typedef: struct syslogin_t
 * Export <syslogin_t> into global namespace. */
typedef struct syslogin_t syslogin_t;

/* typedef: struct sys_userid_t
 * Make <sys_userid_t> an alias of <sys_userid_t>. */
typedef sys_userid_t sys_userid_t;

/* typedef: struct syslogin_info_t
 * Export <syslogin_info_t> into global namespace. */
typedef struct syslogin_info_t syslogin_info_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_syslogin
 * Test <syslogin_t> functionality.
 * This test needs setuid bit set to work properly. */
int unittest_platform_syslogin(void);
#endif


/* struct: syslogin_t
 * Offers functionality to manage system login user.
 *
 * The current implementation supports the idea of two users.
 *
 * - Use <realuser_syslogin> to get the user which started the process.
 * - Use <privilegeduser_syslogin> to get the user which is set at process creation from the system.
 *
 * The user returned from <privilegeduser_syslogin> has higher or special privileges so the process
 * can accomplish system administration tasks for which the real user has not enough rights.
 *
 * Achtung:
 * Ein Prozess kann Signale von anderen Prozessen gesemdet bekommen (auch SIGKILL),
 * wenn die reale oder effektive UID mit der realen bzw. saved-setUID übereinstimmt.
 * Die saved-setUID merkt sich die privilegierte UID, falls der Prozess mittels <switchtorealuser_syslogin>
 * seine Privilegien abgibt, damit ein späterer Aufruf von <switchtoprivilegeduser_syslogin> funktioniert.
 * */
struct syslogin_t {
   /* variable: current
    * Contains the current user the process uses.
    * It is either set to the value of <realuser> or <privilegeduser>. */
   sys_userid_t   currentuser;
   /* variable: realuser
    * Contains user which started the process. */
   sys_userid_t   realuser;
   /* variable: privilegeduser
    * Contains privileged user which is set at process creation from the system. */
   sys_userid_t   privilegeduser;
};

// group: lifetime

/* define: syslogin_FREE
 * Static initializer. Sets user to invalid value. */
#define syslogin_FREE \
         { sys_userid_FREE, sys_userid_FREE, sys_userid_FREE }

/* function: init_syslogin
 * Initializes system user of process at process start.
 *
 * Posix (Linux):
 * On a Posix like system a process can have an effective user id which is different from the id of
 * the real user which started the process. The effectice user id is set from the system to the owner
 * of the program file if the setuid bit is set.
 *
 * This function sets the effective user id to the real user id but remembers it.
 * To get privileged rights call <switchtoprivilegeduser_syslogin>. */
int init_syslogin(/*out*/syslogin_t* syslogin);

/* function: free_syslogin
 * Clears syslogin and resets system user ids.
 * The system user ids are set to the values before <init_syslogin> was called. */
int free_syslogin(syslogin_t* syslogin);

// group: query

/* function: issuperuser_syslogin
 * Returns true if this user has super user (root) rights. */
bool issuperuser_syslogin(sys_userid_t uid);

/* function: isidequal_syslogin
 * Compares two <sys_userid_t> for equality. */
bool isidequal_syslogin(sys_userid_t luid, sys_userid_t ruid);

/* function: isequal_syslogin
 * Returns true if lsyslogin equals rsyslogin. */
bool isequal_syslogin(const syslogin_t* lsyslogin, const syslogin_t* rsyslogin);

/* function: currentuser_syslogin
 * Returns the current active system user. */
sys_userid_t currentuser_syslogin(syslogin_t* syslogin);

/* function: realuser_syslogin
 * Returns <sys_userid_t> of the user which started the process. */
sys_userid_t realuser_syslogin(syslogin_t* syslogin);

/* function: privilegeduser_syslogin
 * Returns <sys_userid_t> of the user which has other privileges.
 * If this user is equal to <realuser_syslogin> the process has no special privileges.
 * A privileged user is not necessarily a superuser or administrator. */
sys_userid_t privilegeduser_syslogin(syslogin_t* syslogin);

/* function: groups_syslogin
 * Gibt supplementary group IDs zurück.
 * Dieser Wert wird bei Programmstart dem Prozess zugeordnet.
 * Falls capacity == 0 wird in size nur die Länge der Liste der GruppenIDs zurückgegeben.
 * Falls capacity > 0, jedoch kleiner der Länge der Liste, wird der Fehler ENOBUFS zurückgegeben.
 * Nach erfolgreicher Ausführung sind alle Werte in grouplist von [0..*size-1] gültig und size ist auf die Länge der Liste gesetzt. */
int groups_syslogin(size_t capacity, /*out*/sys_groupid_t grouplist[capacity], /*out*/size_t* size);


// group: switch

/* function: switchtoprivilegeduser_syslogin
 * Switches current user to <privilegeduser_syslogin>.
 * See <currentuser_syslogin>. */
int switchtoprivilegeduser_syslogin(syslogin_t* syslogin);

/* function: switchtorealuser_syslogin
 * Switches current user to <realuser_syslogin>.
 * See <currentuser_syslogin>. */
int switchtorealuser_syslogin(syslogin_t* syslogin);

/* function: switchpermanent_syslogin
 * Changes realuser, current and privileged user to uid.
 * For an unprivileged process parameter uid must be equal to <realuser_syslogin> or <privilegeduser_syslogin>.
 * A privileged process (<issuperuser_syslogin> returns true) is allowed to use any valid value for uid. */
int switchpermanent_syslogin(syslogin_t* syslogin, sys_userid_t uid);


/* struct: syslogin_info_t
 * Stores information about a <sys_userid_t>. */
struct syslogin_info_t {
   /* variable: size
    * Size in bytes of allocated memory this structure uses. */
   size_t        size;
   sys_userid_t  uid;
   size_t        nrgroups;
   size_t        gmain; // index into gname/gid; denotes the main group of the user
   const char*    uname;
   const char**   gname/*[nrgroups]*/;
   sys_groupid_t* gid/*[nrgroups]*/;
};

// group: lifetime

/* function: new_syslogininfo
 * Returns information about uid (see <sys_userid_t>).
 * The system user and group database is searched for information.
 *
 * If no user wirh uid exists ENOENT is returned and no error log is written in this case. */
int new_syslogininfo(/*out*/syslogin_info_t** info, sys_userid_t uid);

/* function: delete_syslogininfo
 * Frees memory holding system user information. */
int delete_syslogininfo(syslogin_info_t** info);

// group: query

/* function: username_syslogininfo
 * Returns user name stored in info. */
const char* username_syslogininfo(syslogin_info_t* info);



// section: inline implementation

// group: syslogin_t

/* define: currentuser_syslogin
 * Implement <syslogin_t.currentuser_syslogin>. */
#define currentuser_syslogin(syslogin) \
         ((syslogin)->currentuser)

/* define: isidequal_syslogin
 * Implement <syslogin_t.isidequal_syslogin>. */
#define isidequal_syslogin(luid, ruid) \
         ((luid) == (ruid))

/* define: realuser_syslogin
 * Implement <syslogin_t.realuser_syslogin>. */
#define realuser_syslogin(syslogin) \
         ((syslogin)->realuser)

/* define: privilegeduser_syslogin
 * Implement <syslogin_t.privilegeduser_syslogin>. */
#define privilegeduser_syslogin(syslogin) \
         ((syslogin)->privilegeduser)

#if !defined(KONFIG_SUBSYS_SYSLOGIN)
/* define: init_syslogin
 * Implement <syslogin_t.init_syslogin> as noop if !defined(KONFIG_SUBSYS_SYSLOGIN) */
#define init_syslogin(syslogin) \
         ((*(syslogin)) = (syslogin_t) syslogin_FREE, 0)

/* define: free_syslogin
 * Implement <syslogin_t.free_syslogin> as noop if !defined(KONFIG_SUBSYS_SYSLOGIN) */
#define free_syslogin(syslogin) \
         ((*(syslogin)) = (syslogin_t) syslogin_FREE, 0)
#endif

// group: syslogin_info_t

/* define: username_syslogininfo
 * Implements <syslogin_info_t.username_syslogininfo>. */
#define username_syslogininfo(info) \
         ((info)->uname)

#endif
