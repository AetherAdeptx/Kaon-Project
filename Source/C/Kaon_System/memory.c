#include "memory.h"

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *destination_bytes = destination;
    const unsigned char *source_bytes = source;

    for (size_t index = 0; index < count; ++index) {
        destination_bytes[index] = source_bytes[index];
    }

    return destination;
}

void *memset(void *destination, int value, size_t count)
{
    unsigned char *bytes = destination;

    for (size_t index = 0; index < count; ++index) {
        bytes[index] = (unsigned char)value;
    }

    return destination;
}

size_t strlen(const char *text)
{
    size_t length = 0;

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}
