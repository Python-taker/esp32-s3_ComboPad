#include "TouchPadPipeline.h"
#include "../hal/HAL.h"
#include "../usb/USBDevices.h"
#include "../core/ConfigStore.h"
#include "../core/Log.h"

namespace {

struct State {
  bool touching = false;
  int  sx = 0, sy = 0;     // start
  int  px = 0, py = 0;     // prev / last
  uint32_t t_touch = 0;
  uint32_t t_release = 0;

  // tap
  uint32_t lastTap = 0;
  bool firstTapPending = false;
} S;

constexpr uint8_t  DEADZONE = 1;
constexpr uint16_t TAP_MAX  = 180;
constexpr uint16_t DBL_GAP  = 300;

inline int iround(float f){ return (int)((f>0)?(f+0.5f):(f-0.5f)); }

} // anon

namespace TouchPad {

void init(){
  S = State{};
}

void tick(uint32_t now_ms){
  HAL::TouchPt p;
  const bool ok = HAL::touchGetCoord(p);

  float gain = ConfigStore::get().cursor_gain;

  if (ok && !S.touching){
    S.touching = true;
    S.sx = S.px = p.x;
    S.sy = S.py = p.y;
    S.t_touch  = now_ms;
    return;
  }
  if (ok && S.touching){
    const int dx_raw = p.x - S.px;
    const int dy_raw = S.py - p.y;        // Y↑ = 화면↑

    const int dx = iround(dx_raw * gain);
    const int dy = iround(dy_raw * gain);

    if (abs(dx) + abs(dy) >= DEADZONE){
      USBDevices::mouseMove(dx, dy, 0);
      S.px = p.x; S.py = p.y;
    }
    return;
  }
  if (!ok && S.touching){
    S.touching = false;
    S.t_release = now_ms;
    const uint32_t dur = S.t_release - S.t_touch;

    if (dur <= TAP_MAX){
      if (S.firstTapPending && (S.t_release - S.lastTap) <= DBL_GAP) {
        USBDevices::mouseClickLeft();
        S.firstTapPending = false;
      } else {
        S.firstTapPending = true;
        S.lastTap = S.t_release;
      }
    }
  }

  if (S.firstTapPending && (now_ms - S.lastTap) > DBL_GAP){
    S.firstTapPending = false;
  }
}

} // namespace TouchPad
