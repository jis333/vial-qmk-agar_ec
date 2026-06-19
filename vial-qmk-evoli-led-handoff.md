# ydkb/evoli (Agar Mini EC) — RGBL1 LED 디버깅 인계 문서

세션 1~2의 상세 타임라인은 git 히스토리(이 파일 이전 버전) 참조. 이 문서는 **세션 3(2026-06-20) 기준 현재 상태**만 간결히 담음.

## 하드웨어 / 문제
- 보드 `ydkb/evoli`, 키맵 `agar_mini_ec_vial`. STM32F103CBT6(LQFP48) / ChibiOS / EC(정전용량) 커스텀 매트릭스. 부트로더 = UF2 "Double Tap"(매직 `0x9d5bfc2b`, 백업레지스터 `0x20004000`).
- PCB에 LED 단 하나(`RGBL1`, 4핀). **부트로더는 이 LED 색을 바꿈(=하드웨어 정상).** 컴파일·키입력·매트릭스 모두 정상. **남은 문제: 메인 펌웨어가 RGBL1을 제어 못 함.**

## ★ 세션 3 핵심 결론
1. **세션 1~2의 hook 기반 진단(`DIAG_B15_BLINK` 등)은 실행조차 안 됐다.** `CUSTOM_MATRIX=yes` → `builddefs/common_features.mk:654`에서 `quantum/matrix_common.c` 제외 → 그게 유일한 `matrix_scan_kb()`(→`hook_keyboard_loop()`) 호출처라 **hook은 죽은 코드.** 관련 "무반응/실패" 기록 다수 무효.
   - **교훈: 매 루프 훅은 `housekeeping_task_user()` 사용**(`quantum/main.c:75`에서 항상 호출). 단발 부팅 코드는 `rgblight_user_init()` OK(`matrix_init`이 호출).
2. **`xprintf`는 이 빌드에서 no-op**(콘솔 출력 안 됨) → 핀 식별은 "깜빡임 횟수(blink-count)" 같은 시각적 방법으로.
3. 진단을 `housekeeping_task_user()`로 옮겨 **실제로 돌린 뒤 배제된 핀**:
   - **비매트릭스 GPIO 17개**(B15,B7,B8,B9,A0,A3~A10,A15,C13~C15): 흰색 프레임을 보내도 LED는 "전원-인가 흰색" 유지(안 꺼짐) → DIN 아님.
   - **B1**: 부팅 깨끗한 구간(`rgblight_user_init`)에 R→G→B 보내도 무반응 → DIN 아님.
4. **★ DIN은 진짜 WS2812, 위치는 매트릭스 핀 11개 중 하나.**
   - 근거: 매트릭스 스캔을 끄고(matrix.c) 11개 매트릭스 핀(B0,B2~B6,B10~B14)에 OFF 프레임을 보내자, 계속 켜져있던 **"전원-인가 흰색"이 꺼졌다.** → OFF 프레임이 DIN에 도달(=매트릭스 핀) + WS2812 프레임에 반응(=WS2812 확정, 아날로그 RGB 아님).

## 다음 세션 할 일 (우선순위)
1. **11개 중 정확한 핀 1개 식별.** 현재 빌드 그대로 플래시 후 LED를 **1분 이상** 보며 흰색 깜빡임 횟수를 셈 → 핀(led.c `sweep_pins[]` 순서):
   **1=B10 2=B11 3=B12 4=B13 5=B14 6=B0 7=B2 8=B3 9=B4 10=B5 11=B6** (PCB상 B10~B14가 1순위라 앞에 배치). 어려우면: 핀을 2~3개씩 나눠 OFF만 보내 "꺼지는지"로 이분탐색, 또는 핀별 솔리드 흰색 길게 켜기.
2. **핀 확정 → `keyboards/ydkb/evoli/config.h`의 `WS2812_DI_PIN` 변경.**
3. **매트릭스 공존 검증**: round1에서 매트릭스가 그 핀을 휘저어도 LED 흰색이 유지됐다 → 매트릭스 토글은 유효 WS2812 프레임을 못 만든다 → 깨끗한 프레임 1회 latch면 색 유지될 듯. led.c `my_rgblight_flush()`/`set_rgb_user()`의 `gpio_set_pin_output(WS2812_DI_PIN)`(flush 직전 출력 강제) 핵이 정확히 그 용도 → **핀만 맞으면 동작 가능성 큼.** `DIAG_PIN_SWEEP` 끄고 캡스락→청록, 타이핑 시 색 안 깨지는지 확인.
4. **해결 후 진단 제거**: 키맵 config.h의 `DIAG_PIN_SWEEP`/`CONFIG_BOOT_TEST_RGB`, matrix.c의 `#ifdef DIAG_PIN_SWEEP return 0`, led.c의 `DIAG_PIN_SWEEP` 블록·`housekeeping_task_user`·`hook_keyboard_loop`.
5. 캡스락 색: 현재 고정 청록(`led.c user_config_init`) vs VIA "CapsLock Color" 옵션 복원 — 사용자와 재논의.

## 현재 코드 상태 (세션 3 변경분, `WS2812_DI_PIN`은 아직 B1)
- `matrix.c`: `matrix_scan()` 최상단 `#ifdef DIAG_PIN_SWEEP return 0;` (진단 중 스캔 중단).
- `led.c`: `housekeeping_task_user()`에서 `diag_pin_sweep()` 호출 / `hook_keyboard_loop()` 빈 함수. `diag_sweep_send()`=핀 인자화 WS2812 비트뱅(`palSetPort`/`palClearPort`; 스톡 `ws2812_bitbang.c`가 핀 하드코딩이라 자작). `diag_pin_sweep()`=논블로킹 흰색 blink-count, 시작 시 전 후보 OFF, idle LOW 고정. `sweep_pins[]={B10,B11,B12,B13,B14,B0,B2,B3,B4,B5,B6}`. (잔재 유지: flush 직전 `gpio_set_pin_output` 핵, `user_config_init` 고정 청록.)
- 키맵 `config.h`: `DIAG_PIN_SWEEP` on, `CONFIG_BOOT_TEST_RGB` off, 구 진단 매크로 제거.

## 참고
- **빌드(QMK MSYS)**: `MSYSTEM=MINGW64 CHERE_INVOKING=1 C:\QMK_MSYS\usr\bin\bash.exe -lc "cd '/c/Users/wltjd/OneDrive/keyboard/firmware repository/vial-qmk' && qmk compile -kb ydkb/evoli -km agar_mini_ec_vial"` 또는 `go.sh`. 사용자가 직접 컴파일/플래시.
- **STM32F103 핀 사용**: row=B0~B6, col(4051)=B10(EN0)/B11(EN1)/B12~B14(S0~S2), ADC=A1(discharge)/A2(in), USB=A11/A12, SWD=A13/A14. 자유 GPIO(배제 완료)=A0,A3~A10,A15,B7~B9,B15,C13~C15. **남은 DIN 후보=B0,B2~B6,B10~B14.**
- 풀사이즈 형제 `agar_ec_vial`은 B15로 16-LED WS2812 스트립 구동(검증된 핀이나 mini엔 해당 없음).
