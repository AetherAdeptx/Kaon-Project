#include <stddef.h>
#include <stdint.h>

#include "io.h"
#include "vga.h"

enum {
    VGA_COLOR_WHITE_ON_BLACK = 0x0F,
    VGA_CRTC_INDEX_PORT = 0x3D4,
    VGA_CRTC_DATA_PORT = 0x3D5,
    VGA_CURSOR_START_REGISTER = 0x0A,
    VGA_CURSOR_DISABLED = 0x20
};

static volatile uint16_t *const vga_buffer =
    (volatile uint16_t *)(uintptr_t)0xB8000;
static size_t vga_row;
static size_t vga_column;

static uint16_t vga_entry(char character)
{
    return (uint16_t)(unsigned char)character
        | ((uint16_t)VGA_COLOR_WHITE_ON_BLACK << 8);
}

static void vga_new_line(void)
{
    vga_column = 0;

    if (++vga_row == VGA_HEIGHT) {
        vga_row = 0;
    }
}

void vga_initialize(void)
{
    vga_row = 0;
    vga_column = 0;

    vga_disable_hardware_cursor();
    vga_clear();
}

void vga_disable_hardware_cursor(void)
{
    io_write8(VGA_CRTC_INDEX_PORT, VGA_CURSOR_START_REGISTER);
    io_write8(VGA_CRTC_DATA_PORT, VGA_CURSOR_DISABLED);
}

void vga_clear(void)
{
    vga_row = 0;
    vga_column = 0;

    for (size_t index = 0; index < VGA_WIDTH * VGA_HEIGHT; ++index) {
        vga_buffer[index] = vga_entry(' ');
    }
}

void vga_write_at(size_t column, size_t row, char character)
{
    if (column < VGA_WIDTH && row < VGA_HEIGHT) {
        vga_buffer[row * VGA_WIDTH + column] = vga_entry(character);
    }
}

void vga_write_char(char character)
{
    if (character == '\n') {
        vga_new_line();
        return;
    }

    vga_buffer[vga_row * VGA_WIDTH + vga_column] = vga_entry(character);

    if (++vga_column == VGA_WIDTH) {
        vga_new_line();
    }
}

void vga_write(const char *text)
{
    while (*text != '\0') {
        vga_write_char(*text);
        ++text;
    }
}

void vga_write_line(const char *text)
{
    vga_write(text);
    vga_write_char('\n');
}
