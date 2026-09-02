#ifndef KAON_SYSTEM_H
#define KAON_SYSTEM_H

_Noreturn void system_run_forever(void);
_Noreturn void system_idle_forever(void);
_Noreturn void system_panic(const char *message);

#endif
