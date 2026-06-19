#pragma once

// #include "config_common.h"

/* USB Device descriptor parameter */
#define FW_VER_DATE     DP9S
#define CONTACT(x,y)    x##y
#define CONTACT2(x,y)   CONTACT(x,y)
#define FW_VER          CONTACT2(VIAL_, FW_VER_DATE)
// USBSTR()/USBCONCAT() in tmk_core/protocol/usb_descriptor_common.h paste
// `L` directly onto the first token of MANUFACTURER/PRODUCT, so they must
// expand to (a run of adjacent) string literal tokens, not bare text.
// The STRINGIZE indirection forces FW_VER to be expanded before quoting.
#define EVOLI_STRINGIZE2(x) #x
#define EVOLI_STRINGIZE(x)  EVOLI_STRINGIZE2(x)
#define VENDOR_ID       0x9D5B
#define PRODUCT_ID      0x24EF
#define DEVICE_VER      0x0001
#define MANUFACTURER    "KBDFans_YDKB"
#if CONSOLE_ENABLE
#define PRODUCT         "Evoli Keyboard Uni Debug (" EVOLI_STRINGIZE(FW_VER) ")"
#else
#define PRODUCT         "Evoli Keyboard Uni (" EVOLI_STRINGIZE(FW_VER) ")"
#endif

#define USB_MAX_POWER_CONSUMPTION 350


/* key matrix size */
#define MATRIX_ROWS 7
#define MATRIX_COLS 16
/* 32bit for Layout Options */
#define VIA_EEPROM_LAYOUT_OPTIONS_SIZE 2 //default 1,  not enough for ec_ap and indicator color
#define VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT 4 //default AP Level 5.
//#define FORCE_NKRO //32B

#define DEBOUNCE_DN 2
#define DEBOUNCE_UP 2

#define RGBLIGHT_EFFECT_BREATHING
#define RGBLIGHT_EFFECT_RAINBOW_MOOD
#define RGBLIGHT_EFFECT_RAINBOW_SWIRL
#define RGBLIGHT_EFFECT_SNAKE
#define RGBLIGHT_EFFECT_KNIGHT
#define RGBLIGHT_EFFECT_CHRISTMAS
#define RGBLIGHT_EFFECT_STATIC_GRADIENT
#define RGBLIGHT_EFFECT_RGB_TEST
#define RGBLIGHT_EFFECT_ALTERNATING
#define RGBLIGHT_EFFECT_TWINKLE

#define RGBLIGHT_LIMIT_VAL 192
#define RGBLIGHT_SLEEP
// B15 produced zero reaction (real WS2812 protocol and raw toggle alike).
// The bootloader (separate firmware) visibly drives the same, only physical
// LED on the board, and conventionally that bootloader uses the Maple
// Mini's PB1 LED pin -- try that instead. NOTE: B1 is also used as a matrix
// row line in ec_select_row()/matrix.c; this only works because WS2812
// frames are sent rarely (color-change events only) and matrix scanning
// resumes its own pin mode immediately after, so the two don't overlap in
// time. If LED control becomes unreliable or breaks key input, this sharing
// assumption is wrong and needs revisiting.
#define WS2812_DI_PIN B1
#define RGBLED_NUM 16
/* key combination for command */
#define IS_COMMAND() ( \
    (get_mods() == (MOD_BIT(KC_LSFT) | MOD_BIT(KC_RSFT))) || \
    (get_mods() == (MOD_BIT(KC_LSFT) | MOD_BIT(KC_LCTL) | MOD_BIT(KC_RSFT))) \
)

