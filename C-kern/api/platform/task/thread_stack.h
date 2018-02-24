/* title: ThreadStack

   Supports storage (variables and stack space)
   for every creatd thread and the main thread.
   The main thread is initialized with <newmain_threadstack>
   all other with <new_threadstack>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/platform/task/thread_stack.h
    Header file <ThreadStack>.

   file: C-kern/platform/Linux/task/thread_stack.c
    Implementation file <ThreadStack impl>.
*/
#ifndef CKERN_PLATFORM_TASK_THREAD_STACK_HEADER
#define CKERN_PLATFORM_TASK_THREAD_STACK_HEADER

// import
struct memblock_t;
struct thread_t;
struct threadcontext_t;

// === exported types
struct thread_stack_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_thread_stack
 * Test <thread_stack_t> functionality. */
int unittest_platform_task_thread_stack(void);
#endif


/* struct: thread_stack_t
 * Holds thread local memory.
 * The memory comprises the variables <thread_t> and <threadcontext_t>,
 * the signal stack and thread stack and 3 protection pages in between. */
typedef struct thread_stack_t thread_stack_t;

// group: lifetime

/* function: new_threadstack
 * Allocates a memory block big enough to hold all thread local storage data.
 * The access rights of parts of the memory block is changed to catch stack overflows.
 * The allocated memory block is aligned to its own size.
 * The thread object located on the stack is initialized with <thread_t.thread_FREE>.
 * Called from <syscontext_t.initrun_syscontext> therefore initlog is used if needed.
 * Parameter static_size must be set to the number of bytes needed during initialization
 * of <threadcontext_t> (size determined by extsize_threadcontext) and <maincontext_t>
 * (size determined by extsize_maincontext). */
int new_threadstack(/*out*/thread_stack_t** st, ilog_t* initlog, const size_t static_size, /*out*/struct memblock_t* threadstack, /*out*/struct memblock_t* signalstack);

/* function: delete_threadstack
 * Changes protection of memory to normal and frees it.
 * Called from <syscontext_t.initrun_syscontext> therefore initlog is used if needed. */
int delete_threadstack(thread_stack_t** st, ilog_t* initlog);

// group: query

/* function: castPcontext_threadstack
 * Calculates address of <thread_stack_t> from address of contained <threadcontext_t>. */
thread_stack_t* castPcontext_threadstack(struct threadcontext_t * tcontext);

/* function: castPthread_threadstack
 * Calculates address of <thread_stack_t> from address of contained <thread_t>. */
thread_stack_t* castPthread_threadstack(struct thread_t* thread);

/* function: self_threadstack
 * Returns <thread_stack_t> of the current thread.
 * The parameter local_var must point to a local variable on the current stack. */
thread_stack_t* self_threadstack(void);

/* function: context_threadstack
 * Returns pointer to <threadcontext_t> stored in thread local storage.
 * The function <syscontext_t.context_syscontext> is identical with context_threadstack(self_threadstack()).
 * If you change this function change <sycontext_t.context_syscontext> also. */
struct threadcontext_t* context_threadstack(thread_stack_t* st);

/* function: thread_threadstack
 * Returns pointer to <thread_t> stored in thread local storage. */
struct thread_t* thread_threadstack(thread_stack_t* st);

/* function: size_threadstack
 * Returns the size of the allocated memory block. */
size_t size_threadstack(void);

/* function: signalstack_threadstack
 * Returns in stackmem the signalstack from st.
 * If st is in a freed state stackmem is set to <memblock_FREE>.
 * The signal stack is used in case of a signal (exceptions).
 * For example if the thread stack overflows SIGSEGV signal is thrown.
 * To handle this case the system must have an extra signal stack
 * cause signal handling needs stack space. */
struct memblock_t signalstack_threadstack(thread_stack_t* st);

/* function: threadstack_threadstack
 * Returns in stackmem the thread stack from st.
 * If st is in a freed state stackmem is set to <memblock_FREE>. */
struct memblock_t threadstack_threadstack(thread_stack_t* st);

// group: static-memory

/* function: allocstatic_threadstack
 * Allokiert einen über die Laufzeit des Threads gültigen Speicherblock.
 * Der allokierte Speicherblock wird in memblock zurückgegeben und ist nicht minder
 * als bytesize Bytes groß. Die Mindestanzahl allokierbarer Bytes wurde durch Parameter static_size
 * bei Aufruf von new_threadstack festgelegt. */
int allocstatic_threadstack(thread_stack_t* st, ilog_t* initlog, size_t bytesize, /*out*/struct memblock_t* memblock);

/* function: freestatic_threadstack
 * Gibt den zuletzt allokierten Speicherblock wieder frei.
 * Mehrere Speicherblöcke können auch zusammen auf einmal freigegeben werden.
 *
 * Diese Funktion ist eher zum Testen gedacht, da statischer Speicher über die
 * Laufzeit des gesamten Threads gültig zu sein hat.
 *
 * Returns
 * 0      - OK
 * EINVAL - memblock ist nicht der zuletzt von <allocstatic_threadstack> allokierte Speicherblock. */
int freestatic_threadstack(thread_stack_t* st, ilog_t* initlog, struct memblock_t* memblock);

/* function: sizestatic_threadstack
 * Gibt die aufgerundete Anzahl allokierter Bytes an statischem Speicher zurück. */
size_t sizestatic_threadstack(const thread_stack_t* st);



// section: inline implementation

/* define: castPthread_threadstack
 * Implements <thread_stack_t.castPthread_threadstack>. */
#define castPthread_threadstack(thread) \
         ( __extension__ ({            \
            thread_t* _t = (thread);   \
            ((thread_stack_t*) (_t));  \
         }))

/* define: castPcontext_threadstack
 * Implements <thread_stack_t.castPcontext_threadstack>. */
#define castPcontext_threadstack(tcontext) \
         ( __extension__ ({                     \
            threadcontext_t* _t = (tcontext);   \
            ((thread_stack_t*) (_t));           \
         }))

/* define: context_threadstack
 * Implements <thread_stack_t.context_threadstack>. */
#define context_threadstack(st) \
         ( __extension__ ({                     \
            thread_stack_t* _st = (st);         \
            ((struct threadcontext_t*) (_st));  \
         }))

/* define: self_threadstack
 * Implements <thread_stack_t.self_threadstack>. */
#define self_threadstack() \
         ((thread_stack_t*) context_syscontext())

/* define: thread_threadstack
 * Implements <thread_stack_t.thread_threadstack>. */
#define thread_threadstack(st) \
         ( __extension__ ({               \
            thread_stack_t* _st = (st);   \
            ((struct thread_t*) (_st));   \
         }))

/* define: size_threadstack
 * Implements <thread_stack_t.size_threadstack>. */
#define size_threadstack() \
         (stacksize_syscontext())

#endif
