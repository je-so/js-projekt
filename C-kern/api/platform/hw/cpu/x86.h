/* title: x86-CPU-Config

   Definition der Eigenschaften einer generischen x86 CPU.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/konfig.h
    Der Header <Konfiguration> verwendet diese Datei, falls KONFIG_CPU als x86 definiert ist.

   file: C-kern/api/platform/hw/cpu/x86.h
    Definiert Eigenschaften einer generischen x86 CPU.
*/
#ifndef CKERN_PLATFORM_HW_CPU_X86_HEADER
#define CKERN_PLATFORM_HW_CPU_X86_HEADER


// section: cpu specific configurations

/* define: KONFIG_CPU_CACHE_LINESIZE
 * Konfiguration der Größe in Bytes einer Zeile des L1 CPU-Caches.
 * Linux gibt diesen Wert unter "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size" an oder er kann durch Aufruf von sysconf(_SC_LEVEL1_DCACHE_LINESIZE) ermittelt werden. */
#define KONFIG_CPU_CACHE_LINESIZE   64

#endif
