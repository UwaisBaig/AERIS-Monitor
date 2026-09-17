#pragma once
#include <stdint.h>

namespace config {
// Original ESP32 / ESP32-WROOM-32 DevKit V1, not ESP32-C3/S3.
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
constexpr uint8_t GAS_PIN = 34;       // ADC1_CH6; input-only pin
constexpr uint8_t BUTTON_PIN = 27;    // Normally-open button to GND
constexpr uint32_t I2C_HZ = 400000;
constexpr uint32_t SERIAL_BAUD = 115200;

// MQ135 MODULE AOUT -> 15k -> GPIO34 -> 10k -> GND.
// A 100nF ceramic capacitor connects GPIO34 to GND.
constexpr float DIVIDER_TOP_OHMS = 15000.0f;
constexpr float DIVIDER_BOTTOM_OHMS = 10000.0f;
constexpr float AOUT_MULTIPLIER =
    (DIVIDER_TOP_OHMS + DIVIDER_BOTTOM_OHMS) / DIVIDER_BOTTOM_OHMS;
constexpr float MIN_ADC_MV = 150.0f;
constexpr float MAX_ADC_MV = 2100.0f;

// Set true ONLY if this particular MQ135 has already been conditioned
// with its heater continuously powered at 5V for the manufacturer's
// required period. Fresh sensors keep the default below; stored sensors
// may need longer aging (see the Winsen manual and README).
constexpr bool MQ135_ALREADY_CONDITIONED = false;
constexpr uint64_t FIRST_HEAT_MS = 49ULL * 60 * 60 * 1000;
// Software settling gate after subsequent power-ups, not a guarantee of
// chemical stability. Wait longer if the output is still drifting.
constexpr uint64_t RESTART_SETTLE_MS = 10ULL * 60 * 1000;
constexpr uint32_t GAS_INTERVAL_MS = 250;
constexpr uint32_t CLIMATE_INTERVAL_MS = 2000;
constexpr uint32_t FRAME_INTERVAL_MS = 100;
constexpr uint32_t PROBE_INTERVAL_MS = 5000;
constexpr uint32_t HISTORY_INTERVAL_MS = 10000;
constexpr uint32_t REFERENCE_DURATION_MS = 30000;
constexpr uint16_t REFERENCE_MIN_SAMPLES = 80;
constexpr float REFERENCE_MAX_SPREAD = 0.03f;
constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t HOLD_MS = 2000;
constexpr uint8_t ADC_SAMPLES = 16;
constexpr float FILTER_ALPHA = 0.18f;
constexpr uint8_t HISTORY_POINTS = 120;
constexpr uint8_t PAGE_COUNT = 5;
constexpr uint8_t CONTRAST_NORMAL = 0x9F;
constexpr uint8_t CONTRAST_DIM = 0x12;
}  // namespace config
