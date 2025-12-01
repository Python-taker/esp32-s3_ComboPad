#pragma once
//
// HapticsPolicy.h — 하프틱 실행 정책(ERM/LRA 공통 정책층)
//  - 듀티/시간 clamp
//  - ERM 에너지 윈도우 기반 fuse(soft/hard) + cooldown
//  - LRA→ERM 폴백 duty 매핑
//  - 정책 파라미터(ERM 최소 듀티 %) 설정
//

#include <Arduino.h>
#include <stdint.h>

namespace HapticsPolicy {

// 런타임에서 공유하는 ERM 방향
enum class ErmDir : uint8_t { LEFT = 0, RIGHT = 1, BOTH = 2 };

// ----- 설정 파라미터 -----
void init();
void setErmMinPct(uint8_t pct);      // 0..100
uint8_t getErmMinPct();

// ----- 기본 유틸 -----
uint16_t pctToDuty(uint8_t pct);     // 0..100 → 0..1023
uint32_t clampMs(uint32_t ms);       // 상한 클램프
uint16_t clampDuty(uint16_t d);      // 0..1023

// ----- ERM fuse(10s 윈도우 에너지 예산) -----
// 정책 상수는 .cpp에 정의되어 있음(soft/hard/cooldown/캡)
bool fuseCheckAndAdjust(ErmDir dir, uint32_t nowMs, uint32_t &ms, uint16_t &duty);
// 실행 뒤 누적(부하 적산)
void fuseAccumulate(ErmDir dir, uint32_t ms, uint16_t duty);
// 상태 조회(로그/CLI)
void fuseGetLoads(float &loadL, float &loadR, uint32_t nowMs, long &cooldownLeftMs);

// ----- LRA→ERM 폴백 매핑 -----
uint16_t effectToDuty(uint8_t effect);   // DRV2605 효과코드 → ERM duty

} // namespace HapticsPolicy
