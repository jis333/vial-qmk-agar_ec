#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "led.h"
#include "rgblight.h"
#include "raw_hid.h"

extern uint8_t ec_actuation_point[MATRIX_ROWS][MATRIX_COLS];
extern uint8_t ec_key_value[MATRIX_ROWS][MATRIX_COLS];
extern bool print_matrix_adc;

void ec_matrix_init(void);
void ec_matrix_print(void);
void ec_select_col(uint8_t col);
uint8_t ec_get_key(uint8_t row, uint8_t col);
void hook_keyboard_loop(void);
void user_config_init(void);
void rgblight_user_init(void);
void enter_bootloader(void);
void rgb_extra_process(LED_TYPE *rgbled);
void ec_apc_set(uint8_t level);
void rprint(char *msg);

/*  SMPx[2:0]
    000: 1.5 cycles    100: 41.5 cycles
    001: 7.5 cycles    101: 55.5 cycles
    010: 13.5 cycles    110: 71.5 cycles
    011: 28.5 cycles    111: 239.5 cycles  */
    
#ifndef VALID_EC_CHECK_MIN
#define VALID_EC_CHECK_MIN 5
#endif
#ifndef VALID_EC_INIT_MIN
#define VALID_EC_INIT_MIN 16
#endif
#ifndef VALID_EC_INIT_MAX
#define VALID_EC_INIT_MAX 60
#endif

#ifndef EC_KEYDOWN_OFFSET
#define EC_KEYDOWN_OFFSET 25
#endif
#ifndef EC_KEYUP_OFFSET
#define EC_KEYUP_OFFSET 5
#endif