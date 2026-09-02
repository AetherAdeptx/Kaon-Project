#ifndef KAON_CONSOLE_H
#define KAON_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CONSOLE_LINE_CAPACITY = 128,
    CONSOLE_TRANSCRIPT_CAPACITY = 256,
    CONSOLE_HISTORY_CAPACITY = 100,
    CONSOLE_USER_NAME_CAPACITY = 32,
    CONSOLE_DIRECTORY_CAPACITY = 128,
    CONSOLE_HISTORY_DISK_LBA = 128,
    CONSOLE_HISTORY_DISK_SECTORS = 32
};

typedef struct {
    uint8_t bytes[CONSOLE_LINE_CAPACITY];
    uint8_t length;
} ConsoleTextLine;

typedef struct {
    size_t column;
    size_t row;
    size_t width;
    size_t height;
} ConsoleFrame;

typedef struct {
    ConsoleFrame frame;
    ConsoleTextLine transcript[CONSOLE_TRANSCRIPT_CAPACITY];
    ConsoleTextLine history[CONSOLE_HISTORY_CAPACITY];
    ConsoleTextLine input;
    ConsoleTextLine history_draft;
    uint8_t user_name[CONSOLE_USER_NAME_CAPACITY];
    uint8_t current_directory[CONSOLE_DIRECTORY_CAPACITY];
    size_t transcript_start;
    size_t transcript_count;
    size_t history_count;
    size_t history_next;
    size_t history_navigation_index;
    size_t user_name_length;
    size_t current_directory_length;
    size_t dropped_line_count;
    uint64_t cursor_ticks;
    bool initialized;
    bool drawing_enabled;
    bool redraw_requested;
    bool cursor_visible;
    bool caps_lock_enabled;
    bool history_dirty;
    bool history_persistence_available;
    bool history_navigation_active;
} ConsoleState;

void console_initialize(bool drawing_enabled);
void console_set_drawing_enabled(bool enabled);
bool console_set_user_name(const char *user_name);
bool console_set_current_directory(const char *directory);
void console_request_redraw(void);
void console_update(void);
void console_write_line(const ConsoleTextLine *line);
void console_write_text(const char *text);
bool console_flush_history(void);
bool console_get_history_entry(size_t newest_first_index,
                               ConsoleTextLine *entry);
const ConsoleState *console_get_state(void);

#endif
