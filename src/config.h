#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Strip Defaults ---
#define DEFAULT_STRIP_PIN       D1
#define DEFAULT_STRIP_LENGTH    60
#define MAX_STRIPS              5
#define MAX_LEDS_PER_STRIP      300

// Valid GPIO pins for LED data (D1, D2, D5, D6, D7)
const uint8_t VALID_LED_PINS[] = {5, 4, 14, 12, 13};
const uint8_t NUM_VALID_PINS = 5;

// --- Brightness ---
#define DEFAULT_BRIGHTNESS      128

// --- Programmes ---
#define NUM_BUILTIN_PROGRAMMES  4
#define PROG_SUNSET             0
#define PROG_RAINBOW            1
#define PROG_NIGHTLIGHT         2
#define PROG_SKY_AT_NIGHT       3
#define DEFAULT_PROGRAMME       PROG_SUNSET

// Programme names (for display and persistence)
const char* const PROGRAMME_NAMES[] = {
    "sunset",
    "rainbow",
    "nightlight",
    "sky_at_night"
};

// --- Wi-Fi ---
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_AP_SSID_PREFIX     "LED-Controller-"
#define WIFI_AP_PASSWORD        "configure"

// --- LittleFS ---
#define CONFIG_FILE_PATH        "/config.json"

// --- Timing ---
#define TARGET_FPS              60
#define FRAME_INTERVAL_MS       (1000 / TARGET_FPS)

#endif // CONFIG_H
