#pragma once
//
// TouchPadPipeline — CapaTouch → 마우스 커서/탭
//

#include <stdint.h>

namespace TouchPad {

void init();
void tick(uint32_t now_ms);

} // namespace TouchPad
