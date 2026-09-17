#pragma once
#include "Core.h"
#include "Font.h"
#include <stdio.h>
#include <string.h>

// Canvas only needs drawPixel, fillRect, drawRect and drawLine. Firmware and
// desktop previews use this very same renderer. No simulated sensor values
// are compiled into the firmware.
namespace aeris { namespace ui {
template <typename Canvas>
void text(Canvas &d, int x, int y, const char *s, int scale = 1, int maxWidth = 128) {
  const int start = x;
  while (*s && x + 5 * scale <= start + maxWidth) {
    unsigned char ch = (unsigned char)*s++;
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 32 || ch > 90) ch = '?';
    for (int row = 0; row < 7; ++row)
      for (int col = 0; col < 5; ++col)
        if (GLYPHS[ch - 32][row] & (1 << (4 - col)))
          d.fillRect(x + col * scale, y + row * scale, scale, scale, 1);
    x += 6 * scale;
  }
}

template <typename Canvas>
void ring(Canvas &d, int cx, int cy, int radius, uint32_t now) {
  // Segmented radar outline and animated sweep. Decorative, no health scale.
  for (int a = 0; a < 360; a += 6) {
    if ((a / 30) % 2) continue;
    const float angle = a * 0.0174532925f;
    d.drawPixel(cx + int(lroundf(cosf(angle) * radius)),
                cy + int(lroundf(sinf(angle) * radius)), 1);
  }
  for (int a = 0; a < 360; a += 30) {
    const float angle = a * 0.0174532925f;
    d.drawPixel(cx + int(lroundf(cosf(angle) * (radius - 5))),
                cy + int(lroundf(sinf(angle) * (radius - 5))), 1);
  }
  const float phase = float(now % 3000) * 6.283185307f / 3000;
  d.drawLine(cx, cy, cx + int(lroundf(cosf(phase) * (radius - 2))),
             cy + int(lroundf(sinf(phase) * (radius - 2))), 1);
  d.fillRect(cx - 1, cy - 1, 3, 3, 1);
}

template <typename Canvas>
void frame(Canvas &d, const State &s, const char *title, const char *hint) {
  text(d, 2, 1, title);
  char n[8]; snprintf(n, sizeof(n), "%02u/%02u", unsigned(s.page + 1),
                      unsigned(config::PAGE_COUNT));
  text(d, 98, 1, n);
  d.drawLine(0, 11, 124, 11, 1); d.drawLine(124, 11, 127, 8, 1);
  d.drawLine(0, 54, 127, 54, 1);
  text(d, 2, 57, hint, 1, 99);
  for (int i = 0; i < config::PAGE_COUNT; ++i) {
    if (i == s.page) d.fillRect(99 + i * 6, 59, 4, 3, 1);
    else d.drawPixel(100 + i * 6, 60, 1);
  }
}

template <typename Canvas>
void gasPage(Canvas &d, const State &s, uint32_t now) {
  char a[32], b[24];
  frame(d, s, "AERIS / GAS", "TAP>N HOLD:REF");
  if (!s.gasOk) {
    text(d, 3, 18, "ADC OUT OF RANGE");
    text(d, 3, 31, "CHECK AOUT / 5V");
    snprintf(a, sizeof(a), "PIN: %.0fMV", s.adcMv);
    text(d, 3, 44, a);
    return;
  }
  ring(d, 110, 32, 14, now);
  const float relative = s.responsePct();
  const char *label = "SIGNAL / V";
  if (s.capture.active) label = "SET REFERENCE";
  else if (s.warmRemainingMs) label = s.firstHeat ? "FIRST HEAT" : "SETTLING";
  else if (isfinite(relative)) label = "REL. RESPONSE";
  text(d, 2, 15, label, 1, 92);
  if (isfinite(relative) && !s.capture.active) snprintf(a, sizeof(a), "%+.0f%%", relative);
  else snprintf(a, sizeof(a), "%.2fV", s.gasVolts);
  text(d, 2, 27, a, strlen(a) <= 7 ? 2 : 1, 92);
  if (s.capture.active) {
    unsigned secs = unsigned(uint32_t(now - s.capture.startedAt) / 1000);
    snprintf(b, sizeof(b), "REF %02u / 30S", secs > 30 ? 30 : secs);
  } else if (s.warmRemainingMs) {
    const unsigned minutes = unsigned((s.warmRemainingMs + 59999) / 60000);
    if (minutes >= 60) snprintf(b, sizeof(b), "%uH %02uM LEFT", minutes / 60, minutes % 60);
    else snprintf(b, sizeof(b), "%u MIN LEFT", minutes);
  } else if (!isfinite(relative)) snprintf(b, sizeof(b), "HOLD 2S: SET REF");
  else snprintf(b, sizeof(b), "AOUT %.2fV", s.gasVolts);
  text(d, 2, 45, b, 1, 96);
}

template <typename Canvas>
void climatePage(Canvas &d, const State &s) {
  frame(d, s, "ENVIRONMENT", "TAP>N HOLD:DIM");
  if (!s.bmeOk || !isfinite(s.temperatureC)) {
    text(d, 4, 20, "BME280 UNAVAILABLE");
    text(d, 4, 35, "CHECK 0X76 / 0X77");
    text(d, 4, 46, "RETRY EVERY 5S");
    return;
  }
  text(d, 2, 15, "TEMP C"); text(d, 75, 15, "RH %");
  d.drawLine(68, 15, 68, 40, 1);
  char t[16], h[16], p[24];
  snprintf(t, sizeof(t), "%.1f", s.temperatureC);
  snprintf(h, sizeof(h), "%.0f", s.humidityPct);
  text(d, 2, 26, t, 2, 64); text(d, 76, 26, h, 2, 50);
  snprintf(p, sizeof(p), "PRESS %6.1f HPA", s.pressureHpa);
  text(d, 2, 45, p);
}

template <typename Canvas>
void trendPage(Canvas &d, const State &s) {
  frame(d, s, "SIGNAL TREND", "20M / 10S STEP");
  if (!s.history.count) {
    text(d, 4, 22, "COLLECTING HISTORY");
    text(d, 4, 38, "1 POINT EVERY 10S");
    return;
  }
  float lo = 1000, hi = -1000;
  for (uint8_t i = 0; i < s.history.count; ++i) {
    const float v = s.history.at(i);
    if (!isfinite(v)) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (hi < lo) { text(d, 4, 29, "NO VALID GAS DATA"); return; }
  if (hi - lo < 0.1f) { const float mid = (hi + lo) / 2; lo = mid - 0.05f; hi = mid + 0.05f; }
  char limits[24]; snprintf(limits, sizeof(limits), "%.2F-%.2FV AUTO", lo, hi);
  text(d, 2, 15, limits);
  for (int x = 4; x <= 123; x += 8)
    for (int y = 26; y <= 50; y += 8) d.drawPixel(x, y, 1);
  bool previousValid = false;
  int previousX = 0, previousY = 0;
  for (uint8_t i = 0; i < s.history.count; ++i) {
    const float v = s.history.at(i);
    if (!isfinite(v)) { previousValid = false; continue; }
    const int x = 4 + config::HISTORY_POINTS - s.history.count + i;
    const int y = 50 - int(lroundf(clampf((v - lo) / (hi - lo), 0, 1) * 24));
    if (previousValid) d.drawLine(previousX, previousY, x, y, 1);
    else d.drawPixel(x, y, 1);
    previousX = x; previousY = y; previousValid = true;
  }
}

template <typename Canvas>
void systemPage(Canvas &d, const State &s) {
  frame(d, s, "SYSTEM / V1", "TAP>N HOLD:DIM");
  char a[32];
  snprintf(a, sizeof(a), "OLED %02X  BME %s", unsigned(s.oledAddress), s.bmeOk ? "OK" : "--");
  text(d, 2, 15, a);
  snprintf(a, sizeof(a), "ADC %.0FMV  G27 BTN", s.adcMv);
  text(d, 2, 25, a);
  const unsigned long long minutes = s.uptimeMs / 60000;
  snprintf(a, sizeof(a), "UP %lluH%02lluM NVS %s", minutes / 60, minutes % 60, s.nvsOk ? "OK" : "--");
  text(d, 2, 35, a, 1, 126);
  if (validReference(s.referenceVolts)) snprintf(a, sizeof(a), "REF %.2FV / %s", s.referenceVolts, s.dim ? "DIM" : "LIT");
  else snprintf(a, sizeof(a), "REF NONE / %s", s.dim ? "DIM" : "LIT");
  text(d, 2, 45, a);
}

template <typename Canvas>
void dancePage(Canvas &d, const State &s, uint32_t now) {
  frame(d, s, "DANCE MODE", "TAP>N HOLD:DIM");
  // Six procedural poses at 8 FPS. This uses almost no flash compared with
  // storing video frames and animates continuously without blocking sensors.
  const uint8_t pose = (now / 125) % 6;
  const int sway[6] = {-3, -1, 2, 3, 1, -2};
  const int lift[6] = {0, 1, 0, -1, 0, 1};
  const int cx = 63 + sway[pose], headY = 21 + lift[pose];
  d.fillRect(cx - 5, headY - 4, 8, 2, 1);       // hair
  d.fillRect(cx - 6, headY - 2, 11, 7, 1);      // head
  d.fillRect(cx - 2, headY + 5, 4, 3, 1);       // neck
  d.drawLine(cx - 7, headY + 9, cx + 7, headY + 9, 1);
  d.drawLine(cx - 7, headY + 9, cx - 5, headY + 24, 1);
  d.drawLine(cx + 7, headY + 9, cx + 5, headY + 24, 1);
  d.drawLine(cx - 5, headY + 24, cx + 5, headY + 24, 1);
  d.drawLine(cx, headY + 10, cx, headY + 23, 1);
  if (pose < 3) {
    d.drawLine(cx - 6, headY + 11, cx - 16, headY + 5, 1);
    d.drawLine(cx - 16, headY + 5, cx - 21, headY + 9, 1);
    d.drawLine(cx + 6, headY + 11, cx + 17, headY + 17, 1);
    d.drawLine(cx - 3, headY + 24, cx - 10, headY + 31, 1);
    d.drawLine(cx + 3, headY + 24, cx + 12, headY + 28, 1);
  } else {
    d.drawLine(cx - 6, headY + 11, cx - 17, headY + 17, 1);
    d.drawLine(cx + 6, headY + 11, cx + 16, headY + 5, 1);
    d.drawLine(cx + 16, headY + 5, cx + 21, headY + 9, 1);
    d.drawLine(cx - 3, headY + 24, cx - 12, headY + 28, 1);
    d.drawLine(cx + 3, headY + 24, cx + 10, headY + 31, 1);
  }
  d.fillRect(101, 22, 3, 4, 1);                 // microphone
  d.drawLine(102, 26, 102, 49, 1);
  d.drawLine(96, 50, 108, 50, 1);
  for (int i = 0; i < 5; ++i)
    d.drawPixel(8 + i * 9, 18 + ((pose + i) % 3) * 5, 1);
  d.drawLine(3, 51, 123, 51, 1);
}

template <typename Canvas>
void render(Canvas &d, const State &s, uint32_t now, const char *toast = nullptr) {
  switch (s.page) {
    case 0: gasPage(d, s, now); break;
    case 1: climatePage(d, s); break;
    case 2: trendPage(d, s); break;
    case 3: dancePage(d, s, now); break;
    default: systemPage(d, s); break;
  }
  if (toast) {
    d.fillRect(0, 22, 128, 23, 0);
    d.drawRect(0, 22, 128, 23, 1);
    text(d, 4, 30, toast, 1, 120);
  }
}
} }  // namespace aeris::ui
