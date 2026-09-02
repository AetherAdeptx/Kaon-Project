#ifndef KAON_VGA_H
#define KAON_VGA_H

#include <stddef.h>

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25
};

void vga_initialize(void);
void vga_clear(void);
void vga_disable_hardware_cursor(void);
void vga_write_at(size_t column, size_t row, char character);
void vga_write(const char *text);
void vga_write_char(char character);
void vga_write_line(const char *text);

#endif
