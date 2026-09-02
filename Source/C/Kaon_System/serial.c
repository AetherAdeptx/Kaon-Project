#include "io.h"
#include "serial.h"

enum {
    SERIAL_COM1 = 0x3F8,
    SERIAL_DATA = 0,
    SERIAL_INTERRUPT_ENABLE = 1,
    SERIAL_FIFO_CONTROL = 2,
    SERIAL_LINE_CONTROL = 3,
    SERIAL_MODEM_CONTROL = 4,
    SERIAL_LINE_STATUS = 5,
    SERIAL_TRANSMITTER_EMPTY = 0x20
};

void serial_initialize(void)
{
    io_write8(SERIAL_COM1 + SERIAL_INTERRUPT_ENABLE, 0x00);
    io_write8(SERIAL_COM1 + SERIAL_LINE_CONTROL, 0x80);
    io_write8(SERIAL_COM1 + SERIAL_DATA, 0x01);
    io_write8(SERIAL_COM1 + SERIAL_INTERRUPT_ENABLE, 0x00);
    io_write8(SERIAL_COM1 + SERIAL_LINE_CONTROL, 0x03);
    io_write8(SERIAL_COM1 + SERIAL_FIFO_CONTROL, 0xC7);
    io_write8(SERIAL_COM1 + SERIAL_MODEM_CONTROL, 0x03);
}

void serial_write_char(char character)
{
    while ((io_read8(SERIAL_COM1 + SERIAL_LINE_STATUS)
            & SERIAL_TRANSMITTER_EMPTY) == 0) {
    }

    io_write8(SERIAL_COM1 + SERIAL_DATA, (unsigned char)character);
}

void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_write_char('\r');
        }

        serial_write_char(*text);
        ++text;
    }
}

void serial_write_line(const char *text)
{
    serial_write(text);
    serial_write_char('\r');
    serial_write_char('\n');
}
