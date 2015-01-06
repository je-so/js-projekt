/* title: Run-Performance-Test

   Offers function for calling every registered performance test.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/test/run/run_perftest.h
    Header file <Run-Performance-Test>.

   file: C-kern/test/run/run_perftest.c
    Implementation file <Run-Performance-Test impl>.
*/
#ifndef CKERN_TEST_RUN_PERFTEST_HEADER
#define CKERN_TEST_RUN_PERFTEST_HEADER

struct perftest_info_t;

/* typedef: perftest_f
 * Funktionssignatur der von auf <perftest_t.run_perftest> aufgerufenen Performanztest Funktionen.
 * Diese müssen nur den info Parameter befüllen. Der Rest, etwa die Ausführung des eigentlichen Tests,
 * wird von <run_perftest> übernommen. */
typedef int (*perftest_f) (/*out*/struct perftest_info_t* info);



// struct: perftest_t

// group: execute

/* function: run_perftest
 * Ruft jeden registrierten Perfomanztest des C-kern(el) Systems auf.
 * Die Liste aller Tests wird manuell verwaltet.
 * Das statische Testskript »call_all_perftest.sh«
 * in »C-kern/test/static/« überprüft, ob alle Tests aufgerufen werden. */
int run_perftest(maincontext_t* maincontext);


#endif
