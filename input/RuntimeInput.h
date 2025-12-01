#pragma once
//
// RuntimeInput — 오케스트라(입력 파이프라인/제스처 호출 순서만 담당)
// - init(): 서브 파이프라인 초기화
// - tick(now_ms): 매 프레임 호출
//

#include <stdint.h>

namespace RuntimeInput {

void init();
void tick(uint32_t now_ms);

// 팩토리 진입 이벤트를 메인으로 넘기고 싶다면(선택 API)
// 내부 GestureEngine이 판단한 결과를 즉시 소비하지 않고 외부로 전달.
enum class FactoryAction : uint8_t { None, Smoke, Full };
FactoryAction pollFactoryAction();   // 호출 시 1회성으로 action을 소모

} // namespace RuntimeInput
