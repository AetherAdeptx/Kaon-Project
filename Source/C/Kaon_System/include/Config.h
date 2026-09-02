#ifndef KAON_CONFIG_H
#define KAON_CONFIG_H

/*
 * Librarian allocation boundaries, relative to the start of its managed RAM.
 * Both values must be 4 KiB aligned. A zero start begins immediately after
 * Librarian's metadata; a zero end uses the end of the managed region.
 */
#ifndef KAON_LIBRARIAN_ASSIGNMENT_START_OFFSET
#define KAON_LIBRARIAN_ASSIGNMENT_START_OFFSET 0
#endif

#ifndef KAON_LIBRARIAN_ASSIGNMENT_END_OFFSET
#define KAON_LIBRARIAN_ASSIGNMENT_END_OFFSET 0
#endif

/* Keyboard polling remains active regardless of this console option. */
#ifndef KAON_CONSOLE_DRAW_ENABLED
#define KAON_CONSOLE_DRAW_ENABLED 1
#endif

/* Poll-loop ticks, not milliseconds, until a hardware timer is installed. */
#ifndef KAON_CONSOLE_CURSOR_BLINK_TICKS
#define KAON_CONSOLE_CURSOR_BLINK_TICKS 1666667
#endif

#endif
