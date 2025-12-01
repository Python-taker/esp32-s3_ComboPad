#pragma once
//
// GamepadPipeline — 스틱/버튼 → v3.x 리포트 (드리프트 보정/EMA/감마/원형클램프)
//

#include <stdint.h>

namespace Gamepad {

void init();
void tick(uint32_t now_ms);

// 수동 센터 재보정(제스처/버튼 조합에서 호출 가능)
void recalibrateCenters(bool blinkRed=true, uint16_t finishGreenMs=600);

} // namespace Gamepad
