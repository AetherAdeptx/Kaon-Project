#ifndef KAON_KEYBOARD_H
#define KAON_KEYBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    KEYBOARD_EVENT_CAPACITY = 128,
    KEYBOARD_KEY_STATE_COUNT = 256
};

typedef struct {
    uint8_t scan_code;
    bool pressed;
    bool extended;
    bool shift_pressed;
} KeyboardEvent;

typedef struct {
    KeyboardEvent events[KEYBOARD_EVENT_CAPACITY];
    bool keys_pressed[KEYBOARD_KEY_STATE_COUNT];
    size_t read_index;
    size_t write_index;
    size_t event_count;
    size_t dropped_event_count;
    bool extended_prefix;
    uint8_t pause_bytes_remaining;
} KeyboardState;

void keyboard_initialize(void);
void keyboard_poll(void);
bool keyboard_has_event(void);
bool keyboard_peek_event(KeyboardEvent *event);
bool keyboard_read_event(KeyboardEvent *event);
bool keyboard_is_key_pressed(uint8_t scan_code, bool extended);
const KeyboardState *keyboard_get_state(void);

#endif
