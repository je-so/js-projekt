/* title: Thread

   Encapsulates os specific threading model.

   Includes:
   If you call <lock_thread> or <unlock_thread> you need to include <AtomicOps> first.

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

// import
struct timevalue_t;
struct dlist_node_t;

// === exported types
struct thread_t;

/* typedef: thread_f
 * Defines function type executed by <thread_t>. */
typedef int (* thread_f) (void * thread_arg);


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
typedef struct thread_t {
   /* variable: threadcontext
    * Adds thread context to thread variable.
    * Add cpuling of higher level to low level module.
    * But threadcontext must be initialized during thread initialization
    * so storing it in thread_t feels natural
    * (and makes <thread_t>,<threadcontext_t>, and <thread_stack_t> having the same
    * start address in memory). */
   threadcontext_t   threadcontext;
   /* variable: wait
    * Points to next/prev thread which waits on the same synchronization structure like <thrmutex_t> or <waitlist_t>.
    * This ensures that waiting does not need to allocate list nodes and therefore never generates error ENOMEM.
    * Supports double linked lists. See wait_next for a description. */
   struct {
      struct dlist_node_t*  next;
      struct dlist_node_t*  prev;
   }              wait;
   /* variable: task
    * Function executed after thread has been created. */
   thread_f       task;
   /* variable: task_arg
    * Parameter of executed <task> function. */
   void*          task_arg;
   /* variable: returncode
    * Contains the return value of <task>.
    * This value is only valid after <task> has returned. */
   volatile int   returncode;
   /* variable: syserr
    * Contains the error code produced by init or free operations of thread context.
    * This value is only valid after thread has stopped running. */
   volatile int   syserr; // TODO: add test for this value
   /* variable: lockflag
    * Lock flag used to protect access to data members.
    * Set and cleared with atomic operations. */
   uint8_t        lockflag;
   /* variable: ismain
    * Set to true if thread is main thread. */
   uint8_t        ismain;
   /* variable: sys_thread
    * Contains system specific thread type. */
   sys_thread_t   sys_thread;
   /* variable: continuecontext
    * Contains thread machine context before <task> is called.
    * This context could be used in any aborthandler.
    * The aborthandler should call <abort_thread> which sets returncode to value ENOTRECOVERABLE
    * and calls setcontext (see: man 2 setcontext) with <continuecontext> as argument. */
   ucontext_t     continuecontext;
} thread_t;

// group: lifetime

/* define: thread_FREE
 * Static initializer.
 * Used to initialize thread in <thread_stack_t>. */
#define thread_FREE \
         { threadcontext_FREE, {0, 0}, 0, 0, 0, 0, 0, 0, sys_thread_FREE, { .uc_link = 0 } }

/* function: runmain_thread
 * Calls task on a new stack with a new thread context.
 * If an error occurs during initialization only an error code (> 0) is returned and
 * task is not called. In this case thread_retcode is set to -1.
 * If task has been called its return value is stored in retcode if this pointer is not null.
 * If an error occurs during freeing of resources this error code is returned but
 * retcode is not changed and keeps its value returned from task.
 *
 * The main thread usually calls this function cause no new system is created.
 * The thread of the caller is reused.
 *
 * Before task is called a new (<thread_stack_t>) is initialized and also a
 * new <threadcontext_t> which is initialized to <threadcontext_t.threadcontext_INIT_STATIC>.
 * So only a minimalistic statically initialized <logwriter_t> is available during
 * execution of task. task has to initialize <threadcontext_t> for itself.
 *
 * It is assumed that <maincontext_t> has not been initialized except that initlog is working.
 * In case of an error yyy_LOG(INIT,...) is called.
 *
 * This function is called during execution of <maincontext_t.initrun_maincontext>
 * to set up the thread environment of the main thread.
 * */
int runmain_thread(/*out;err*/int* retcode, thread_f task, void* task_arg, ilog_t* initlog, maincontext_e type, int argc, const char* argv[]);

/* function: new_thread
 * Creates and starts a new system thread.
 * On success the parameter thread points to the new thread object.
 * The thread has to do some internal initialization after running the first time
 * and before task is called.
 * Especially <init_threadcontext> is called to initialized a new thread context.
 * If this operation fails the new thread exits immediately with an error code (ENOMEM for example.)
 * If any other internal preparation goes wrong (which should never occur) <maincontext_t.abort_maincontext> is called.
 * It is unspecified if task is called before new_thread returns.
 * On Linux new_thread returns before the newly created thread is scheduled. */
int new_thread(/*out*/thread_t** thread, thread_f task, void* task_arg);

/* define: newgeneric_thread
 * Same as <new_thread> except that it accepts functions with generic argument type.
 * The function argument must be of size sizeof(void*). */
int newgeneric_thread(/*out*/thread_t** thread, thread_f task, void* task_arg);

/* function: delete_thread
 * Calls <join_thread> (if not already called) and deletes resources.
 * This function waits until the thread has terminated. So be careful ! */
int delete_thread(thread_t** thread);

/* function: initstatic_thread
 * */
int initstatic_thread(/*out*/thread_t* thread);

/* function: freestatic_thread
 * Gibt Ressourcen von thread frei, die während Ausführung von <initstatic_thread> belegt wurden.
 * Diese Funktion wird von <thread_stack_t.delete_threadstack> aufgerufen,
 * sie ruft ihrerseits <threadcontext_t.freestatic_threadcontext> auf.
 * Aber erst nachdem <free_threadcontext> aufgerufen wurde am Ende der Ausführung eines Threads. */
int freestatic_thread(/*out*/thread_t* thread);

// group: query

/* function: context_thread
 * Returns a pointer to context of thread_t. */
threadcontext_t* context_thread(thread_t* thread);

/* function: self_thread
 * Returns a pointer to own <thread_t>. */
thread_t* self_thread(void);

/* function: returncode_thread
 * Returns the returncode of the joined thread.
 * The returncode is only valid if <join_thread> was called before.
 * 0 is returned in case the thread has not already been joined. */
int returncode_thread(const thread_t* thread);

/* function: task_thread
 * Returns <thread_t.task>.
 * This value is set to task given as parameter in <new_thread>. */
thread_f task_thread(const thread_t* thread);

/* function: taskarg_thread
 * Reads <thread_t.task_arg> field of <thread_t> object.
 * This value is set to task_arg given as parameter in <new_thread>. */
void* taskarg_thread(const thread_t* thread);

/* function: ismain_thread
 * Returns true if the calling thread is the main thread. */
bool ismain_thread(const thread_t* thread);

// group: update

/* function: lock_thread
 * Wait until thread->lockflag (<lockflag>) is cleared and set it atomically.
 * Includes an acuire memory barrier (see <AtomicOps>).  */
void lock_thread(thread_t* thread);

/* function: unlock_thread
 * Clear thread->lockflag (<lockflag>). The function assumes that the flag
 * was previously set by a call to <lock_thread>.
 * Include a release memory barrier (see <AtomicOps>). */
void unlock_thread(thread_t* thread);

/* function: settask_thread
 * Changes values returned by <task_thread> and <taskarg_thread>. */
void settask_thread(thread_t* thread, thread_f task, void* task_arg);

/* function: settaskarg_thread
 * Sets value returned by <taskarg_thread>. */
void settaskarg_thread(thread_t* thread, void* task_arg);

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
 * Macro which expects 0 or 1 argument.
 * Calls either <suspend0_thread> or <suspend1_thread> according to the number of arguments. */
int suspend_thread(struct timevalue_t* timeout/*optional*/);

/* function: suspend1_thread
 * The calling thread will sleep until <resume_thread> is called or timeout expires.
 * <resume_thread> must be called from another thread.
 *
 * Linux specific:
 * Internally sigwaitinfo with signal SIGINT is used to sleep.
 *
 * Attention !:
 * It is possible that signals are received which are generated from outside this process
 * therefore make sure with some other mechanism that returning
 * from <suspend_thread> emanates from a corresponding call to <resume_thread>.
 *
 * Returns:
 * 0      - Woken up from resume_thread.
 * EAGAIN - Timeout timer expired. No resume_thread was called during time defined by timeout. */
int suspend1_thread(struct timevalue_t* timeout/*null: NO TIMEOUT*/);

/* function: suspend0_thread
 * Calls suspend_thread with timeout disabled. This function waits infinitely. Interrupts (signals) are ignored. */
void suspend0_thread(void);

/* function: trysuspend_thread
 * The function returns 0 if the calling thread has been resumed.
 * IF the functions returns 0 this is the same as calling <suspend_thread>
 * and returning immediately. The resume event is consumed.
 * It returns EAGAIN if there is no pending resume. Nothing is done in
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

/* function: interrupt_thread
 * Sends an interrupt to a thread to intterupt a system call.
 * Use this function to wake up a thread which is waiting in a system call.
 * If the thread is waiting in a system call the syscall returns with error EINTR.
 * If the thread is running in user space the interrupt handler is executed but nothing
 * else happends. */
void interrupt_thread(thread_t* thread);

/* function: sleepms_thread
 * Makes calling thread to sleep msec milli-seconds. */
void sleepms_thread(uint32_t msec);

/* function: yield_thread
 * Schedules another thread on this Processor. */
void yield_thread(void);

/* function: exit_thread
 * Ends the calling thread and sets retcode as its return code.
 * If the caller is the main thread the value EPROTO is returned and
 * nothing is done. The main thread must call <maincontext_t.free_maincontext> and exit(retcode).
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
 * Set the value returned from <returncode_thread> to 0 before calling this function
 * the first time to determine if this function returns after storing the current
 * cpu execution the first time or from a call to <abort_thread>.
 * For any started thread <setcontinue_thread> is called before task
 * (parameter in <new_thread>) will be called. The main thread which calls <maincontext_t.init_maincontext>
 * must call <setcontinue_thread> explicitly.
 *
 * Unchecked Precondition:
 * thread == self_thread() */
int setcontinue_thread(thread_t* thread);



// section: inline implementation

/* define: context_thread
 * Implements <thread_t.context_thread>. */
#define context_thread(thread) \
         (&(thread)->threadcontext)

/* define: ismain_thread
 * Implements <thread_t.ismain_thread>. */
#define ismain_thread(thread) \
         ( __extension__ ({   \
            volatile const    \
            thread_t* _thr;   \
            _thr = (thread);  \
            _thr->ismain;     \
         }))

/* define: lock_thread
 * Implements <thread_t.lock_thread>. */
#define lock_thread(thread) \
         do {                                                 \
            thread_t* const _thread = (thread);               \
            while (0 != set_atomicflag(&_thread->lockflag)) { \
               yield_thread();                                \
            }                                                 \
         } while (0)                                          \

/* define: newgeneric_thread
 * Implements <thread_t.newgeneric_thread>. */
#define newgeneric_thread(thread, task, task_arg) \
         ( __extension__ ({                                    \
            int (*_task) (typeof(task_arg)) = (task);          \
            static_assert( sizeof(task_arg) == sizeof(void*),  \
                           "same as void*");                   \
            new_thread( thread, (thread_f)_task,               \
                        (void*)task_arg);                      \
         }))

/* define: task_thread
 * Implements <thread_t.task_thread>. */
#define task_thread(thread) \
         ( __extension__ ({   \
            volatile const    \
            thread_t* _thr;   \
            _thr = (thread);  \
            _thr->task;       \
         }))

/* define: returncode_thread
 * Implements <thread_t.returncode_thread>. */
#define returncode_thread(thread) \
         ((thread)->returncode)

/* define: self_thread
 * Implements <thread_t.self_thread>. */
#define self_thread() \
         ((thread_t*) context_syscontext())

/* define: setcontinue_thread
 * Implements <thread_t.setcontinue_thread>. */
#define setcontinue_thread(thread) \
         ( __extension__ ({                              \
            int _err = getcontext(                       \
               &(thread)->continuecontext);              \
            if (_err) {                                  \
               _err = errno;                             \
               TRACESYSCALL_ERRLOG("getcontext", _err);  \
            }                                            \
            _err;                                        \
         }))

/* define: setreturncode_thread
 * Implements <thread_t.setreturncode_thread>. */
#define setreturncode_thread(thread, retcode) \
         (thread)->returncode = retcode;

/* define: settask_thread
 * Implements <thread_t.settask_thread>. */
#define settask_thread(thread, _task, _task_arg) \
         do {                                   \
            volatile thread_t* _thr = (thread); \
            _thr->task     = (_task);           \
            _thr->task_arg = (_task_arg);       \
         } while(0)

/* define: settaskarg_thread
 * Implements <thread_t.settaskarg_thread>. */
#define settaskarg_thread(thread, _task_arg) \
         do {                                   \
            volatile thread_t* _thr = (thread); \
            _thr->task_arg = (_task_arg);       \
         } while(0)


/* define: taskarg_thread
 * Implements <thread_t.taskarg_thread>. */
#define taskarg_thread(thread) \
         ( __extension__ ({                           \
            volatile const thread_t* _thr = (thread); \
            _thr->task_arg;                           \
         }))

/* define: unlock_thread
 * Implements <thread_t.unlock_thread>. */
#define unlock_thread(thread) \
         (clear_atomicflag(&(thread)->lockflag))

/* define: yield_thread
 * Implements <thread_t.yield_thread>. */
#define yield_thread() \
         (pthread_yield())

/* define: suspend_thread
 * Implements <thread_t.suspend_thread>. */
#define suspend_thread(...) \
         fctname_nrargsof(suspend,_thread,__VA_ARGS__) (__VA_ARGS__)

/* define: suspend0_thread
 * Implements <thread_t.suspend0_thread>. */
#define suspend0_thread() \
         ((void) suspend1_thread(0))

#endif
