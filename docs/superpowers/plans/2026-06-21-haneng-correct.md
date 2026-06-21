# HanEngCorrect Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a custom QMK keycode `HANENG_CORRECT` that deletes the previously typed word, toggles the 한/영 input mode, and retypes the same keystrokes — fixing the common "typed a word in the wrong IME mode" mistake with one keypress.

**Architecture:** A 16-entry keystroke ring buffer tracks the current "word" as it's typed (keycode + modifier state per entry). Word-boundary keys (space/enter/tab/backspace), blocking modifiers (ctrl/alt/gui), and a 5-second idle timeout all reset the buffer. Pressing `HANENG_CORRECT` replays: select-previous-word (`Ctrl+Shift+Left`) → delete (`Backspace`) → toggle IME (`KC_LANG1`) → replay buffered keystrokes.

**Tech Stack:** C, QMK firmware (vial-qmk fork), board-level `process_record_user` in `keyboards/ydkb/evoli/led.c` (this board already claims `process_record_user` — there is no keymap-level hook available, so all changes land in the board file, not `keymap.c`).

## Global Constraints

- Word boundary characters: space / enter / tab only (no punctuation) — per `docs/superpowers/specs/2026-06-21-haneng-correct-design.md`.
- Buffer max length: 16 entries. Exceeding it marks the buffer invalid (no partial replay).
- Idle timeout: 5000 ms since the last buffered keystroke invalidates the buffer.
- Any backspace press resets the buffer entirely (no partial undo tracking) — see design doc "백스페이스 처리".
- Ctrl/Alt/Gui held (Shift excluded) resets the buffer.
- IME toggle keycode is fixed to `KC_LANG1` (confirmed to match the user's actual Windows binding).
- Word deletion uses `Ctrl+Shift+Left` + `Backspace`, never `Ctrl+Backspace` or `End` (per design doc rationale).
- No host-side test framework exists for this firmware. Each task's automated gate is `qmk compile -kb ydkb/evoli -km agar_ec_vial` succeeding. Final behavioral verification is manual, on real hardware, by the user — write exact step-by-step scripts for it, do not claim it works without the user confirming.

---

### Task 1: Reserve the custom keycode and expose it in Vial

**Files:**
- Modify: `keyboards/ydkb/evoli/led.c:33-38` (add enum before the existing globals)
- Modify: `keyboards/ydkb/evoli/keymaps/agar_ec_vial/vial.json:8-12` (customKeycodes array)

**Interfaces:**
- Produces: `HANENG_CORRECT` (uint16_t keycode constant, value `QK_USER_0 + 4`). Later tasks switch on this constant inside `process_record_user`.

**Context:** `keyboards/ydkb/evoli/led.c:326-328` already reserves `QK_USER_0`..`QK_USER_3` for the four Vial custom-UI menu entries (RGB underglow, CapsLock color, EC sensitivity, 2U backspace). `QK_USER_4` is the next free slot in that same reserved range (`QK_USER_0`..`QK_USER_31` per `quantum/keycodes.h:805-836`), so it can't collide with anything else in this board.

- [ ] **Step 1: Add the enum constant**

In `keyboards/ydkb/evoli/led.c`, right after the existing `extern rgblight_config_t rgblight_config;` line (line 33), add:

```c
enum custom_keycodes {
    HANENG_CORRECT = QK_USER_0 + 4,
};
```

- [ ] **Step 2: Add the Vial custom keycode label**

In `keyboards/ydkb/evoli/keymaps/agar_ec_vial/vial.json`, the `customKeycodes` array currently reads:

```json
    "customKeycodes": [
        {"name": "ST(W)", "title": "", "shortName": "ST(W)"},
        {"name": "ST(A)", "title": "", "shortName": "ST(A)"},
        {"name": "ST(S)", "title": "", "shortName": "ST(S)"},
        {"name": "ST(D)", "title": "", "shortName": "ST(D)"}
    ], 
```

Add a fifth entry so it reads:

```json
    "customKeycodes": [
        {"name": "ST(W)", "title": "", "shortName": "ST(W)"},
        {"name": "ST(A)", "title": "", "shortName": "ST(A)"},
        {"name": "ST(S)", "title": "", "shortName": "ST(S)"},
        {"name": "ST(D)", "title": "", "shortName": "ST(D)"},
        {"name": "HanEngCorrect", "title": "", "shortName": "한영교정"}
    ], 
```

Vial assigns customKeycodes labels positionally starting at `QK_USER_0` — the 5th entry (index 4) corresponds to `QK_USER_0 + 4`, matching the enum value from Step 1.

- [ ] **Step 3: Compile to verify no errors**

Run: `qmk compile -kb ydkb/evoli -km agar_ec_vial`
Expected: build succeeds (ends with `Creating binary load file` / `.uf2` output), no "redefinition" or "unused" errors. The enum constant isn't referenced anywhere yet — that's fine, C doesn't warn on unused enum constants.

- [ ] **Step 4: Commit**

```bash
git add keyboards/ydkb/evoli/led.c keyboards/ydkb/evoli/keymaps/agar_ec_vial/vial.json
git commit -m "feat: reserve HANENG_CORRECT keycode and expose it in Vial"
```

---

### Task 2: Keystroke buffer and tracking bookkeeping

**Files:**
- Modify: `keyboards/ydkb/evoli/led.c:33-38` (state + helper functions, added right after Task 1's enum)

**Interfaces:**
- Consumes: `HANENG_CORRECT` from Task 1.
- Produces: `static void haneng_track_keystroke(uint16_t keycode, keyrecord_t *record);` — called by Task 3 from inside `process_record_user`. Also produces the file-static state (`keystroke_buf`, `buf_len`, `buf_invalid`) that Task 3's `HANENG_CORRECT` case reads directly (same translation unit).

- [ ] **Step 1: Add the buffer state and helper function**

Immediately after the enum added in Task 1, add:

```c
typedef struct {
    uint16_t keycode;
    uint8_t  mods;
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

static void haneng_track_keystroke(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return;
    if (keycode == HANENG_CORRECT) return; // never buffer the correction key itself

    // Stale buffer from an idle gap - start fresh before processing this key.
    if (haneng_buf_len > 0 && timer_elapsed(haneng_last_keytime) > HANENG_TIMEOUT_MS) {
        haneng_buffer_reset();
    }

    // Word boundaries: reset and don't buffer the boundary key itself.
    if (keycode == KC_SPC || keycode == KC_ENT || keycode == KC_TAB || keycode == KC_BSPC) {
        haneng_buffer_reset();
        haneng_last_keytime = timer_read();
        return;
    }

    // Bare Ctrl/Alt/Gui keydown: treat as a boundary too, don't buffer the modifier itself.
    switch (keycode) {
        case KC_LCTL: case KC_RCTL:
        case KC_LALT: case KC_RALT:
        case KC_LGUI: case KC_RGUI:
            haneng_buffer_reset();
            haneng_last_keytime = timer_read();
            return;
        case KC_LSFT: case KC_RSFT:
            // Shift alone produces no character - ignore, don't touch the buffer.
            return;
        default:
            break;
    }

    uint8_t pressed_mods = get_mods();
    const uint8_t blocking_mods = MOD_BIT(KC_LCTL) | MOD_BIT(KC_RCTL) |
                                   MOD_BIT(KC_LALT) | MOD_BIT(KC_RALT) |
                                   MOD_BIT(KC_LGUI) | MOD_BIT(KC_RGUI);
    if (pressed_mods & blocking_mods) {
        haneng_buffer_reset();
        haneng_last_keytime = timer_read();
        return;
    }

    haneng_last_keytime = timer_read();

    if (haneng_buf_invalid) return;

    if (haneng_buf_len >= HANENG_BUF_MAX) {
        haneng_buf_invalid = true;
        return;
    }

    haneng_buf[haneng_buf_len].keycode = keycode;
    haneng_buf[haneng_buf_len].mods    = pressed_mods;
    haneng_buf_len++;
}
```

This function is written but not called yet — Task 3 wires it into `process_record_user`.

- [ ] **Step 2: Compile to verify no errors**

Run: `qmk compile -kb ydkb/evoli -km agar_ec_vial`
Expected: build succeeds. Expect a compiler warning that `haneng_track_keystroke` is defined but not used — that's expected at this point and resolved in Task 3. If the build hard-fails (not just a warning) on this, stop and fix the syntax error before proceeding.

- [ ] **Step 3: Commit**

```bash
git add keyboards/ydkb/evoli/led.c
git commit -m "feat: add HanEngCorrect keystroke buffer and tracking logic"
```

---

### Task 3: Wire tracking and the correction action into `process_record_user`

**Files:**
- Modify: `keyboards/ydkb/evoli/led.c:200-232` (existing `process_record_user`)

**Interfaces:**
- Consumes: `haneng_track_keystroke()`, `haneng_buf`, `haneng_buf_len`, `haneng_buf_invalid` from Task 2; `HANENG_CORRECT` from Task 1.

**Context:** This board already defines `process_record_user` (shown below, current state) to handle a bootloader-reset keycode and one F4/tilde remap keycode. We add our tracking call unconditionally at the top, and a new `case HANENG_CORRECT:` branch, without touching the existing two cases.

Current function for reference:

```c
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    static uint8_t mod_keys_registered;
    uint8_t pressed_mods = get_mods();
    switch (keycode) {
        case 0x5c00: // via/vial reset to bootloader
            ...
        case 0x5F8F:
            ...
        default:
            return true; // Process all other keycodes normally
    }
}
```

- [ ] **Step 1: Add the tracking call and the new case**

Replace the function with:

```c
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    static uint8_t mod_keys_registered;
    uint8_t pressed_mods = get_mods();

    haneng_track_keystroke(keycode, record);

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
                if (!haneng_buf_invalid && haneng_buf_len > 0) {
                    register_code(KC_LCTL);
                    register_code(KC_LSFT);
                    tap_code(KC_LEFT);
                    unregister_code(KC_LSFT);
                    unregister_code(KC_LCTL);
                    tap_code(KC_BSPC);
                    tap_code(KC_LANG1);
                    for (uint8_t i = 0; i < haneng_buf_len; i++) {
                        uint8_t mods = haneng_buf[i].mods;
                        if (mods) register_mods(mods);
                        tap_code(haneng_buf[i].keycode);
                        if (mods) unregister_mods(mods);
                        wait_ms(10);
                    }
                }
            }
            return false;
        default:
            return true; // Process all other keycodes normally
    }
}
```

- [ ] **Step 2: Compile to verify no errors**

Run: `qmk compile -kb ydkb/evoli -km agar_ec_vial`
Expected: build succeeds, no warnings about `haneng_track_keystroke` being unused anymore.

- [ ] **Step 3: Commit**

```bash
git add keyboards/ydkb/evoli/led.c
git commit -m "feat: wire HanEngCorrect tracking and replay into process_record_user"
```

---

### Task 4: Flash and manually verify on real hardware

This task has no automated gate — QMK firmware behavior depends on the real OS IME, which can't be simulated in this environment. **The user must perform these steps and report the actual observed behavior before this task is considered done.**

**Files:** none (build artifact only)

- [ ] **Step 1: Build the flashable image**

Run: `qmk compile -kb ydkb/evoli -km agar_ec_vial`
Expected: produces `ydkb_evoli_agar_ec_vial.bin` (or `.uf2` depending on board config) in the repo root or `.build/`.

- [ ] **Step 2: User flashes the firmware**

Double-tap the reset button to enter UF2 bootloader mode, copy the built image to the mounted bootloader drive (per existing project flashing procedure — see `vial-qmk-evoli-led-handoff.md` if unsure).

- [ ] **Step 3: User assigns HANENG_CORRECT to a physical key in Vial**

Open Vial GUI → select the keyboard → pick any free key slot → in the keycode picker, find the custom keycode labeled "HanEngCorrect" (from Task 1's vial.json entry) → assign it to that key.

- [ ] **Step 4: Manual test — English typed while IME was in Korean mode**

1. Switch Windows IME to Korean (한글) mode.
2. Type an English word, e.g. `hello` (du-beolsik will turn this into garbled Hangul jamo).
3. Without moving the cursor, press the HANENG_CORRECT key.
4. Expected: the garbled Hangul is deleted, IME switches to English, and `hello` appears correctly.

- [ ] **Step 5: Manual test — Korean typed while IME was in English mode**

1. Switch Windows IME to English mode.
2. Type a Korean word using du-beolsik key positions intending Hangul, e.g. intending `안녕` (this will appear as literal English letters since the IME is in English mode).
3. Without moving the cursor, press the HANENG_CORRECT key.
4. Expected: the English letters are deleted, IME switches to Korean, and `안녕` is composed correctly.

- [ ] **Step 6: Manual test — buffer reset behaviors**

1. Type a word, wait 6+ seconds without typing anything else, then press HANENG_CORRECT.
   Expected: nothing happens (buffer expired from the 5-second timeout).
2. Type a word, press Backspace once anywhere in it, finish typing, then press HANENG_CORRECT immediately.
   Expected: nothing happens (backspace invalidated the buffer for that word).
3. Type a 17+ character single word (no spaces) then press HANENG_CORRECT.
   Expected: nothing happens (buffer overflow marks it invalid).

- [ ] **Step 7: Report results**

User confirms which of Steps 4-6 passed as expected. Any mismatch should be filed as a follow-up fix, not silently accepted.

---

## Self-Review Notes

- **Spec coverage:** word-boundary set (space/enter/tab) → Task 2 Step 1; backspace full-reset → Task 2 Step 1; ctrl/alt/gui boundary with shift exemption → Task 2 Step 1; 16-entry buffer + overflow invalidation → Task 2 Step 1; 5s timeout → Task 2 Step 1; `Ctrl+Shift+Left`+`Backspace` deletion (no `Ctrl+Backspace`, no `End`) → Task 3 Step 1; `KC_LANG1` toggle → Task 3 Step 1; replay of buffered keystrokes → Task 3 Step 1; manual verification of both mode-mistake directions and all three reset triggers → Task 4.
- **Placeholder scan:** none found — all steps contain complete code or exact manual instructions.
- **Type consistency:** `HANENG_CORRECT` (Task 1) used identically in Task 2's exclusion check and Task 3's `case` label. `haneng_buf` / `haneng_buf_len` / `haneng_buf_invalid` names introduced in Task 2 are used identically in Task 3 — no renames across tasks.
