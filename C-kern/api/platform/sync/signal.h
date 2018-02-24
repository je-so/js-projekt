/* title: PosixSignals
   - Offers storing and comparing of different signal handler configurations.
   - Offers interface to set signal handling configuration at process start up.
     The configuration is read from "C-kern/resource/config/signalhandler"
     during compilation time.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/platform/sync/signal.h
    Header file of <PosixSignals>.

   file: C-kern/platform/Linux/sync/signal.c
    Linux specific implementation <PosixSignals Linuximpl>.
*/
#ifndef CKERN_PLATFORM_SYNC_SIGNAL_HEADER
#define CKERN_PLATFORM_SYNC_SIGNAL_HEADER

// import
struct thread_t;

// === exported types
struct signalhandler_t;
struct signalstate_t;
// signalwait_t;

/* typedef: signalrt_t
 * Define <signalrt_t> signal type as uint8_t. */
typedef uint8_t signalrt_t;

/* typedef: signalhandler_f
 * Export function signature of a signal handler.
 * Use this signature to write your own signal handler
 * for user send signals. */
typedef void (* signalhandler_f) (unsigned signr, uintptr_t value);

/* typedef: signalhandler_segv_f
 * Export function signature of segmentation violation (fault) signal handler.
 * Use this signature to write a signal handler which handles an address error
 * where a thread tried to write to a read-only memory location or if it tried to
 * access an unmapped memory region. The parameter ismapped is true if the memory
 * protection does not allow the access (write on read-only or read on none-access).
 * The parameter ismapped is false if memaddr is not mapped in the address space of the process. */
typedef void (* signalhandler_segv_f) (void* memaddr, bool ismapped);

/* typedef: enums signal_config_e
 * Zeigt Aktion an, die bei Empfang dieses Signals vom OS ausgelöst wird.
 *
 * Blockierte Signale: Das Signal wird empfangen, aber keine Aktion ausgeführt.
 *                     Es wird als "pending" vermerkt. Auf empfangene Signale kann dann gewartet
 *                     werden oder deren Empfang abgefragt werden.
 *
 * signal_config_DEFAULT - Führe vom Betriebssystem (OS) festgelegte Standardaktion aus (man 7 signal).
 * signal_config_IGNORED - Ignoriere den Empfang dieses Signals.
 * signal_config_HANDLER - Führt eine Programm festgelegte Funktion asynchron aus, wenn dieses Signal empfangen wird.
 *                         Falls ein Thread speziell auf dieses Signal wartet, wird bei Empfang dieser
 *                         bevorzugt und die Funktion nicht ausgeführt.
 * */
typedef enum signal_config_e {
   signal_config_DEFAULT,        // default action (if unblocked)
   signal_config_IGNORED,        // signal will be ignored (if unblocked)
   signal_config_HANDLER,        // signal handler will be called asynchronously
   // blocked signals:  configured action will be prevented and signal be added to pending set of signals.
   //                   Pending signals could be waited for or unblocked.
} signal_config_e;

#define signal_config__NROF  3


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_platform_sync_signal
 * Tests posix signals functionality. */
int unittest_platform_sync_signal(void);
#endif


/* struct: signal_config_t
 * Konfiguration eines OS-Signals, wie auf den Epmfang eines solchen zu reagieren ist.
 * Wird verwendet, um vorherige Einstellungen zu speichern. */
typedef struct signal_config_t {
   uint8_t signr;
   uint8_t config;
   uint8_t isblocked;
   void  (*handler) (int signr);
} signal_config_t;


/* struct: signals_t
 * Stores signal handler for the whole process. */
typedef struct signals_t {
   int                     isinit;
   signalhandler_segv_f    segv;
   sigset_t                sys_old_mask;
   signal_config_t         old_config[3]; // stores previous config of newly configured signal handler
} signals_t;

// group: init

/* define: signals_FREE
 * Wird benutzt für statische Initialisierung. */
#define signals_FREE \
         { .isinit = 0 }

/* function: init_signalhandler
 * Sets signal handlers and signal masks at process initialization time.
 * The configuration is read from "C-kern/resource/config/signalhandler"
 * during compilation time. */
int init_signals(signals_t* sigs);

/* function: free_signals
 * Restores default signal configuration set by the OS at process start. */
int free_signals(signals_t* sigs);

// group: query

/* function: getsegv_signals
 * Returns the current segmentation fault signal handler.
 * There is only one handler per process. A value of 0
 * indicates no handler installed. */
signalhandler_segv_f getsegv_signals(void);

// group: change

/* function: clearsegv_signals
 * Resets to signalhandler_segv_f to NULL.
 * A segmentation fault will execute the default handling of the OS. */
void clearsegv_signals(void);

/* function: setsegv_signals
 * Change segmentation fault signal handler.
 * The handler is the same for all threads of a process.
 * The handler is called (in the corresponding thread context) whenever a thread
 * tries to access an unmapped memory page or accesses a mapped memory page
 * which is protected (write to read-only).
 * If no handler is installed the default is to abort the process.
 * Set segfault_handler to 0 if you want to restore the default behaviour (abort process). */
void setsegv_signals(signalhandler_segv_f segfault_handler);


/* struct: signalwait_t
 * Manages a set of signals which can be waited on for a read event.
 * There is no explicit wait operation but <io_signalwait>
 * returns an <iochannel_t> which can be used in poll / select / wait operation.
 * if the <iochannel_t> is readable the waiting thread has received at least
 * one signal from the set. A thread receives signals send to it and signals send
 * to the whole process.
 *
 * Defines signalwait_t as alias for <sys_iochannel_t>. */
typedef sys_iochannel_t signalwait_t;

// group: lifetime

/* define: signalwait_FREE
 * Static initializer. */
#define signalwait_FREE sys_iochannel_FREE

/* function: initrealtime_signalwait
 * Initializes a set of realtime signals which can be waited upon.
 * All realtime signals in the range [minrt, maxrt] are contained in the set. */
int initrealtime_signalwait(/*out*/signalwait_t * signalwait, signalrt_t minrt, signalrt_t maxrt) ;

/* function: free_signalwait
 * Frees all resources associated with signalwait.
 * After return the <iochannel_t> returned from <io_signalwait> is invalid. */
int free_signalwait(signalwait_t * signalwait) ;

// group: query

/* function: io_signalwait
 * Returns the <iochannel_t> of signalwait.
 * You can wait for a read event on the returned iochannel.
 * A read event signals that the calling thread received at least one realtime signal
 * from the configured set. The returned iochannel is valid as long as signalwait
 * is not freed. */
sys_iochannel_t io_signalwait(const signalwait_t signalwait) ;


// struct: signalrt_t
// Supports real time signal numbers.
// The supported number range is 0..<maxnr_signalrt>. At least 8 realtime signals are supported [0..7].

// group: query

/* function: maxnr_signalrt
 * Returns the maximum supported signal number <signalrt_t>.
 * The supported range is [0..maxnr_signalrt()]. */
signalrt_t maxnr_signalrt(void) ;

// group: change

/* function: send_signalrt
 * Sends realtime signal to any thread in the process.
 * If more than one thread waits (<wait_signalrt>) for the signal
 * one is chosen (unspecified which one) which receives the signal.
 * If the receiving queue is full EAGAIN is returned and no signal is sent. */
int send_signalrt(signalrt_t nr, uintptr_t value) ;

/* function: send2_signalrt
 * Sends realtime signal to a specific thread.
 * If more than one thread waits (<wait_signalrt>) for the signal
 * only the specified thread receives the signal.
 * If the receiving queue is full EAGAIN is returned and no signal is sent. */
int send2_signalrt(signalrt_t nr, uintptr_t value, const struct thread_t* thread) ;

/* function: wait_signalrt
 * Waits for a realtime signal with number nr.
 * The signal is removed from the queue.
 * Only signals send to the calling thread or to the whole process are received.
 * If value is 0 no value is returned.
 * If a signal is send without using <send_signalrt> or <send2_signalrt>
 * (from another source) the value is set to 0. */
int wait_signalrt(signalrt_t nr, /*out*/uintptr_t * value) ;

/* function: trywait_signalrt
 * Polls the queue for a single realtime signal.
 * If the queue is empty EAGAIN ist returned.
 * Else it is consumed and 0 is returned.
 * Only signals send to the calling thread or to the whole process are received.
 * If value is 0 no value is returned.
 * If a signal is send without using <send_signalrt> or <send2_signalrt>
 * (from another source) the value is set to 0. */
int trywait_signalrt(signalrt_t nr, /*out*/uintptr_t * value) ;


/* struct: signalstate_t
 * Stores current state of all signal handlers and the signal mask.
 * Use this object to compare settings of all signal handlers
 * to be equal with each other. */
typedef struct signalstate_t signalstate_t;

// group: lifetime

/* function: new_signalstate
 * Stores in <signalstate_t> the current settings of all signal handlers. */
int new_signalstate(/*out*/signalstate_t ** sigstate);

/* function: delete_signalstate
 * Frees any resources associated with sigstate. */
int delete_signalstate(signalstate_t ** sigstate) ;

// group: query

/* function: compare_signalstate
 * Returns 0 if sigstate1 and sigstate2 contains equal settings. */
int compare_signalstate(const signalstate_t * sigstate1, const signalstate_t * sigstate2) ;



// section: inline implementation

// group: signals_t

#if !defined(KONFIG_SUBSYS_THREAD)
/* define: init_signals
 * Implement init as a no op if !defined(KONFIG_SUBSYS_THREAD) */
#define init_signals(signs)          (0)
/* define: free_signals
 * Implement free as a no op if !defined(KONFIG_SUBSYS_THREAD) */
#define free_signals(signs)          (0)
#endif

// group: signalwait_t

/* define: io_signalwait
 * Implement <signalwait_t.io_signalwait>. */
#define io_signalwait(signalwait)         ((sys_iochannel_t)signalwait)

#endif
