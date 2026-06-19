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

// TEMPORARY DIAGNOSTIC: WS2812 data-pin sweep (see led.c). ROUND 2 -- now
// sweeping the MATRIX pins (rows B0-B6, mux B10-B14), since the 17 non-matrix
// pins and B1 were all ruled out (RGBL1 just stays power-on white). While this
// is defined, matrix_scan() is suspended (matrix.c) so the sweep can drive
// GPIOB cleanly -- KEYS ARE DEAD in this build, that's expected. Identified by
// BLINK COUNT (xprintf is a no-op here): candidate i blinks white (i+1) times;
// only the pin wired to RGBL1 blinks -> count the white blinks from power-on.
// Remove once the DIN pin is found.
#define DIAG_PIN_SWEEP

// CONFIG_BOOT_TEST_RGB (boot R->G->B on B1) is now OFF: B1 was ruled out (no
// R->G->B appeared) and it would only add a confusing flash. Re-enable only to
// re-test a specific WS2812_DI_PIN value.
//#define CONFIG_BOOT_TEST_RGB

// Superseded / disabled:
//   DIAG_B15_BLINK              - raw GPIO toggle, invalid test for a WS2812
//   DIAG_FORCE_INDICATOR_ON_CAPS- any-keypress toggle, confirmed no effect