#include "RuntimeInput.h"
#include "TouchPadPipeline.h"
#include "SliderPipeline.h"
#include "GamepadPipeline.h"
#include "GestureEngine.h"

#include "../core/ConfigStore.h"
#include "../core/Log.h"

using RuntimeInput::FactoryAction;

namespace {
  // 제스처가 올린 팩토리 이벤트를 버퍼링
  volatile FactoryAction g_pendingAction = FactoryAction::None;
}

static void onFactoryEvent(Gesture::FactoryEvent ev){
  switch(ev){
    case Gesture::FactoryEvent::Smoke: g_pendingAction = FactoryAction::Smoke; break;
    case Gesture::FactoryEvent::Full:  g_pendingAction = FactoryAction::Full;  break;
    default: break;
  }
}

namespace RuntimeInput {

void init() {
  TouchPad::init();
  Slider::init();
  Gamepad::init();
  Gesture::init(onFactoryEvent); // 팩토리 이벤트 콜백 등록
}

void tick(uint32_t now_ms) {
  // 순서: 제스처(모드/토글/진입) → 터치패드/슬라이더 → 게임패드
  Gesture::tick(now_ms);
  TouchPad::tick(now_ms);
  Slider::tick(now_ms);
  Gamepad::tick(now_ms);
}

FactoryAction pollFactoryAction(){
  FactoryAction out = g_pendingAction;
  g_pendingAction = FactoryAction::None;
  return out;
}

} // namespace RuntimeInput
