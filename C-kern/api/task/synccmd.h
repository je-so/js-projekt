/* title: SyncCommand

   Beschreibt das Kommando, das and <syncfunc_f> gesendet wird
   und das als Rückgabewert in Return erwartet wird.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/synccmd.h
    Header file <SyncCommand>.
*/
#ifndef CKERN_TASK_SYNCCOMMAND_HEADER
#define CKERN_TASK_SYNCCOMMAND_HEADER

/* enums: synccmd_e
 * Kommando Rückgabe Parameter.
 * Wird verwendet als »return« Wert von <syncfunc_f>.
 *
 * synccmd_RUN         - Als Rückgabewert verlangt es, die nächste Ausführung mit dem Kommando <synccmd_RUN>
 *                       zu starten – genau an der Stelle, die in <syncfunc_t.contlabel> abgelegt wurde.
 *                       Wird 0 in <syncfunc_t.contlabel> abgelegt, wird die nächste Ausführung
 *                       am Anfang der Funktion gestartet.
 * synccmd_EXIT        - Als Rückgabewert verlangt es, die Funktion nicht mehr aufzurufen, da die Berechnung beendet wurde.
 *                       Der Rückgabewert muss vorher in <syncfunc_param_t.err> abgelegt werden.
 *                       Die Funktion <syncfunc_t.exit_syncfunc> übernimmt diese Aufgabe. Wert 0 wird als Erfolg und ein Wert > 0
 *                       als Fehlercode interpretiert.
 * synccmd_WAIT       -  Als Rückgabewert bedeutet es, daß vor der nächsten Ausführung die durch die Variable
 *                       <syncfunc_param_t.condition> referenzierte Bedingung erfüllt sein muss.
 *                       Die Ausführung pausiert und wird bei Erfüllung der Bedingung wieder aufgenommen.
 *                       Also genau an der Stelle wieder gestartet, die vor Return in <syncfunc_t.contlabel> abgelegt wurde.
 *                       Der abgelegte Wert in <syncfunc_t.contlabel> darf dabei nicht 0 sein!
 *                       Falls die Warteoperation mangels Ressourcen nicht durchgeführt werden konnte,
 *                       oder <syncfunc_param_t.condition> 0 ist, wird in <syncfunc_param_t.err> anstatt 0 ein Fehlercode != 0 abgelegt.
 *                       Die Funktionen <syncfunc_t.wait_syncfunc> implementiert dieses Protokoll.
 * Invalid Value      -  Als Rückgabewert bedeutet es, daß es als <synccmd_RUN> interpretiert wird.
 * */
typedef enum synccmd_e {
   synccmd_RUN,
   synccmd_EXIT,
   synccmd_WAIT
} synccmd_e;


#endif
