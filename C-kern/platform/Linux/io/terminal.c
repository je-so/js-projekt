/* title: Terminal impl

   Implements <Terminal>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/terminal.h
    Header file <Terminal>.

   file: C-kern/platform/Linux/io/terminal.c
    Implementation file <Terminal impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/terminal.h"
#include "C-kern/api/err.h"
#include "C-kern/api/io/iochannel.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/directory.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/platform/task/thread.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif



// section: terminal_t

// group: environment-variables

// TEXTDB:SELECT('/* variable: ENVIRON_'id\n' * Name of environment variable – 'description'. */'\n'#define ENVIRON_'id' "'envname'"')FROM("C-kern/resource/config/environvars")WHERE(id=='TERM')
/* variable: ENVIRON_TERM
 * Name of environment variable – used to determine the terminal type. */
#define ENVIRON_TERM "TERM"
// TEXTDB:END

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_terminal_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_terminal_errtimer = test_errortimer_FREE;
#endif

// group: helper

/* function: readconfig
 * Lese Konfiguration von Terminal verbunden mit Filedescriptor fd nach tconf.
 *
 * Return:
 * 0      - OK, tconf ist gültig.
 * ENOTTY - fd ist nicht mit einem (Pseudo-)Terminal verbunden.
 *
 * Hintergrundwissen Kommandozeile:
 *
 * Die Konfiguratin des Terminals kann mittels des Kommandos "stty -a"
 * (siehe auch: man 1 stty) auf der Kommandozeile angezeigt werden.
 *
 * Mittels "stty intr ^C" setzt man etwa die Controltaste "Control-C" zum Erzeugen
 * des Interrupts SIGINT, der an den im Vordergrund laufenden Prozess gesandt wird
 * ("Control-C" ist die Defaultbelegung).
 *
 * Der Parameter "^C" kann entweder als '^' und dann 'C' eingegeben werden oder
 * als numerischer Wert (dezimal 3, oktal 03 oder hexadezimal 0x3). Die dritte
 * Möglichkeit ist mittels Control-V und Control-C. Der Controlcode ^V sorgt dafür,
 * daß die nachfolgende Taste als Wert genommen wird und nicht in ihrer besonderen
 * Control-Funktion interpretiert wird. Falls Control-C nicht speziell interpretiert
 * wird, kann auch nur Control-C (ohne ^V davor) eingegeben werden.
 *
 * */
static inline int readconfig(/*out*/struct termios* tconf, sys_iochannel_t fd)
{
   int err;

   err = tcgetattr(fd, tconf);
   if (err) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

static inline int writeconfig(struct termios* tconf, sys_iochannel_t fd)
{
   int err;

   do {
      err = tcsetattr(fd, TCSAFLUSH, tconf);
      if (err) {
         err = errno;
         if (err != EINTR) goto ONERR;
      }
   } while (err);

   return 0;
ONERR:
   return err;
}

static inline int readwinsize(/*out*/struct winsize* size, sys_iochannel_t fd)
{
   int err;

   if (ioctl(fd, TIOCGWINSZ, size)) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

static inline int writewinsize(struct winsize* size, sys_iochannel_t fd)
{
   int err;

   if (ioctl(fd, TIOCSWINSZ, size)) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

/* function: configstore
 * Calls <readconfig> and stores values into <terminal_t.ctrl_lnext> ... <terminal_t.oldconf_onlcr>. */
static inline int configstore(/*out*/terminal_t* term, sys_iochannel_t fd)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, fd);
   SETONERROR_testerrortimer(&s_terminal_errtimer, &err);
   if (err) goto ONERR;

   term->ctrl_lnext     = tconf.c_cc[VLNEXT];
   term->ctrl_susp      = tconf.c_cc[VSUSP];
   term->oldconf_vmin   = tconf.c_cc[VMIN];
   term->oldconf_vtime  = tconf.c_cc[VTIME];
   term->oldconf_echo   = (0 != (tconf.c_lflag & ECHO));
   term->oldconf_icanon = (0 != (tconf.c_lflag & ICANON));
   term->oldconf_icrnl  = (0 != (tconf.c_iflag & ICRNL));
   term->oldconf_isig   = (0 != (tconf.c_lflag & ISIG));
   term->oldconf_ixon   = (0 != (tconf.c_iflag & IXON));
   term->oldconf_onlcr  = (0 != (tconf.c_oflag & ONLCR));

   return 0;
ONERR:
   return err;
}

// group: lifetime

/* define: INIT
 * Helper macro to set all fields of terminal_t. */
#define INIT(term, _io, _doclose) \
         /* inits all term->oldconf_<name> values */ \
         err = configstore(term, _io); \
         if (err) goto ONERR;          \
         (term)->sysio   = _io;        \
         (term)->doclose = _doclose;


int init_terminal(/*out*/terminal_t* term)
{
   int err;
   file_t sysio = iochannel_STDIN;
   bool doclose = false;

   if (  ! iscontrolling_terminal(sysio)) {
      ONERROR_testerrortimer(&s_terminal_errtimer, &err, ONERR);
      err = init_file(&sysio, "/dev/tty", accessmode_RDWR, 0);
      if (err) goto ONERR;
      doclose = true;
   }

   INIT(term, sysio, doclose);

   return 0;
ONERR:
   if (doclose) {
      free_file(&sysio);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initPpath_terminal(/*out*/terminal_t* term, const uint8_t* path)
{
   int err;
   int sysio = open((const char*)path, O_RDWR|O_CLOEXEC|O_NONBLOCK);
   if (sysio == -1) {
      err = errno;
      TRACESYSCALL_ERRLOG("open(path)", err);
      PRINTCSTR_ERRLOG(path);
      goto ONERR;
   }

   if (!isatty(sysio)) {
      err = ENOTTY;
      goto ONERR;
   }

   INIT(term, sysio, true);

   return 0;
ONERR:
   if (sysio != -1) close(sysio);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initPio_terminal(/*out*/terminal_t* term, sys_iochannel_t io, bool doClose)
{
   int err;

   if (!isatty(io)) {
      err = ENOTTY;
      goto ONERR;
   }

   INIT(term, io, doClose);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_terminal(terminal_t* term)
{
   int err;

   if (term->doclose) {
      term->doclose = false;

      err = free_file(&term->sysio);
      SETONERROR_testerrortimer(&s_terminal_errtimer, &err);

      if (err) goto ONERR;

   } else {
      term->sysio = sys_iochannel_FREE;
   }

   return 0;
ONERR:
   return err;
}

// group: query

/* function: hascontrolling_terminal
 * If a process has a controlling terminal, opening the special file /dev/tty obtains
 * a file descriptor for that terminal. This is useful if standard input and output are redirected,
 * and a program wants to ensure that it is communicating with the controlling terminal. */
bool hascontrolling_terminal(void)
{
   if (! iscontrolling_terminal(sys_iochannel_STDIN)) {
      int fd = open("/dev/tty", O_RDWR|O_CLOEXEC);
      if (fd == -1) return false;

      close(fd);
   }

   return true;
}

bool is_terminal(sys_iochannel_t fd)
{
   return isatty(fd);
}

bool iscontrolling_terminal(sys_iochannel_t fd)
{
   return getsid(0) == tcgetsid(fd);
}

bool issizechange_terminal()
{
   sigset_t signset;
   siginfo_t info;
   sigemptyset(&signset);
   sigaddset(&signset, SIGWINCH);

   return (SIGWINCH == sigtimedwait(&signset, &info, & (const struct timespec) { 0, 0 }));
}

bool isutf8_terminal(terminal_t* term)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, term->sysio);
   SETONERROR_testerrortimer(&s_terminal_errtimer, &err);
   if (err) goto ONERR;

   return (tconf.c_iflag & IUTF8);
ONERR:
   TRACEEXIT_ERRLOG(err);
   return false;
}

int pathname_terminal(const terminal_t* term, uint16_t len, uint8_t name[len])
{
   int err;

   err = ttyname_r(term->sysio, (char *)name, len);
   if (err) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   if (err == ERANGE) {
      err = ENOBUFS;
   } else {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}

int waitsizechange_terminal()
{
   sigset_t sigset;
   siginfo_t info;
   sigemptyset(&sigset);
   sigaddset(&sigset, SIGWINCH);

   return (SIGWINCH == sigwaitinfo(&sigset, &info)) ? 0 : EINTR;
}

int type_terminal(uint16_t len, /*out*/uint8_t type[len])
{
   const char * envterm = getenv(ENVIRON_TERM);

   if (!envterm) return ENODATA;

   size_t envlen = strlen(envterm);
   if (envlen >= len) return ENOBUFS;

   memcpy(type, envterm, envlen+1u);

   return 0;
}

// group: read

size_t tryread_terminal(terminal_t* term, size_t len, /*out*/uint8_t keys[len])
{
   size_t        nrbytes = 0;
   struct pollfd pfd = { .events = POLLIN, .fd = term->sysio };

   for (unsigned i = 0; i < 5 && nrbytes < len; ++i) {
      int nr = poll(&pfd, 1, 10);
      if (1 == nr) {
         if (! (pfd.revents & POLLIN)) break;
         ssize_t bytes;
         do {
            bytes = read(term->sysio, keys+nrbytes, len+nrbytes);
         } while (bytes == -1 && errno == EINTR);
         if (bytes > 0) nrbytes += (size_t) bytes;
      }
   }

   return nrbytes;
}

int size_terminal(terminal_t* term, uint16_t* nrcolsX, uint16_t* nrrowsY)
{
   int err;
   struct winsize size;

   err = readwinsize(&size, term->sysio);
   if (err) goto ONERR;

   *nrcolsX = size.ws_col;
   *nrrowsY = size.ws_row;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: update

int removecontrolling_terminal(void)
{
   int err;

   if (iscontrolling_terminal(sys_iochannel_STDIN)) {
      err = ioctl(sys_iochannel_STDIN, TIOCNOTTY);

   } else {
      int fd = open("/dev/tty", O_RDWR|O_CLOEXEC);
      if (fd == -1) {
         err = errno;
         goto ONERR;
      }
      err = ioctl(fd, TIOCNOTTY);
      close(fd);
   }

   if (err) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int setsize_terminal(terminal_t* term, uint16_t nrcolsX, uint16_t nrrowsY)
{
   int err;
   struct winsize size = { .ws_col = nrcolsX, .ws_row = nrrowsY };

   err = writewinsize(&size, term->sysio);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int setstdio_terminal(terminal_t* term)
{
   int err;
   int fd;

   if (term->sysio != iochannel_STDIN) {
      fd = dup3(term->sysio, iochannel_STDIN, O_CLOEXEC);
      if (-1 == fd) {
         err = errno;
         TRACESYSCALL_ERRLOG("dup3(sysio, _STDIN, O_CLOEXEC)", err);
         PRINTINT_ERRLOG(term->sysio);
         PRINTINT_ERRLOG(iochannel_STDIN);
         goto ONERR;
      }
   }

   if (term->sysio != iochannel_STDOUT) {
      fd = dup3(term->sysio, iochannel_STDOUT, O_CLOEXEC);
      if (-1 == fd) {
         err = errno;
         TRACESYSCALL_ERRLOG("dup3(sysio, _STDOUT, O_CLOEXEC)", err);
         PRINTINT_ERRLOG(term->sysio);
         PRINTINT_ERRLOG(iochannel_STDOUT);
         goto ONERR;
      }
   }

   if (term->sysio != iochannel_STDERR) {
      fd = dup3(term->sysio, iochannel_STDERR, O_CLOEXEC);
      if (-1 == fd) {
         err = errno;
         TRACESYSCALL_ERRLOG("dup3(sysio, _STDERR, O_CLOEXEC)", err);
         PRINTINT_ERRLOG(term->sysio);
         PRINTINT_ERRLOG(iochannel_STDERR);
         goto ONERR;
      }
   }

   if (term->doclose) {
      static_assert( iochannel_STDIN == 0 && iochannel_STDOUT == 1 && iochannel_STDERR == 2, );
      term->doclose = (term->sysio > iochannel_STDERR);
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int switchcontrolling_terminal(const uint8_t* path)
{
   int err;
   terminal_t term = terminal_FREE;

   // check that path is OK
   err = initPpath_terminal(&term, path);
   if (err) goto ONERR;
   err = free_terminal(&term);
   if (err) goto ONERR;

   // new sesion detaches current process from controlling terminal
   if ((pid_t) -1 == setsid()) {
      err = errno;
      TRACESYSCALL_ERRLOG("setsid", err);
      goto ONERR;
   }

   // opening terminal now makes it the controlling terminal and
   // the calling process is the controlling process
   err = initPpath_terminal(&term, path);
   if (err) goto ONERR;

   // connect standard I/O with new controlling terminal
   err = setstdio_terminal(&term);
   if (err) goto ONERR;

   // done
   err = free_terminal(&term);
   if (err) goto ONERR;

   return 0;
ONERR:
   free_terminal(&term);
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: config line discipline

int configcopy_terminal(terminal_t* dest, terminal_t* src)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, src->sysio);
   if (err) goto ONERR;

   err = writeconfig(&tconf, dest->sysio);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int configstore_terminal(terminal_t* term)
{
   int err;

   err = configstore(term, term->sysio);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int configrestore_terminal(terminal_t* term)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, term->sysio);
   if (err) goto ONERR;

   tconf.c_cc[VMIN]  = term->oldconf_vmin;
   tconf.c_cc[VTIME] = term->oldconf_vtime;
   if (term->oldconf_icrnl) {
      tconf.c_iflag |= ICRNL;
   }
   if (term->oldconf_ixon) {
      tconf.c_iflag |= IXON;
   }
   if (term->oldconf_onlcr) {
      tconf.c_oflag |= ONLCR;
   }
   if (term->oldconf_icanon) {
      tconf.c_lflag |= ICANON;
   }
   if (term->oldconf_echo) {
      tconf.c_lflag |= ECHO;
   }
   if (term->oldconf_isig) {
      tconf.c_lflag |= ISIG;
   }

   err = writeconfig(&tconf, term->sysio);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int configrawedit_terminal(terminal_t* term)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, term->sysio);
   if (err) goto ONERR;

   // set raw mode, receive characters immediately, and receive them unaltered
   // turn off signal generating with CTRL-C, CTRL-\, CTRL-Z

   tconf.c_iflag &= (unsigned) ~(ICRNL|IXON);
   tconf.c_oflag &= (unsigned) ~ONLCR;
   tconf.c_lflag &= (unsigned) ~(ICANON/*char mode*/ | ECHO/*echo off*/ | ISIG/*no signals*/);
   tconf.c_cc[VMIN]  = 1;
   tconf.c_cc[VTIME] = 0;

   err = writeconfig(&tconf, term->sysio);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}




// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_helper(void)
{
   struct termios tconf;
   struct termios tconf2;
   struct termios oldconf;
   struct winsize size;
   struct winsize size2 = { 0 };
   terminal_t     term;
   terminal_t     term2;
   file_t         file = file_FREE;
   bool           isoldconf = false;

   // prepare
   TEST(0 == readconfig(&oldconf, sys_iochannel_STDIN));
   isoldconf = true;
   memset(&tconf, 0, sizeof(tconf));
   memset(&tconf2, 0, sizeof(tconf2));
   memset(&term, 0, sizeof(term));
   memset(&term2, 0, sizeof(term2));
   TEST(0 == memcmp(&tconf, &tconf2, sizeof(tconf)));
   TEST(0 == initcreate_file(&file, "./xxx", 0));

   // TEST readconfig
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));
   TEST(0 != memcmp(&tconf, &tconf2, sizeof(tconf)));
   memset(&tconf, 0, sizeof(tconf));

   // TEST readconfig: ENOTTY
   TEST(ENOTTY == readconfig(&tconf, io_file(file)));
   TEST(0 == memcmp(&tconf, &tconf2, sizeof(tconf)));

   // TEST readconfig: EBADF
   TEST(EBADF  == readconfig(&tconf, sys_iochannel_FREE));
   TEST(0 == memcmp(&tconf, &tconf2, sizeof(tconf)));

   // TEST writeconfig
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));
   TEST(0 == writeconfig(&tconf, sys_iochannel_STDIN));

   // TEST readwinsize
   TEST(0 == readwinsize(&size, sys_iochannel_STDIN));
   TEST(0 <  size.ws_col);
   TEST(0 <  size.ws_row);

   // TEST readwinsize: ENOTTY
   memset(&size, 0, sizeof(size));
   TEST(ENOTTY == readwinsize(&size, io_file(file)));
   TEST(0 == memcmp(&size, &size2, sizeof(size)));

   // TEST readwinsize: EBADF
   TEST(EBADF == readwinsize(&size, sys_iochannel_FREE));
   TEST(0 == memcmp(&size, &size2, sizeof(size)));

   // TEST writewinsize: size changes
   TEST(0 == readwinsize(&size2, sys_iochannel_STDIN));
   size2.ws_col = (uint16_t) (size2.ws_col - 3);
   size2.ws_row = (uint16_t) (size2.ws_row - 1);
   TEST(! issizechange_terminal());
   TEST(0 == writewinsize(&size2, sys_iochannel_STDIN));
   TEST( issizechange_terminal());
   TEST(0 == readwinsize(&size, sys_iochannel_STDIN));
   TEST(size.ws_col == size2.ws_col);
   TEST(size.ws_row == size2.ws_row);
   size2.ws_col = (uint16_t) (size2.ws_col + 3);
   size2.ws_row = (uint16_t) (size2.ws_row + 1);
   TEST(! issizechange_terminal());
   TEST(0 == writewinsize(&size2, sys_iochannel_STDIN));
   TEST( issizechange_terminal());
   TEST(0 == readwinsize(&size, sys_iochannel_STDIN));
   TEST(size.ws_col == size2.ws_col);
   TEST(size.ws_row == size2.ws_row);

   // TEST writewinsize: size does not change
   TEST(0 == writewinsize(&size2, sys_iochannel_STDIN));
   TEST(! issizechange_terminal());

   // TEST writewinsize: ENOTTY
   TEST(ENOTTY == writewinsize(&size, io_file(file)));

   // TEST writewinsize: EBADF
   TEST(EBADF == writewinsize(&size, sys_iochannel_FREE));

   // TEST configstore
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));
   for (int i = 0; i < 10; ++i) {
      for (int state = 0; state < 2; ++state) {
         memcpy(&tconf2, &tconf, sizeof(tconf2));

         // change config
         switch (i) {
         case 0: tconf2.c_cc[VMIN]  = state ? 10 : 0; break;
         case 1: tconf2.c_cc[VTIME] = state ? 10 : 0; break;
         case 2: tconf2.c_lflag    &= (unsigned) ~ECHO; if (state) tconf2.c_lflag |= ECHO; break;
         case 3: tconf2.c_lflag    &= (unsigned) ~ICANON; if (state) tconf2.c_lflag |= ICANON; break;
         case 4: tconf2.c_iflag    &= (unsigned) ~ICRNL; if (state) tconf2.c_iflag |= ICRNL; break;
         case 5: tconf2.c_lflag    &= (unsigned) ~ISIG; if (state) tconf2.c_lflag |= ISIG; break;
         case 6: tconf2.c_iflag    &= (unsigned) ~IXON; if (state) tconf2.c_iflag |= IXON; break;
         case 7: tconf2.c_oflag    &= (unsigned) ~ONLCR; if (state) tconf2.c_oflag |= ONLCR; break;
         case 8: tconf2.c_cc[VLNEXT] = state ? 10 : 0; break;
         case 9: tconf2.c_cc[VSUSP]  = state ? 10 : 0; break;
         default: TEST(0); break;
         }
         TEST(0 == writeconfig(&tconf2, sys_iochannel_STDIN));

         // test
         TEST(0 == configstore(&term, sys_iochannel_STDIN));
         // compare result
         TEST(0 != memcmp(&term, &term2, sizeof(term)));
         TEST(term.ctrl_lnext     == tconf2.c_cc[VLNEXT]);
         TEST(term.ctrl_susp      == tconf2.c_cc[VSUSP]);
         TEST(term.oldconf_vmin   == tconf2.c_cc[VMIN]);
         TEST(term.oldconf_vtime  == tconf2.c_cc[VTIME]);
         TEST(term.oldconf_echo   == (0 != (tconf2.c_lflag & ECHO)));
         TEST(term.oldconf_icanon == (0 != (tconf2.c_lflag & ICANON)));
         TEST(term.oldconf_icrnl  == (0 != (tconf2.c_iflag & ICRNL)));
         TEST(term.oldconf_isig   == (0 != (tconf2.c_lflag & ISIG)));
         TEST(term.oldconf_ixon   == (0 != (tconf2.c_iflag & IXON)));
         TEST(term.oldconf_onlcr  == (0 != (tconf2.c_oflag & ONLCR)));
      }
   }
   TEST(0 == writeconfig(&tconf, sys_iochannel_STDIN));

   // TEST configstore: ENOTTY
   memset(&term, 0, sizeof(term));
   TEST(ENOTTY == configstore(&term, io_file(file)));
   TEST(0 == memcmp(&term, &term2, sizeof(term)));

   // TEST configstore: EBADF
   TEST(EBADF == configstore(&term, sys_iochannel_FREE));
   TEST(0 == memcmp(&term, &term2, sizeof(term)));

   // unprepare
   TEST(0 == free_file(&file));
   TEST(0 == remove_file("./xxx", 0));
   TEST(0 == writeconfig(&oldconf, sys_iochannel_STDIN));

   return 0;
ONERR:
   if (isoldconf) {
      writeconfig(&oldconf, sys_iochannel_STDIN);
   }
   free_file(&file);
   remove_file("./xxx", 0);
   return EINVAL;
}

static int test_initfree(void)
{
   terminal_t     term = terminal_FREE;
   struct termios tconf;
   int            stdfd;
   file_t         oldstd = file_FREE;
   file_t         file = file_FREE;
   uint8_t        filename[100] = { 0 };
   uint8_t        buffer[16];
   const char*    name;

   // prepare
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));

   // TEST terminal_FREE
   TEST(isfree_file(term.sysio));
   TEST(0 == term.oldconf_vmin);
   TEST(0 == term.oldconf_vtime);
   TEST(0 == term.oldconf_echo);
   TEST(0 == term.oldconf_icanon);
   TEST(0 == term.oldconf_icrnl);
   TEST(0 == term.oldconf_isig);
   TEST(0 == term.oldconf_onlcr);
   TEST(0 == term.doclose);

   // TEST init_terminal: use sys_iochannel_STDIN
   TEST(0 == init_terminal(&term));
   TEST(sys_iochannel_STDIN  == term.sysio);
   TEST(term.ctrl_lnext     == tconf.c_cc[VLNEXT]);
   TEST(term.ctrl_susp      == tconf.c_cc[VSUSP]);
   TEST(term.oldconf_vmin   == tconf.c_cc[VMIN]);
   TEST(term.oldconf_vtime  == tconf.c_cc[VTIME]);
   TEST(term.oldconf_echo   == (0 != (tconf.c_lflag & ECHO)));
   TEST(term.oldconf_icanon == (0 != (tconf.c_lflag & ICANON)));
   TEST(term.oldconf_icrnl  == (0 != (tconf.c_iflag & ICRNL)));
   TEST(term.oldconf_isig   == (0 != (tconf.c_lflag & ISIG)));
   TEST(term.oldconf_ixon   == (0 != (tconf.c_iflag & IXON)));
   TEST(term.oldconf_onlcr  == (0 != (tconf.c_oflag & ONLCR)));
   TEST(0 == term.doclose);

   // TEST free_terminal: fd not closed
   TEST(0 == term.doclose);
   TEST(0 == free_terminal(&term));
   TEST(isfree_file(term.sysio));
   TEST(0 == term.doclose);
   TEST(isvalid_file(sys_iochannel_STDIN));

   // TEST init_terminal: ERROR
   for (unsigned i = 1; i <= 1; ++i) {
      init_testerrortimer(&s_terminal_errtimer, i, EINVAL);
      TEST(EINVAL == init_terminal(&term));
      TEST(isfree_file(term.sysio));
      TEST(0 == term.doclose);
   }

   // TEST free_terminal: NO ERROR possible (fd not closed)
   TEST(0 == init_terminal(&term));
   TEST(0 == term.doclose);
   init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
   TEST(0 == free_terminal(&term));
   TEST(isfree_file(term.sysio));
   TEST(0 == term.doclose);
   TEST(isvalid_file(sys_iochannel_STDIN));
   init_testerrortimer(&s_terminal_errtimer, 0, EINVAL);

   // prepare
   stdfd = iochannel_STDIN;
   memset(&term, 0, sizeof(term));
   oldstd = dup(stdfd);
   TEST(oldstd > 0);
   TEST(0 == close(stdfd));

   // TEST init_terminal: open file
   TEST(! isvalid_file(stdfd));
   TEST(0 == init_terminal(&term));
   TEST( isvalid_file(stdfd));
   TEST(stdfd == term.sysio);
   TEST(term.ctrl_lnext     == tconf.c_cc[VLNEXT]);
   TEST(term.ctrl_susp      == tconf.c_cc[VSUSP]);
   TEST(term.oldconf_vmin   == tconf.c_cc[VMIN]);
   TEST(term.oldconf_vtime  == tconf.c_cc[VTIME]);
   TEST(term.oldconf_echo   == (0 != (tconf.c_lflag & ECHO)));
   TEST(term.oldconf_icanon == (0 != (tconf.c_lflag & ICANON)));
   TEST(term.oldconf_icrnl  == (0 != (tconf.c_iflag & ICRNL)));
   TEST(term.oldconf_isig   == (0 != (tconf.c_lflag & ISIG)));
   TEST(term.oldconf_ixon   == (0 != (tconf.c_iflag & IXON)));
   TEST(term.oldconf_onlcr  == (0 != (tconf.c_oflag & ONLCR)));
   TEST(0 != term.doclose);

   // TEST free_terminal: fd closed
   TEST( isvalid_file(stdfd));
   TEST(0 == free_terminal(&term));
   TEST(! isvalid_file(stdfd));
   TEST(isfree_file(term.sysio));
   TEST(0 == term.doclose);

   // TEST init_terminal: ERROR
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_terminal_errtimer, i, EINVAL);
      TEST(EINVAL == init_terminal(&term));
      TEST(isfree_file(term.sysio));
      TEST(0 == term.doclose);
   }

   // TEST free_terminal: ERROR (fd closed)
   TEST(0 == init_terminal(&term));
   TEST( isvalid_file(stdfd));
   // test
   TEST(0 != term.doclose);
   init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
   TEST(EINVAL == free_terminal(&term));
   TEST(! isvalid_file(stdfd));
   TEST(isfree_file(term.sysio));
   TEST(0 == term.doclose);

   // unprepare
   TEST(stdfd == dup2(oldstd, stdfd));
   TEST(0 == close(oldstd));
   oldstd = file_FREE;
   TEST( isvalid_file(stdfd));

   // TEST initPpath_terminal
   file = posix_openpt(O_RDWR|O_NOCTTY);
   TEST(0 <  file);
   TEST(0 == grantpt(file));
   TEST(0 == unlockpt(file));
   name = ptsname(file);
   TEST(0 != name);
   TEST(0 == initPpath_terminal(&term, (const uint8_t*)name));
   // check fields
   TEST(! isfree_file(term.sysio));
   TEST(1 == term.doclose);
   // check I/O
   TEST(3 == write(term.sysio, "xyc", 3));
   TEST(3 == read(file, buffer, sizeof(buffer)));
   TEST(0 == memcmp("xyc", buffer, 3));
   TEST(4 == write(file, "asd\n", 4));
   struct pollfd pfd = { .fd = term.sysio, .events = POLLIN };
   TEST(1 == poll(&pfd, 1, 10000));
   TEST(4 == read(term.sysio, buffer, sizeof(buffer)));
   TEST(0 == memcmp("asd\n", buffer, 4));
   TEST(0 == free_terminal(&term));

   // TEST initPpath_terminal: configstore fails
   init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
   TEST(EINVAL == initPpath_terminal(&term, (const uint8_t*)name));
   TEST(isfree_file(term.sysio));
   TEST(0 == free_file(&file));

   // TEST initPpath_terminal: ENOTTY
   TEST(0 == initcreatetemp_file(&file, &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(filename), filename)));
   TEST(ENOTTY == initPpath_terminal(&term, filename));
   TEST(isfree_file(term.sysio));
   TEST(0 == removefile_directory(0, (char*)filename));
   TEST(0 == free_file(&file));

   // TEST initPpath_terminal: ENOENT
   memcpy(filename+strlen((char*)filename)-6, "xXq_Yz", 6);
   TEST(ENOENT == initPpath_terminal(&term, filename));
   TEST(isfree_file(term.sysio));

   // TEST initPpath_terminal: EFAULT (NULL parameter)
   TEST(EFAULT == initPpath_terminal(&term, 0));
   TEST(isfree_file(term.sysio));

   // TEST initPio_terminal
   for (int isClose = 0; isClose <= 1; ++isClose) {
      file = posix_openpt(O_RDWR|O_NOCTTY);
      TEST(0 <  file);
      TEST(0 == initPio_terminal(&term, file, isClose));
      // check fields
      TEST(file == term.sysio);
      TEST(isClose == term.doclose);
      TEST(0 == free_terminal(&term));
      if (!isClose) {
         TEST(0 == free_file(&file));
      }
   }

   // TEST initPio_terminal: configstore fails
   file = posix_openpt(O_RDWR|O_NOCTTY);
   TEST(0 < file);
   init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
   TEST(EINVAL == initPio_terminal(&term, file, true));
   // check fields
   TEST(isfree_file(term.sysio));
   TEST(!term.doclose);
   TEST(0 == free_file(&file));

   // TEST initPio_terminal: ENOTTY
   TEST(0 == initcreatetemp_file(&file, &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(filename), filename)));
   TEST(ENOTTY == initPio_terminal(&term, file, true));
   // check fields
   TEST(isfree_file(term.sysio));
   TEST(!term.doclose);
   TEST(0 == removefile_directory(0, (char*)filename));
   TEST(0 == free_file(&file));

   return 0;
ONERR:
   if (! isfree_file(file)) {
      if (filename[0]) {
         removefile_directory(0, (char*)filename);
      }
      free_file(&file);
   }
   if (! isfree_file(oldstd)) {
      dup2(oldstd, stdfd);
      close(oldstd);
   }
   return EINVAL;
}

static int test_enodata_typeterminal(void)
{
   uint8_t buffer[100] = {0};

   // TEST type_terminal: ENODATA
   unsetenv(ENVIRON_TERM);
   TEST(ENODATA == type_terminal(sizeof(buffer), buffer));
   TEST(0 == buffer[0]);

   return 0;
ONERR:
   return EINVAL;
}

static void test_sighandler(int signr)
{
   (void) signr;
}

static int thread_callwaitsize(void* arg)
{
   sigset_t sigmask;
   TEST(0 == sigemptyset(&sigmask));
   TEST(0 == sigaddset(&sigmask, SIGINT));
   TEST(1 == write((int)arg, "S", 1));

   int err = waitsizechange_terminal();
   TEST(0 == sigprocmask(SIG_BLOCK, &sigmask, 0));
   TEST(1 == write((int)arg, "E", 1));

   return err;
ONERR:
   return -1;
}

static int test_query(void)
{
   terminal_t     term  = terminal_FREE;
   terminal_t     term2 = terminal_FREE; // keeps unitialized
   file_t         file   = file_FREE;
   int            pfd[2] = { sys_iochannel_FREE, sys_iochannel_FREE };
   uint8_t        name[100];
   uint8_t        type[100];
   struct timeval starttime;
   struct timeval endtime;
   struct
   itimerspec     exptime;
   timer_t        timerid;
   bool           istimer = false;
   thread_t*      thread = 0;
   sigset_t       sigmask;
   sigset_t       oldmask;
   struct sigaction act;
   struct sigaction oldact;

   // prepare
   file = dup(sys_iochannel_STDERR);
   TEST(0 < file);
   TEST(0 == pipe2(pfd, O_CLOEXEC));
   TEST(0 == init_terminal(&term));
   sigevent_t sigev;
   sigev.sigev_notify = SIGEV_SIGNAL;
   sigev.sigev_signo  = SIGWINCH;
   sigev.sigev_value.sival_int = 0;
   TEST(0 == timer_create(CLOCK_MONOTONIC, &sigev, &timerid));
   istimer = true;

   // TEST io_terminal
   TEST(sys_iochannel_STDIN == io_terminal(&term));
   for (sys_iochannel_t i = 1; i; i = i << 1) {
      terminal_t tx = { .sysio = i };
      TEST(i == io_terminal(&tx));
   }

   // TEST isutf8_terminal
   TEST(0 != isutf8_terminal(&term));

   // TEST isutf8_terminal: ERROR
   init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
   TEST(0 == isutf8_terminal(&term));

   // TEST pathname_terminal
   TEST(0 == pathname_terminal(&term, sizeof(name), name));
   TEST(5 <  strlen((char*)name));
   TEST(0 == strncmp((char*)name, "/dev/", 5));

   // TEST pathname_terminal: ENOBUFS
   TEST(ENOBUFS == pathname_terminal(&term, 5, name));

   // TEST pathname_terminal: EBADF
   TEST(EBADF == pathname_terminal(&term2, sizeof(name), name));

   // TEST hascontrolling_terminal: true (false is tested in test_controlterm)
   TEST(0 != hascontrolling_terminal());

   // TEST is_terminal: true
   TEST(0 != is_terminal(sys_iochannel_STDIN));
   TEST(0 != is_terminal(sys_iochannel_STDOUT));
   TEST(0 != is_terminal(sys_iochannel_STDERR));
   TEST(0 != is_terminal(file));

   // TEST is_terminal: false
   TEST(0 == is_terminal(sys_iochannel_FREE));
   TEST(0 == is_terminal(pfd[0]));
   TEST(0 == is_terminal(pfd[1]));

   // TEST iscontrolling_terminal: true
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDIN));
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDOUT));
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDERR));
   TEST(0 != iscontrolling_terminal(file));

   // TEST iscontrolling_terminal: false
   TEST(0 == iscontrolling_terminal(sys_iochannel_FREE));
   TEST(0 == iscontrolling_terminal(pfd[0]));
   TEST(0 == iscontrolling_terminal(pfd[1]));

   // TEST issizechange_terminal
   TEST(0 == issizechange_terminal());
   raise(SIGWINCH); // send signal to self
   TEST(0 != issizechange_terminal()); // removes signal from queue
   TEST(0 == issizechange_terminal());

   // TEST waitsizechange_terminal: return 0 (signal received)
   raise(SIGWINCH);
   sigset_t pending;
   TEST(0 == sigpending(&pending));
   TEST(1 == sigismember(&pending, SIGWINCH));
   TEST(0 == waitsizechange_terminal()); // removes signal from queue
   TEST(0 == issizechange_terminal());
   TEST(0 == sigpending(&pending));
   TEST(0 == sigismember(&pending, SIGWINCH));

   // TEST waitsizechange_terminal: test waiting
   memset(&exptime, 0, sizeof(exptime));
   exptime.it_value.tv_nsec = 1000000000/10;    // 10th of a second
   TEST(0 == timer_settime(timerid, 0, &exptime, 0)); // generate SIGWINCH after timeout
   TEST(0 == gettimeofday(&starttime, 0));
   TEST(0 == waitsizechange_terminal());
   TEST(0 == gettimeofday(&endtime, 0));
   unsigned elapsedms = (unsigned) (1000 * (endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec / 1000 - starttime.tv_usec / 1000);
   TESTP(50 < elapsedms && elapsedms < 500, "elapsedms=%d", elapsedms);

   // TEST waitsizechange_terminal: EINTR (SIGINT)
   TEST(0 == sigemptyset(&sigmask));
   TEST(0 == sigaddset(&sigmask, SIGINT));
   TEST(0 == sigprocmask(SIG_UNBLOCK, &sigmask, &oldmask));
   TEST(0 == sigemptyset(&act.sa_mask));
   act.sa_handler = &test_sighandler;
   act.sa_flags = SA_RESTART; // does not work (sigwaitinfo is aborted !)
   TEST(0 == sigaction(SIGINT, &act, &oldact));
   TEST(0 == new_thread(&thread, &thread_callwaitsize, (void*)pfd[1]));
   TEST(0 == sigprocmask(SIG_SETMASK, &oldmask, 0));
   TEST(1 == read(pfd[0], name, 1)); // wait for thread_callwaitsize
   for (int i = 0; i < 100; ++i) {
      sleepms_thread(1);
      pthread_kill(thread->sys_thread, SIGINT);
      struct pollfd fds = { .events = POLLIN, .fd = pfd[0] };
      if (1 == poll(&fds, 1, 0)) break;
   }
   TEST(0 == sigaction(SIGINT, &oldact, 0));
   TEST(0 == join_thread(thread));
   TEST(EINTR == returncode_thread(thread)); // execution of handler interrupted waitsizechange
   TEST(0 == delete_thread(&thread));

   // TEST waitsizechange_terminal: EINTR (SIGSTOP / SIGCONT)
   TEST(0 == new_thread(&thread, &thread_callwaitsize, (void*)pfd[1]));
   TEST(0 == sigprocmask(SIG_SETMASK, &oldmask, 0));
   TEST(1 == read(pfd[0], name, 1)); // wait for thread_callwaitsize
   for (int i = 0; i < 100; ++i) {
      sleepms_thread(1);
      pthread_kill(thread->sys_thread, SIGSTOP);
      pthread_kill(thread->sys_thread, SIGCONT);
      struct pollfd fds = { .events = POLLIN, .fd = pfd[0] };
      if (1 == poll(&fds, 1, 0)) break;
   }
   TEST(0 == sigaction(SIGINT, &oldact, 0));
   TEST(0 == join_thread(thread));
   TEST(EINTR == returncode_thread(thread)); // execution of handler interrupted waitsizechange
   TEST(0 == delete_thread(&thread));


   // TEST type_terminal
   memset(type, 255, sizeof(type));
   TEST(0 == type_terminal(sizeof(type), type));
   size_t len = strlen((char*)type);
   TEST(0 < len && len < sizeof(type));
   if (strcmp("xterm", (char*)type) && strcmp("linux", (char*)type)) {
      logwarning_unittest("unknown terminal type (not xterm, linux)");
   }

   // TEST type_terminal: ENOBUFS
   memset(type, 0, sizeof(type));
   TEST(ENOBUFS == type_terminal((uint16_t)len, type));
   TEST(0 == type[0]);

   // TEST type_terminal: ENODATA
   int err;
   TEST(0 == execasprocess_unittest(&test_enodata_typeterminal, &err));
   TEST(0 == err);

   // TEST ctrllnext_terminal
   TEST(ctrllnext_terminal(&term) == term.ctrl_lnext);
   for (uint8_t i = 1; i; i = (uint8_t)(i << 1)) {
      term2.ctrl_lnext = i;
      TEST(i == ctrllnext_terminal(&term2));
   }

   // TEST ctrlsusp_terminal
   TEST(ctrlsusp_terminal(&term) == term.ctrl_susp);
   for (uint8_t i = 1; i; i = (uint8_t)(i << 1)) {
      term2.ctrl_susp = i;
      TEST(i == ctrlsusp_terminal(&term2));
   }

   // unprepare
   istimer = false;
   TEST(0 == timer_delete(timerid));
   TEST(0 == close(file));
   TEST(0 == close(pfd[0]));
   TEST(0 == close(pfd[1]));
   TEST(0 == free_terminal(&term));

   return 0;
ONERR:
   if (istimer) {
      timer_delete(timerid);
   }
   close(file);
   close(pfd[0]);
   close(pfd[1]);
   free_terminal(&term);
   return EINVAL;
}

static void sigalarm_signalhandler(int nr)
{
   (void) nr;
   // interrupts systemcalls with EINTR
}

static int test_read(void)
{
   terminal_t     term = terminal_FREE;
   struct winsize oldsize;
   uint16_t       nrcols;
   uint16_t       nrrows;
   systimer_t     timer = systimer_FREE;
   uint64_t       duration_ms;
   int            fd[2];
   uint8_t        buf[10];
   struct sigaction sigact;
   struct sigaction old_sigact;
   sigset_t       sigset;
   sigset_t       old_sigset;

   // prepare
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   // TEST(0 == init_terminal(&term));
   term.sysio = iochannel_STDIN;
   TEST(0 == ioctl(term.sysio, TIOCGWINSZ, &oldsize));
   tcflush(term.sysio, TCIFLUSH); // discards any pressed keys

   // TEST tryread_terminal: waits 50ms
   TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .seconds = 0, .nanosec = 1000000 }));
   TEST(0 == tryread_terminal(&term, sizeof(buf), buf));
   TEST(0 == expirationcount_systimer(timer, &duration_ms));
   TESTP(40 <= duration_ms && duration_ms <= 100, "duration=%" PRIu64, duration_ms);

   // TEST tryread_terminal: EBADF
   terminal_t term2 = terminal_FREE;
   TEST(0 == tryread_terminal(&term2, sizeof(buf), buf));

   // TEST tryread_terminal: closed pipe
   TEST(0 == pipe2(fd, O_CLOEXEC));
   TEST(0 == close(fd[1]));
   term2 = term;
   term2.sysio = fd[0];
   TEST(0 == tryread_terminal(&term2, sizeof(buf), buf));
   TEST(0 == close(fd[0]));

   // TEST tryread_terminal: read bytes from pipe
   TEST(0 == pipe2(fd, O_CLOEXEC));
   term2 = term;
   term2.sysio = fd[0];
   TEST(10 == write(fd[1], "1234567890", 10));
   memset(buf, 0, sizeof(buf));
   TEST(10 == tryread_terminal(&term2, sizeof(buf), buf));
   TEST(0 == memcmp(buf, "1234567890", 10));

   // TEST tryread_terminal: EINTR (!!ignored in tryread!!)
   // ----------------------------
   // prepare
   sigact.sa_flags   = 0;
   sigact.sa_handler = &sigalarm_signalhandler;
   sigemptyset(&sigact.sa_mask);
   TEST(0 == sigaction(SIGALRM, &sigact, &old_sigact));
   sigemptyset(&sigset);
   sigaddset(&sigset, SIGALRM);
   TEST(0 == sigprocmask(SIG_UNBLOCK, &sigset, &old_sigset));
   struct itimerval itime = { .it_interval = { .tv_usec = 100 }, .it_value = { .tv_usec = 100 } };
   TEST(0 == setitimer(ITIMER_REAL, &itime, 0));

   // test interrupt is working
   for (int i = 0; i < 3; ++i) {
      struct pollfd pfd = { .events = POLLIN, .fd = term.sysio };
      TEST(-1 == poll(&pfd, 1, -1));
      TEST(EINTR == errno);
   }

   // test (no input)
   for (int i = 0; i < 2; ++i) {
      TEST(0 == tryread_terminal(&term2, sizeof(buf), buf));
   }

   // test with input
   for (int i = 0; i < 5000; ++i) {
      int err;
      do {
         err = write(fd[1], "1234567890", 10);
      } while (err == -1 && errno == EINTR);
      TEST(10 == err);
      memset(buf, 0, sizeof(buf));
      TEST(10 == tryread_terminal(&term2, sizeof(buf), buf));
      TEST(0 == memcmp(buf, "1234567890", 10));
   }

   // unprepare
   itime.it_interval.tv_usec = 0;
   itime.it_value.tv_usec    = 0;
   TEST(0 == setitimer(ITIMER_REAL, &itime, 0));
   TEST(0 == sigprocmask(SIG_SETMASK, &old_sigset, 0));
   TEST(0 == sigaction(SIGALRM, &old_sigact, 0));
   TEST(0 == close(fd[0]));
   TEST(0 == close(fd[1]));
   // ----------------------------

   // TEST size_terminal
   nrcols = 0;
   nrrows = 0;
   TEST(0 == size_terminal(&term, &nrcols, &nrrows));
   TEST(2 < nrcols);
   TEST(2 < nrrows);
   TEST(oldsize.ws_col == nrcols);
   TEST(oldsize.ws_row == nrrows);

   // TEST size_terminal: read changed size
   tcdrain(term.sysio);
   struct winsize newsize = oldsize;
   newsize.ws_col = (unsigned short) (newsize.ws_col - 2);
   newsize.ws_row = (unsigned short) (newsize.ws_row - 2);
   TEST(0 == writewinsize(&newsize, term.sysio)); // change size
   TEST(0 != issizechange_terminal()); // we received notification
   TEST(0 == size_terminal(&term, &nrcols, &nrrows));
   TEST(newsize.ws_col == nrcols);
   TEST(newsize.ws_row == nrrows);
   TEST(0 == writewinsize(&oldsize, term.sysio)); // restore size
   TEST(0 != issizechange_terminal()); // we received notification
   TEST(0 == size_terminal(&term, &nrcols, &nrrows));
   TEST(oldsize.ws_col == nrcols);
   TEST(oldsize.ws_row == nrrows);

   // unprepare
   TEST(0 == configrestore_terminal(&term));
   TEST(0 == free_terminal(&term));
   TEST(0 == free_systimer(&timer));

   // TEST size_terminal: EBADF
   TEST(EBADF == size_terminal(&term, &nrcols, &nrrows));

   return 0;
ONERR:
   configrestore_terminal(&term);
   free_terminal(&term);
   free_systimer(&timer);
   return EINVAL;
}

static int s_param_iochannel;

static int process_setstdio(void)
{
   terminal_t term;
   bool       isclose;

   if (-1 == s_param_iochannel) {
      isclose = false;
      TEST(0 == init_terminal(&term));
      TEST(iochannel_STDIN == term.sysio);

      TEST(isvalid_iochannel(iochannel_STDOUT));
      TEST(isvalid_iochannel(iochannel_STDERR));
      TEST(0 == close(iochannel_STDOUT));
      TEST(0 == close(iochannel_STDERR));
      term.doclose = 1;

   } else {
      isclose = true;
      TEST(0 == initPio_terminal(&term, s_param_iochannel, true));

      static_assert( iochannel_STDIN+1 == iochannel_STDOUT
                     && iochannel_STDOUT+1 == iochannel_STDERR, "used in for loop");
      for (int i = iochannel_STDIN; i <= iochannel_STDERR; ++i) {
         if (s_param_iochannel != i) {
            TEST(0 == close(i));
         } else {
            isclose = false;
         }
      }
   }

   TEST(0 == setstdio_terminal(&term));
   TEST(isclose == term.doclose);
   TEST(isvalid_iochannel(iochannel_STDIN));
   TEST(isvalid_iochannel(iochannel_STDOUT));
   TEST(isvalid_iochannel(iochannel_STDERR));

   if (isclose) {
      term.doclose = 0;
      TEST(0 == setstdio_terminal(&term));
      TEST(0 == term.doclose); // does not switch flag on
   }

   return 0;
ONERR:
   return EINVAL;
}

static int process_switchcontrolling(void)
{
   const char* name;
   struct stat st;

   name = ptsname(s_param_iochannel);
   TEST(0 == stat(name, &st));
   pid_t pid = getpid();
   pid_t sid = getsid(pid);
   TEST(0 == switchcontrolling_terminal((const uint8_t*)name));
   TEST(sid != getsid(pid));
   TEST(pid == getsid(pid));
   for (int i = iochannel_STDIN; i <= iochannel_STDERR; ++i) {
      struct stat st2;
      TEST(0 == fstat(i, &st2));
      TEST(st.st_dev == st2.st_dev);
      TEST(st.st_rdev == st2.st_rdev);
      TEST(st.st_ino == st2.st_ino);
      TEST(iscontrolling_terminal(i));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   int err;
   terminal_t term = terminal_FREE;
   uint16_t   x, y;
   uint16_t   x2, y2;
   file_t     file = file_FREE;
   const char* name;

   // prepare
   TEST(0 == init_terminal(&term));

   // TEST setsize_terminal: size changed
   TEST(0 == size_terminal(&term, &x, &y));
   TEST(! issizechange_terminal());
   TEST(0 == setsize_terminal(&term, (uint16_t) (x+1), (uint16_t) (y+2)));
   TEST( issizechange_terminal());
   TEST(0 == size_terminal(&term, &x2, &y2));
   TEST(x2 == x+1);
   TEST(y2 == y+2);
   TEST(0 == setsize_terminal(&term, x, y));
   TEST( issizechange_terminal());
   TEST(0 == size_terminal(&term, &x2, &y2));
   TEST(x2 == x);
   TEST(y2 == y);

   // TEST setsize_terminal: size not changed
   TEST(0 == setsize_terminal(&term, x, y));
   TEST(! issizechange_terminal());

   // unprepare
   TEST(0 == free_terminal(&term));

   // TEST setsize_terminal: EBADF
   TEST(EBADF == setsize_terminal(&term, x, y));

   // TEST setstdio_terminal: set iochannel_STDIN iochannel_STDOUT
   s_param_iochannel = -1;
   TEST(0 == execasprocess_unittest(&process_setstdio, &err));
   TEST(0 == err);

   // TEST setstdio_terminal: one of iochannel_STDIN ...
   static_assert( iochannel_STDIN+1 == iochannel_STDOUT
                  && iochannel_STDOUT+1 == iochannel_STDERR, "used in for loop");
   for (int i = iochannel_STDIN; i <= iochannel_STDERR; ++i) {
      s_param_iochannel = i;
      TEST(0 == execasprocess_unittest(&process_setstdio, &err));
      TEST(0 == err);
   }

   // TEST setstdio_terminal: other fd
   file = posix_openpt(O_RDWR|O_NOCTTY);
   TEST(0 <  file);
   TEST(0 == grantpt(file));
   TEST(0 == unlockpt(file));
   name = ptsname(file);
   TEST(0 == initPpath_terminal(&term, (const uint8_t*)name));
   s_param_iochannel = term.sysio;
   TEST(term.sysio > iochannel_STDERR);
   TEST(0 == execasprocess_unittest(&process_setstdio, &err));
   TEST(0 == err);
   TEST(0 == free_terminal(&term));
   TEST(0 == free_file(&file));

   // TEST setstdio_terminal: EBADF
   TEST(EBADF == setstdio_terminal(&term));

   // TEST switchcontrolling_terminal:
   file = posix_openpt(O_RDWR|O_NOCTTY);
   TEST(0 <  file);
   TEST(0 == grantpt(file));
   TEST(0 == unlockpt(file));
   TEST(0 != ptsname(file));
   s_param_iochannel = file;
   TEST(0 == execasprocess_unittest(&process_switchcontrolling, &err));
   TEST(0 == err);
   TEST(0 == free_file(&file));

   // TEST switchcontrolling_terminal: ENOTTY
   uint8_t filename[100];
   TEST(0 == initcreatetemp_file(&file, &(wbuffer_t) wbuffer_INIT_STATIC(sizeof(filename), filename)));
   TEST(ENOTTY == switchcontrolling_terminal(filename));
   TEST(0 == free_file(&file));

   return 0;
ONERR:
   free_file(&file);
   free_terminal(&term);
   return EINVAL;
}

static int test_config(void)
{
   terminal_t     term = terminal_FREE;
   terminal_t     psterm = terminal_FREE;
   struct termios oldconf;
   struct termios tconf;
   bool           isold = false;

   // prepare
   TEST(0 == initPio_terminal(&psterm, posix_openpt(O_RDWR|O_NOCTTY), true));
   TEST(0 == init_terminal(&term));
   TEST(0 == readconfig(&oldconf, term.sysio));
   isold = true;

   // TEST configcopy_terminal
   tconf = oldconf;
   ++ tconf.c_cc[VLNEXT];
   ++ tconf.c_cc[VSUSP];
   ++ tconf.c_cc[VMIN];
   ++ tconf.c_cc[VTIME];
   tconf.c_lflag = ~ tconf.c_lflag;
   tconf.c_iflag = ~ tconf.c_iflag;
   tconf.c_oflag = ~ tconf.c_oflag;
   TEST(0 == writeconfig(&tconf, psterm.sysio));
   TEST(0 == configcopy_terminal(&psterm, &term));
   TEST(0 == readconfig(&tconf, psterm.sysio));
   TEST(tconf.c_cc[VLNEXT] == oldconf.c_cc[VLNEXT]);
   TEST(tconf.c_cc[VSUSP]  == oldconf.c_cc[VSUSP]);
   TEST(tconf.c_cc[VMIN]   == oldconf.c_cc[VMIN]);
   TEST(tconf.c_cc[VTIME]  == oldconf.c_cc[VTIME]);
   TEST(tconf.c_lflag      == oldconf.c_lflag);
   TEST(tconf.c_iflag      == oldconf.c_iflag);
   TEST(tconf.c_oflag      == oldconf.c_oflag);

   // TEST configcopy_terminal: EBADF
   {
      terminal_t term2 = terminal_FREE;
      TEST(EBADF == configcopy_terminal(&psterm, &term2));
      TEST(EBADF == configcopy_terminal(&term2, &psterm));
   }

   // TEST configstore_terminal: line edit mode
   for (int i = 0; i <= 1; ++i) {
      terminal_t term2;
      memset(&term2, i ? 255 : 0, sizeof(term2));
      term2.sysio = term.sysio;
      TEST(0 == configstore_terminal(&term2));
      TEST(term2.ctrl_lnext     == oldconf.c_cc[VLNEXT]);
      TEST(term2.ctrl_susp      == oldconf.c_cc[VSUSP]);
      TEST(term2.oldconf_vmin   == oldconf.c_cc[VMIN]);
      TEST(term2.oldconf_vtime  == oldconf.c_cc[VTIME]);
      TEST(term2.oldconf_echo   == (0 != (oldconf.c_lflag & ECHO)));
      TEST(term2.oldconf_icanon == (0 != (oldconf.c_lflag & ICANON)));
      TEST(term2.oldconf_icrnl  == (0 != (oldconf.c_iflag & ICRNL)));
      TEST(term2.oldconf_isig   == (0 != (oldconf.c_lflag & ISIG)));
      TEST(term2.oldconf_ixon   == (0 != (oldconf.c_iflag & IXON)));
      TEST(term2.oldconf_onlcr  == (0 != (oldconf.c_oflag & ONLCR)));
   }

   // TEST configstore_terminal: ERROR
   for (int i = 0; i <= 1; ++i) {
      terminal_t term2;
      terminal_t term3;
      memset(&term2, i ? 255 : 0, sizeof(term2));
      memset(&term3, i ? 255 : 0, sizeof(term2));
      init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
      TEST(EINVAL == configstore_terminal(&term2));
      TEST(0 == memcmp(&term2, &term3, sizeof(term2))); // not changed
   }

   // TEST configrawedit_terminal
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == (tconf.c_iflag & ICRNL));
   TEST(0 == (tconf.c_oflag & ONLCR));
   TEST(0 == (tconf.c_lflag & ICANON));
   TEST(0 == (tconf.c_lflag & ECHO));
   TEST(0 == (tconf.c_lflag & ISIG));
   TEST(1 == tconf.c_cc[VMIN]);
   TEST(0 == tconf.c_cc[VTIME]);
   TEST(oldconf.c_cc[VLNEXT] == tconf.c_cc[VLNEXT]);
   TEST(oldconf.c_cc[VSUSP]  == tconf.c_cc[VSUSP]);

   // TEST configstore_terminal: raw edit mode
   {
      terminal_t term2;
      memset(&term2, 255, sizeof(term2));
      term2.sysio = term.sysio;
      TEST(0 == configstore_terminal(&term2));
      TEST(term2.ctrl_lnext     == oldconf.c_cc[VLNEXT]);
      TEST(term2.ctrl_susp      == oldconf.c_cc[VSUSP]);
      TEST(term2.oldconf_vmin   == 1);
      TEST(term2.oldconf_vtime  == 0);
      TEST(term2.oldconf_echo   == 0);
      TEST(term2.oldconf_icanon == 0);
      TEST(term2.oldconf_icrnl  == 0);
      TEST(term2.oldconf_isig   == 0);
      TEST(term2.oldconf_ixon   == 0);
      TEST(term2.oldconf_onlcr  == 0);
   }

   // TEST configrestore_terminal
   TEST(0 == configrestore_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(oldconf.c_iflag == tconf.c_iflag);
   TEST(oldconf.c_oflag == tconf.c_oflag);
   TEST(oldconf.c_lflag == tconf.c_lflag);
   TEST(0 == memcmp(oldconf.c_cc, tconf.c_cc, sizeof(tconf.c_cc)))

   // TEST configrawedit_terminal, configrestore_terminal: VMIN
   if (tconf.c_cc[VMIN]) {
      tconf.c_cc[VMIN] = 0;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // sets VMIN to 1
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(1 == tconf.c_cc[VMIN]);
   for (int i = 2; i >= 0; --i) {
      term.oldconf_vmin = (uint8_t) i;
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == tconf.c_cc[VMIN]);
   }

   // TEST configrawedit_terminal, configrestore_terminal: VTIME
   if (0 == tconf.c_cc[VTIME]) {
      tconf.c_cc[VTIME] = 1;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // sets VTIME to 0
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == tconf.c_cc[VTIME]);
   for (int i = 2; i >= 0; --i) {
      term.oldconf_vtime = (uint8_t) i;
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == tconf.c_cc[VTIME]);
   }

   // TEST configrawedit_terminal, configrestore_terminal: ICRNL
   if (! (tconf.c_iflag & ICRNL)) {
      tconf.c_iflag |= ICRNL;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // clears ICRNL
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == (tconf.c_iflag & ICRNL));
   for (int i = 0; i <= 1; ++i) {
      term.oldconf_icrnl = (i != 0);
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == (0 != (tconf.c_iflag & ICRNL)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ONLCR
   if (! (tconf.c_oflag & ONLCR)) {
      tconf.c_oflag |= ONLCR;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // clears ONLCR
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == (tconf.c_oflag & ONLCR));
   for (int i = 0; i <= 1; ++i) {
      term.oldconf_onlcr = (i != 0);
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == (0 != (tconf.c_oflag & ONLCR)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ICANON
   if (! (tconf.c_lflag & ICANON)) {
      tconf.c_lflag |= ICANON;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // clears ICANON
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == (tconf.c_lflag & ICANON));
   for (int i = 0; i <= 1; ++i) {
      term.oldconf_icanon = (i != 0);
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == (0 != (tconf.c_lflag & ICANON)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ECHO
   if (! (tconf.c_lflag & ECHO)) {
      tconf.c_lflag |= ECHO;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // clears ECHO
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == (tconf.c_lflag & ECHO));
   for (int i = 0; i <= 1; ++i) {
      term.oldconf_echo = (i != 0);
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == (0 != (tconf.c_lflag & ECHO)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ISIG
   if (! (tconf.c_lflag & ISIG)) {
      tconf.c_lflag |= ISIG;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // clears ISIG
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == (tconf.c_lflag & ISIG));
   for (int i = 0; i <= 1; ++i) {
      term.oldconf_isig = (i != 0);
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == (0 != (tconf.c_lflag & ISIG)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: IXON
   if (! (tconf.c_iflag & IXON)) {
      tconf.c_iflag |= IXON;
      TEST(0 == writeconfig(&tconf, term.sysio));
   }
   // clears IXON
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == readconfig(&tconf, term.sysio));
   TEST(0 == (tconf.c_iflag & IXON));
   for (int i = 0; i <= 1; ++i) {
      term.oldconf_ixon = (i != 0);
      TEST(0 == configrestore_terminal(&term));
      TEST(0 == readconfig(&tconf, term.sysio));
      TEST(i == (0 != (tconf.c_iflag & IXON)));
   }

   // unprepare
   isold = false;
   TEST(0 == writeconfig(&oldconf, term.sysio));
   TEST(0 == free_terminal(&term));
   TEST(0 == free_terminal(&psterm));

   return 0;
ONERR:
   if (isold) {
      writeconfig(&oldconf, term.sysio);
   }
   free_terminal(&term);
   free_terminal(&psterm);
   return EINVAL;
}

static int test_doremove1(void)
{
   resourceusage_t   usage = resourceusage_FREE;

   TEST(0 == init_resourceusage(&usage));

   // TEST hascontrolling_terminal: true; STDIN
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDIN));
   TEST(0 != hascontrolling_terminal());

   // TEST removecontrolling_terminal: STDIN
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDIN));
   removecontrolling_terminal();

   // TEST hascontrolling_terminal: false
   TEST(0 == hascontrolling_terminal());

   // TEST removecontrolling_terminal: ENXIO (already removed)
   TEST(ENXIO == removecontrolling_terminal());

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

static int test_doremove2(void)
{
   resourceusage_t   usage = resourceusage_FREE;

   TEST(0 == init_resourceusage(&usage));

   // TEST hascontrolling_terminal: true; open /dev/tty
   TEST(0 == iscontrolling_terminal(sys_iochannel_STDIN));
   TEST(0 != hascontrolling_terminal());

   // TEST removecontrolling_terminal: open /dev/tty
   TEST(0 == iscontrolling_terminal(sys_iochannel_STDIN));
   removecontrolling_terminal();

   // TEST hascontrolling_terminal: false
   TEST(0 == hascontrolling_terminal());

   // TEST removecontrolling_terminal: ENXIO (already removed)
   TEST(ENXIO == removecontrolling_terminal());

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

static int test_doremove3(void)
{
   resourceusage_t   usage = resourceusage_FREE;

   TEST(0 == init_resourceusage(&usage));

   // TEST hascontrolling_terminal: true
   TEST(0 != hascontrolling_terminal());

   // TEST setsid: chaning session id is same as removing control terminal
   TEST(getpid() == setsid());

   // TEST hascontrolling_terminal: false
   TEST(0 == hascontrolling_terminal());

   // TEST removecontrolling_terminal: ENXIO (already removed)
   TEST(ENXIO == removecontrolling_terminal());

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

static int test_controlterm(void)
{
   int err;
   file_t   oldstdin = file_FREE;

   // TEST hascontrolling_terminal, removecontrolling_terminal: sys_iochannel_STDIN
   TEST(0 == execasprocess_unittest(&test_doremove1, &err));
   TEST(0 == execasprocess_unittest(&test_doremove3, &err));
   TEST(0 == err);

   // prepare
   oldstdin = dup(sys_iochannel_STDIN);
   TEST(oldstdin > 0);
   close(sys_iochannel_STDIN);

   // TEST hascontrolling_terminal, removecontrolling_terminal: open /dev/tty
   TEST(0 == execasprocess_unittest(&test_doremove2, &err));
   TEST(0 == execasprocess_unittest(&test_doremove3, &err));
   TEST(0 == err);

   // unprepare
   TEST(sys_iochannel_STDIN == dup2(oldstdin, sys_iochannel_STDIN));
   TEST(0 == close(oldstdin));
   oldstdin = file_FREE;

   return 0;
ONERR:
   if (! isfree_file(oldstdin)) {
      dup2(oldstdin, sys_iochannel_STDIN);
      close(oldstdin);
   }
   return EINVAL;
}


int unittest_io_terminal_terminal()
{
   uint8_t    termpath1[128];
   uint8_t    termpath2[128];

   // get path to controlling terminal
   TEST(0 == ttyname_r(sys_iochannel_STDIN, (char*) termpath1, sizeof(termpath1)));

   if (test_helper())         goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;
   if (test_config())         goto ONERR;
   if (test_read())           goto ONERR;
   if (test_controlterm())    goto ONERR;

   // check controlling terminal has not changed
   TEST(getsid(0) == tcgetsid(sys_iochannel_STDIN));
   TEST(0 == ttyname_r(sys_iochannel_STDIN, (char*) termpath2, sizeof(termpath2)));
   TEST(0 == strcmp((char*)termpath1, (char*)termpath2));

   return 0;
ONERR:
   return EINVAL;
}

#endif
