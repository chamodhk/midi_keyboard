#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include "bsp/board_api.h"
#include "tusb.h"

//--------------------------------------------------------------------
// Matrix Keyboard Configuration
//--------------------------------------------------------------------
const uint ROWS = 11;
const uint COLS = 6;

uint row_pins[11] = {0,1,2,3,4,11,12,13,17,18,19};
uint col_pins[6] = {20, 6, 7, 8, 9, 21};



uint8_t key_state[11][6] = {0};  // 1 = pressed, 0 = released
const uint LED_PIN = 25;

void matrix_init() {
    for (int r = 0; r < ROWS; r++) {
        gpio_init(row_pins[r]);
        gpio_set_dir(row_pins[r], GPIO_OUT);
        gpio_put(row_pins[r], 1); // idle HIGH
    }
    for (int c = 0; c < COLS; c++) {
        gpio_init(col_pins[c]);
        gpio_set_dir(col_pins[c], GPIO_IN);
        gpio_pull_down(col_pins[c]);
    }
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
}

// convert (row, col) → MIDI note
uint8_t get_note(int r, int c) {
    return  36 + (r * COLS) + c;
}

//--------------------------------------------------------------------
// MIDI SENDER HELPERS
//--------------------------------------------------------------------
void send_note_on(uint8_t note) {
    uint8_t cable = 0;
    uint8_t msg[3] = {0x90, note, 127};
    tud_midi_stream_write(cable, msg, 3);
}

void send_note_off(uint8_t note) {
    uint8_t cable = 0;
    uint8_t msg[3] = {0x80, note, 0};
    tud_midi_stream_write(cable, msg, 3);
}

//--------------------------------------------------------------------
// Scan one row and generate MIDI
//--------------------------------------------------------------------
void scan_row(int r, bool *any_pressed) {
    // deactivate all rows
    for (int i = 0; i < ROWS; i++) {
        gpio_put(row_pins[i], 0);
    }



    gpio_put(row_pins[r], 1);  // activate row
    sleep_us(30);

    for (int c = 0; c < COLS; c++) {
        bool pressed = gpio_get(col_pins[c]);

        if (pressed && !key_state[r][c]) {
            uint8_t note = get_note(r, c);
            send_note_on(note);
            key_state[r][c] = 1;
            *any_pressed = true;
        }
        else if (!pressed && key_state[r][c]) {
            uint8_t note = get_note(r, c);
            send_note_off(note);
            key_state[r][c] = 0;
        }

        if (pressed) *any_pressed = true;
    }
}

//--------------------------------------------------------------------
// BLINKING TASK
//--------------------------------------------------------------------
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};
static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void) {
    static uint32_t start_ms = 0;
    static bool led_state = false;

    if (board_millis() - start_ms < blink_interval_ms) return;
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = !led_state;
}

//--------------------------------------------------------------------
// MAIN
//--------------------------------------------------------------------
int main(void) {
    board_init();
    matrix_init();

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    board_init_after_tusb();

    while (1) {
        tud_task();  // TinyUSB
        led_blinking_task();

        bool any_pressed = false;
        for (int r = 0; r < ROWS; r++) {
            scan_row(r, &any_pressed);
        }

        gpio_put(LED_PIN, any_pressed);
        sleep_ms(2);
    }
}