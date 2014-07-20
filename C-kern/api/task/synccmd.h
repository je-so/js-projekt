/* title: SyncCommand

   Beschreibt das Kommando, das and <syncfunc_f> gesendet wird
   und das als Rückgabewert in Return erwartet wird.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/synccmd.h
    Header file <SyncCommand>.
*/
#ifndef CKERN_TASK_SYNCCOMMAND_HEADER
#define CKERN_TASK_SYNCCOMMAND_HEADER

/* enums: synccmd_e
 * Kommando Parameter. Wird verwendet für sfcmd Parameter in <syncfunc_f>
 * und als »return« Wert von <syncfunc_f>.
 *
 * synccmd_RUN         - *Ein*: Das Kommando wird von <start_syncfunc> verarbeitet und
 *                            startet die Ausführung an einer zumeist mit »ONRUN:« gekennzeichneten Stelle.
 *                       *Aus*: Als Rückgabewert verlangt es, die nächste Ausführung mit dem Kommando <synccmd_RUN>
 *                            zu starten – <syncfunc_param_t.contlabel> ist ungültig.
 * synccmd_CONTINUE    - *Ein*: Das Kommando wird von <start_syncfunc> verarbeitet und
 *                            setzt die Ausführung an der Stelle, die in <syncfunc_param_t.contlabel> gespeichert wurde.
 *                       *Aus*: Als Rückgabewert verlangt es, die nächste Ausführung mit dem Kommando <synccmd_CONTINUE>
 *                            zu starten, genau an der Stelle, die in <syncfunc_param_t.contlabel> abgelegt wurde.
 *                            Der abgelegte Wert in <syncfunc_param_t.contlabel> darf nicht 0 sein,
 *                            sonst wird die nächste Ausführung mit dem Kommando <synccmd_RUN> gestartet.
 * synccmd_EXIT        - *Ein*: Das Kommando wird von <start_syncfunc> verarbeitet und
 *                            startet die Ausführung an einer zumeist mit »ONEXIT:« gekennzeichneten Stelle.
 *                            Die Umgebung hat zu wenig Ressourcen, um die Funktion weiter auszuführen. Deshalb sollte sie
 *                            alle belegten und durch <syncfunc_param_t.state> referenzierten Ressourcen freigeben.
 *                            Danach mit dem Rückgabewert <synccmd_EXIT> zurückkehren – jeder andere »return« Wert wird aber
 *                            auch als <synccmd_EXIT> interpretiert.
 *                       *Aus*: Als Rückgabewert verlangt es, die Funktion nicht mehr aufzurufen, da die Berechnung beendet wurde.
 *                            Der als Erfolgsmeldung interpretierte Rückgabewert muss vorher in <syncfunc_param_t.retcode> abgelegt werden.
 *                            Die Funktion <syncfunc_t.exit_syncfunc> übernimmt diese Aufgabe. Wert 0 wird als Erfolg und ein Wert > 0
 *                            als Fehlercode interpretiert.
 * synccmd_WAIT       -  *Ein*: Wird für die Kommandoeingabe nicht verwendet.
 *                       *Aus*: Als Rückgabewert bedeutet es, daß vor der nächsten Ausführung die durch die Variable
 *                            <syncfunc_param_t.condition> referenzierte Bedingung erfüllt sein muss. Der spezielle Wert 0
 *                            in <syncfunc_param_t.condition> bedeutet, daß auf das Ende der zuletzt erzeugten <syncfunc_t>
 *                            gewartet werden soll – siehe <syncrunner_t>.
 *                            Die Ausführung pausiert und wird bei Erfüllung der Bedingung durch das Kommando <synccmd_CONTINUE>
 *                            wieder aufgenommen. Also genau an der Stelle wieder gestartet, die vor Return in
 *                            <syncfunc_param_t.contlabel> abgelegt wurde. Der abgelegte Wert in <syncfunc_param_t.contlabel>
 *                            darf dabei nicht 0 sein!  Falls die Warteoperation mangels Ressourcen nicht durchgeführt werden konnte,
 *                            wird in <syncfunc_param_t.waiterr> anstatt 0 ein Fehlercode != 0 abgelegt.
 *                            Falls auf die Beendigung einer anderen Funktion gewartet werden soll, aber keine weitere gestartet wurde,
 *                            wird die Warteoperation mit <syncfunc_param_t.waiterr> == EINVAL abgebrochen.
 *                            Die Funktionen <wait_syncfunc> und <waitexit_syncfunc> implementieren dieses Protokoll.
 * Invalid Value      - *Ein*: Das Kommando wird von <start_syncfunc> ignoriert.
 *                      *Aus*: Das Kommando wird als <synccmd_RUN> interpretiert.
 * */
enum synccmd_e {
   synccmd_RUN,
   synccmd_CONTINUE,
   synccmd_EXIT,
   synccmd_WAIT
} ;

typedef enum synccmd_e synccmd_e;

#endif
