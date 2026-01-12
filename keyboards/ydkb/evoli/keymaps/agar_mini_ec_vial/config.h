#pragma once
#undef  PRODUCT_ID
#define PRODUCT_ID    0x2509

#undef  PRODUCT
#if CONSOLE_ENABLE
#define PRODUCT    Agar Mini Debug (FW_VER)
#else
#define PRODUCT    Agar Mini EC Keyboard (FW_VER)
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
#undef  RGBLED_NUM
#define RGBLED_NUM    0
#define PHY_INDICATOR_NUM    1
#define INDICATOR_FUNCT    {(1<<USB_LED_CAPS_LOCK)}
//#define RGB_EXTRA_PROCESS_ENABLE