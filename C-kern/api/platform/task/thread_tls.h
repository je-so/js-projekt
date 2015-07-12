/* title: ThreadLocalStorage

   Supports storage (variables and stack space)
   for every creatd thread and the main thread.
   The main thread is initialized with <newmain_threadtls>
   all other with <new_threadtls>.

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
struct thread_tls_t;

// group: lifetime

/* function: new_threadtls
 * Allocates a memory block big enoug to hold all thread local storage data.
 * The access rights of parts of the memory block is changed to protect
 * the stack from overflowing.
 * The allocated memory block is aligned to its own size.
 * The thread local variables (<threadcontext_t> and <thread_t>) are initialized
 * with their INIT_STATIC resp. INIT_FREEABLE values. */
int new_threadtls(/*out*/thread_tls_t** tls, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack);

/* function: delete_threadtls
 * Changes protection of memory to normal and frees it. */
int delete_threadtls(thread_tls_t** tls);

/* function: newmain_threadtls
 * Same as <new_threadtls> but calls no other functions of C-kern system.
 * Called from <platform_t.init_platform>.
 * Especially no logging is done and no calls to <pagesize_vm> and <initaligned_vmpage> are made. */
int newmain_threadtls(/*out*/thread_tls_t** tls, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack);

/* function: deletemain_threadtls
 * Same as <delete_threadtls> but calls no other functions of C-kern system.
 * Especially no logging is done and no calls to <pagesize_vm> and <free_vmpage> are made. */
int deletemain_threadtls(thread_tls_t** tls);

// group: query

/* function: castPthread_threadtls
 * Calculates address of <thread_tls_t> from address of <thread_t>. */
thread_tls_t* castPthread_threadtls(struct thread_t* thread);

/* function: self_threadtls
 * Returns <thread_tls_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack. */
thread_tls_t* self_threadtls(void);

/* function: context_threadtls
 * Returns pointer to <threadcontext_t> stored in thread local storage.
 * The function <sys_context_threadtls> is identical with context_threadtls(self_threadtls()).
 * If you change this function change sys_context_threadtls also. */
struct threadcontext_t* context_threadtls(thread_tls_t* tls);

/* function: thread_threadtls
 * Returns pointer to <thread_t> stored in thread local storage.
 * The function <sys_thread_threadtls> is identical with thread_threadtls(self_threadtls()).
 * If you change this function change sys_thread_threadtls also. */
struct thread_t* thread_threadtls(thread_tls_t* tls);

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
void signalstack_threadtls(thread_tls_t* tls, /*out*/struct memblock_t * stackmem);

/* function: threadstack_threadtls
 * Returns in stackmem the thread stack from tls.
 * If tls is in a freed state stackmem is set to <memblock_FREE>. */
void threadstack_threadtls(thread_tls_t* tls, /*out*/struct memblock_t * stackmem);

// group: static-memory

/* function: allocstatic_threadtls
 * Allokiert einen über die Laufzeit des Threads gültigen Speicherblock.
 * Der allokierte Speicherblock wird in memblock zurückgegeben und ist nicht minder
 * als bytesize Bytes groß. */
int allocstatic_threadtls(thread_tls_t* tls, size_t bytesize, /*out*/struct memblock_t* memblock);

/* function: freestatic_threadtls
 * Gibt den zuletzt allokierten Speicherblock wieder frei.
 * Mehrere Speicherblöcke können auch zusammen auf einmal freigegeben werden.
 *
 * Diese Funktion ist eher zum Testen gedacht, da statischer Speicher über die
 * Laufzeit des gesamten Threads gültig zu sein hat.
 *
 * Returns
 * 0      - OK
 * EINVAL - memblock ist nicht der zuletzt von <allocstatic_threadtls> allokierte Speicherblock. */
int freestatic_threadtls(thread_tls_t* tls, struct memblock_t* memblock);

/* function: sizestatic_threadtls
 * Gibt die aufgerundete Anzahl allokierter Bytes an statischem Speicher zurück. */
size_t sizestatic_threadtls(const thread_tls_t* tls);


// section: inline implementation

/* define: castPthread_threadtls
 * Implements <thread_tls_t.castPthread_threadtls>. */
#define castPthread_threadtls(thread) \
         ( __extension__ ({                                                  \
            thread_t* _t = (thread);                                         \
            ((thread_tls_t*) ( ((uint8_t*)(_t)) - sizeof(threadcontext_t))); \
         }))

/* define: context_threadtls
 * Implements <thread_tls_t.context_threadtls>. */
#define context_threadtls(tls) \
         ((threadcontext_t*) (tls))

/* define: self_threadtls
 * Implements <thread_tls_t.self_threadtls>. */
#define self_threadtls() \
         (sys_self_threadtls())

/* define: thread_threadtls
 * Implements <thread_tls_t.thread_threadtls>. */
#define thread_threadtls(tls) \
         ( __extension__ ({                                                \
            thread_tls_t* _tls = (tls);                                    \
            ((thread_t*) ( ((uint8_t*)(_tls)) + sizeof(threadcontext_t))); \
         }))

/* define: size_threadtls
 * Implements <thread_tls_t.size_threadtls>. */
#define size_threadtls() \
         (sys_size_threadtls())

#endif
