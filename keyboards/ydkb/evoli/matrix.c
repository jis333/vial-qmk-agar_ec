/*
Copyright 2025 YANG <drk@live.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* rough code */

#include "ch.h"
#include "hal.h"

/*
 * scan matrix
 */
#include "action.h"
#include "print.h"
#include "debug.h"
#include "timer.h"
#include "util.h"
#include "matrix.h"
#include "debounce_pk.h"
#include "wait.h"
#include "ec_matrix.h"
#include "rgblight.h"



/* matrix state(1:on, 0:off) */
static matrix_row_t matrix[MATRIX_ROWS] = {0};
static uint8_t matrix_debouncing[MATRIX_ROWS][MATRIX_COLS] = {0};

void unselect_rows(void);
static uint8_t get_key(uint8_t row, uint8_t col);

uint16_t scan_speed=0;

__attribute__ ((weak))
void matrix_scan_user(void) {}

__attribute__ ((weak))
void matrix_scan_kb(void)
{
    matrix_scan_user();
    hook_keyboard_loop();
}

void matrix_init(void)
{
    user_config_init();
    ec_matrix_init();

    rgblight_user_init();
}

static bool process_key_press = 0;
bool should_process_keypress(void) {
    return process_key_press;
}

uint8_t matrix_scan(void)
{
    uint8_t matrix_keys_down = 0;
    uint8_t matrix_keys_scan = 0;
    static bool scan_forward = 0;
    for (uint8_t col=0; col<MATRIX_COLS; col++) {
        ec_select_col(col);
        for (uint8_t r=0; r<MATRIX_ROWS; r++) {
            uint8_t row = scan_forward? r : (MATRIX_ROWS - 1 - r);
            uint8_t *debounce = &matrix_debouncing[row][col];

            uint8_t key = get_key(row, col);
            matrix_keys_scan++;
            if (key != 0b10) {
                *debounce = (*debounce >> 1) | key;
                matrix_row_t *p_row = &matrix[row];
                matrix_row_t col_mask = ((matrix_row_t)1 << col);
                if        (*debounce > DEBOUNCE_DN_MASK) {  //debounce KEY DOWN
                    *p_row |=  col_mask;
                } else if (*debounce < DEBOUNCE_UP_MASK) { //debounce KEY UP
                    *p_row &= ~col_mask;
                }
            }
            if (*debounce) matrix_keys_down++;
        }
    }
    scan_forward ^= 1;

    // to avoid all the keys being down in some cases like KEY is connected to GND.
    process_key_press = (matrix_keys_down < matrix_keys_scan);

    static uint16_t test=0;
    static uint16_t test_timestamp = 0;
    test++;
    if (timer_elapsed(test_timestamp) >= 1000) {
        test_timestamp = timer_read();
        scan_speed = test;
        test = 0;
      #if CONSOLE_ENABLE
        ec_matrix_print();
      #endif
    }

    matrix_scan_quantum(); // qmk needs this to run hook_keyboard_loop()
    return matrix_keys_down;
}


inline
bool matrix_is_on(uint8_t row, uint8_t col)
{
    return (matrix[row] & ((matrix_row_t)1<<col));
}

inline
matrix_row_t matrix_get_row(uint8_t row)
{
    return matrix[row];
}

void matrix_print(void)
{
    print("\nr/c 0123456789ABCDEF\n");
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        print_hex8(row); print(": ");
        print_bin_reverse16(matrix_get_row(row));
        print("\n");
    }
}

uint8_t matrix_key_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        count += bitpop16(matrix[i]);
    }
    return count;
}

static uint8_t get_key(uint8_t row, uint8_t col)
{

    //if (row == 4 && (col >= 3 && col <= 10 && col != 7)) return 0;
    return ec_get_key(row, col);
}

void unselect_rows(void)
{
}

static void select_row(uint8_t row)
{
}

#include "eeprom.h"
#include "via.h"

void bootmagic_lite(void)
{

    for (uint8_t i=0; i < (DEBOUNCE_DN * 2); i++) {
        matrix_scan();
        wait_ms(2);
    }

    //check result
    uint8_t keys_down_pos[3] = {0xff, 0xff, 0xff};
    uint8_t i = 0;
    for (uint8_t row=0; row<MATRIX_ROWS; row++) {
      #ifdef MAX_ROWS
        if (row >= MAX_ROWS) break;
      #endif
        for (uint8_t col=0; col<MATRIX_COLS; col++) {
            if (matrix_get_row(row) & (1<<col)) {
                keys_down_pos[i] = row * MATRIX_COLS + col;
                if (i < 2) i++;
            }
        }
    }

    if (keys_down_pos[0] == 0) { 
        if (keys_down_pos[1] == 0xff) {
            // only esc down
            enter_bootloader();
        } else if (keys_down_pos[2] == 0xff) {
            //two keys down. if the other key is KC_E, clear eeprom.
            if (eeprom_read_byte((const uint8_t *)(VIA_EEPROM_CONFIG_END + 1 + keys_down_pos[1]*2)) == KC_E) {
                eeconfig_init_via();
            }
        }
    }
}

void early_hardware_init_pre(void)
{
    // Override hard-wired USB pullup to disconnect and reconnect
    palSetPadMode(GPIOA, 12, PAL_MODE_OUTPUT_PUSHPULL);
    palClearPad(GPIOA, 12);
    for (uint32_t i = 0; i < 800000; i++) {
        __asm__("nop");
    }
}
