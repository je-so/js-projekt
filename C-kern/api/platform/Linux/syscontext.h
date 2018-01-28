/* title: LinuxSystemContext

   Defines <syscontext_t.sys_tcontext_syscontext> and other thread context specific functions.
   The defined functions are implemented inline to speed up performance.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/platform/Linux/syscontext.h
    Header file <LinuxSystemContext>.

   file: C-kern/konfig.h
    Included from header <Konfiguration>.
*/
#ifndef CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER
#define CKERN_PLATFORM_LINUX_SYSCONTEXT_HEADER

// import
struct threadcontext_t;

// === exported types
struct syscontext_t;


/* struct: syscontext_t
 * Defines system specific information stored in <maincontext_t>.
 * Initialization is done in module <PlatformInit>. */
typedef struct syscontext_t {
   // group: public
   /* variable: pagesize_vm
    * The size of a virtual memory page in bytes.
    * Same value as returned by <sys_pagesize_vm>.
    * This value can be queried with function pagesize_vm. */
   size_t         pagesize_vm;
   /* variable: log2pagesize_vm
    * The <log2_int> value of <pagesize_vm>. */
   uint8_t        log2pagesize_vm;
} syscontext_t;

// group: lifetime

#define syscontext_FREE \
         { 0, 0 }

// group: query

/* function: isfree_syscontext
 * Returns true in case syscontext_t is set to <syscontext_FREE>. */
static inline int isfree_syscontext(const syscontext_t * scontext);

/* function: sys_tlssize_syscontext
 * Returns the size in bytes of the thread local storage.
 * This size is reserved for every created thread and the main thread.
 * The size includes the stack and the signal stack size. */
size_t sys_tlssize_syscontext(void);

/* function: sys_tcontext_syscontext
 * Returns <threadcontext_t> of the current thread. */
struct threadcontext_t* sys_tcontext_syscontext(void);

/* function: sys_tcontext2_syscontext
 * Returns <threadcontext_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack.
 * This function is called from <sys_tcontext_syscontext>. */
struct threadcontext_t* sys_tcontext2_syscontext(void * local_var);


// section: inline implementation

// group: syscontext_t

/* define: isfree_syscontext
 * Implements <syscontext_t.isfree_syscontext>. */
static inline int isfree_syscontext(const syscontext_t * scontext)
{
         return   0 == scontext->pagesize_vm
                  && 0 == scontext->log2pagesize_vm;
}

/* define: sys_tcontext_syscontext
 * Implements <syscontext_t.sys_tcontext_syscontext>. */
#define sys_tcontext_syscontext() \
         ( __extension__ ({                     \
            int _addr;                          \
            sys_tcontext2_syscontext(&_addr);   \
         }))

/* define: sys_tcontext2_syscontext
 * Implements <syscontext_t.sys_tcontext2_syscontext>. */
#define sys_tcontext2_syscontext(local_var) \
         ((struct threadcontext_t*) ( (uintptr_t)(local_var) & ~(uintptr_t) (sys_tlssize_syscontext()-1)))

/* define: sys_tlssize_syscontext
 * Implements <syscontext_t.sys_tlssize_syscontext>. */
#define sys_tlssize_syscontext() \
         (512*1024)

#endif
