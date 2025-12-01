#include "GestureEngine.h"
#include "SliderPipeline.h"

#include "../hal/HAL.h"
#include "../haptics/HapticsRuntime.h"
#include "../core/ConfigStore.h"
#include "../core/Log.h"

namespace {

Gesture::FactoryCb s_cb;

constexpr uint32_t HOLD_DEBOUNCE_MS = 10;
constexpr uint32_t HOLD_MIN_MS      = 2500;
constexpr uint32_t HOLD_MAX_MS      = 6200;

constexpr uint32_t HAPTIC_TOGGLE_HOLD_MS = 3000;

// 부팅 팩토리 윈도우
constexpr uint32_t FACTORY_WINDOW_MS = 10000;
constexpr uint32_t FACTORY_HOLD_MS   = 2500;
uint32_t s_bootStartMs = 0;

// 터치 홀드/디바운스
bool     touchStable = false, touchLast = false;
uint32_t touchLastChange = 0;
uint32_t touchPressStart = 0;

// 하프틱 토글 콤보
bool     hapticComboActive = false;
uint32_t hapticComboStart  = 0;
uint32_t lastHapticToggleMs = 0;

// 런타임 “터치 3탭 + A&Y”
uint8_t  tapCount=0;
uint32_t lastTapMs=0;
constexpr uint32_t TAP_WINDOW_MS=1800;

inline bool allABXYPressed(){
  return HAL::pressed(HAL::Button::A)
      && HAL::pressed(HAL::Button::B)
      && HAL::pressed(HAL::Button::X)
      && HAL::pressed(HAL::Button::Y);
}

void blinkMode(Slider::Mode m){
  for(int i=0;i<3;i++){
    if (m==Slider::Mode::Zoom){ HAL::ledR(true);  HAL::ledG(false); }
    else                      { HAL::ledR(false); HAL::ledG(true);  }
    delay(333);
    HAL::ledR(false); HAL::ledG(false);
    delay(333);
  }
}

} // anon

namespace Gesture {

void init(FactoryCb cb){
  s_cb = cb;
  s_bootStartMs = millis();
}

void tick(uint32_t now_ms){
  // ---- 부팅 구간 팩토리 진입 ----
  if (now_ms - s_bootStartMs < FACTORY_WINDOW_MS){
    static uint32_t holdStart=0;
    const bool l3 = HAL::pressed(HAL::Button::L3);
    const bool r3 = HAL::pressed(HAL::Button::R3);
    const bool a  = HAL::pressed(HAL::Button::A);
    if (l3 && r3 && a){
      if (!holdStart) holdStart = now_ms;
      if (now_ms - holdStart >= FACTORY_HOLD_MS){
        // 3초 간 선택창 — A=SMOKE, B=FULL
        LOGI("FACT", "boot-combo detected: select A=SMOKE, B=FULL within 3s");
        const uint32_t selT = millis();
        while(millis() - selT < 3000){
          if (HAL::pressed(HAL::Button::A)){ if(s_cb) s_cb(FactoryEvent::Smoke); break; }
          if (HAL::pressed(HAL::Button::B)){ if(s_cb) s_cb(FactoryEvent::Full);  break; }
          static uint32_t hb=0; static bool on=false;
          if(millis()-hb>500){ on=!on; HAL::ledB(on); hb=millis(); }
          delay(5);
        }
        if (s_cb) s_cb(FactoryEvent::Smoke); // 타임아웃 default
        // 부팅 창 종료
        s_bootStartMs = 0;
      }
    } else {
      holdStart = 0;
    }
  }

  // ---- 런타임 스모크 쇼트컷: “터치 3탭 + A&Y 유지” ----
  {
    const bool ayHeld = (HAL::pressed(HAL::Button::A) && HAL::pressed(HAL::Button::Y));
    static bool prevTouch=false;
    const bool touch = HAL::pressed(HAL::Button::TouchDigital);
    if (touch != prevTouch){
      prevTouch = touch;
      if (!touch){
        if (ayHeld){
          if (now_ms - lastTapMs > TAP_WINDOW_MS) tapCount=0;
          tapCount++;
          lastTapMs=now_ms;
          if (tapCount>=3){
            LOGI("FACT", "runtime shortcut: touch 3-tap + A&Y -> SMOKE");
            if (s_cb) s_cb(FactoryEvent::Smoke);
            tapCount=0;
          }
        } else {
          tapCount=0;
        }
      }
    }
  }

  // ---- 터치 디바운스/홀드(모드 토글 & 하프틱 글로벌 토글) ----
  {
    const bool reading = HAL::pressed(HAL::Button::TouchDigital);
    if (reading != touchLast){ touchLastChange = now_ms; touchLast = reading; }
    if ((now_ms - touchLastChange) > HOLD_DEBOUNCE_MS){
      if (touchStable != reading){
        touchStable = reading;

        if (touchStable){
          touchPressStart = now_ms;
          hapticComboActive = false;
          hapticComboStart  = 0;
        } else {
          // release
          const uint32_t held = now_ms - touchPressStart;
          if (now_ms - lastHapticToggleMs < 800) {
            // 최근 글로벌 토글 후 바운스 윈도 — 무시
          } else if (held >= HOLD_MIN_MS && held <= HOLD_MAX_MS){
            // 모드 토글
            Slider::Mode m = Slider::getMode();
            m = (m==Slider::Mode::Wheel) ? Slider::Mode::Zoom : Slider::Mode::Wheel;
            Slider::setMode(m);
            blinkMode(m);
            HapticsRuntime::LraPlay(120, 11);
            LOGI("MODE", "toggled to %s", (m==Slider::Mode::Zoom?"Zoom":"Wheel"));
          }
        }
      }
    }

    if (touchStable){
      if (allABXYPressed()){
        if (!hapticComboActive){
          hapticComboActive = true;
          hapticComboStart  = now_ms;
        } else if (now_ms - hapticComboStart >= HAPTIC_TOGGLE_HOLD_MS){
          const bool newEn = !HapticsRuntime::isEnabled();
          HapticsRuntime::setEnabled(newEn);
          auto cfg = ConfigStore::get();
          cfg.haptics_on = newEn;
          ConfigStore::apply(cfg);      // 런타임 반영
          lastHapticToggleMs = now_ms;
          hapticComboActive = false;
          LOGI("HAPTICS", "global %s", newEn?"ENABLED":"DISABLED");
        }
      } else {
        hapticComboActive = false;
        hapticComboStart  = 0;
      }
    }
  }
}

} // namespace Gesture
