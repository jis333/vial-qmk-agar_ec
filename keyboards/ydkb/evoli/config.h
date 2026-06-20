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
// RGBL1's WS2812 data-in is B15. Confirmed by the round-3 diagnostic sweep
// (DIAG_PIN_SWEEP): from a cold-boot dark start, driving B15 solid red lit the
// LED; the console printed "[sweep] driving B15" at that moment. B15 was wrongly
// excluded in earlier sessions -- those tests compared a white frame against the
// LED's *frozen bootloader colour* (handoff -> firmware leaves the last colour
// latched), so "stayed lit" was misread as "no reaction". B15 is a free GPIO
// (NOT a matrix pin), so unlike the old B1 guess there is no matrix-scan sharing
// to worry about. (The same sibling board agar_ec_vial also drives its strip
// from B15.)
#define WS2812_DI_PIN B15
#define RGBLED_NUM 16
/* key combination for command */
#define IS_COMMAND() ( \
    (get_mods() == (MOD_BIT(KC_LSFT) | MOD_BIT(KC_RSFT))) || \
    (get_mods() == (MOD_BIT(KC_LSFT) | MOD_BIT(KC_LCTL) | MOD_BIT(KC_RSFT))) \
)

