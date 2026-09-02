#include "Globals.h"
#include "Keyboard.h"
#include "io.h"
#include "memory.h"

enum {
    PS2_DATA_PORT = 0x60,
    PS2_STATUS_PORT = 0x64,
    PS2_OUTPUT_BUFFER_FULL = 0x01,
    PS2_MOUSE_DATA = 0x20,
    PS2_EXTENDED_PREFIX = 0xE0,
    PS2_PAUSE_PREFIX = 0xE1,
    PS2_PAUSE_REMAINING_BYTES = 5,
    KEYBOARD_MAX_BYTES_PER_POLL = 32
};

static KeyboardState keyboard_state KAON_GLOBAL_VARIABLE;

static size_t key_state_index(uint8_t scan_code, bool extended)
{
    return (size_t)scan_code + (extended ? 128U : 0U);
}

static void queue_event(KeyboardEvent event)
{
    if (keyboard_state.event_count == KEYBOARD_EVENT_CAPACITY) {
        keyboard_state.read_index =
            (keyboard_state.read_index + 1) % KEYBOARD_EVENT_CAPACITY;
        --keyboard_state.event_count;
        ++keyboard_state.dropped_event_count;
    }

    keyboard_state.events[keyboard_state.write_index] = event;
    keyboard_state.write_index =
        (keyboard_state.write_index + 1) % KEYBOARD_EVENT_CAPACITY;
    ++keyboard_state.event_count;
}

static void process_scan_code(uint8_t raw_scan_code)
{
    KeyboardEvent event;
    size_t state_index;

    if (keyboard_state.pause_bytes_remaining != 0) {
        --keyboard_state.pause_bytes_remaining;
        return;
    }

    if (raw_scan_code == PS2_EXTENDED_PREFIX) {
        keyboard_state.extended_prefix = true;
        return;
    }

    if (raw_scan_code == PS2_PAUSE_PREFIX) {
        keyboard_state.extended_prefix = false;
        keyboard_state.pause_bytes_remaining = PS2_PAUSE_REMAINING_BYTES;
        return;
    }

    event.scan_code = raw_scan_code & 0x7F;
    event.pressed = (raw_scan_code & 0x80) == 0;
    event.extended = keyboard_state.extended_prefix;
    keyboard_state.extended_prefix = false;

    state_index = key_state_index(event.scan_code, event.extended);
    keyboard_state.keys_pressed[state_index] = event.pressed;
    event.shift_pressed =
        keyboard_state.keys_pressed[key_state_index(0x2A, false)]
        || keyboard_state.keys_pressed[key_state_index(0x36, false)];
    queue_event(event);
}

void keyboard_initialize(void)
{
    memset(&keyboard_state, 0, sizeof(keyboard_state));
}

void keyboard_poll(void)
{
    for (size_t byte_count = 0;
         byte_count < KEYBOARD_MAX_BYTES_PER_POLL;
         ++byte_count) {
        const uint8_t status = io_read8(PS2_STATUS_PORT);
        uint8_t data;

        if ((status & PS2_OUTPUT_BUFFER_FULL) == 0) {
            return;
        }

        data = io_read8(PS2_DATA_PORT);
        if ((status & PS2_MOUSE_DATA) == 0) {
            process_scan_code(data);
        }
    }
}

bool keyboard_has_event(void)
{
    return keyboard_state.event_count != 0;
}

bool keyboard_peek_event(KeyboardEvent *event)
{
    if (event == NULL || !keyboard_has_event()) {
        return false;
    }

    *event = keyboard_state.events[keyboard_state.read_index];
    return true;
}

bool keyboard_read_event(KeyboardEvent *event)
{
    if (!keyboard_peek_event(event)) {
        return false;
    }

    keyboard_state.read_index =
        (keyboard_state.read_index + 1) % KEYBOARD_EVENT_CAPACITY;
    --keyboard_state.event_count;
    return true;
}

bool keyboard_is_key_pressed(uint8_t scan_code, bool extended)
{
    if (scan_code >= 128) {
        return false;
    }

    return keyboard_state.keys_pressed[key_state_index(scan_code, extended)];
}

const KeyboardState *keyboard_get_state(void)
{
    return &keyboard_state;
}
