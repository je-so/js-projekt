/* title: LinuxSystemContext

   Defines <syscontext_t.context_syscontext> and other thread context specific functions.
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


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_syscontext
 * Validates initialization of system context. */
int unittest_platform_syscontext(void);
#endif



/* struct: syscontext_t
 * Defines system specific information stored in <maincontext_t>.
 * Initialization is done during initialization of <maincontext_t>. */
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

/* function: free_syscontext
 * Queries system specific variables and writes the obtained values into scontext. */
int init_syscontext(/*out*/syscontext_t* scontext);

/* function: free_syscontext
 * Resets scontext to <syscontext_FREE>. Nothing else is done. */
static inline int free_syscontext(syscontext_t* scontext);

// group: query

/* function: isfree_syscontext
 * Returns true in case syscontext_t is set to <syscontext_FREE>. */
int isfree_syscontext(const syscontext_t* scontext);

/* function: isvalid_syscontext
 * Returns true in case syscontext_t is initialized with valid values. */
int isvalid_syscontext(const syscontext_t* scontext);

/* function: stacksize_syscontext
 * Returns the size in bytes of the thread local storage.
 * This size is reserved for every created thread and the main thread.
 * The size includes the stack and the signal stack size. */
size_t stacksize_syscontext(void);

/* function: context_syscontext
 * Returns <threadcontext_t> of the current thread. */
struct threadcontext_t* context_syscontext(void);

/* function: context2_syscontext
 * Returns <threadcontext_t> of the current thread.
 * The parameter addr must point to a local variable on the current stack.
 * This function is called from <context_syscontext>. */
struct threadcontext_t* context2_syscontext(void* addr);


// section: inline implementation

// group: syscontext_t

/* define: free_syscontext
 * Implements <syscontext_t.free_syscontext>. */
static inline int free_syscontext(syscontext_t* scontext)
{
         *scontext = (syscontext_t) syscontext_FREE;
         return 0;
}

/* define: context_syscontext
 * Implements <syscontext_t.context_syscontext>. */
#define context_syscontext() \
         ( __extension__ ({                     \
            int _addr;                          \
            context2_syscontext(&_addr);   \
         }))

/* define: context2_syscontext
 * Implements <syscontext_t.context2_syscontext>. */
#define context2_syscontext(addr) \
         ((struct threadcontext_t*) ( (uintptr_t)(addr) & ~(uintptr_t) (stacksize_syscontext()-1)))

/* define: stacksize_syscontext
 * Implements <syscontext_t.stacksize_syscontext>. */
#define stacksize_syscontext() \
         (512*1024)

#endif
