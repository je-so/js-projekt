/* title: ThreadLocalStorage

   Supports storage (variables and stack space)
   for every creatd thread and the main thread.
   The main thread is initialized with <initmain_threadtls>
   all other with <init_threadtls>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/task/thread_tls.h
    Header file <ThreadLocalStorage>.

   file: C-kern/platform/Linux/task/thread_tls.c
    Implementation file <ThreadLocalStorage impl>.
*/
#ifndef CKERN_PLATFORM_TASK_THREAD_TLS_HEADER
#define CKERN_PLATFORM_TASK_THREAD_TLS_HEADER

// forward
struct memblock_t;
struct thread_t;
struct threadcontext_t;

/* typedef: struct thread_tls_t
 * Export <thread_tls_t> into global namespace. */
typedef struct thread_tls_t thread_tls_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_thread_tls
 * Test <thread_tls_t> functionality. */
int unittest_platform_task_thread_tls(void);
#endif


/* struct: thread_tls_t
 * Holds thread local memory.
 * The memory comprises the variables <thread_t> and <threadcontext_t>,
 * the signal stack and thread stack and 3 protection pages in between. */
struct thread_tls_t {
   uint8_t * addr;
};

// group: lifetime

/* define: thread_tls_FREE
 * Static initializer. */
#define thread_tls_FREE { 0 }

/* function: init_threadtls
 * Allocates a memory block big enoug to hold all thread local storage data.
 * The access rights of parts of the memory block is changed to protect
 * the stack from overflowing.
 * The allocated memory block is aligned to its own size.
 * The thread local variables (<threadcontext_t> and <thread_t>) are initialized
 * with their INIT_STATIC resp. INIT_FREEABLE values. */
int init_threadtls(/*out*/thread_tls_t * tls);

/* function: free_threadtls
 * Changes protection of memory to normal and frees it. */
int free_threadtls(thread_tls_t * tls);

/* function: initmain_threadtls
 * Same as <init_threadtls> but calls no other functions of C-kern system.
 * Called from <platform_t.init_platform>.
 * Especially no logging is done and no calls to <pagesize_vm> and <initaligned_vmpage> are made. */
int initmain_threadtls(/*out*/thread_tls_t * tls, /*out*/struct memblock_t * threadstack, /*out*/struct memblock_t * signalstack);

/* function: freemain_threadtls
 * Same as <free_threadtls> but calls no other functions of C-kern system.
 * Especially no logging is done and no calls to <pagesize_vm> and <free_vmpage> are made. */
int freemain_threadtls(thread_tls_t * tls);

// group: query

/* function: current_threadtls
 * Returns <thread_tls_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack.
 * The function <sys_context_thread> is identical with context_threadtls(&current_threadtls(&err)).
 * If you change this function change sys_context_thread (and self_thread) also. */
thread_tls_t current_threadtls(void * local_var);

/* function: context_threadtls
 * Returns pointer to <threadcontext_t> stored in thread local storage.
 * The function <sys_context_thread> is identical with context_threadtls(&current_threadtls()).
 * If you change this function change sys_context_thread also. */
struct threadcontext_t * context_threadtls(const thread_tls_t * tls);

/* function: thread_threadtls
 * Returns pointer to <thread_t> stored in thread local storage.
 * The function <self_thread> is identical with thread_threadtls(&current_threadtls()).
 * If you change this function change self_thread also. */
struct thread_t * thread_threadtls(const thread_tls_t * tls);

/* function: size_threadtls
 * Returns the size of the allocated memory block. */
size_t size_threadtls(void);

/* function: signalstack_threadtls
 * Returns in stackmem the signalstack from tls.
 * If tls is in a freed state stackmem is set to <memblock_FREE>.
 * The signal stack is used in case of a signal (exceptions).
 * For example if the thread stack overflows SIGSEGV signal is thrown.
 * To handle this case the system must have an extra signal stack
 * cause signal handling needs stack space. */
void signalstack_threadtls(const thread_tls_t * tls, /*out*/struct memblock_t * stackmem);

/* function: threadstack_threadtls
 * Returns in stackmem the thread stack from tls.
 * If tls is in a freed state stackmem is set to <memblock_FREE>. */
void threadstack_threadtls(const thread_tls_t * tls, /*out*/struct memblock_t * stackmem);

// group: generic

/* function: cast_threadtls
 * Casts pointer to an object into pointer to <thread_tls_t>.
 * The object must have a single member with name nameprefix##addr
 * of the same type as <thread_tls_t>. */
thread_tls_t * cast_threadtls(void * obj, IDNAME nameprefix);



// section: inline implementation

/* define: context_threadtls
 * Implements <thread_tls_t.context_threadtls>. */
#define context_threadtls(tls)            \
         ((threadcontext_t*) (            \
               (tls)->addr                \
         ))

/* define: current_threadtls
 * Implements <thread_tls_t.current_threadtls>. */
#define current_threadtls(local_var)      \
         (thread_tls_t) {                 \
            (uint8_t*) (                  \
               (uintptr_t)(local_var)     \
               & ~(uintptr_t)             \
                  (size_threadtls()-1)    \
            )                             \
         }

/* define: cast_threadtls
 * Implements <thread_tls_t.cast_threadtls>. */
#define cast_threadtls(obj, nameprefix) \
         ( __extension__ ({                         \
            typeof(obj) _o;                         \
            _o = (obj);                             \
            static_assert(                          \
               &(_o->nameprefix##addr)              \
               == &((thread_tls_t *)                \
                    &(_o->nameprefix##addr))->addr, \
               "compatible struct");                \
            (thread_tls_t *)                        \
             &(_o->nameprefix##addr);               \
         }))

/* define: thread_threadtls
 * Implements <thread_tls_t.thread_threadtls>. */
#define thread_threadtls(tls) \
         ((thread_t*) (               \
            (tls)->addr               \
            + sizeof(threadcontext_t) \
         ))

/* define: size_threadtls
 * Implements <thread_tls_t.size_threadtls>. */
#define size_threadtls() \
         (sys_size_threadtls())

#endif
