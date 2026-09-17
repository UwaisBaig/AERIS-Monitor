/* AERIS Air Monitor | ESP32 DevKit V1 / SSD1306 128x64
 * See README.md and docs/AERIS-Build-Guide.pdf before wiring.
 * Gas output is a relative electrical response, never AQI or CO2 ppm.
 * This is an educational monitor, not a certified gas/smoke alarm.
 */
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include "Config.h"
#include "Core.h"
#include "Ui.h"

Adafruit_SSD1306 oled(128, 64, &Wire, -1, config::I2C_HZ, config::I2C_HZ);
Adafruit_BME280 bme;
Preferences preferences;
aeris::State state;
aeris::Clock runClock;
aeris::Button button;
uint64_t warmDurationMs = config::FIRST_HEAT_MS;
uint32_t gasAt = 0, climateAt = 0, frameAt = 0, historyAt = 0, probeAt = 0;
uint32_t toastAt = 0;
bool toastVisible = false, heatFinished = false;
char toastText[21] = {};

bool i2cPresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void notifyUser(const char *message) {
  snprintf(toastText, sizeof(toastText), "%s", message);
  toastAt = millis(); toastVisible = true;
  Serial.println(message);
}

void setContrast() {
  if (!state.oledOk) return;
  oled.ssd1306_command(SSD1306_SETCONTRAST);
  oled.ssd1306_command(state.dim ? config::CONTRAST_DIM : config::CONTRAST_NORMAL);
}

void probeDevices() {
  if (state.oledOk && !i2cPresent(state.oledAddress)) state.oledOk = false;
  if (!state.oledOk) {
    const uint8_t addresses[] = {0x3C, 0x3D};
    for (uint8_t address : addresses) {
      if (i2cPresent(address) && oled.begin(SSD1306_SWITCHCAPVCC, address, false, false)) {
        state.oledAddress = address; state.oledOk = true;
        oled.clearDisplay(); oled.display(); setContrast();
        Serial.printf("OLED found at 0x%02X\n", unsigned(address));
        break;
      }
    }
  }
  if (state.bmeOk && !i2cPresent(state.bmeAddress)) state.bmeOk = false;
  if (!state.bmeOk) {
    const uint8_t addresses[] = {0x76, 0x77};
    for (uint8_t address : addresses) {
      if (i2cPresent(address) && bme.begin(address, &Wire)) {
        bme.setSampling(Adafruit_BME280::MODE_NORMAL,
            Adafruit_BME280::SAMPLING_X2, Adafruit_BME280::SAMPLING_X4,
            Adafruit_BME280::SAMPLING_X2, Adafruit_BME280::FILTER_X4,
            Adafruit_BME280::STANDBY_MS_500);
        state.bmeAddress = address; state.bmeOk = true;
        Serial.printf("BME280 found at 0x%02X\n", unsigned(address));
        break;
      }
    }
  }
}

void requestReference(uint32_t now) {
  if (state.capture.active) { notifyUser("REF IN PROGRESS"); return; }
  if (state.warmRemainingMs) { notifyUser("WAIT FOR WARM-UP"); return; }
  if (!state.gasOk) { notifyUser("CHECK GAS SIGNAL"); return; }
  state.capture.start(now);
  notifyUser("SAMPLING REF: 30S");
}

void handleButton(aeris::ButtonEvent event, uint32_t now) {
  if (event == aeris::ButtonEvent::Tap) {
    state.page = (state.page + 1) % config::PAGE_COUNT;
    toastVisible = false;
  } else if (event == aeris::ButtonEvent::Hold) {
    if (state.page == 0) requestReference(now);
    else { state.dim = !state.dim; setContrast(); notifyUser(state.dim ? "DISPLAY DIM" : "DISPLAY BRIGHT"); }
  }
}

void readGas(uint32_t now) {
  uint32_t total = 0;
  for (uint8_t i = 0; i < config::ADC_SAMPLES; ++i)
    total += analogReadMilliVolts(config::GAS_PIN);
  state.adcMv = float(total) / config::ADC_SAMPLES;
  state.gasOk = isfinite(state.adcMv) && state.adcMv >= config::MIN_ADC_MV &&
      state.adcMv <= config::MAX_ADC_MV;
  float rawVolts = state.gasOk
      ? state.adcMv * config::AOUT_MULTIPLIER / 1000.0f : NAN;
  if (!state.gasOk) state.gasVolts = NAN;
  else if (!isfinite(state.gasVolts)) state.gasVolts = rawVolts;
  else state.gasVolts += config::FILTER_ALPHA * (rawVolts - state.gasVolts);

  const aeris::CaptureResult result = state.capture.add(rawVolts, now, state.referenceVolts);
  if (result == aeris::CaptureResult::Saved) {
    const bool saved = state.nvsOk &&
        preferences.putFloat("reference", state.referenceVolts) == sizeof(float);
    notifyUser(saved ? "REFERENCE SAVED" : "REF SET: RAM ONLY");
  } else if (result == aeris::CaptureResult::Unstable) notifyUser("REF UNSTABLE: RETRY");
  else if (result == aeris::CaptureResult::BadSignal) notifyUser("REF FAILED: SIGNAL");
}

void readClimate() {
  state.temperatureC = state.humidityPct = state.pressureHpa = NAN;
  if (!state.bmeOk || !i2cPresent(state.bmeAddress)) { state.bmeOk = false; return; }
  const float t = bme.readTemperature();
  const float h = bme.readHumidity();
  const float p = bme.readPressure() / 100.0f;
  state.bmeOk = isfinite(t) && isfinite(h) && isfinite(p) &&
      t >= -40 && t <= 85 && h >= 0 && h <= 100 && p >= 300 && p <= 1100;
  if (state.bmeOk) { state.temperatureC = t; state.humidityPct = h; state.pressureHpa = p; }
}

void printTelemetry() {
  // nan means missing/unavailable; never replace missing readings with zero.
  Serial.printf("ms=%llu,page=%u,adc_mV=%.1f,aout_V=%.3f,ref_V=%.3f,relative_pct=%.1f,"
                "temp_C=%.2f,rh_pct=%.1f,pressure_hPa=%.1f,warm_s=%llu,bme=%u,oled=%u\n",
      (unsigned long long)state.uptimeMs, unsigned(state.page + 1), state.adcMv,
      state.gasVolts, state.referenceVolts, state.responsePct(), state.temperatureC,
      state.humidityPct, state.pressureHpa,
      (unsigned long long)((state.warmRemainingMs + 999) / 1000),
      unsigned(state.bmeOk), unsigned(state.oledOk));
}

void handleSerial(uint32_t now) {
  // One character per loop keeps a noisy serial sender from starving sampling.
  if (!Serial.available()) return;
  switch (Serial.read()) {
    case 'n': handleButton(aeris::ButtonEvent::Tap, now); break;
    case 'r': requestReference(now); break;
    case 'd': state.dim = !state.dim; setContrast(); break;
    case 'x': {
      state.referenceVolts = NAN; state.capture.active = false;
      const bool persisted = state.nvsOk && preferences.putFloat("reference", NAN) == sizeof(float);
      notifyUser(persisted ? "REFERENCE CLEARED" : "REF CLEAR: RAM ONLY");
      break;
    }
    case 'h': Serial.println("n=next | r=30s reference | d=dim | x=clear reference | h=help"); break;
    default: break;
  }
}

void setup() {
  Serial.begin(config::SERIAL_BAUD);
  pinMode(config::BUTTON_PIN, INPUT_PULLUP);
  pinMode(config::GAS_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(config::GAS_PIN, ADC_11db);
  Wire.begin(config::SDA_PIN, config::SCL_PIN, config::I2C_HZ);
  Wire.setTimeOut(25);
  state.nvsOk = preferences.begin("aeris-v1", false);
  const bool conditioned = config::MQ135_ALREADY_CONDITIONED ||
      (state.nvsOk && preferences.getBool("conditioned", false));
  state.firstHeat = !conditioned;
  warmDurationMs = conditioned ? config::RESTART_SETTLE_MS : config::FIRST_HEAT_MS;
  state.warmRemainingMs = warmDurationMs;
  const float stored = state.nvsOk ? preferences.getFloat("reference", NAN) : NAN;
  state.referenceVolts = aeris::validReference(stored) ? stored : NAN;
  Serial.println("AERIS 1.0 | ESP32-WROOM-32 | relative gas response only");
  Serial.println("5V MQ135 heater + 15k/10k divider required; GPIO34 must never receive 5V.");
  Serial.println("Hold 2s on GAS for reference; tap=next; hold on other pages=dim. Serial h=help.");
  probeDevices();
  const uint32_t now = millis();
  button.begin(digitalRead(config::BUTTON_PIN) == LOW, now);
  runClock.begin(now);
  gasAt = now - config::GAS_INTERVAL_MS;
  climateAt = now - config::CLIMATE_INTERVAL_MS;
  frameAt = now - config::FRAME_INTERVAL_MS;
  probeAt = historyAt = now;
}

void loop() {
  const uint32_t now = millis();
  runClock.update(now); state.uptimeMs = runClock.ms();
  state.warmRemainingMs = state.uptimeMs < warmDurationMs
      ? warmDurationMs - state.uptimeMs : 0;
  if (!state.warmRemainingMs && !heatFinished) {
    heatFinished = true;
    if (state.firstHeat) {
      const bool saved = state.nvsOk && preferences.putBool("conditioned", true) == sizeof(bool);
      Serial.println(saved ? "Initial heater timer completed and saved." : "Initial heater timer completed; flag not saved.");
    }
    notifyUser(aeris::validReference(state.referenceVolts) ? "READY: REF LOADED" : "READY: SET REFERENCE");
  }
  handleButton(button.update(digitalRead(config::BUTTON_PIN) == LOW, now), now);
  handleSerial(now);
  if (aeris::elapsed(now, gasAt, config::GAS_INTERVAL_MS)) { gasAt = now; readGas(now); }
  if (aeris::elapsed(now, climateAt, config::CLIMATE_INTERVAL_MS)) {
    climateAt = now; readClimate(); printTelemetry();
  }
  if (aeris::elapsed(now, historyAt, config::HISTORY_INTERVAL_MS)) {
    historyAt = now; state.history.push(state.gasOk ? state.gasVolts : NAN);
  }
  if (aeris::elapsed(now, probeAt, config::PROBE_INTERVAL_MS)) { probeAt = now; probeDevices(); }
  if (toastVisible && aeris::elapsed(now, toastAt, 2400)) toastVisible = false;
  if (aeris::elapsed(now, frameAt, config::FRAME_INTERVAL_MS)) {
    frameAt = now;
    if (state.oledOk) {
      oled.clearDisplay();
      aeris::ui::render(oled, state, now, toastVisible ? toastText : nullptr);
      oled.display();
    }
  }
  delay(1);  // Yield to the ESP32 scheduler; all multi-second tasks use timers.
}
