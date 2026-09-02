#ifndef KAON_GLOBALS_H
#define KAON_GLOBALS_H

/* Persistent kernel globals are grouped in a linker-managed BSS section. */
#define KAON_GLOBAL_VARIABLE __attribute__((section(".kaon_globals")))

extern unsigned char __kaon_globals_start[];
extern unsigned char __kaon_globals_end[];

#endif
