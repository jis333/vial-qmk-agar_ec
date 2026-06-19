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
// TEMPORARY DIAGNOSTIC: WS2812 data-pin sweep (blink-count identification).
//
// ROUND 2 -- matrix pins. The 17 NON-matrix GPIOs were swept first and RGBL1
// never reacted (it sits at its power-on white and never blinked), which
// rules them out: had any been the DIN, it would have blinked. B1 was also
// ruled out (the boot R->G->B test on it showed nothing). So the DIN must be
// one of the MATRIX pins (rows B0-B6, 4051 mux/EN B10-B14) -- which the first
// sweep skipped because live matrix scanning keeps re-driving them.
//
// To test them cleanly, matrix_scan() is SUSPENDED while DIAG_PIN_SWEEP is
// defined (see matrix.c), so nothing else drives GPIOB. Keys are dead in this
// diagnostic build -- that is expected and temporary.
//
// Identification is by BLINK COUNT (xprintf is a no-op here): the candidate
// at index i blinks the LED white (i+1) times. Only the pin wired to RGBL1's
// DIN blinks, so watch from power-on and count the white blinks -- that count
// (1-based) is the pin's position in sweep_pins[] below. White is used so the
// result does not depend on the LED's byte order. If NOTHING here ever blinks
// either, RGBL1 is likely not a single-wire WS2812 at all (e.g. an analog RGB
// LED) and we switch to a raw per-channel test next.
// =====================================================================
#include "gpio.h"
#include "chibios_config.h"   // CPU_CLOCK

// The matrix GPIOs (the only pins not yet cleanly tested). Likely candidates
// first: the 4051 mux/EN lines B10-B14 (the PCB photo put a LED data trace
// near the MCU's right edge), then the row lines. B1 is omitted -- it was
// ruled out by the boot test and rgblight still flushes to it. Order == blink
// count (index 0 -> 1 blink): 1=B10 2=B11 3=B12 4=B13 5=B14 6=B0 7=B2 8=B3
// 9=B4 10=B5 11=B6.
static const pin_t  sweep_pins[] = { B10, B11, B12, B13, B14, B0, B2, B3, B4, B5, B6 };
#define SWEEP_COUNT   (sizeof(sweep_pins) / sizeof(sweep_pins[0]))
#define SWEEP_ON_MS   220
#define SWEEP_OFF_MS  220
#define SWEEP_GAP_MS  1300   // dark gap between one pin's blink train and the next

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

// Non-blocking blink-count sweep. For candidate `idx` it shows (idx+1) WHITE
// blinks, then a dark gap, then advances to the next pin. Only the real DIN
// pin blinks visibly -> count the blinks to identify it (see header above).
// White (all channels equal) is used so the result does not depend on the
// LED's byte order (GRB/RGB/BGR). Every candidate pin is parked driven-LOW
// when idle (never left floating) so a floating DIN can't latch noise and
// appear randomly lit.
#define SWEEP_WHITE 64, 64, 64
static void diag_pin_sweep(void) {
    static uint32_t t0    = 0;
    static uint16_t idx   = 0;
    static int16_t  state = -1; // -1 = start train, 0..2N-1 = blink states, -2 = gap
    static bool     init  = false;

    if (!init) {
        init = true;
        // Park every candidate driven-low (no floating) AND push one OFF frame
        // to each. If any of these pins IS the DIN, RGBL1's power-on white
        // clears to dark immediately -- which by itself confirms the DIN is one
        // of these matrix pins, and gives a clean dark background for the white
        // blink-count to show against.
        for (uint16_t i = 0; i < SWEEP_COUNT; i++) {
            gpio_set_pin_output(sweep_pins[i]);
            gpio_write_pin_low(sweep_pins[i]);
            diag_sweep_send(sweep_pins[i], 0, 0, 0);
        }
        t0    = timer_read32();
        state = -1;
    }

    if (state == -1) {                          // begin this pin's train: first ON
        diag_sweep_send(sweep_pins[idx], SWEEP_WHITE);
        state = 0;
        t0    = timer_read32();
        return;
    }
    if (state == -2) {                          // dark gap between pins (LED off)
        if (timer_elapsed32(t0) >= SWEEP_GAP_MS) {
            idx   = (idx + 1) % SWEEP_COUNT;
            state = -1;
        }
        return;
    }

    bool currently_on = ((state & 1) == 0);     // even state = LED on, odd = off
    if (timer_elapsed32(t0) < (currently_on ? SWEEP_ON_MS : SWEEP_OFF_MS)) return;

    state++;
    t0 = timer_read32();
    if (state >= (int16_t)(2 * (idx + 1))) {    // (idx+1) blinks done -> gap
        diag_sweep_send(sweep_pins[idx], 0, 0, 0); // off; frame leaves pin driven low
        state = -2;
        return;
    }
    if ((state & 1) == 0) {                     // new ON edge
        diag_sweep_send(sweep_pins[idx], SWEEP_WHITE);
    } else {                                    // new OFF edge
        diag_sweep_send(sweep_pins[idx], 0, 0, 0);
    }
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
