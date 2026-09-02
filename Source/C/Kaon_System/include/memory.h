#ifndef KAON_MEMORY_H
#define KAON_MEMORY_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t count);
void *memset(void *destination, int value, size_t count);
size_t strlen(const char *text);

#endif
