#pragma once
//
// MainCLI.h — 메인 담당 CLI(설정/로그/팩토리 진입 등)
//  - Serial 입력 한 줄 파싱
//  - ConfigStore/Log/HapticsRuntime/FactoryTests 연동
//

#include <Arduino.h>
#include "ConfigStore.h"

namespace MainCLI {

// 오케스트라에서 현재 RAM 설정 포인터를 전달해 주세요.
// (cfg는 cfgLoad()로 채워져 있어야 하며, 이후 CLI가 값을 수정합니다)
void begin(ConfigStore::Config* cfg);

// loop()에서 주기적으로 호출
void poll();

// 간단 도움말 출력(선택)
void help();

} // namespace MainCLI
