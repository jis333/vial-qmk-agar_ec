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

enum custom_keycodes {
    // Vial's "customKeycodes" picker maps array entries positionally to
    // QK_KB_0 onward (not QK_USER_0) - confirmed by checking what keycode
    // Vial actually assigned when picking this entry from the User tab.
    // ST(W)/ST(A)/ST(S)/ST(D) occupy indices 0-3 (QK_KB_0..QK_KB_0+3), so
    // this is the next free slot.
    HANENG_CORRECT = QK_KB_0 + 4,
};

typedef struct {
    uint8_t keycode;
    uint8_t mods;
} haneng_buffered_key_t;

#define HANENG_BUF_MAX 16
#define HANENG_TIMEOUT_MS 5000

static haneng_buffered_key_t haneng_buf[HANENG_BUF_MAX];
static uint8_t  haneng_buf_len = 0;
static bool     haneng_buf_invalid = false;
static uint16_t haneng_last_keytime = 0;

static void haneng_buffer_reset(void) {
    haneng_buf_len     = 0;
    haneng_buf_invalid = false;
}

// Set while HANENG_CORRECT's own correction sequence (select, delete, IME
// toggle, replay) is sending synthetic input - register_code/register_mods
// fire for all of that too, and without this guard we'd capture our own
// replay as if the user had typed it, corrupting or looping the buffer.
static bool haneng_replaying = false;

static void haneng_buffer_append(uint8_t code, uint8_t mods) {
    if (haneng_buf_invalid) return;
    if (haneng_buf_len >= HANENG_BUF_MAX) {
        haneng_buf_invalid = true;
        return;
    }
    haneng_buf[haneng_buf_len].keycode = code;
    haneng_buf[haneng_buf_len].mods    = mods;
    haneng_buf_len++;
}

// Called from __wrap_add_key_to_report with the keycode QMK is actually
// about to send to the host - this is already fully resolved (tap-hold and
// tap-dance keys have already decided their real output by the time it
// reaches here), so no raw-keycode guessing is needed.
//
// add_key_to_report (tmk_core/protocol/report.c) rather than register_code
// is the hook point: register_code's main caller, process_action, is
// defined in the same translation unit (quantum/action.c) as register_code
// itself, so GCC resolves that call locally and `--wrap=register_code`
// can't intercept it - confirmed by inspecting the linked ELF, where
// process_action's call to register_code branched straight to the original
// address instead of __wrap_register_code. add_key_to_report is a genuinely
// separate translation unit from every caller (action.c, quantum.c, vial.c),
// so wrapping it is reliable regardless of which path produced the key.
static void haneng_on_add_key(uint8_t code) {
    if (haneng_replaying) return;

    if (haneng_buf_len > 0 && timer_elapsed(haneng_last_keytime) > HANENG_TIMEOUT_MS) {
        haneng_buffer_reset();
    }
    haneng_last_keytime = timer_read();

    if (code == KC_SPC || code == KC_ENT || code == KC_TAB || code == KC_BSPC ||
        code == KC_LNG1 || code == KC_LNG2) {
        // The IME toggle key itself (or Hanja) must never end up in the
        // buffer: replaying it later would re-toggle the mode a second
        // time mid-replay, cancelling out our own deliberate toggle. A
        // manual mid-word toggle also means whatever was typed before it
        // belongs to a different mode context than what comes after, so
        // treat it as a hard word boundary like Enter/Space/Tab/Backspace.
        haneng_buffer_reset();
        return;
    }
    switch (code) {
        case KC_LCTL: case KC_RCTL:
        case KC_LALT: case KC_RALT:
        case KC_LGUI: case KC_RGUI:
            haneng_buffer_reset();
            return;
        case KC_LSFT: case KC_RSFT:
            // Shift alone produces no character - ignore, don't touch the buffer.
            return;
        default:
            break;
    }

    haneng_buffer_append(code, get_mods());
}

// Called from __wrap_add_mods - catches real Ctrl/Alt/Gui engaged as a hold
// (e.g. a home-row mod resolving to a hold instead of its tap target), which
// never goes through add_key_to_report at all. Shift is exempt, matching
// haneng_on_add_key above. add_mods (quantum/action_util.c) is, for the same
// same-translation-unit reason as above, the reliable wrap target rather
// than register_mods (quantum/action.c).
static void haneng_on_add_mods(uint8_t mods) {
    if (haneng_replaying) return;
    const uint8_t blocking_mods = MOD_BIT(KC_LCTL) | MOD_BIT(KC_RCTL) |
                                   MOD_BIT(KC_LALT) | MOD_BIT(KC_RALT) |
                                   MOD_BIT(KC_LGUI) | MOD_BIT(KC_RGUI);
    if (mods & blocking_mods) {
        haneng_buffer_reset();
        haneng_last_keytime = timer_read();
    }
}

extern void __real_add_key_to_report(uint8_t key);
extern void __real_del_key_from_report(uint8_t key);
extern void __real_add_mods(uint8_t mods);
extern void __real_del_mods(uint8_t mods);

void __wrap_add_key_to_report(uint8_t key) {
    haneng_on_add_key(key);
    __real_add_key_to_report(key);
}

void __wrap_del_key_from_report(uint8_t key) {
    __real_del_key_from_report(key);
}

void __wrap_add_mods(uint8_t mods) {
    haneng_on_add_mods(mods);
    __real_add_mods(mods);
}

void __wrap_del_mods(uint8_t mods) {
    __real_del_mods(mods);
}

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
    // The physical indicator pixel (rgbled[0]) is owned by the status-LED code
    // (status_led_paint(), driven by housekeeping_task_user()). We must NOT
    // overwrite it here. The old indicator_state path is retired, so the loop
    // that used to live here forced rgbled[0] = OFF on every rgblight flush
    // (indicator_state is now always 0). Because rgblight core calls this flush
    // independently of housekeeping (e.g. animation ticks), that turned the
    // status color off a frame after housekeeping set it -- the LED lit briefly
    // then went dark. We now leave rgbled[0] exactly as the status code set it,
    // so this flush re-transmits the current status color rather than fighting it.

#ifdef RGB_EXTRA_PROCESS_ENABLE
    rgb_extra_process(rgbled);
#endif

    for (uint16_t i = 0; i < PHY_INDICATOR_NUM + RGBLED_NUM; i++) {
        ws2812_set_color(i, rgbled[i].r, rgbled[i].g, rgbled[i].b);
    }

    // Re-assert the data pin as output right before transmitting. Defensive:
    // ws2812_init() already configures it at boot and WS2812_DI_PIN (B15) is a
    // free GPIO that matrix scanning never touches, so this is belt-and-suspenders.
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
    // see my_rgblight_flush(): re-assert output mode before transmitting.
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
    set_rgb_user(0, 0, 0);
}

// ====================================================================
// Status indicator LED: explicit color per (caps / layer) state
// --------------------------------------------------------------------
// The single physical LED (rgbled[0]) shows one color chosen from 4 states,
// evaluated in priority order (1 = highest):
//
//   1  CapsLock ON     -> red               (150,  0,  0)
//   2  Layer 2 active  -> bright teal-green ( 60,190,120)
//   3  Layer 1/4/5/6   -> bright amber      (190,120, 20)
//   4  Layer 0 (base)  -> off
//
// Layer 3 (and any layer not listed) falls into the Layer 0 group -> off.
//
// Depends only on runtime *state* (host caps LED, layer_state), not on
// keycodes or key positions, so remapping the keymap in Vial does not break it.
//
// Polled from housekeeping_task_user() every main loop, because layer changes
// do NOT generate a USB-LED report (the only trigger for led_update_user). The
// WS2812 flush (bitbang, interrupts off) is gated on a color change so it only
// transmits on actual edges, not every loop.
// ====================================================================

static const ws2812_led_t STATUS_OFF  = { .r = 0,   .g = 0,   .b = 0   };
static const ws2812_led_t STATUS_CAPS = { .r = 150, .g = 0,   .b = 0   }; // 1 red
static const ws2812_led_t STATUS_L2   = { .r = 60,  .g = 190, .b = 120 }; // 2 bright teal-green
static const ws2812_led_t STATUS_L1   = { .r = 190, .g = 120, .b = 20  }; // 3 bright amber  (Layer 0 = off)

static inline bool status_color_eq(ws2812_led_t a, ws2812_led_t b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Priority-ordered (1 highest): caps, then layer 2, then the layer-1 group,
// then base. layer_state_is() is used (not get_highest_layer) so layer 2 still
// wins over the layer-1 group even if several layers are active at once.
static ws2812_led_t status_color(void) {
    if (host_keyboard_led_state().caps_lock) return STATUS_CAPS;       // 1
    if (layer_state_is(2)) return STATUS_L2;                           // 2
    if (layer_state_is(1) || layer_state_is(4) ||
        layer_state_is(5) || layer_state_is(6)) return STATUS_L1;      // 3
    return STATUS_OFF;                                                 // 4
}

static void status_led_paint(ws2812_led_t c) {
    rgbled[0] = c;
    ws2812_set_color(0, c.r, c.g, c.b);
    // see my_rgblight_flush(): re-assert the data pin as output before TX.
    gpio_set_pin_output(WS2812_DI_PIN);
    ws2812_flush();
}

void housekeeping_task_user(void) {
    static ws2812_led_t last = { .r = 1, .g = 1, .b = 1 }; // impossible -> force first paint
    ws2812_led_t shown = status_color();
    if (!status_color_eq(shown, last)) {
        status_led_paint(shown);
        last = shown;
    }
}

bool led_update_user(led_t led_state)
{
    // CapsLock (and any other USB-LED) is no longer painted here. The status
    // LED is driven entirely by housekeeping_task_user(), which reads
    // host_keyboard_led_state()/get_mods()/get_highest_layer() through one
    // priority-ordered path. Calling rgblight_set() here would make
    // my_rgblight_flush() paint the old indicator color for one frame and
    // fight the housekeeping paint, so we don't.
    (void)led_state;
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    static uint8_t mod_keys_registered;
    uint8_t pressed_mods = get_mods();

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
        case HANENG_CORRECT:
            if (record->event.pressed) {
                // The HANENG_CORRECT key's own keydown HID report needs a moment
                // to settle before we start sending synthetic reports, or the
                // first register_code below gets lost on the host side.
                wait_ms(50);
                // The capture hooks only check staleness when a new key
                // arrives - if the user just sits idle after the word and
                // then presses this key directly, no such event happened to
                // invalidate it. Check here too, or a stale buffer survives
                // indefinitely and keeps getting replayed.
                if (haneng_buf_len > 0 && timer_elapsed(haneng_last_keytime) > HANENG_TIMEOUT_MS) {
                    haneng_buffer_reset();
                }
                if (!haneng_buf_invalid && haneng_buf_len > 0) {
                    haneng_replaying = true;
                    register_code(KC_LCTL);
                    register_code(KC_LSFT);
                    tap_code(KC_LEFT);
                    unregister_code(KC_LSFT);
                    unregister_code(KC_LCTL);
                    tap_code(KC_BSPC);
                    wait_ms(30);
                    tap_code(KC_LNG1);
                    // The IME needs time to actually switch mode before the
                    // first replayed character arrives, or it (and sometimes
                    // several characters after it) get composed under the
                    // stale mode.
                    wait_ms(50);
                    for (uint8_t i = 0; i < haneng_buf_len; i++) {
                        uint8_t mods = haneng_buf[i].mods;
                        if (mods) register_mods(mods);
                        tap_code(haneng_buf[i].keycode);
                        if (mods) unregister_mods(mods);
                        // Hangul composition needs more processing time per
                        // keystroke than a plain ASCII send - 10ms was too
                        // tight and dropped characters under fast replay.
                        wait_ms(30);
                    }
                    haneng_replaying = false;
                }
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
    // (CapsLock color is now handled by status_base_color()/housekeeping_task_user();
    //  the old fixed-cyan indicator_color[0] override is no longer used.)
    led_wakeup();
    rprint("Layout set change\n");
}

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
