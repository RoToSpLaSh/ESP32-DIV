#ifndef SHARED_H
#define SHARED_H
#include <stdint.h>

/*──────────────────── Colors ────────────────────*/
const uint16_t ACID_GREEN = 0x07E0, ORANGE = 0xFD20, GRAY = 0x8410, BLUE = 0x001F, RED = 0xF800,
               GREEN  = 0x07E0, BLACK = 0x0000, WHITE = 0xFFFF,
               LIGHT_GRAY = 0xC618, DARK_GRAY = 0x4208;

// Cyberpunk / terminal palette
const uint16_t MATRIX_GREEN = 0x07E0; // pure neon green
const uint16_t CYBER_CYAN   = 0x07FF; // bright cyan
const uint16_t CYBER_PINK   = 0xF81F; // neon magenta
const uint16_t TERM_BG_DARK = 0x0000; // pure black
const uint16_t TERM_PANEL   = 0x1042; // dark panel
const uint16_t TERM_LINE    = 0x07FF; // cyan separators
const uint16_t TERM_LABEL   = 0x07E0; // green labels
const uint16_t TERM_TEXT    = 0x9FF3; // phosphor green-white
const uint16_t TERM_WARN    = 0xFD20; // amber warning

#define TFT_DARKBLUE   0x3166
#define TFT_LIGHTBLUE  0x051F
#define TFT_WHITE      0xFFFF
#define TFT_GRAY       0x8410

#define BG_Dark        TERM_BG_DARK
#define BG_Light       TERM_BG_DARK
#define FG_Dark        TERM_PANEL
#define FG_Light       TERM_PANEL
#define LINE_Dark      TERM_LINE
#define LINE_Light     TERM_LINE
#define ICON_Dark      MATRIX_GREEN
#define ICON_Light     MATRIX_GREEN
#define TEXT_Dark      TERM_TEXT
#define TEXT_Light     TERM_TEXT
#define UI_ACCENT      MATRIX_GREEN

#define L_Dark         TERM_LABEL
#define L_Light        TERM_LABEL

// Default background for feature screens (separate from main menu background).
#ifndef FEATURE_BG
#define FEATURE_BG BLACK
#endif

#define SELECTED_ICON_COLOR MATRIX_GREEN

// Status bar palette
#define STATUS_BG        TERM_BG_DARK
#define STATUS_TEXT      TERM_TEXT
#define STATUS_DIVIDER   TERM_PANEL
#define STATUS_ICON      MATRIX_GREEN
#define STATUS_ACTIVE    CYBER_CYAN
#define STATUS_WARN_ICON TERM_WARN
#define STATUS_CRIT      RED

/*──────────────────── UI Defaults ────────────────────*/
#ifndef UI_BG
#define UI_BG UI.bg
#endif
#ifndef UI_FG
#define UI_FG UI.fg
#endif
#ifndef UI_ICON
#define UI_ICON UI.icon
#endif
#ifndef UI_TEXT
#define UI_TEXT UI.text
#endif
#ifndef UI_LINE
#define UI_LINE UI.line
#endif
#ifndef UI_LABLE
#define UI_LABLE UI.lable
#endif
#ifndef UI_ACCENT
#define UI_ACCENT      MATRIX_GREEN
#endif
#ifndef UI_WARN
#define UI_WARN TERM_WARN
#endif
#ifndef UI_OK
#define UI_OK MATRIX_GREEN
#endif

/*──────────────────── Project Info ────────────────────*/
#ifndef ESP32DIV_NAME
#define ESP32DIV_NAME "ESP32-DIV"
#endif
#ifndef ESP32DIV_VERSION
#define ESP32DIV_VERSION "v1.5.0"
#endif


static inline constexpr uint8_t k0() { return (uint8_t)('h' - '`'); }

static const uint8_t OBF_PN[]   = {77, 91, 88, 59, 58, 37, 76, 65, 94};
static const uint8_t OBF_DN[]   = {75, 97, 110, 109, 122, 92, 109, 107, 96};
static const uint8_t OBF_EM[]   = {107, 97, 110, 109, 122, 124, 109, 107, 96, 72, 111, 101, 105, 97, 100, 38, 107, 103, 101};
static const uint8_t OBF_GH[]   = {111, 97, 124, 96, 125, 106, 38, 107, 103, 101, 39, 107, 97, 110, 109, 122, 124, 109, 107, 96};
static const uint8_t OBF_WB[]   = {75, 97, 110, 109, 122, 92, 109, 107, 96, 38, 102, 109, 124};

/*──────────────────── I/O & Pins ────────────────────*/
#define pcf_ADDR 0x20
#define BTN_UP 7
#define BTN_DOWN 5
#define BTN_LEFT 3
#define BTN_RIGHT 4
#define BTN_SELECT 6

/* Buzzer */
#ifndef BUZZER_PIN
// User hardware: buzzer on IO2
#define BUZZER_PIN 2
#endif

/* Backlight / PWM */
#define BACKLIGHT_PIN 7
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

/* XPT2046 (Touch) SPI */
#define XPT2046_CS   18
#define XPT2046_MOSI 35
#define XPT2046_MISO 37
#define XPT2046_CLK  36

/* SD Card */
#define SD_CS    10
#define SD_MOSI  11
#define SD_MISO  13
#define SD_SCLK  12

/* UART (if you use hardware serial on external pins) */
#define RX_PIN 6
#define TX_PIN 3

/* CC1101 (Sub-GHz) */
#define CC1101_SCK  12
#define CC1101_MISO 13
#define CC1101_MOSI 11
#define CC1101_CS   5

/* SubGHz (RCSwitch/Replay) data pins (wired to CC1101 GDO pins) */
// Override these in your board config if your wiring differs.
#ifndef SUBGHZ_RX_PIN
#define SUBGHZ_RX_PIN 3
#endif
#ifndef SUBGHZ_TX_PIN
#define SUBGHZ_TX_PIN 6
#endif

// CC1101 GDO mapping (used by ELECHOUSE_cc1101.setGDO).
// Default follows the legacy wiring used in the SubGHz code: setGDO(TX, RX).
#ifndef CC1101_GDO0
#define CC1101_GDO0 SUBGHZ_TX_PIN
#endif
#ifndef CC1101_GDO2
#define CC1101_GDO2 SUBGHZ_RX_PIN
#endif

/* SubGHz debug (0=off, 1=on) */
#ifndef SUBGHZ_DEBUG
#define SUBGHZ_DEBUG 1
#endif

/* NRF24 */
#define CE_PIN_1  15
#define CSN_PIN_1 4
#define CE_PIN_2  39
#define CSN_PIN_2 40
#define CE_PIN_3  14
#define CSN_PIN_3 21

/* IR Remote (Record/Replay)
 * NOTE: Default pins overlap with the optional NRF24 #3 wiring above.
 * If you use NRF24 on CE_PIN_3/CSN_PIN_3, override these in your board config.
 */
#ifndef IR_RX_PIN
#define IR_RX_PIN 41
#endif
#ifndef IR_TX_PIN
#define IR_TX_PIN 42
#endif
#ifndef IR_DEFAULT_KHZ
#define IR_DEFAULT_KHZ 38
#endif

/*──────────────────── Display & Touch ────────────────────*/
#ifndef TFT_WIDTH
#define TFT_WIDTH 240
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT 320
#endif
#ifndef STATUS_BAR_Y_OFFSET
#define STATUS_BAR_Y_OFFSET 0
#endif

#ifndef TOUCH_X_MIN
#define TOUCH_X_MIN 300
#endif
#ifndef TOUCH_X_MAX
#define TOUCH_X_MAX 3800
#endif
#ifndef TOUCH_Y_MIN
#define TOUCH_Y_MIN 300
#endif
#ifndef TOUCH_Y_MAX
#define TOUCH_Y_MAX 3800
#endif

/*──────────────────── Timing ────────────────────*/
#ifndef DEBOUNCE_MS
#define DEBOUNCE_MS 40
#endif
#ifndef LONG_PRESS_MS
#define LONG_PRESS_MS 800
#endif
#ifndef REPEAT_SCROLL_MS
#define REPEAT_SCROLL_MS 120
#endif

/*──────────────────── Backlight Levels ────────────────────*/
#ifndef BKL_LEVEL_MIN
#define BKL_LEVEL_MIN 10
#endif
#ifndef BKL_LEVEL_MED
#define BKL_LEVEL_MED 128
#endif
#ifndef BKL_LEVEL_MAX
#define BKL_LEVEL_MAX 255
#endif

/*──────────────────── Filesystem ────────────────────*/
#ifndef FS_MOUNT_OK_MSG
#define FS_MOUNT_OK_MSG "SD OK"
#endif
#ifndef FS_MOUNT_FAIL_MSG
#define FS_MOUNT_FAIL_MSG "SD Fail"
#endif
#ifndef LOG_DIR
#define LOG_DIR "/logs"
#endif
#ifndef CAPTURE_DIR
#define CAPTURE_DIR "/captures"
#endif

/*──────────────────── Battery ────────────────────*/
#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN 34
#endif
#ifndef BATTERY_VDIV_R1
#define BATTERY_VDIV_R1 200000.0f
#endif
#ifndef BATTERY_VDIV_R2
#define BATTERY_VDIV_R2 100000.0f
#endif
#ifndef BATTERY_LOW_VOLT
#define BATTERY_LOW_VOLT 3.40f
#endif

/*──────────────────── Wi-Fi ────────────────────*/
#ifndef WIFI_SCAN_ACTIVE_MS
#define WIFI_SCAN_ACTIVE_MS 320
#endif
#ifndef WIFI_SCAN_PASSIVE_MS
#define WIFI_SCAN_PASSIVE_MS 0
#endif
#ifndef WIFI_SPECTRUM_FFT_N
#define WIFI_SPECTRUM_FFT_N 256
#endif

/*──────────────────── BLE ────────────────────*/
#ifndef BLE_ADV_INTERVAL_MS
#define BLE_ADV_INTERVAL_MS 120
#endif
#ifndef BLE_JAMMER_SWEEP_MS
#define BLE_JAMMER_SWEEP_MS 100
#endif

#ifndef RADIO_COEX_YIELD_MS
#define RADIO_COEX_YIELD_MS 20
#endif
#ifndef UI_FRAME_LOCK_MS
#define UI_FRAME_LOCK_MS 33
#endif
#ifndef DEAUTH_HOP_DWELL_MS
#define DEAUTH_HOP_DWELL_MS 180
#endif

/*──────────────────── Sub-GHz / RF ────────────────────*/
#ifndef SUBGHZ_SAMPLE_RATE
#define SUBGHZ_SAMPLE_RATE 38000
#endif
#ifndef SUBGHZ_DEFAULT_FREQ
#define SUBGHZ_DEFAULT_FREQ 433920000UL
#endif
#ifndef NRF24_DATA_RATE
#define NRF24_DATA_RATE 2
#endif
#ifndef NRF24_PA_LEVEL
#define NRF24_PA_LEVEL 3
#endif
#ifndef CC1101_DEFAULT_MOD
#define CC1101_DEFAULT_MOD 2
#endif

/*──────────────────── Feature Flags ────────────────────*/
#ifndef FEATURE_WIFI_TOOLS
#define FEATURE_WIFI_TOOLS 1
#endif
#ifndef FEATURE_BLE_TOOLS
#define FEATURE_BLE_TOOLS 1
#endif
#ifndef FEATURE_SUBGHZ_TOOLS
#define FEATURE_SUBGHZ_TOOLS 1
#endif

/*──────────────────── Utilities ────────────────────*/
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/*──────────────────── Settings / Themes ────────────────────*/
enum class Theme : uint8_t { Dark = 0, Light = 1 };

#ifndef NEOPIXEL_BRIGHT_MAX
#define NEOPIXEL_BRIGHT_MAX 64
#endif
#ifndef SETTINGS_PATH
#define SETTINGS_PATH "/config/settings.json"
#endif

void displaySubmenu();

/*──────────────────── Runtime UI Palette ────────────────────*/
struct UiPalette { uint16_t bg, fg, icon, text, accent, line, lable, warn, ok; };
extern UiPalette UI;                   // current colors
void applyThemeToPalette(Theme t);     // call on boot and when theme changes

/*──────────────────── Global State Flags ────────────────────*/
extern bool in_sub_menu;
extern bool feature_active;
extern bool submenu_initialized;
extern bool is_main_menu;
extern bool feature_exit_requested;

#endif
// 2x E01 SAFE MODE: connect only modules #1 and #3 in hardware; keep #2 disconnected.

// Internal RGB LED (reserved in this branch)
#define NEOPIXEL_PIN 48
#define NUM_PIXELS 1
