# ydkb/evoli (Agar / Agar Mini EC) — LED 사용·디버깅 노트

이 보드의 단일 인디케이터 LED를 만질 때 **먼저 이 문서를 읽으세요.** 과거 여러
세션이 아래 함정들 때문에 LED 하나에 매달려 헤맸습니다. (그 디버깅 전체 기록은
저장소 루트 `vial-qmk-evoli-led-handoff.md` 참조.)

## TL;DR (핵심만)
- 물리 LED는 **하나**(`RGBL1`), **WS2812**(단선 데이터, 색 주소지정 가능).
- **데이터 핀 `WS2812_DI_PIN = B15`** (`keyboards/ydkb/evoli/config.h`). 매트릭스 핀 아님 → 핀 공유 문제 없음. 풀사이즈 형제 `agar_ec_vial`도 B15.
- RGB 드라이버는 **커스텀**: 코어 weak override가 아니라 `led.c`가 `rgblight_driver_t` 구조체(vtable)를 직접 등록. (`rules.mk`: `RGBLIGHT_DRIVER = custom`, `WS2812_DRIVER = bitbang`, `WS2812_DRIVER_REQUIRED = yes`.)
- 그 하나의 LED는 **rgblight '언더글로우'가 아니라 '인디케이터'로 구동됨**(아래 참조). 기본 동작 = **CapsLock 시 청록색**.

## 펌웨어가 LED를 켜는 방식
`led.c`의 `rgbled[]` 버퍼 크기 = `WS2812_LED_COUNT = PHY_INDICATOR_NUM(1) + RGBLED_NUM(1) = 2`.
WS2812 체인의 **첫 픽셀 = 물리 LED**:
- `rgbled[0]` = **인디케이터** 슬롯 → 실제 RGBL1.
- `rgbled[1]` = rgblight 언더글로우 슬롯 → 두 번째 픽셀이 없으니 **어디에도 안 보임**.
  (`my_rgblight_set_color()`가 `rgbled[PHY_INDICATOR_NUM + index]`로 오프셋해 저장하기 때문.)

→ **결론: rgblight 애니메이션/언더글로우 효과는 이 LED에 안 나온다.** 이 LED는 오직
인디케이터 경로로만 켜진다.

### 인디케이터 경로 (CapsLock 등)
1. 호스트가 USB LED 리포트를 보내면 `led_update_user()`가 호출됨.
2. `INDICATOR_FUNCT = {(1<<1)}` (=CapsLock 비트)로 `indicator_state` 비트를 세움.
3. `rgblight_set()` → 커스텀 `my_rgblight_flush()`가 `indicator_state` 비트가 켜진 인디케이터는 `indicator_color[i]`로, 꺼진 건 OFF로 칠한 뒤 `ws2812_flush()`.
4. **CapsLock 색은 `user_config_init()`에서 `indicator_color[0] = {0,255,255}`(청록)으로 고정** — VIA 레이아웃 옵션 색 계산을 덮어씀. 색 바꾸려면 거기 수정. (또는 VIA "CapsLock Color" 옵션 복원은 미정 숙제.)

### 임의 색을 직접 칠하려면
`void set_rgb_user(uint8_t r, uint8_t g, uint8_t b)` — `rgbled[]` 전체(=물리 LED 포함)를 그 색으로 채우고 즉시 flush. 부팅 시 OFF, 단발 테스트 등에 사용. **바이트 순서 GRB**(기본)라 디버깅 땐 흰색(셋 다 동일)이 순서 무관해 편함.

## ★ 시간 잡아먹은 함정들 (반드시 기억)
1. **WS2812 LED 테스트는 반드시 "콜드 부팅(전원 완전 차단 후 재인가)"에서.**
   - 진짜 WS2812는 **전원 인가 시 꺼짐(검정)**으로 시작한다.
   - UF2 부트로더가 LED를 R/B/G로 구동하다가, 펌웨어로 넘어가는 순간 **마지막 색이 latch되어 유지**된다. 플래시 직후의 "켜진 색"은 펌웨어가 켠 게 아니라 **부트로더가 남긴 잔상**이다.
   - 과거에 이 잔상을 "전원-인가 흰색"으로 오해 → "흰색 프레임 보내도 흰색 유지 → 이 핀 아님"으로 **진짜 핀(B15)을 가장 먼저 오배제**함(white-on-white 혼동). **테스트 색은 흰색 말고 빨강 같은 뚜렷한 색**으로, **콜드 부팅**에서 볼 것.

2. **`CONSOLE_ENABLE`이 키맵에서 꺼져 있으면 `xprintf`/`print`는 조용한 no-op.**
   - 보드 `rules.mk`는 `CONSOLE_ENABLE ?= yes`지만 키맵 `rules.mk`가 `no`로 덮음(프로덕션). 디버그 출력이 필요하면 키맵 `rules.mk`에서 `CONSOLE_ENABLE = yes`로 켜고 `qmk console`로 수신. 끝나면 `no`로 환원.

3. **`CUSTOM_MATRIX = yes`라 매 루프 훅 `matrix_scan_kb()`/`hook_keyboard_loop()`는 죽은 코드.**
   - 그 호출처(`quantum/matrix_common.c`)가 컴파일되지 않음(`builddefs/common_features.mk`). **매 루프 작업은 `housekeeping_task_user()`**(quantum/main.c가 항상 호출)에 둘 것. 단발 부팅 코드는 `rgblight_user_init()`(matrix_init이 호출)에.
   - `led.c`의 빈 `hook_keyboard_loop()`는 `matrix.c`의 weak `matrix_scan_kb()` 링크를 위해 **남겨둔 것**(지우면 링크 에러).

4. **커스텀 드라이버라 `RGBLED_NUM`/`WS2812_LED_COUNT` 0이면 안 됨.**
   - 현 툴체인은 0에서 div-by-zero/array-bounds(-Werror). 물리 LED가 사실상 1개여도 `RGBLED_NUM=1`, `WS2812_LED_COUNT=PHY_INDICATOR_NUM+RGBLED_NUM`로 유지.

## 새 보드/핀에서 DIN 핀을 다시 찾아야 한다면 (검증된 절차)
1. 키맵 `rules.mk`에 `CONSOLE_ENABLE = yes`.
2. `housekeeping_task_user()`에서, 후보 GPIO를 **하나씩** 솔리드 빨강으로 구동하며 `xprintf`로 핀 이름 출력하는 sweep을 돈다(핀 인자화 비트뱅; 스톡 `ws2812_bitbang.c` 타이밍 그대로 — `NOP_FUDGE 0.4`, `NUMBER_NOPS 6`). USB(A11/A12)·SWD(A13/A14)·EC ADC(A1/A2)는 제외. 매트릭스 핀을 구동할 땐 `matrix_scan()`을 임시 중단.
3. `qmk console` 띄우고 **콜드 부팅** → LED가 빨강으로 켜지는 순간 콘솔에 찍힌 핀 = DIN.
4. `WS2812_DI_PIN` 수정 → 진단 전부 제거 → `CONSOLE_ENABLE` 환원 → 캡스락/타이핑 검증.

## 관련 파일
- `keyboards/ydkb/evoli/config.h` — `WS2812_DI_PIN B15`, 보드 기본 RGB 설정.
- `keyboards/ydkb/evoli/keymaps/agar_mini_ec_vial/config.h` — `RGBLED_NUM`/`PHY_INDICATOR_NUM`/`WS2812_LED_COUNT`/`INDICATOR_FUNCT`.
- `keyboards/ydkb/evoli/led.c` — 커스텀 `rgblight_driver`, `set_rgb_user`, 인디케이터 경로, `user_config_init`(캡스락 색).
- `keyboards/ydkb/evoli/rules.mk` — `RGBLIGHT_DRIVER=custom`, `WS2812_DRIVER=bitbang`, `CUSTOM_MATRIX=yes`.
- `vial-qmk-evoli-led-handoff.md` (루트) — 전체 디버깅 타임라인.
