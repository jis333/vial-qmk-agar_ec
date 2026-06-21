# HanEngCorrect — design

## 배경

QMK firmware는 host OS의 IME(한/영) 상태를 직접 읽을 수 없다 (HID 표준상 호스트→키보드 방향으로 그런 정보가 전달되지 않음). 마찬가지로 화면에 실제로 어떤 글자가 그려졌는지도 키보드는 알 수 없다. 이 설계는 이 두 가지 제약을 정면으로 풀려고 하지 않고, **toggle은 방향에 의존하지 않는다는 점**과 **OS 자체의 단어 단위 텍스트 선택 기능에 위임할 수 있다는 점**을 이용해 우회한다.

## 목표

타이핑한 직전 한 단어가 잘못된 언어 모드(한글/영문)로 입력되어 깨졌을 때, 전용 키 한 번으로 "지우고, 모드 전환하고, 다시 입력"을 한 번에 처리한다.

## 동작 흐름

사용자가 `HANENG_CORRECT` 키를 누르면:

1. `Ctrl+Shift+Left` — 커서 기준 직전 단어 선택
2. `KC_BSPC` — 선택 영역 삭제
3. `KC_LANG1` tap — 한영 토글 (실제 OS에 바인딩된 한영전환 키와 일치)
4. 내부 버퍼에 저장된 직전 단어의 keycode 시퀀스를 그대로 재입력

토글은 현재 실제 모드를 모를 필요가 없다 — 토글이라는 동작 자체가 "지금 뭐든 반대로" 이므로, 사용자가 눈으로 확인하고 키를 누르는 시점엔 항상 올바른 방향으로 전환된다.

단어 삭제는 `Ctrl+Backspace` 대신 `Ctrl+Shift+Left` + `Backspace`를 쓴다. `Ctrl+Backspace`는 일부 Windows 앱에서 다른 동작에 가로채여 신뢰도가 낮은 반면, 텍스트 선택 단축키는 더 광범위하게 동일하게 동작한다 (텍스트 네비게이션은 단어삭제 단축키보다 보편적으로 구현돼 있음). `End` 키는 커서 위치를 줄 끝으로 강제 이동시켜 의도와 다른 텍스트까지 건드릴 위험이 있어 제외한다.

## 데이터 모델

```c
typedef struct {
    uint16_t keycode;
    uint8_t  mods;
} buffered_key_t;

static buffered_key_t keystroke_buf[16];
static uint8_t  buf_len;
static bool     buf_invalid;
static uint16_t last_keytime;
```

- `keystroke_buf`: 현재 추적 중인 단어의 keycode+모디파이어 시퀀스. 최대 16건.
- `buf_invalid`: 16건을 넘는 키스트로크가 들어와 추적을 포기한 상태. 이 상태에서 `HANENG_CORRECT`는 아무 동작도 하지 않는다.
- `last_keytime`: 마지막으로 버퍼에 키가 추가된 시각. 5초 경과 시 stale로 간주해 리셋.

## 버퍼 리셋 트리거

다음 중 하나라도 발생하면 `buf_len = 0`, `buf_invalid = false`로 리셋한다:

- `KC_SPC` / `KC_ENT` / `KC_TAB` 입력
- `KC_BSPC` 입력 (사유: 아래 "백스페이스 처리" 참조)
- `Ctrl` / `Alt` / `Gui`가 (단독이든 조합이든, `Shift` 제외) 눌린 상태에서의 키 입력
- 마지막 버퍼 추가 시점으로부터 5초 경과

## 백스페이스 처리 — 부분 추적을 포기하는 이유

한글 조합 중 backspace 1회는 보통 키스트로크 1개를 되돌리는 것과 거의 일치한다 (아직 확정되지 않은 자모는 입력 순서대로 분해됨). 그러나 이미 확정(commit)된 음절까지 거슬러 지우는 경우, OS는 그 음절을 자모 단위로 분해하지 않고 통째로 1회에 지운다. 이 경우 backspace 1회가 버퍼 entry 여러 개에 해당하는데 버퍼는 1개만 줄이게 되어, 버퍼가 화면보다 많은 내용이 남아있다고 오인하는 상태로 어긋난다.

이 desync를 정밀하게 추적하는 대신, **backspace가 눌리면 버퍼를 전체 리셋**한다. 타이핑 도중 자기수정을 한 단어는 이후 `HANENG_CORRECT` 대상에서 제외되며, 사용자는 처음부터 다시 입력해야 한다. 한영 모드 실수는 보통 단어를 다 입력한 후에야 인지하므로, 타이핑 중간 자기수정과 언어모드 실수가 겹치는 경우는 드물다고 보고 이 한계를 받아들인다.

## 키스트로크 추적 (정상 입력 중)

`process_record_user`에서, 위 리셋 트리거에 해당하지 않는 일반 키 입력은:

- `buf_invalid == true` → 추가하지 않음 (계속 무시 상태 유지)
- `buf_len < 16` → `keystroke_buf[buf_len++] = {keycode, mods}`, `last_keytime` 갱신
- `buf_len == 16`에서 한 건 더 들어오면 → `buf_invalid = true`로 전환 (이미 버퍼에 든 16건도 더 이상 신뢰할 수 없으므로 폐기 취급)

## `HANENG_CORRECT` 처리

```
if buf_invalid or buf_len == 0:
    return   // 아무 동작 안 함

tap: Ctrl+Shift+Left
tap: KC_BSPC
tap: KC_LANG1
for entry in keystroke_buf[0..buf_len]:
    keystroke entry 재현 (각 사이 짧은 delay)
```

버퍼는 동작 후에도 비우지 않는다 — 같은 내용이 화면에 재현되었으므로, 사용자가 다시 잘못 판단해 한 번 더 누르면 다시 반대로 토글되는 대칭적 동작이 된다.

## 알려진 한계 (의도적으로 받아들이는 범위)

- 타이핑 직후, 커서를 옮기지 않은 상태에서만 정상 동작한다 (마우스 클릭/화살표 이동 후에는 버퍼와 실제 커서 위치가 어긋날 수 있음 — 5초 타임아웃으로 확률을 낮춤).
- `Ctrl+Shift+Left`의 단어 경계 판정은 앱의 OS 텍스트 필드 구현에 의존한다. 사용자의 현재 Windows 환경에서는 정상 동작 확인됨 (CJK 음절 단위로 끊기는 문제 없음).
- 자동완성/자동수정이 활성화된 입력창에서는 버퍼와 화면 내용이 어긋날 수 있다 (사용자의 현재 환경에는 해당 없음).
- 16건을 넘는 긴 단어는 추적을 포기하고 `HANENG_CORRECT`가 무동작한다.
- 한영 토글 키는 `KC_LANG1`로 고정 (사용자의 실제 OS 설정과 일치 확인됨).

## 적용 대상

`keyboards/ydkb/evoli/keymaps/agar_ec_vial/keymap.c` — `process_record_user` 신규 추가, 커스텀 키코드 `HANENG_CORRECT` 정의 후 keymap 배열의 빈 슬롯에 배치.
