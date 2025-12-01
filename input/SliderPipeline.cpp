#include "SliderPipeline.h"
#include "../hal/HAL.h"
#include "../usb/USBDevices.h"
#include "../haptics/HapticsRuntime.h"
#include "../core/ConfigStore.h"
#include "../core/Log.h"

namespace {

struct State {
  int last = -1;
  long acc = 0;           // 누적 Δ (모드별로 사용)
  int  stepTick = 0;      // 4 스텝마다 작은 LRA
  Slider::Mode mode = Slider::Mode::Wheel;

  // LED indicator 유지
  bool indicatorRed = false;
  uint32_t indicatorUntil = 0;
} S;

inline void showIndicator(bool red, uint32_t now_ms){
  S.indicatorRed = red;
  S.indicatorUntil = now_ms + 250;
  if (red){ HAL::ledR(true); HAL::ledG(false); }
  else    { HAL::ledR(false); HAL::ledG(true); }
}

inline void tickIndicator(uint32_t now_ms){
  if (S.indicatorUntil && now_ms > S.indicatorUntil){
    S.indicatorUntil = 0;
    HAL::ledR(false); HAL::ledG(false);
  }
}

} // anon

namespace Slider {

void init(){
  S = State{};
  S.mode = (ConfigStore::get().initial_mode == 1) ? Mode::Zoom : Mode::Wheel;
}

Slider::Mode getMode(){ return S.mode; }
void setMode(Slider::Mode m){ S.mode = m; }

void tick(uint32_t now_ms){
  tickIndicator(now_ms);

  // 터치 중엔 억제(손가락이 패드에 있을 때 슬라이더 동작하지 않음)
  if (HAL::pressed(HAL::Button::TouchDigital)) return;

  const int v = HAL::readSliderRaw();
  if (S.last < 0) { S.last = v; return; }

  const int dv = v - S.last;
  const int th = ConfigStore::get().slider_thresh;
  if (abs(dv) <= th) return;

  S.last = v;

  // 공통: 4스텝마다 촉각
  auto tickHaptics = [&](){
    S.stepTick++;
    if (S.stepTick >= 4){
      HapticsRuntime::LraPlay(80, 11); // 짧은 클릭감
      S.stepTick = 0;
    }
  };

  if (S.mode == Mode::Zoom){
    S.acc += dv;
    const int step = ConfigStore::get().zoom_step_dv;
    int localTicks = 0;

    while (S.acc >= step){ USBDevices::keyZoomIn();  S.acc -= step; localTicks++; }
    while (S.acc <= -step){USBDevices::keyZoomOut(); S.acc += step; localTicks++; }

    if (localTicks>0){
      tickHaptics();
      showIndicator(true, now_ms); // RED = Zoom
    } else if (S.indicatorRed) {
      S.indicatorUntil = now_ms + 250; // 유지 연장
    }
  } else {
    S.acc += dv;
    const int step = ConfigStore::get().wheel_step_dv;
    int localTicks = 0;

    while (S.acc >= step){ USBDevices::mouseWheel(+1); S.acc -= step; localTicks++; }
    while (S.acc <= -step){USBDevices::mouseWheel(-1); S.acc += step; localTicks++; }

    if (localTicks>0){
      tickHaptics();
      showIndicator(false, now_ms); // GREEN = Wheel
    } else if (!S.indicatorRed) {
      S.indicatorUntil = now_ms + 250;
    }
  }
}

} // namespace Slider
