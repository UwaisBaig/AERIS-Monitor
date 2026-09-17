#pragma once
#include "Config.h"
#include <math.h>
#include <stdint.h>

namespace aeris {
inline bool elapsed(uint32_t now, uint32_t then, uint32_t interval) {
  return uint32_t(now - then) >= interval;
}
inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
inline bool validReference(float volts) {
  return isfinite(volts) &&
      volts >= config::MIN_ADC_MV * config::AOUT_MULTIPLIER / 1000.0f &&
      volts <= config::MAX_ADC_MV * config::AOUT_MULTIPLIER / 1000.0f;
}
inline float relativeResponse(float volts, float reference) {
  return isfinite(volts) && validReference(reference)
      ? 100.0f * (volts / reference - 1.0f) : NAN;
}

enum class ButtonEvent { None, Tap, Hold };
class Button {
 public:
  void begin(bool pressed, uint32_t now) {
    raw_ = stable_ = pressed;
    changedAt_ = pressedAt_ = now;
    // A button held during startup must be released before it acts.
    held_ = pressed;
  }
  ButtonEvent update(bool pressed, uint32_t now) {
    if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
    if (stable_ != raw_ && elapsed(now, changedAt_, config::DEBOUNCE_MS)) {
      stable_ = raw_;
      if (stable_) { pressedAt_ = now; held_ = false; }
      else if (!held_) {
        held_ = true;
        // Release debounce must not turn a just-under-2s tap into a hold.
        return elapsed(changedAt_, pressedAt_, config::HOLD_MS)
            ? ButtonEvent::Hold : ButtonEvent::Tap;
      }
    }
    if (stable_ && raw_ && !held_ && elapsed(now, pressedAt_, config::HOLD_MS)) {
      held_ = true;
      return ButtonEvent::Hold;
    }
    return ButtonEvent::None;
  }
 private:
  bool raw_ = false, stable_ = false, held_ = false;
  uint32_t changedAt_ = 0, pressedAt_ = 0;
};

class Clock {
 public:
  void begin(uint32_t now) { last_ = now; total_ = 0; }
  void update(uint32_t now) { total_ += uint32_t(now - last_); last_ = now; }
  uint64_t ms() const { return total_; }
 private:
  uint32_t last_ = 0;
  uint64_t total_ = 0;
};

struct History {
  float values[config::HISTORY_POINTS] = {};
  uint8_t head = 0, count = 0;
  void push(float v) {
    values[head] = v;
    head = (head + 1) % config::HISTORY_POINTS;
    if (count < config::HISTORY_POINTS) ++count;
  }
  float at(uint8_t oldestIndex) const {
    if (oldestIndex >= count) return NAN;
    return values[(head + config::HISTORY_POINTS - count + oldestIndex)
        % config::HISTORY_POINTS];
  }
};

enum class CaptureResult { None, Saved, Unstable, BadSignal };
struct ReferenceCapture {
  bool active = false;
  uint32_t startedAt = 0;
  uint16_t samples = 0;
  double sum = 0;
  float low = 1000, high = -1000;
  void start(uint32_t now) {
    active = true; startedAt = now; samples = 0;
    sum = 0; low = 1000; high = -1000;
  }
  CaptureResult add(float rawVolts, uint32_t now, float &reference) {
    if (!active) return CaptureResult::None;
    if (!validReference(rawVolts)) {
      active = false; return CaptureResult::BadSignal;
    }
    sum += rawVolts; ++samples;
    if (rawVolts < low) low = rawVolts;
    if (rawVolts > high) high = rawVolts;
    if (!elapsed(now, startedAt, config::REFERENCE_DURATION_MS))
      return CaptureResult::None;
    active = false;
    const float mean = float(sum / samples);
    if (samples < config::REFERENCE_MIN_SAMPLES ||
        (high - low) / mean > config::REFERENCE_MAX_SPREAD)
      return CaptureResult::Unstable;
    reference = mean;
    return CaptureResult::Saved;
  }
};

struct State {
  uint8_t page = 0, oledAddress = 0, bmeAddress = 0;
  bool oledOk = false, bmeOk = false, gasOk = false;
  bool dim = false, firstHeat = true, nvsOk = false;
  uint64_t uptimeMs = 0, warmRemainingMs = config::FIRST_HEAT_MS;
  float adcMv = NAN, gasVolts = NAN, referenceVolts = NAN;
  float temperatureC = NAN, humidityPct = NAN, pressureHpa = NAN;
  float responsePct() const {
    return gasOk && warmRemainingMs == 0
        ? relativeResponse(gasVolts, referenceVolts) : NAN;
  }
  History history;
  ReferenceCapture capture;
};
}  // namespace aeris
