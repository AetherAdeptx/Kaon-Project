#include <stddef.h>
#include <stdint.h>

#include "Console.h"
#include "KaonInfo.h"
#include "Keyboard.h"
#include "Librarian.h"

static bool command_matches(const ConsoleTextLine *command,
                            const char *expected)
{
    size_t index = 0;

    while (expected[index] != '\0') {
        if (index >= command->length
            || command->bytes[index] != (uint8_t)expected[index]) {
            return false;
        }
        ++index;
    }

    return index == command->length;
}

static void line_append_char(ConsoleTextLine *line, char character)
{
    if (line->length < CONSOLE_LINE_CAPACITY) {
        line->bytes[line->length++] = (uint8_t)character;
    }
}

static void line_append_text(ConsoleTextLine *line, const char *text)
{
    while (*text != '\0') {
        line_append_char(line, *text++);
    }
}

static void line_append_unsigned(ConsoleTextLine *line, uint64_t value)
{
    char digits[20];
    size_t digit_count = 0;

    do {
        digits[digit_count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0 && digit_count < sizeof(digits));

    while (digit_count != 0) {
        line_append_char(line, digits[--digit_count]);
    }
}

static void line_append_hex(ConsoleTextLine *line, uintptr_t value)
{
    static const char hex_digits[] = "0123456789abcdef";
    bool emit_digit = false;

    line_append_text(line, "0x");
    for (size_t shift = sizeof(value) * 8; shift != 0; shift -= 4) {
        const uint8_t digit = (uint8_t)((value >> (shift - 4)) & 0x0F);

        if (digit != 0 || emit_digit || shift == 4) {
            line_append_char(line, hex_digits[digit]);
            emit_digit = true;
        }
    }
}

static void write_memory_summary(void)
{
    ConsoleTextLine line = {0};

    line_append_text(&line, "pages: ");
    line_append_unsigned(&line, librarian_get_free_page_count());
    line_append_text(&line, " free / ");
    line_append_unsigned(&line, librarian_get_page_count());
    line_append_text(&line, " total");
    console_write_line(&line);

    line = (ConsoleTextLine){0};
    line_append_text(&line, "allocation: ");
    line_append_hex(&line, librarian_get_assignment_start());
    line_append_text(&line, " - ");
    line_append_hex(&line, librarian_get_assignment_end());
    console_write_line(&line);
}

static void write_keyboard_summary(void)
{
    const KeyboardState *keyboard = keyboard_get_state();
    ConsoleTextLine line = {0};

    line_append_text(&line, "keyboard: ");
    line_append_unsigned(&line, keyboard->event_count);
    line_append_text(&line, " pending, ");
    line_append_unsigned(&line, keyboard->dropped_event_count);
    line_append_text(&line, " dropped");
    console_write_line(&line);
}

bool kaon_info_execute(const ConsoleTextLine *command)
{
    if (command == NULL) {
        return false;
    }

    if (command_matches(command, "help")) {
        console_write_text("commands: kaon-info, help");
        return true;
    }

    if (!command_matches(command, "kaon-info")) {
        return false;
    }

    console_write_text("Kaon information utility");
    write_memory_summary();
    write_keyboard_summary();
    return true;
}
