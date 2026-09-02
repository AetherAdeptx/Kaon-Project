#ifndef KAON_INFO_H
#define KAON_INFO_H

#include <stdbool.h>

#include "Console.h"

/* Runs the resident kaon-info binary when the submitted command matches it. */
bool kaon_info_execute(const ConsoleTextLine *command);

#endif
