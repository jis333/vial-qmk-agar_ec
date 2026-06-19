# project specific files
SRC ?=	matrix.c ec_matrix.c led.c rawhid_user.c

# MCU name
MCU = STM32F103
BOARD = MAPLEMINI_STM32_F103

# Bootloader selection
MCU_LDSCRIPT = STM32F103CBT6_uf2_bootloader
BOOTLOADER = custom

# Build Options
#   change yes to no to disable
#
CUSTOM_MATRIX           = yes # Custom matrix file
UNICODE_ENABLE          = yes # Unicode
BOOTMAGIC_ENABLE = no       # Enable Bootmagic Lite
MOUSEKEY_ENABLE = yes       # Mouse keys
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE ?= yes       # Console for debug
COMMAND_ENABLE = yes        # Commands for debug and configuration
NKRO_ENABLE = yes           # Enable N-Key Rollover
BACKLIGHT_ENABLE = no      # Enable keyboard backlight functionality
RGBLIGHT_ENABLE = yes       # Enable keyboard RGB underglow
RGBLIGHT_DRIVER = custom    # led.c에서 직접 rgblight_driver_t를 정의함 (인디케이터 오버레이 등 커스텀 로직 때문)
WS2812_DRIVER_REQUIRED = yes # custom 모드에서는 자동 설정 안되므로 명시 필요
WS2812_DRIVER = bitbang     # STM32 GPIO 직접 비트뱅잉. 보드에 별도 I2C LED 컨트롤러 IC가 있다면 i2c로 변경
AUDIO_ENABLE = no           # Audio output
SLEEP_LED_ENABLE = no




# Enter lower-power sleep mode when on the ChibiOS idle thread
OPT_DEFS += -DCORTEX_ENABLE_WFI_IDLE=TRUE
OPT_DEFS += -DCORTEX_VTOR_INIT=0x4000
