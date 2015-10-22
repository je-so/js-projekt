/* title: LinuxSystemContext

   Defines <sys_context_threadlocalstore> and other thread context specific functions.
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

// forward
struct thread_t;
struct threadcontext_t;

/* typedef: struct syscontext_t
 * Export <syscontext_t>. */
typedef struct syscontext_t syscontext_t;



/* struct: syscontext_t
 * Defines system specific information stored in <maincontext_t>.
 * Initialization is done in module <PlatformInit>. */
struct syscontext_t {
   // group: public
   /* variable: pagesize_vm
    * The size of a virtual memory page in bytes.
    * Same value as returned by <sys_pagesize_vm>.
    * This value can be queried with function pagesize_vm. */
   uint32_t       pagesize_vm;
   /* variable: log2pagesize_vm
    * The <log2_int> value of <pagesize_vm>. */
   uint8_t        log2pagesize_vm;
};

// group: lifetime

#define syscontext_FREE \
         { 0, 0 }

// group: query

/* function: isfree_syscontext
 * Returns true in case syscontext_t is set to <syscontext_FREE>. */
static inline int isfree_syscontext(const syscontext_t * scontext);



// struct: thread_localstore_t
struct thread_localstore_t;

// group: query

/* function: sys_context_threadlocalstore
 * Returns <threadcontext_t> of the current thread.
 * This function is called from all other functions using <threadcontext_t>.
 * The returned pointer is calculated relative to the address of the current stack. */
struct threadcontext_t* sys_context_threadlocalstore(void);

/* function: sys_thread_threadlocalstore
 * Returns current <thread_t> object. */
struct thread_t* sys_thread_threadlocalstore(void);

/* function: sys_size_threadlocalstore
 * Returns the size in bytes of the thread local storage.
 * This size is reserved for every created thread and the main thread.
 * The size includes the stack and the signal stack size. */
size_t sys_size_threadlocalstore(void);

/* function: sys_self_threadlocalstore
 * Returns <thread_localstore_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack.
 * This function is called from <sys_context_threadlocalstore>. */
struct thread_localstore_t* sys_self_threadlocalstore(void);

/* function: sys_self2_threadlocalstore
 * Returns <thread_localstore_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack.
 * This function is called from <sys_self_threadlocalstore>. */
struct thread_localstore_t* sys_self2_threadlocalstore(void * local_var);


// section: inline implementation

// group: syscontext_t

/* define: isfree_syscontext
 * Implements <syscontext_t.isfree_syscontext>. */
static inline int isfree_syscontext(const syscontext_t * scontext)
{
   return 0 == scontext->pagesize_vm
         && 0 == scontext->log2pagesize_vm;
}

// group: thread_localstore_t

/* define: sys_context_threadlocalstore
 * Implements <thread_localstore_t.sys_context_threadlocalstore>. */
#define sys_context_threadlocalstore() \
         ((struct threadcontext_t*) sys_self_threadlocalstore())

/* define: sys_size_threadlocalstore
 * Implements <thread_localstore_t.sys_size_threadlocalstore>. */
#define sys_size_threadlocalstore() \
         (512*1024)

/* define: sys_self_threadlocalstore
 * Implements <thread_localstore_t.sys_self_threadlocalstore>. */
#define sys_self_threadlocalstore() \
         ( __extension__ ({               \
            int _addr;                    \
            sys_self2_threadlocalstore(&_addr);  \
         }))

/* define: sys_self2_threadlocalstore
 * Implements <thread_localstore_t.sys_self2_threadlocalstore>. */
#define sys_self2_threadlocalstore(local_var) \
         ((struct thread_localstore_t*) ( (uintptr_t)(local_var) & ~(uintptr_t) (sys_size_threadlocalstore()-1)))

/* define: sys_thread_threadlocalstore
 * Implements <thread_localstore_t.sys_thread_threadlocalstore>. */
#define sys_thread_threadlocalstore() \
         ((struct thread_t*) ( ((uint8_t*)sys_self_threadlocalstore()) + sizeof(threadcontext_t)))

#endif
