VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
QMK_SETTINGS = yes
TAP_DANCE_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
# DIAG: console ON so the WS2812 data-pin sweep can xprintf which pin it is
# driving (read it via `qmk console`). Board default is `?= yes`; this keymap
# previously hard-set `no`, which is why xprintf was a silent no-op. Revert to
# `no` once the DIN pin is identified.
CONSOLE_ENABLE = yes