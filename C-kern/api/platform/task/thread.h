/* title: Thread

   Encapsulates os specific threading model.

   Includes:
   If you call <lockflag_thread> or <unlockflag_thread> you need to include <AtomicOps> first.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/task/thread.h
    Header file of <Thread>.

   file: C-kern/platform/Linux/task/thread.c
    Linux specific implementation <Thread Linux>.
*/
#ifndef CKERN_PLATFORM_TASK_THREAD_HEADER
#define CKERN_PLATFORM_TASK_THREAD_HEADER

// forward
struct slist_node_t;

/* typedef: struct thread_t
 * Export <thread_t>. */
typedef struct thread_t thread_t;

/* typedef: thread_f
 * Defines function type executed by <thread_t>. */
typedef int (* thread_f) (void * main_arg);


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_task_thread
 * Tests <thread_t> interface. */
int unittest_platform_task_thread(void);
#endif


/* struct: thread_t
 * Describes a system thread.
 *
 * Attention:
 * Never forget to lock / unlock a thread object before you access
 * the fields which can be changed by other threads.
 * This ensures that you read a consistent state and that on some
 * architectures proper read and write barriers are executed.
 *
 * Use <lock_thread> and <unlock_thread> for that matter. */
struct thread_t {
   /* variable: nextwait
    * Points to next thread which waits on the same <thrmutex_t> or <waitlist_t>.
    * This ensures that waiting does not need to allocate list nodes and therefore never
    * generates error ENOMEM. */
   struct
   slist_node_t*  nextwait;
   /* variable: lockflag
    * Lock flag used to protect access to data members.
    * Set and cleared with atomic operations. */
   uint8_t        lockflag;
   /* variable: main_task
    * Function executed after thread has been created. */
   thread_f       main_task;
   /* variable: main_arg
    * Parameter of executed <main_task> function. */
   void*          main_arg;
   /* variable: returncode
    * Contains the return value of <main_task>.
    * This value is only valid after <main_task> has returned. */
   int            returncode;
   /* variable: sys_thread
    * Contains system specific thread type. */
   sys_thread_t   sys_thread;
   /* variable: tls_addr
    * Contains start address of thread local storage.
    * See <thread_tls>. */
   uint8_t*       tls_addr;
   /* variable: continuecontext
    * Contains thread machine context before <main_task> is called.
    * This context could be used in any aborthandler.
    * The aborthandler should call <abort_thread> which sets returncode to value ENOTRECOVERABLE
    * and calls setcontext (see: man 2 setcontext) with <continuecontext> as argument. */
   ucontext_t     continuecontext;
};

// group: lifetime

/* define: thread_FREE
 * Static initializer.
 * Used to initialize thread in <thread_tls_t>. */
#define thread_FREE \
         { 0, 0, 0, 0, 0, sys_thread_FREE, 0, { .uc_link = 0 } }

/* function: initmain_thread
 * Initializes main thread. Called from <platform_t.init_platform>.
 * Returns EINVAL if thread is not the main thread. */
void initmain_thread(/*out*/thread_t* thread, thread_f thread_main, void* main_arg);

/* function: new_thread
 * Creates and starts a new system thread.
 * On success the parameter thread points to the new thread object.
 * The thread has to do some internal initialization after running the first time
 * and before thread_main is called.
 * If the internal preparation goes wrong <maincontext_t.abort_maincontext> is called.
 * It is unspecified if thread_main is called before new_thread returns.
 * On Linux new_thread returns before the newly created thread is scheduled. */
int new_thread(/*out*/thread_t** thread, thread_f thread_main, void* main_arg);

/* define: newgeneric_thread
 * Same as <new_thread> except that it accepts functions with generic argument type.
 * The function argument must be of size sizeof(void*). */
int newgeneric_thread(/*out*/thread_t** thread, thread_f thread_main, void* main_arg);

/* function: delete_thread
 * Calls <join_thread> (if not already called) and deletes resources.
 * This function waits until the thread has terminated. So be careful ! */
int delete_thread(thread_t** thread);

// group: query

/* function: self_thread
 * Returns a pointer to own <thread_t>. */
thread_t* self_thread(void);

/* function: returncode_thread
 * Returns the returncode of the joined thread.
 * The returncode is only valid if <join_thread> was called before.
 * 0 is returned in case the thread has not already been joined. */
int returncode_thread(const thread_t* thread);

/* function: maintask_thread
 * Returns <thread_t.main_task>.
 * This value is set to thread_main given as parameter in <new_thread>. */
thread_f maintask_thread(const thread_t* thread);

/* function: mainarg_thread
 * Reads <thread_t.main_arg> field of <thread_t> object.
 * This value is set to main_arg given as parameter in <new_thread>. */
void* mainarg_thread(const thread_t* thread);

/* function: ismain_thread
 * Returns true if the calling thread is the main thread. */
bool ismain_thread(const thread_t* thread);

// group: update

/* function: lockflag_thread
 * Wait until thread->lockflag (<lockflag>) is cleared and set it atomically.
 * Includes an acuire memory barrier (see <AtomicOps>).  */
void lockflag_thread(thread_t* thread);

/* function: unlockflag_thread
 * Clear thread->lockflag (<lockflag>). The function assumes that the flag
 * was previously set by a call to <lockflag_thread>.
 * Include a release memory barrier (see <AtomicOps>). */
void unlockflag_thread(thread_t* thread);

/* function: settask_thread
 * Changes values returned by <maintask_thread> and <mainarg_thread>. */
void settask_thread(thread_t* thread, thread_f main, void* main_arg);

/* function: setreturncode_thread
 * Changes value returned by <returncode_thread>. */
void setreturncode_thread(thread_t* thread, int retcode);

// group: synchronize

/* function: join_thread
 * The function suspends execution of the caller until thread terminates.
 * If the thread has already been joined this function returns immediately.
 * The error EDEADLK is returned if you want to join <self_thread>.
 * The error ESRCH is returned if the thread has exited already (if thread is not updated properly). */
int join_thread(thread_t* thread);

/* function: tryjoin_thread
 * The function poll if thread has terminated. No error is logged.
 *
 * Returns:
 * 0 - Thread has been terminated or the thread has already been joined.
 * EDEADLK - Joining <self_thread> is an error.
 * EBUSY - Thread is running - joining is not possible. */
int tryjoin_thread(thread_t* thread);

// group: change-run-state

/* function: suspend_thread
 * The calling thread will sleep until <resume_thread> is called.
 * <resume_thread> must be called from another thread.
 *
 * Linux specific:
 * Internally sigwaitinfo wirh signal SIGINT is used to sleep.
 *
 * Attention !:
 * It is possible that signals are received which are generated from outside this process
 * therefore make sure with some other mechanism that returning
 * from <suspend_thread> emanates from a corresponding call to <resume_thread>. */
void suspend_thread(void);

/* function: trysuspend_thread
 * The function returns 0 if the calling thread has been resumed.
 * IF the functions returns 0 this is the same as calling <suspend_thread>
 * and returning immediately. The resume event is consumed.
 * It returns EAGAIN if no there is no pending resume. Nothing is done in
 * this case. Use this function to poll for any pending <resume_thread>
 * without sleeping.  */
int trysuspend_thread(void);

/* function: resume_thread
 * The thread which is refered by thread is woken up.
 * The status of resume is conserved if the other thread is currently not sleeping.
 * So the next call to suspend will return immediately.
 * This behaviour is needed cause a thread calling suspend could be preempted before
 * it really enters a sleep state.
 *
 * Linux specific:
 * Internally pthread_kill with signal SIGINT is used to wake up a another
 * thread from sleeping. */
void resume_thread(thread_t* thread);

/* function: sleepms_thread
 * Makes calling thread to sleep msec milli-seconds. */
void sleepms_thread(uint32_t msec);

/* function: yield_thread
 * Schedules another thread on this Processor. */
void yield_thread(void);

/* function: exit_thread
 * Ends the calling thread and sets retcode as its return code.
 * If the caller is the main thread the value EPROTO is returned and
 * nothing is done. The main thread must calle <maincontext_t.free_maincontext> and exit(retcode).
 * No cleanup handlers are executed. */
int exit_thread(int retcode);

// group: abort

/* function: abort_thread
 * Aborts the calling thread.
 * This functions sets returncode (see <returncode_thread>) to value ENOTRECOVERABLE
 * and continues execution at the place which was marked with <setcontinue_thread> previously.
 * No cleanup handlers are executed. */
void abort_thread(void);

/* function: setcontinue_thread
 * Stores the current execution context and returns 0 on success.
 * Parameter is_abort is set to false if <setcontinue_thread> stored
 * the execution context where execution should continue if
 * <abort_thread> will be called.
 * Parameter is_abort is set to true if <setcontinue_thread> returns as
 * a reaction to a call to <abort_thread>.
 * For any started thread <setcontinue_thread> is called before thread_main
 * (parameter in <new_thread>) will be called. The main thread which calls <maincontext_t.init_maincontext>
 * must call <setcontinue_thread> explicitly. */
int setcontinue_thread(bool* is_abort);



// section: inline implementation

/* define: ismain_thread
 * Implements <thread_t.ismain_thread>. */
#define ismain_thread(thread) \
         ( __extension__ ({        \
            volatile const         \
            thread_t* _thr;        \
            _thr = (thread);       \
            (0 == _thr->tls_addr); \
         }))

/* define: initmain_thread
 * Implements <thread_t.initmain_thread>. */
#define initmain_thread(thread, _thread_main, _main_arg) \
         do {                                   \
            volatile thread_t* _thr;            \
            _thr = (thread);                    \
            _thr->main_task  = _thread_main;    \
            _thr->main_arg   = _main_arg;       \
            _thr->sys_thread = pthread_self();  \
            _thr->tls_addr   = 0;               \
         } while(0)

/* define: lockflag_thread
 * Implements <thread_t.lockflag_thread>. */
#define lockflag_thread(thread) \
         do {                                                 \
            thread_t* const _thread = (thread);               \
            while (0 != set_atomicflag(&_thread->lockflag)) { \
               yield_thread();                                \
            }                                                 \
         } while (0)                                          \

/* define: newgeneric_thread
 * Implements <thread_t.newgeneric_thread>. */
#define newgeneric_thread(thread, thread_main, main_arg) \
         ( __extension__ ({                                   \
            int (*_thread_main) (typeof(main_arg));           \
            _thread_main = (thread_main);                     \
            static_assert( sizeof(main_arg) == sizeof(void*), \
                           "same as void*");                  \
            new_thread( thread, (thread_f)_thread_main,       \
                        (void*)main_arg);                     \
         }))

/* define: maintask_thread
 * Implements <thread_t.maintask_thread>. */
#define maintask_thread(thread) \
         ( __extension__ ({   \
            volatile const    \
            thread_t* _thr;   \
            _thr = (thread);  \
            _thr->main_task;  \
         }))

/* define: mainarg_thread
 * Implements <thread_t.mainarg_thread>. */
#define mainarg_thread(thread) \
         ( __extension__ ({   \
            volatile const    \
            thread_t* _thr;   \
            _thr = (thread);  \
            _thr->main_arg;   \
         }))

/* define: returncode_thread
 * Implements <thread_t.returncode_thread>. */
#define returncode_thread(thread) \
         ( __extension__ ({   \
            volatile const    \
            thread_t* _thr;   \
            _thr = (thread);  \
            _thr->returncode; \
         }))

/* define: self_thread
 * Implements <thread_t.self_thread>. */
#define self_thread() \
         (sys_thread_threadtls())

/* define: setcontinue_thread
 * Implements <thread_t.setcontinue_thread>. */
#define setcontinue_thread(is_abort) \
         ( __extension__ ({              \
            thread_t* _self;             \
            _self = self_thread();       \
            setreturncode_thread(        \
               _self, 0);                \
            int _err = getcontext(       \
               &_self->continuecontext); \
            if (_err) {                  \
               _err = errno;             \
               TRACESYSCALL_ERRLOG(      \
                  "getcontext", _err);   \
            }                            \
            if (returncode_thread(       \
                  _self)) {              \
               *(is_abort) = true;       \
            } else {                     \
               *(is_abort) = false;      \
            }                            \
            _err;                        \
         }))

/* define: settask_thread
 * Implements <thread_t.settask_thread>. */
#define settask_thread(thread, _main, _main_arg) \
         do {                                 \
            volatile typeof(*(thread))* _thr; \
            _thr = (thread);                  \
            _thr->main_task = (_main);        \
            _thr->main_arg  = (_main_arg);    \
         } while(0)

/* define: setreturncode_thread
 * Implements <thread_t.setreturncode_thread>. */
#define setreturncode_thread(thread, retcode) \
         do {                                 \
            volatile typeof(*(thread))* _thr; \
            _thr = (thread);                  \
            _thr->returncode = (retcode);     \
         } while(0)

/* define: unlockflag_thread
 * Implements <thread_t.unlockflag_thread>. */
#define unlockflag_thread(thread) \
         (clear_atomicflag(&(thread)->lockflag))

/* define: yield_thread
 * Implements <thread_t.yield_thread>. */
#define yield_thread() \
         (pthread_yield())

#endif
