#pragma once
//
// SliderPipeline — 아날로그 슬라이더 → (모드별) 휠/줌 + 촉각 피드백 + RG 인디케이터
//

#include <stdint.h>

namespace Slider {

void init();
void tick(uint32_t now_ms);

// 외부에서 모드를 읽고 싶을 때(보통은 Gesture가 내부적으로 관리)
enum class Mode : uint8_t { Wheel=0, Zoom=1 };
Mode getMode();
void setMode(Mode m); // Gesture가 토글 시 호출

} // namespace Slider
