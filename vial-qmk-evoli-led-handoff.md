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
4. ~~**★ DIN은 진짜 WS2812, 위치는 매트릭스 핀 11개 중 하나.**~~ **← 세션 4에서 반증됨(아래 참조).**
   - (당시 근거: 11개 매트릭스 핀에 OFF 프레임 → "전원-인가 흰색"이 꺼졌다. 그러나 그 "흰색"은 부트로더 인계 시 얼어붙은 색이었고, 모든 핀을 LOW park 한 것만으로도 꺼질 수 있어 근거가 약했음.)

## ★ 세션 4 핵심 결론 (2026-06-20, 세션 3 #2·#3·#4 정정)
1. **`CONSOLE_ENABLE`은 고장이 아니라 키맵 `rules.mk`에서 `no`로 꺼져 있었을 뿐**(보드 기본 `?=yes`를 덮음). `yes`로 켜니 `qmk console`로 정상 수신 → **이제 핀 식별은 깜빡임 세기가 아니라 콘솔 글자 출력으로.** (세션3 #2 정정: xprintf는 끄여 있어 no-op였던 것.)
2. **RGBL1은 WS2812가 맞다(확정).** 근거: 부트로더는 R/B/G로 구동, **펌웨어 인계 시 색이 latch되어 유지**, **콜드 부팅(전원 off→on)은 LED 꺼짐**. latch 유지 + 전원-인가-꺼짐 = WS2812 고유 동작. (이전의 "전원-인가 흰색"은 부트로더 인계 시 얼어붙은 색이었음 → 세션3 #3·#4의 관찰 전제가 깨짐.)
3. **매트릭스 핀 11개(B0,B2~B6,B10~B14)는 이제 신뢰성 있게 배제.** 콜드 부팅(깜깜)에서 흰색을 쏴도 무반응 → 어두움→흰색이면 보였어야 함.
4. **세션3의 "비매트릭스 17핀 배제"는 무효.** 그때 "흰색 프레임→흰색 유지→배제"의 기준 흰색이 사실 **얼어붙은 부트로더 색**이라, 올바른 DIN에 흰색을 줘도 "흰색 유지=변화 없음"으로 잘못 배제됐을 수 있음(white-on-white 혼동). → **재시험 필요.**
5. **진단 자작 비트뱅 타이밍은 스톡과 동일**(`NOP_FUDGE 0.4`/`NUMBER_NOPS 6`/동일 공식; 출력만 `palSetPort`). 타이밍은 범인 아님.
6. **★★ DIN = B15 확정.** ROUND 3(콜드 부팅+빨강 sweep)에서 `[sweep] driving B15` 순간 RGBL1이 빨강 점등. B15는 세션1~2에서 "무반응"이라 **가장 먼저 배제**했던 핀인데, 그 배제가 바로 white-on-frozen 혼동이었음. B15는 **매트릭스 핀 아님(자유 GPIO)** → 공유 문제 없음. 풀사이즈 형제 `agar_ec_vial`도 B15 사용. → `config.h` `WS2812_DI_PIN B1→B15`, 키맵 `DIAG_PIN_SWEEP` off(코드는 #ifdef 뒤에 보존). **하드웨어 검증 대기**(캡스락→청록, 타이핑 시 색 유지).

## 상태: 해결됨 (DIN=B15, 진단 제거 완료) — 브랜치 `bugfix/led`
- **하드웨어 검증 OK**: 캡스락→청록 점등, 타이핑해도 색 안 깨짐(사용자 확인).
- **진단 코드 전부 제거**: 키맵 `config.h`의 `DIAG_PIN_SWEEP`/`CONFIG_BOOT_TEST_RGB`/구 매크로 주석, `rules.mk` `CONSOLE_ENABLE`→`no` 환원, `matrix.c`의 `#ifdef DIAG_PIN_SWEEP return 0`, `led.c`의 `diag_*` 블록·`housekeeping_task_user`·`CONFIG_BOOT_TEST_RGB`·`DIAG_FORCE_INDICATOR_ON_CAPS` 제거. (`hook_keyboard_loop()` 빈 함수는 `matrix.c`의 weak `matrix_scan_kb()` 링크용으로 보존.)
- `config.h` `WS2812_DI_PIN = B15`. flush 직전 `gpio_set_pin_output` 핵은 보존(B15는 비공유라 방어적 무해).

## 남은 논의거리 (별건)
- 캡스락 색: 현재 고정 청록(`led.c user_config_init`) vs VIA "CapsLock Color" 옵션 복원 — 사용자와 재논의.

## 참고
- **빌드(QMK MSYS)**: `MSYSTEM=MINGW64 CHERE_INVOKING=1 C:\QMK_MSYS\usr\bin\bash.exe -lc "cd '/c/Users/wltjd/OneDrive/keyboard/firmware repository/vial-qmk' && qmk compile -kb ydkb/evoli -km agar_mini_ec_vial"` 또는 `go.sh`. 사용자가 직접 컴파일/플래시.
- **STM32F103 핀 사용**: row=B0~B6, col(4051)=B10(EN0)/B11(EN1)/B12~B14(S0~S2), ADC=A1(discharge)/A2(in), USB=A11/A12, SWD=A13/A14. 자유 GPIO(배제 완료)=A0,A3~A10,A15,B7~B9,B15,C13~C15. **남은 DIN 후보=B0,B2~B6,B10~B14.**
- 풀사이즈 형제 `agar_ec_vial`은 B15로 16-LED WS2812 스트립 구동(검증된 핀이나 mini엔 해당 없음).
