#include "../AerisMonitor/Core.h"
#include "../AerisMonitor/Ui.h"
#include <assert.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace aeris;
int checks = 0;
template <typename F> void test(const char *name, F run) {
  run(); ++checks; std::cout << "PASS " << name << '\n';
}

// Exact one-bit, 128x64 canvas for the firmware's shared renderer.
struct Canvas {
  uint8_t pixels[64][128] = {};
  int outOfBounds = 0;
  void drawPixel(int x, int y, int c) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) { ++outOfBounds; return; }
    pixels[y][x] = c ? 255 : 0;
  }
  void fillRect(int x, int y, int w, int h, int c) {
    for (int j = y; j < y + h; ++j)
      for (int i = x; i < x + w; ++i) drawPixel(i, j, c);
  }
  void drawRect(int x, int y, int w, int h, int c) {
    drawLine(x,y,x+w-1,y,c); drawLine(x,y+h-1,x+w-1,y+h-1,c);
    drawLine(x,y,x,y+h-1,c); drawLine(x+w-1,y,x+w-1,y+h-1,c);
  }
  void drawLine(int x0, int y0, int x1, int y1, int c) {
    int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
      drawPixel(x0,y0,c); if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  void save(const std::string &path) {
    std::ofstream f(path, std::ios::binary);
    f << "P5\n128 64\n255\n";
    f.write(reinterpret_cast<char *>(pixels), sizeof(pixels));
    assert(f.good());
  }
};

int main(int argc, char **argv) {
  const std::string output = argc > 1 ? argv[1] : ".";
  test("divider maps 5V to 2V and back", [] {
    assert(fabsf(5.0f / config::AOUT_MULTIPLIER - 2.0f) < 0.00001f);
    assert(fabsf(2000 * config::AOUT_MULTIPLIER / 1000 - 5.0f) < 0.00001f);
  });
  test("relative response is defined by voltage ratio", [] {
    assert(fabsf(relativeResponse(2.2f, 2.0f) - 10.0f) < 0.001f);
    assert(fabsf(relativeResponse(1.8f, 2.0f) + 10.0f) < 0.001f);
  });
  test("invalid and zero baselines never produce a number", [] {
    assert(isnan(relativeResponse(2, 0))); assert(isnan(relativeResponse(2, NAN)));
    assert(isnan(relativeResponse(NAN, 2))); assert(!validReference(6));
  });
  test("a bounced tap produces one navigation event", [] {
    Button b; b.begin(false,0);
    assert(b.update(true,10) == ButtonEvent::None);
    assert(b.update(false,20) == ButtonEvent::None);
    assert(b.update(true,25) == ButtonEvent::None);
    assert(b.update(true,60) == ButtonEvent::None);
    assert(b.update(false,100) == ButtonEvent::None);
    assert(b.update(true,105) == ButtonEvent::None);
    assert(b.update(false,110) == ButtonEvent::None);
    assert(b.update(false,145) == ButtonEvent::Tap);
    assert(b.update(false,500) == ButtonEvent::None);
  });
  test("hold fires once and release does not navigate", [] {
    Button b; b.begin(false,0); b.update(true,10); b.update(true,45);
    assert(b.update(true,2044) == ButtonEvent::None);
    assert(b.update(true,2045) == ButtonEvent::Hold);
    assert(b.update(true,6000) == ButtonEvent::None);
    b.update(false,6010); assert(b.update(false,6045) == ButtonEvent::None);
  });
  test("release debounce does not manufacture a hold", [] {
    Button b; b.begin(false,0); b.update(true,10); b.update(true,45);
    assert(b.update(false,2044) == ButtonEvent::None);
    assert(b.update(false,2079) == ButtonEvent::Tap);
  });
  test("button held at boot is ignored until released", [] {
    Button b; b.begin(true,0);
    assert(b.update(true,3000) == ButtonEvent::None);
    b.update(false,4000); assert(b.update(false,4035) == ButtonEvent::None);
    b.update(true,5000); b.update(true,5035); b.update(false,5100);
    assert(b.update(false,5135) == ButtonEvent::Tap);
  });
  test("button debounce survives millis rollover", [] {
    Button b; b.begin(false,0xFFFFFFE0U); b.update(true,0xFFFFFFF0U);
    assert(b.update(true,0x13U) == ButtonEvent::None);
    b.update(false,0x40U); assert(b.update(false,0x63U) == ButtonEvent::Tap);
  });
  test("scheduler and uptime survive millis rollover", [] {
    assert(elapsed(20,0xFFFFFFF0U,36)); assert(!elapsed(20,0xFFFFFFF0U,37));
    Clock c; c.begin(0xFFFFFFF0U); c.update(20); assert(c.ms() == 36);
    c.update(100); assert(c.ms() == 116);
  });
  test("history retains the newest 120 samples in time order", [] {
    History h; for (int i=0;i<150;++i) h.push(float(i));
    assert(h.count == 120); assert(h.at(0) == 30); assert(h.at(119) == 149);
    assert(isnan(h.at(120)));
    h.push(NAN); assert(isnan(h.at(119)));
  });
  test("stable 30-second reference commits the mean", [] {
    ReferenceCapture c; float ref = NAN; c.start(0);
    for (int i=1;i<120;++i) assert(c.add(2.0f,i*250,ref) == CaptureResult::None);
    assert(c.add(2.0f,30000,ref) == CaptureResult::Saved);
    assert(ref == 2.0f && !c.active);
  });
  test("drifting reference preserves previous baseline", [] {
    ReferenceCapture c; float ref = 1.5f; c.start(0);
    for (int i=1;i<120;++i) c.add(i < 60 ? 1.8f : 2.2f,i*250,ref);
    assert(c.add(2.2f,30000,ref) == CaptureResult::Unstable);
    assert(ref == 1.5f);
  });
  test("reference aborts on invalid signal and preserves baseline", [] {
    ReferenceCapture c; float ref = 1.5f; c.start(0);
    assert(c.add(NAN,250,ref) == CaptureResult::BadSignal); assert(ref == 1.5f);
  });
  test("reference rejects too few samples", [] {
    ReferenceCapture c; float ref = 1.5f; c.start(0);
    assert(c.add(2,30000,ref) == CaptureResult::Unstable); assert(ref == 1.5f);
  });
  test("reference capture survives millis rollover", [] {
    ReferenceCapture c; float ref = NAN; uint32_t start = 0xFFFFF000U; c.start(start);
    for (int i=1;i<120;++i) c.add(2,start+i*250,ref);
    assert(c.add(2,start+30000,ref) == CaptureResult::Saved);
  });
  test("relative reading stays unavailable during warm-up or a fault", [] {
    State s; s.gasVolts = 2.2f; s.referenceVolts = 2; s.gasOk = true;
    assert(isnan(s.responsePct())); s.warmRemainingMs=0;
    assert(fabsf(s.responsePct()-10) < 0.001f); s.gasOk=false;
    assert(isnan(s.responsePct()));
  });
  State s; s.oledOk=s.bmeOk=s.gasOk=s.nvsOk=true;
  s.oledAddress=0x3C; s.bmeAddress=0x76; s.warmRemainingMs=0;
  s.uptimeMs=3780000; s.gasVolts=1.82f; s.adcMv=728;
  s.referenceVolts=1.625f; s.temperatureC=26.4f; s.humidityPct=48;
  s.pressureHpa=1008.2f;
  for (int i=0;i<120;++i) s.history.push(1.63f+0.04f*sinf(i*0.13f)+0.0014f*i);
  for (uint8_t page=0; page<config::PAGE_COUNT; ++page) {
    s.page=page; Canvas c; ui::render(c,s,1875); assert(c.outOfBounds==0);
    c.save(output+"/screen-"+std::to_string(page+1)+".pgm");
  }
  test("five main pages render within 128x64", [] {});
  test("all transient states and extremes stay within 128x64", [&] {
    std::vector<State> states;
    states.push_back(State{});
    State w=s; w.page=0; w.warmRemainingMs=config::FIRST_HEAT_MS; states.push_back(w);
    w.capture.start(0); states.push_back(w);
    w.capture.active=false; w.warmRemainingMs=0; w.referenceVolts=NAN; states.push_back(w);
    w.gasOk=false; states.push_back(w);
    w=s; w.page=1; w.temperatureC=-40; w.humidityPct=100; states.push_back(w);
    w.bmeOk=false; states.push_back(w);
    w=s; w.page=2; w.history=History{}; states.push_back(w);
    w.history.push(NAN); states.push_back(w); w.history.push(2); states.push_back(w);
    w=s; w.page=0; w.gasVolts=5.25f; w.referenceVolts=0.375f; states.push_back(w);
    w=s; w.page=3; states.push_back(w);
    w=s; w.page=4; w.uptimeMs=100000000000ULL; states.push_back(w);
    for (const auto &state:states) {
      Canvas c; ui::render(c,state,5000); assert(c.outOfBounds==0);
      Canvas toast; ui::render(toast,state,5000,"REF UNSTABLE: RETRY");
      assert(toast.outOfBounds==0);
    }
  });
  // Supplementary preview assets for the guide.
  s.page=0; s.warmRemainingMs=config::FIRST_HEAT_MS;
  Canvas warm; ui::render(warm,s,1875); warm.save(output+"/screen-warmup.pgm");
  s.warmRemainingMs=0; s.capture.start(0);
  Canvas capture; ui::render(capture,s,12500); capture.save(output+"/screen-reference.pgm");
  std::cout << checks << " behavioral/render checks passed; five display frames exported.\n";
}
