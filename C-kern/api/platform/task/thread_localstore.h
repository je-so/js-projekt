/* title: ThreadLocalStorage

   Supports storage (variables and stack space)
   for every creatd thread and the main thread.
   The main thread is initialized with <newmain_threadlocalstore>
   all other with <new_threadlocalstore>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/task/thread_localstore.h
    Header file <ThreadLocalStorage>.

   file: C-kern/platform/Linux/task/thread_localstore.c
    Implementation file <ThreadLocalStorage impl>.
*/
#ifndef CKERN_PLATFORM_TASK_THREAD_LOCALSTORE_HEADER
#define CKERN_PLATFORM_TASK_THREAD_LOCALSTORE_HEADER

// forward
struct memblock_t;
struct thread_t;
struct threadcontext_t;
struct logwriter_t;

/* typedef: struct thread_localstore_t
 * Export <thread_localstore_t> into global namespace. */
typedef struct thread_localstore_t thread_localstore_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_thread_localstore
 * Test <thread_localstore_t> functionality. */
int unittest_platform_task_thread_localstore(void);
#endif


/* struct: thread_localstore_t
 * Holds thread local memory.
 * The memory comprises the variables <thread_t> and <threadcontext_t>,
 * the signal stack and thread stack and 3 protection pages in between. */
struct thread_localstore_t;

// group: lifetime

/* function: new_threadlocalstore
 * Allocates a memory block big enoug to hold all thread local storage data.
 * The access rights of parts of the memory block is changed to protect
 * the stack from overflowing.
 * The allocated memory block is aligned to its own size.
 * The thread local variables (<threadcontext_t> and <thread_t>) are initialized
 * with their INIT_STATIC resp. INIT_FREEABLE values.
 * pagesize should be set to <pagesize_vm> (or <sys_pagesize_vm> during initialization).
 * Called from <syscontext_t.initrun_syscontext> therefore initlog is used if needed. */
int new_threadlocalstore(/*out*/thread_localstore_t** tls, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack, size_t pagesize);

/* function: delete_threadlocalstore
 * Changes protection of memory to normal and frees it.
 * Called from <syscontext_t.initrun_syscontext> therefore initlog is used if needed. */
int delete_threadlocalstore(thread_localstore_t** tls);

// group: query

/* function: castPcontext_threadlocalstore
 * Calculates address of <thread_localstore_t> from address of contained <threadcontext_t>. */
thread_localstore_t* castPcontext_threadlocalstore(struct threadcontext_t * tcontext);

/* function: castPthread_threadlocalstore
 * Calculates address of <thread_localstore_t> from address of contained <thread_t>. */
thread_localstore_t* castPthread_threadlocalstore(struct thread_t* thread);

/* function: self_threadlocalstore
 * Returns <thread_localstore_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack. */
thread_localstore_t* self_threadlocalstore(void);

/* function: context_threadlocalstore
 * Returns pointer to <threadcontext_t> stored in thread local storage.
 * The function <sys_context_threadlocalstore> is identical with context_threadlocalstore(self_threadlocalstore()).
 * If you change this function change sys_context_threadlocalstore also. */
struct threadcontext_t* context_threadlocalstore(thread_localstore_t* tls);

/* function: thread_threadlocalstore
 * Returns pointer to <thread_t> stored in thread local storage.
 * The function <sys_thread_threadlocalstore> is identical with thread_threadlocalstore(self_threadlocalstore()).
 * If you change this function change sys_thread_threadlocalstore also. */
struct thread_t* thread_threadlocalstore(thread_localstore_t* tls);

/* function: logwriter_threadlocalstore
 * Returns <logwriter_t> used during init and free operations of current <threadcontext_t>.
 * This logwriter is the one allocated statically. For every thread a separate one. */
struct logwriter_t * logwriter_threadlocalstore(thread_localstore_t* tls);


/* function: size_threadlocalstore
 * Returns the size of the allocated memory block. */
size_t size_threadlocalstore(void);

/* function: signalstack_threadlocalstore
 * Returns in stackmem the signalstack from tls.
 * If tls is in a freed state stackmem is set to <memblock_FREE>.
 * The signal stack is used in case of a signal (exceptions).
 * For example if the thread stack overflows SIGSEGV signal is thrown.
 * To handle this case the system must have an extra signal stack
 * cause signal handling needs stack space. */
void signalstack_threadlocalstore(thread_localstore_t* tls, /*out*/struct memblock_t * stackmem);

/* function: threadstack_threadlocalstore
 * Returns in stackmem the thread stack from tls.
 * If tls is in a freed state stackmem is set to <memblock_FREE>. */
void threadstack_threadlocalstore(thread_localstore_t* tls, /*out*/struct memblock_t * stackmem);

// group: static-memory

/* function: memalloc_threadlocalstore
 * Allokiert einen über die Laufzeit des Threads gültigen Speicherblock.
 * Der allokierte Speicherblock wird in memblock zurückgegeben und ist nicht minder
 * als bytesize Bytes groß. */
int memalloc_threadlocalstore(thread_localstore_t* tls, size_t bytesize, /*out*/struct memblock_t* memblock);

/* function: memfree_threadlocalstore
 * Gibt den zuletzt allokierten Speicherblock wieder frei.
 * Mehrere Speicherblöcke können auch zusammen auf einmal freigegeben werden.
 *
 * Diese Funktion ist eher zum Testen gedacht, da statischer Speicher über die
 * Laufzeit des gesamten Threads gültig zu sein hat.
 *
 * Returns
 * 0      - OK
 * EINVAL - memblock ist nicht der zuletzt von <memalloc_threadlocalstore> allokierte Speicherblock. */
int memfree_threadlocalstore(thread_localstore_t* tls, struct memblock_t* memblock);

/* function: sizestatic_threadlocalstore
 * Gibt die aufgerundete Anzahl allokierter Bytes an statischem Speicher zurück. */
size_t sizestatic_threadlocalstore(const thread_localstore_t* tls);



// section: inline implementation

/* define: castPthread_threadlocalstore
 * Implements <thread_localstore_t.castPthread_threadlocalstore>. */
#define castPthread_threadlocalstore(thread) \
         ( __extension__ ({           \
            thread_t* _t = (thread);  \
            ((thread_localstore_t*) ( ((uint8_t*)(_t)) - sizeof(threadcontext_t))); \
         }))

/* define: castPcontext_threadlocalstore
 * Implements <thread_localstore_t.castPcontext_threadlocalstore>. */
#define castPcontext_threadlocalstore(tcontext) \
         ( __extension__ ({                     \
            threadcontext_t* _t = (tcontext);   \
            ((thread_localstore_t*) (_t));      \
         }))

/* define: context_threadlocalstore
 * Implements <thread_localstore_t.context_threadlocalstore>. */
#define context_threadlocalstore(tls) \
         ((threadcontext_t*) (tls))

/* define: self_threadlocalstore
 * Implements <thread_localstore_t.self_threadlocalstore>. */
#define self_threadlocalstore() \
         (sys_self_threadlocalstore())

/* define: thread_threadlocalstore
 * Implements <thread_localstore_t.thread_threadlocalstore>. */
#define thread_threadlocalstore(tls) \
         ( __extension__ ({                                                \
            thread_localstore_t* _tls = (tls);                             \
            ((thread_t*) ( ((uint8_t*)(_tls)) + sizeof(threadcontext_t))); \
         }))

/* define: size_threadlocalstore
 * Implements <thread_localstore_t.size_threadlocalstore>. */
#define size_threadlocalstore() \
         (sys_size_threadlocalstore())

#endif
