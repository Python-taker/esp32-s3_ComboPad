#pragma once
//
// FactoryTests — 테이블 드리븐 팩토리/스모크 테스트 (T00~T10)
//  - run(Profile::Smoke) : 핵심만 빠르게
//  - run(Profile::Full)  : 모든 항목 + 정량 체크
//
// 의존: HAL, HapticsRuntime, IMU, USBDevices, Log
//

#include <stdint.h>

namespace FactoryTests {

enum class Profile : uint8_t { Smoke, Full };

struct Result {
  bool     all_pass;
  int16_t  first_fail_index;  // -1이면 전부 통과
};

void init();  // 필요시 확장용(현재 NOP)
Result run(Profile profile, bool interactive = true);

// 개별 테스트를 외부에서 원할 때(옵션)
bool T00_I2CScan();
bool T01_LED();
bool T02_Buttons();
bool T03_JoyCenter();
bool T04_JoySweep();
bool T05_Slider();
bool T06_Touch(bool full);
bool T07_IMU();
bool T08_LRA();
bool T09_ERM();
bool T10_HID();

} // namespace FactoryTests
