/*
Copyright 2023 YANG

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

#include "hal.h"
#include "ch.h"
#include "led.h"
#include "rgblight.h"
#include "rgblight_drivers.h"
#include "ws2812.h"
#include "ec_matrix.h"

#include "stdint.h"
#include "quantum.h"

#ifndef LOGIC_INDICATOR_NUM
#define LOGIC_INDICATOR_NUM PHY_INDICATOR_NUM
#endif

extern rgblight_config_t rgblight_config;

static ws2812_led_t RGBLIGHT_COLOR_OFF = { .r = 0, .g = 0, .b = 0 };
uint8_t indicator_state = 0;

uint8_t indicator_color_config[3];
ws2812_led_t indicator_color[3];

ws2812_led_t rgbled[PHY_INDICATOR_NUM+RGBLED_NUM];

// ---------------------------------------------------------------------
// NOTE: 최신 QMK/vial-qmk는 RGBLIGHT 드라이버를 weak 함수 오버라이드
// (rgblight_call_driver) 방식이 아니라, rgblight_driver_t 구조체(vtable)
// 등록 방식으로 처리합니다. rules.mk에서 RGBLIGHT_DRIVER = custom 으로
// 설정해야 코어가 자체 rgblight_driver를 정의하지 않고, 여기서 정의한
// 것이 사용됩니다 (안 그러면 링커에서 중복 정의 에러가 납니다).
// ---------------------------------------------------------------------

static void my_rgblight_init(void) {
    ws2812_init();
}

// rgblight 코어가 RGBLIGHT_LED_COUNT(RGBLED_NUM) 범위로 index를 넘겨주므로
// 인디케이터 영역(PHY_INDICATOR_NUM)만큼 뒤로 밀어서 저장
static void my_rgblight_set_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    rgbled[PHY_INDICATOR_NUM + index].r = r;
    rgbled[PHY_INDICATOR_NUM + index].g = g;
    rgbled[PHY_INDICATOR_NUM + index].b = b;
}

static void my_rgblight_set_color_all(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < RGBLED_NUM; i++) {
        rgbled[PHY_INDICATOR_NUM + i].r = r;
        rgbled[PHY_INDICATOR_NUM + i].g = g;
        rgbled[PHY_INDICATOR_NUM + i].b = b;
    }
}

static void my_rgblight_flush(void) {
    // keep indicator color (기존 rgblight_call_driver 로직)
    for (uint8_t i = 0; i < PHY_INDICATOR_NUM; i++) {
        if (indicator_state & (1 << i)) {
            rgbled[i] = indicator_color[i];
        } else {
            rgbled[i] = RGBLIGHT_COLOR_OFF;
        }
    }

#ifdef RGB_EXTRA_PROCESS_ENABLE
    rgb_extra_process(rgbled);
#endif

    for (uint16_t i = 0; i < PHY_INDICATOR_NUM + RGBLED_NUM; i++) {
        ws2812_set_color(i, rgbled[i].r, rgbled[i].g, rgbled[i].b);
    }

    // WS2812_DI_PIN (B1) is shared with matrix row 1 -- ec_select_row()/
    // ec_unselect_rows() constantly flip its pin mode between
    // INPUT_PULLUP and OUTPUT_PUSHPULL during normal matrix scanning, so by
    // the time we get here (e.g. on a caps lock toggle, long after boot)
    // the pin is likely not in output mode anymore. ws2812_flush() assumes
    // ws2812_init()'s one-time mode setup still holds, so re-assert it here
    // right before transmitting. The next matrix scan reclaims the pin
    // immediately after, which is fine since by then our frame is already
    // latched.
    gpio_set_pin_output(WS2812_DI_PIN);
    ws2812_flush();
}

const rgblight_driver_t rgblight_driver = {
    .init          = my_rgblight_init,
    .set_color     = my_rgblight_set_color,
    .set_color_all = my_rgblight_set_color_all,
    .flush         = my_rgblight_flush,
};

void set_rgb_user(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < (PHY_INDICATOR_NUM+RGBLED_NUM); i++) {
        rgbled[i].r = r;
        rgbled[i].g = g;
        rgbled[i].b = b;
    }
    for (uint16_t i = 0; i < PHY_INDICATOR_NUM + RGBLED_NUM; i++) {
        ws2812_set_color(i, rgbled[i].r, rgbled[i].g, rgbled[i].b);
    }
    // see my_rgblight_flush() for why this is needed -- WS2812_DI_PIN (B1)
    // is shared with matrix row 1.
    gpio_set_pin_output(WS2812_DI_PIN);
    ws2812_flush();
}

void rgblight_user_init(void)
{
    // matrix_init() (which calls this) runs in quantum/keyboard.c before
    // rgblight_init() does, so the registered driver's .init (my_rgblight_init
    // -> ws2812_init) hasn't run yet at this point. Without this, the data
    // pin is still in its reset-state (not configured as output) when we
    // try to flush colors below, so every write here is silently lost.
    ws2812_init();
#ifdef CONFIG_BOOT_TEST_RGB
    // Clean one-shot R->G->B->off on WS2812_DI_PIN (B1) at boot, before matrix
    // scanning starts. If RGBL1 shows these three colours at power-on, B1 is
    // the DIN. Slow/bright so it is easy to catch by eye.
    set_rgb_user(64, 0, 0);
    wait_ms(550);
    set_rgb_user(0, 64, 0);
    wait_ms(550);
    set_rgb_user(0, 0, 64);
    wait_ms(550);
#endif
    set_rgb_user(0, 0, 0);
}

bool led_update_user(led_t led_state)
{
    uint8_t usb_led = led_state.raw;
    indicator_state = 0;
#ifdef INDICATOR_FUNCT
    static uint8_t indicator_funct[LOGIC_INDICATOR_NUM] = INDICATOR_FUNCT;
    for (uint8_t i=0; i<LOGIC_INDICATOR_NUM; i++) {
        if (usb_led & indicator_funct[i]) {
            indicator_state |= (1<<i);
        }
    }

    if (rgblight_config.mode == 1) rgblight_mode_noeeprom(rgblight_config.mode);
    rgblight_set(); //set rgb even when rgblight.enable=0
#endif
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    static uint8_t mod_keys_registered;
    uint8_t pressed_mods = get_mods();
#ifdef DIAG_FORCE_INDICATOR_ON_CAPS
    // TEMPORARY DIAGNOSTIC: drive the indicator directly on ANY keypress
    // (any physical key), bypassing the host USB LED report / led_update_user()
    // entirely. If this lights up cyan but the real caps-lock-state path
    // doesn't, the WS2812 write itself is fine and the bug is in the host
    // LED report never reaching led_update_user(). If this ALSO doesn't
    // light up, the WS2812 write path itself is still broken post-boot.
    if (record->event.pressed) {
        indicator_state ^= 1;
        rgblight_set();
    }
#endif
    switch (keycode) {
        case 0x5c00: // via/vial reset to bootloader
            if (record->event.pressed) {
                clear_keyboard();
                volatile uint32_t *uf2bl_backup_reg = (uint32_t*)0x20004000;
                *uf2bl_backup_reg = 0x9d5bfc2bUL;
                NVIC_SystemReset();
            }
            return false;
        // 0x5f8f for Alt+Esc=f4 and RShift+Esc=~
        case 0x5F8F:
            if (record->event.pressed) {
                if ((pressed_mods & MOD_BIT(KC_RSFT)) && (~pressed_mods & MOD_BIT(KC_LCTL))) {
                    mod_keys_registered = KC_GRV;
                } else if (pressed_mods & MOD_BIT(KC_LALT)) {
                    mod_keys_registered = KC_F4;
                } else {
                    mod_keys_registered = KC_ESC;
                }
                register_code(mod_keys_registered);
                send_keyboard_report();
            } else {
                unregister_code(mod_keys_registered);
                send_keyboard_report();
            }
            return false;
        default:
            return true; // Process all other keycodes normally
    }
}

void enter_bootloader(void) {
    clear_keyboard();
    volatile uint32_t *uf2bl_backup_reg = (uint32_t*)0x20004000;
    *uf2bl_backup_reg = 0x9d5bfc2bUL;
    NVIC_SystemReset();
}
/* LShift+RShift+LCtrl+B to Bootloader */
#include "command.h"

bool command_extra(uint8_t code)
{
    uint8_t pressed_mods = get_mods();
    clear_keyboard();
    switch (code) {
        case KC_A:
            print_matrix_adc ^= 1;
            return true;
        case KC_B:
            ;
            wait_us(500*1000);
            if (pressed_mods & MOD_BIT(KC_LCTL)) {
                enter_bootloader();
            }
            //soft reset
            NVIC_SystemReset();
            //*(uint32_t *)(0xE000ED0CUL) = 0x05FA0000UL | (*(uint32_t *)(0xE000ED0CUL) & 0x0700) | 0x04;
            break;
        default:
            return false;   // yield to default command
    }
    return true;
}

void restart_usb_driver(USBDriver *usbp) {
    NVIC_SystemReset();
}

void user_config_init(void)
{



    static const uint8_t indicator_hue_preset[8] = {0, 21, 42, 85, 127, 170, 212, 255};
    #ifdef INDICATOR_VAL
    static uint8_t val = INDICATOR_VAL;
    #else 
    static uint8_t val = 255;
    #endif


    uint16_t layout_value = via_get_layout_options();

    ec_apc_set(layout_value & 0b111);

    for (uint8_t i=0; i<(LOGIC_INDICATOR_NUM+1); i++) {
        layout_value >>= 3;
        indicator_color_config[i] = (layout_value & 0b111);
        uint8_t hue = indicator_hue_preset[ indicator_color_config[i] ];
        if (hue == 255) {
            indicator_color[i] = (ws2812_led_t){.r = val/2, .g = val/2, .b = val/2};
        } else {
            rgb_t rgb = hsv_to_rgb((HSV){hue, 255, val});
            indicator_color[i] = (ws2812_led_t){.r = rgb.r, .g = rgb.g, .b = rgb.b};
        }
        xprintf("\n indicator %d R: %d, G: %d, B:%d", i, indicator_color[i].r, indicator_color[i].g, indicator_color[i].b);
    }
    // Caps Lock indicator: fixed cyan, regardless of the VIA layout-option
    // color computed above.
    indicator_color[0] = (ws2812_led_t){.r = 0, .g = 255, .b = 255};
    led_wakeup();
    rprint("Layout set change\n");
}

#ifdef DIAG_PIN_SWEEP
// =====================================================================
// TEMPORARY DIAGNOSTIC: WS2812 data-pin sweep (console-synced identification).
//
// ROUND 3 -- ALL safe GPIOs, from a COLD-boot dark start.
//
// What we now know (session 4, console working):
//  - Bootloader cycles R/B/G on RGBL1; on handoff to firmware the current colour
//    FREEZES and holds; a true cold power-cycle starts the LED DARK. A held-latch
//    static colour + power-up-dark are exactly WS2812 behaviour -> RGBL1 IS a
//    single-wire WS2812 (the earlier "power-on white" was just the frozen
//    bootloader colour, not a real power-on state).
//  - Sweeping the 11 matrix pins with WHITE from a dark start lit NOTHING. Since
//    dark->white would be visible, those 11 are now credibly EXCLUDED -- the
//    handoff's "DIN is a matrix pin" conclusion does not hold.
//  - The earlier exclusion of the 17 NON-matrix pins is UNRELIABLE: it used
//    "white frame -> LED stayed white -> not DIN", but that "white" was the
//    frozen bootloader colour, so the real DIN could have been masked
//    (white-on-white shows no change). They must be re-tested from a dark start.
//
// So this round drives EVERY safe GPIO (skipping USB A11/A12, SWD A13/A14, and
// ADC A1/A2) one at a time with solid RED (a distinct colour, refreshed every
// ~50ms in case it is analog after all), printing each pin's name. Non-matrix
// pins first (most likely, since they were excluded unreliably), matrix pins +
// B1 last. matrix_scan() stays SUSPENDED (matrix.c) so nothing else drives the
// shared GPIOB pins; keys are dead in this build -- expected and temporary.
//
// Identification is by CONSOLE READOUT (CONSOLE_ENABLE=yes, read with
// `qmk console`): the name printed when RGBL1 lights RED is the DIN. If NOTHING
// lights across the whole sweep, the DIN is reachable by no GPIO we drive, which
// would instead implicate the drive routine -> fall back to a stock-driver,
// compile-time per-pin test (set WS2812_DI_PIN, known-good ws2812_flush).
// =====================================================================
#include "gpio.h"
#include "chibios_config.h"   // CPU_CLOCK

// Every GPIO we can safely drive, NON-matrix first (those were excluded
// unreliably, so most suspect), then the matrix pins + B1 last (better
// excluded). Deliberately omitted: A11/A12 (USB -- driving them kills the link
// and the console), A13/A14 (SWD), A1/A2 (EC discharge / ADC input). sweep_names
// is 1:1 and printed over the console -- KEEP BOTH IN SYNC if either changes.
static const pin_t  sweep_pins[] = {
    A0, A3, A4, A5, A6, A7, A8, A9, A10, A15,   // free port-A GPIOs
    B7, B8, B9, B15,                            // free port-B GPIOs
    C13, C14, C15,                              // port-C GPIOs
    B0, B2, B3, B4, B5, B6,                     // matrix rows (now well-excluded)
    B10, B11, B12, B13, B14,                    // 4051 mux/EN (now well-excluded)
    B1,                                         // current WS2812_DI_PIN (paranoia)
};
static const char *const sweep_names[] = {
    "A0", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10", "A15",
    "B7", "B8", "B9", "B15",
    "C13", "C14", "C15",
    "B0", "B2", "B3", "B4", "B5", "B6",
    "B10", "B11", "B12", "B13", "B14",
    "B1",
};
#define SWEEP_COUNT      (sizeof(sweep_pins) / sizeof(sweep_pins[0]))
#define SWEEP_RED        80, 0, 0   // distinct colour (not white): unmistakably "our" signal
#define SWEEP_ON_MS      2500       // solid window per pin (long: easy to catch by eye)
#define SWEEP_GAP_MS     700        // dark gap between consecutive pins
#define SWEEP_REFRESH_MS 50         // re-send within a window (insurance if RGBL1 were analog)

// Pin-parameterised WS2812 bitbang. The stock driver
// (platforms/chibios/drivers/ws2812_bitbang.c) hardcodes WS2812_DI_PIN, so
// we replicate its timing here but drive an arbitrary, runtime-selected pin
// via PAL port ops (palSetPort/palClearPort) with a precomputed port+mask.
#define DIAG_NOP_FUDGE       0.4
#define DIAG_NUMBER_NOPS     6
#define DIAG_CYCLES_PER_SEC  (CPU_CLOCK / DIAG_NUMBER_NOPS * DIAG_NOP_FUDGE)
#define DIAG_NS_PER_CYCLE    (1000000000L / DIAG_CYCLES_PER_SEC)
#define DIAG_NS_TO_CYCLES(n) ((n) / DIAG_NS_PER_CYCLE)
#define diag_wait_ns(x)                                       \
    do {                                                      \
        for (int _i = 0; _i < DIAG_NS_TO_CYCLES(x); _i++) {   \
            __asm__ volatile("nop\n\tnop\n\tnop\n\t"          \
                             "nop\n\tnop\n\tnop\n\t");         \
        }                                                     \
    } while (0)

static void diag_sweep_send(pin_t line, uint8_t r, uint8_t g, uint8_t b) {
    // Use ChibiOS PAL port ops (as ec_matrix.c does) so we don't depend on a
    // particular GPIO struct typedef; port+mask are precomputed so each edge
    // stays constant-time (within WS2812 spec) for a runtime-selected pin.
    ioportid_t   port = PAL_PORT(line);
    ioportmask_t mask = (ioportmask_t)(1u << PAL_PAD(line));
    uint8_t      grb[3] = { g, r, b }; // WS2812 GRB, MSB first
    chSysLock();
    for (uint8_t px = 0; px < (PHY_INDICATOR_NUM + RGBLED_NUM); px++) {
        for (uint8_t k = 0; k < 3; k++) {
            uint8_t byte = grb[k];
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (byte & (0x80 >> bit)) {
                    palSetPort(port, mask);   diag_wait_ns(WS2812_T1H);
                    palClearPort(port, mask); diag_wait_ns(WS2812_T1L);
                } else {
                    palSetPort(port, mask);   diag_wait_ns(WS2812_T0H);
                    palClearPort(port, mask); diag_wait_ns(WS2812_T0L);
                }
            }
        }
    }
    diag_wait_ns(1000 * WS2812_TRST_US); // latch / reset gap
    chSysUnlock();
}

// Non-blocking console-synced sweep. For each candidate in sweep_pins[] order it
// drives that ONE pin solid RED for ~2.5s (refreshed every ~50ms as insurance in
// case RGBL1 is analog rather than a latching WS2812), prints the pin's name,
// then parks it low for a short dark gap before the next. Only the real DIN pin
// lights -> read the name the console printed at that moment. Boot starts dark
// (cold-boot WS2812 state), so any RED at all is unambiguously our signal. A
// "=== cycle start ===" line is printed each time the list wraps.
static void diag_pin_sweep(void) {
    static uint32_t t0    = 0;
    static uint32_t tref  = 0;
    static uint16_t idx   = 0;
    static int8_t   phase = -1; // -1 = announce + begin window, 0 = hold window, 1 = gap
    static bool     init  = false;

    if (!init) {
        init = true;
        // Cold boot starts dark; just park every candidate driven-low (no
        // floating) so nothing latches noise before its turn.
        for (uint16_t i = 0; i < SWEEP_COUNT; i++) {
            gpio_set_pin_output(sweep_pins[i]);
            gpio_write_pin_low(sweep_pins[i]);
        }
        idx   = 0;
        phase = -1;
        t0    = timer_read32();
        xprintf("[sweep] === cycle start ===\n");
        return;
    }

    if (phase == -1) {                          // begin this pin's window
        xprintf("[sweep] driving %s\n", sweep_names[idx]);
        gpio_set_pin_output(sweep_pins[idx]);
        diag_sweep_send(sweep_pins[idx], SWEEP_RED);
        phase = 0;
        t0    = timer_read32();
        tref  = timer_read32();
        return;
    }

    if (phase == 0) {                           // hold window (periodic refresh)
        if (timer_elapsed32(t0) < SWEEP_ON_MS) {
            if (timer_elapsed32(tref) >= SWEEP_REFRESH_MS) {
                diag_sweep_send(sweep_pins[idx], SWEEP_RED);
                tref = timer_read32();
            }
            return;
        }
        diag_sweep_send(sweep_pins[idx], 0, 0, 0); // off; frame leaves pin driven low
        gpio_write_pin_low(sweep_pins[idx]);
        phase = 1;
        t0    = timer_read32();
        return;
    }

    // phase == 1: dark gap, then advance to the next pin
    if (timer_elapsed32(t0) < SWEEP_GAP_MS) return;
    idx = (idx + 1) % SWEEP_COUNT;
    if (idx == 0) xprintf("[sweep] === cycle start ===\n");
    phase = -1;
}
#endif

// hook_keyboard_loop() is effectively DEAD on this board: it is only ever
// called from matrix_scan_kb(), which is only called from
// quantum/matrix_common.c -- and that file is NOT compiled when
// CUSTOM_MATRIX = yes (builddefs/common_features.mk:654). It is kept here
// only so the (unused, weak) matrix_scan_kb() in matrix.c still links.
// Per-loop work must instead go through housekeeping_task_user(), which
// quantum/main.c calls every iteration of the main loop.
void hook_keyboard_loop(void)
{
}

void housekeeping_task_user(void)
{
#ifdef DIAG_PIN_SWEEP
    diag_pin_sweep();
#endif
}

// Snap Tap / SOCD
static const uint8_t SOCD_KEY[2][2] = {
    { KC_W, KC_S },
    { KC_A, KC_D }
};

bool socd_key_state[2][2] = { {0,0},{0,0}};

void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= QK_USER_0 && keycode <= QK_USER_3) {
        uint8_t key = keycode - QK_USER_0;
        uint8_t k_group = key&1;
        uint8_t k_num = key>>1;
        uint8_t k_op_num = k_num?0:1;
        if (record->event.pressed) {
            socd_key_state[k_group][k_num] = 1;
            if (socd_key_state[k_group][k_op_num]) {
                unregister_code(SOCD_KEY[k_group][k_op_num]);
            }
            register_code(SOCD_KEY[k_group][k_num]);
        } else {
            socd_key_state[k_group][k_num] = 0;
            unregister_code(SOCD_KEY[k_group][k_num]);
            if (socd_key_state[k_group][k_op_num]) {
                register_code(SOCD_KEY[k_group][k_op_num]);
            }
        }
    }
}
