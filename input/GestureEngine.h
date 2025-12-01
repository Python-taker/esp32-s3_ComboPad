#pragma once
//
// GestureEngine — 글로벌 하프틱 토글 / 모드 토글(슬라이더) / 팩토리 진입 제스처
//  - 터치+ABXY 3초: 하프틱 ON/OFF
//  - 터치 홀드 2.5~6.2s: Wheel <-> Zoom 모드 토글(+ LRA 피드백)
//  - 부팅 10초창 내 L3+R3+A 2.5s: 팩토리(SMOKE/FULL) 프로파일 선택
//  - 런타임 “터치 3탭 + A&Y 유지”: 스모크 진입
//

#include <stdint.h>
#include <functional>

namespace Gesture {

enum class FactoryEvent : uint8_t { None, Smoke, Full };

// 외부(오케스트라)로 팩토리 이벤트를 전달할 콜백 등록
using FactoryCb = std::function<void(FactoryEvent)>;

void init(FactoryCb cb);
void tick(uint32_t now_ms);

} // namespace Gesture
