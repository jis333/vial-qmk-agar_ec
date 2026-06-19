#pragma once
#undef  PRODUCT_ID
#define PRODUCT_ID    0x2509

#undef  PRODUCT
#if CONSOLE_ENABLE
#define PRODUCT    "Agar Mini Debug (" EVOLI_STRINGIZE(FW_VER) ")"
#else
#define PRODUCT    "Agar Mini EC Keyboard (" EVOLI_STRINGIZE(FW_VER) ")"
#endif

#undef  MATRIX_ROWS
#define MATRIX_ROWS 6
#undef  MATRIX_COLS
#define MATRIX_COLS 8
#define MATRIX_KEYS 44

#define APC_ENABLE

#define DYNAMIC_KEYMAP_LAYER_COUNT    8
#define FLASH_KEYMAP_COUNT    4
#define VIAL_KEYBOARD_UID    {0x2E, 0xE6, 0x0E, 0x23, 0x34, 0xEF, 0x99, 0x37}

#undef  RGBLIGHT_LIMIT_VAL
#define RGBLIGHT_LIMIT_VAL    192
// RGBLIGHT_LED_COUNT == RGBLED_NUM is used directly by quantum/rgblight/rgblight.c
// for array sizing and modulo operations, so it must be >= 1 even though this
// board has no underglow strip (only the indicator LED, handled separately
// via PHY_INDICATOR_NUM in led.c). 0 compiled under old QMK but trips
// -Werror=div-by-zero/array-bounds on current toolchains.
#undef  RGBLED_NUM
#define RGBLED_NUM    1
#define PHY_INDICATOR_NUM    1
// RGBLIGHT_DRIVER=custom skips the automatic RGBLIGHT_WS2812 define, so
// drivers/led/ws2812.h never gets WS2812_LED_COUNT. Must match the total
// size of rgbled[] in led.c (PHY_INDICATOR_NUM + RGBLED_NUM).
#define WS2812_LED_COUNT    (PHY_INDICATOR_NUM + RGBLED_NUM)
// led_t.raw bit layout: 0=num_lock, 1=caps_lock, 2=scroll_lock, 3=compose, 4=kana
#define INDICATOR_FUNCT    {(1<<1)}
//#define RGB_EXTRA_PROCESS_ENABLE
