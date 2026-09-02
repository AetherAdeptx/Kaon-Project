#ifndef KAON_SERIAL_H
#define KAON_SERIAL_H

void serial_initialize(void);
void serial_write(const char *text);
void serial_write_char(char character);
void serial_write_line(const char *text);

#endif
