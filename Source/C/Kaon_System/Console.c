#include "Config.h"
#include "Console.h"
#include "Disk.h"
#include "Globals.h"
#include "Keyboard.h"
#include "memory.h"
#include "vga.h"

enum {
    CONSOLE_HISTORY_MAGIC = 0x4B48434B,
    CONSOLE_HISTORY_VERSION = 1,
    CONSOLE_HISTORY_DISK_BYTES =
        CONSOLE_HISTORY_DISK_SECTORS * DISK_SECTOR_SIZE
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t history_count;
    uint32_t history_next;
    uint32_t checksum;
    ConsoleTextLine history[CONSOLE_HISTORY_CAPACITY];
} ConsoleHistoryDiskRecord;

typedef union {
    ConsoleHistoryDiskRecord record;
    uint8_t sectors[CONSOLE_HISTORY_DISK_BYTES];
} ConsoleHistoryDiskImage;

_Static_assert(sizeof(ConsoleHistoryDiskRecord) <= CONSOLE_HISTORY_DISK_BYTES,
               "Console history exceeds its reserved disk area");

static ConsoleState console_state KAON_GLOBAL_VARIABLE;
static ConsoleHistoryDiskImage history_disk_image KAON_GLOBAL_VARIABLE;

static uint32_t checksum_byte(uint32_t checksum, uint8_t value)
{
    return (checksum ^ value) * UINT32_C(16777619);
}

static uint32_t history_checksum(const ConsoleHistoryDiskRecord *record)
{
    uint32_t checksum = UINT32_C(2166136261);

    for (size_t shift = 0; shift < 32; shift += 8) {
        checksum = checksum_byte(
            checksum, (uint8_t)(record->history_count >> shift));
        checksum = checksum_byte(
            checksum, (uint8_t)(record->history_next >> shift));
    }

    for (size_t index = 0; index < CONSOLE_HISTORY_CAPACITY; ++index) {
        checksum = checksum_byte(checksum, record->history[index].length);
        for (size_t byte = 0; byte < CONSOLE_LINE_CAPACITY; ++byte) {
            checksum = checksum_byte(
                checksum, record->history[index].bytes[byte]);
        }
    }

    return checksum;
}

static size_t copy_bounded(uint8_t *destination, size_t capacity,
                           const char *source)
{
    size_t length = 0;

    if (source == NULL) {
        return 0;
    }

    while (length < capacity && source[length] != '\0') {
        destination[length] = (uint8_t)source[length];
        ++length;
    }

    return length;
}

static bool set_bounded_text(uint8_t *destination, size_t capacity,
                             size_t *stored_length, const char *source)
{
    const size_t length = copy_bounded(destination, capacity, source);

    if (source == NULL || (length == capacity && source[length] != '\0')) {
        return false;
    }

    memset(destination + length, 0, capacity - length);
    *stored_length = length;
    console_request_redraw();
    return true;
}

static void reset_history(void)
{
    memset(console_state.history, 0, sizeof(console_state.history));
    console_state.history_count = 0;
    console_state.history_next = 0;
    console_state.history_dirty = false;
}

static void load_history(void)
{
    ConsoleHistoryDiskRecord *record = &history_disk_image.record;

    memset(&history_disk_image, 0, sizeof(history_disk_image));
    if (!disk_read_sectors(CONSOLE_HISTORY_DISK_LBA,
                           CONSOLE_HISTORY_DISK_SECTORS,
                           history_disk_image.sectors)) {
        console_state.history_persistence_available = false;
        reset_history();
        return;
    }

    console_state.history_persistence_available = true;
    if (record->magic != CONSOLE_HISTORY_MAGIC
        || record->version != CONSOLE_HISTORY_VERSION
        || record->history_count > CONSOLE_HISTORY_CAPACITY
        || record->history_next >= CONSOLE_HISTORY_CAPACITY
        || record->checksum != history_checksum(record)) {
        reset_history();
        return;
    }

    for (size_t index = 0; index < CONSOLE_HISTORY_CAPACITY; ++index) {
        if (record->history[index].length > CONSOLE_LINE_CAPACITY) {
            reset_history();
            return;
        }
    }

    memcpy(console_state.history, record->history,
           sizeof(console_state.history));
    console_state.history_count = record->history_count;
    console_state.history_next = record->history_next;
    console_state.history_dirty = false;
}

static void append_transcript_line(const ConsoleTextLine *line)
{
    size_t destination_index;

    if (console_state.transcript_count < CONSOLE_TRANSCRIPT_CAPACITY) {
        destination_index =
            (console_state.transcript_start + console_state.transcript_count)
            % CONSOLE_TRANSCRIPT_CAPACITY;
        ++console_state.transcript_count;
    } else {
        destination_index = console_state.transcript_start;
        console_state.transcript_start =
            (console_state.transcript_start + 1)
            % CONSOLE_TRANSCRIPT_CAPACITY;
        ++console_state.dropped_line_count;
    }

    console_state.transcript[destination_index] = *line;
}

static void record_command(const ConsoleTextLine *command)
{
    if (command->length == 0) {
        return;
    }

    console_state.history[console_state.history_next] = *command;
    console_state.history_next =
        (console_state.history_next + 1) % CONSOLE_HISTORY_CAPACITY;
    if (console_state.history_count < CONSOLE_HISTORY_CAPACITY) {
        ++console_state.history_count;
    }
    console_state.history_dirty = true;
}

static void stop_history_navigation(void)
{
    console_state.history_navigation_active = false;
    console_state.history_navigation_index = 0;
    memset(&console_state.history_draft, 0,
           sizeof(console_state.history_draft));
}

static void navigate_history_up(void)
{
    if (console_state.history_count == 0) {
        return;
    }

    if (!console_state.history_navigation_active) {
        console_state.history_draft = console_state.input;
        console_state.history_navigation_index = 0;
        console_state.history_navigation_active = true;
    } else if (console_state.history_navigation_index + 1
               < console_state.history_count) {
        ++console_state.history_navigation_index;
    }

    (void)console_get_history_entry(
        console_state.history_navigation_index, &console_state.input);
    console_state.redraw_requested = true;
}

static void navigate_history_down(void)
{
    if (!console_state.history_navigation_active) {
        return;
    }

    if (console_state.history_navigation_index == 0) {
        console_state.input = console_state.history_draft;
        stop_history_navigation();
    } else {
        --console_state.history_navigation_index;
        (void)console_get_history_entry(
            console_state.history_navigation_index, &console_state.input);
    }

    console_state.redraw_requested = true;
}

static char translate_scan_code(uint8_t scan_code, bool shifted,
                                bool caps_lock)
{
    static const char unshifted[128] = {
        [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
        [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
        [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
        [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
        [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
        [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
        [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
        [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
        [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
        [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
        [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
        [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x39] = ' '
    };
    static const char shifted_characters[128] = {
        [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
        [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
        [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
        [0x1A] = '{', [0x1B] = '}', [0x27] = ':', [0x28] = '"',
        [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>',
        [0x35] = '?'
    };
    char character = unshifted[scan_code];

    if (character >= 'a' && character <= 'z') {
        if (shifted != caps_lock) {
            character = (char)(character - 'a' + 'A');
        }
        return character;
    }

    if (shifted && shifted_characters[scan_code] != '\0') {
        return shifted_characters[scan_code];
    }

    return character;
}

static void finish_input_line(bool submit_command)
{
    append_transcript_line(&console_state.input);
    if (submit_command) {
        record_command(&console_state.input);
        (void)console_flush_history();
    }

    memset(&console_state.input, 0, sizeof(console_state.input));
    stop_history_navigation();
    console_state.cursor_visible = true;
    console_state.cursor_ticks = 0;
    console_state.redraw_requested = true;
}

static void handle_keyboard_event(const KeyboardEvent *event)
{
    char character;

    if (!event->pressed) {
        return;
    }

    if (event->extended) {
        if (event->scan_code == 0x48) {
            navigate_history_up();
        } else if (event->scan_code == 0x50) {
            navigate_history_down();
        }
        return;
    }

    if (event->scan_code == 0x3A) {
        console_state.caps_lock_enabled =
            !console_state.caps_lock_enabled;
        return;
    }

    if (event->scan_code == 0x1C) {
        finish_input_line(!event->shift_pressed);
        return;
    }

    if (event->scan_code == 0x39 && event->shift_pressed) {
        finish_input_line(false);
        return;
    }

    if (event->scan_code == 0x0E) {
        if (console_state.input.length != 0) {
            stop_history_navigation();
            --console_state.input.length;
            console_state.input.bytes[console_state.input.length] = 0;
            console_state.redraw_requested = true;
        }
        return;
    }

    character = translate_scan_code(
        event->scan_code, event->shift_pressed,
        console_state.caps_lock_enabled);
    if (character != '\0'
        && console_state.input.length < CONSOLE_LINE_CAPACITY) {
        stop_history_navigation();
        console_state.input.bytes[console_state.input.length] =
            (uint8_t)character;
        ++console_state.input.length;
        console_state.redraw_requested = true;
    }
}

static void draw_bytes(size_t row, size_t *column,
                       const uint8_t *bytes, size_t length)
{
    for (size_t index = 0;
         index < length && *column < VGA_WIDTH;
         ++index) {
        vga_write_at(*column, row, (char)bytes[index]);
        ++*column;
    }
}

static void draw_console(void)
{
    const size_t transcript_rows = VGA_HEIGHT - 2;
    const size_t displayed_lines =
        console_state.transcript_count < transcript_rows
            ? console_state.transcript_count : transcript_rows;
    const size_t first_displayed =
        console_state.transcript_count - displayed_lines;
    size_t column = 0;

    vga_clear();
    draw_bytes(0, &column, console_state.user_name,
               console_state.user_name_length);
    if (column < VGA_WIDTH) {
        vga_write_at(column++, 0, '@');
    }
    draw_bytes(0, &column, (const uint8_t *)"kaon:", 5);
    draw_bytes(0, &column, console_state.current_directory,
               console_state.current_directory_length);

    for (size_t displayed = 0; displayed < displayed_lines; ++displayed) {
        const size_t transcript_index =
            (console_state.transcript_start + first_displayed + displayed)
            % CONSOLE_TRANSCRIPT_CAPACITY;
        size_t line_column = 0;

        draw_bytes(displayed + 1, &line_column,
                   console_state.transcript[transcript_index].bytes,
                   console_state.transcript[transcript_index].length);
    }

    column = 0;
    vga_write_at(column++, VGA_HEIGHT - 1, '>');
    vga_write_at(column++, VGA_HEIGHT - 1, ' ');

    {
        const size_t available_columns = VGA_WIDTH - column;
        const size_t input_start =
            console_state.input.length > available_columns
                ? console_state.input.length - available_columns : 0;

        draw_bytes(VGA_HEIGHT - 1, &column,
                   console_state.input.bytes + input_start,
                   console_state.input.length - input_start);
    }

    if (console_state.cursor_visible) {
        const size_t cursor_column =
            column < VGA_WIDTH ? column : VGA_WIDTH - 1;
        vga_write_at(cursor_column, VGA_HEIGHT - 1, '_');
    }
}

void console_initialize(bool drawing_enabled)
{
    memset(&console_state, 0, sizeof(console_state));
    console_state.frame.column = 0;
    console_state.frame.row = 0;
    console_state.frame.width = VGA_WIDTH;
    console_state.frame.height = VGA_HEIGHT;
    console_state.drawing_enabled = drawing_enabled;
    console_state.redraw_requested = drawing_enabled;
    console_state.cursor_visible = true;
    console_state.initialized = true;
    (void)console_set_user_name("user");
    (void)console_set_current_directory("/");
    load_history();
}

void console_set_drawing_enabled(bool enabled)
{
    console_state.drawing_enabled = enabled;
    console_state.redraw_requested = enabled;
}

bool console_set_user_name(const char *user_name)
{
    return set_bounded_text(console_state.user_name,
                            CONSOLE_USER_NAME_CAPACITY,
                            &console_state.user_name_length, user_name);
}

bool console_set_current_directory(const char *directory)
{
    return set_bounded_text(console_state.current_directory,
                            CONSOLE_DIRECTORY_CAPACITY,
                            &console_state.current_directory_length,
                            directory);
}

void console_request_redraw(void)
{
    if (console_state.initialized) {
        console_state.redraw_requested = true;
    }
}

void console_update(void)
{
    KeyboardEvent event;

    if (!console_state.initialized || !console_state.drawing_enabled) {
        return;
    }

    while (keyboard_read_event(&event)) {
        handle_keyboard_event(&event);
    }

    ++console_state.cursor_ticks;
    if (console_state.cursor_ticks >= KAON_CONSOLE_CURSOR_BLINK_TICKS) {
        console_state.cursor_ticks = 0;
        console_state.cursor_visible = !console_state.cursor_visible;
        console_state.redraw_requested = true;
    }

    if (console_state.redraw_requested) {
        draw_console();
        console_state.redraw_requested = false;
    }
}

bool console_flush_history(void)
{
    ConsoleHistoryDiskRecord *record = &history_disk_image.record;

    if (!console_state.initialized) {
        return false;
    }

    memset(&history_disk_image, 0, sizeof(history_disk_image));
    record->magic = CONSOLE_HISTORY_MAGIC;
    record->version = CONSOLE_HISTORY_VERSION;
    record->history_count = (uint32_t)console_state.history_count;
    record->history_next = (uint32_t)console_state.history_next;
    memcpy(record->history, console_state.history,
           sizeof(console_state.history));
    record->checksum = history_checksum(record);

    if (!disk_write_sectors(CONSOLE_HISTORY_DISK_LBA,
                            CONSOLE_HISTORY_DISK_SECTORS,
                            history_disk_image.sectors)) {
        console_state.history_persistence_available = false;
        return false;
    }

    console_state.history_persistence_available = true;
    console_state.history_dirty = false;
    return true;
}

bool console_get_history_entry(size_t newest_first_index,
                               ConsoleTextLine *entry)
{
    size_t history_index;

    if (entry == NULL || newest_first_index >= console_state.history_count) {
        return false;
    }

    history_index =
        (console_state.history_next + CONSOLE_HISTORY_CAPACITY - 1
         - newest_first_index) % CONSOLE_HISTORY_CAPACITY;
    *entry = console_state.history[history_index];
    return true;
}

const ConsoleState *console_get_state(void)
{
    return &console_state;
}
