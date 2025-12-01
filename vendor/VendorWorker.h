#pragma once
//
// VendorWorker.h — Vendor HID 워커(반복/선점/딜레이 처리)
//  - TinyUSB 콜백/시리얼 파서는 VendorHID에서 수행
//  - 실제 재생/반복/선점은 이 워커 태스크에서 비동기 처리
//

#include <Arduino.h>
#include <stdint.h>

namespace VendorWorker {

// ===== 스펙 플래그 (OUTPUT 레포트 flags 바이트) =====
// bit0 useLRA, bit1 useERM, bit2 sideL, bit3 sideR, bit4 exclusive, bit5 allowFallback
enum : uint8_t {
  FLAG_USE_LRA        = 0x01,
  FLAG_USE_ERM        = 0x02,
  FLAG_SIDE_L         = 0x04,
  FLAG_SIDE_R         = 0x08,
  FLAG_EXCLUSIVE      = 0x10,
  FLAG_ALLOW_FALLBACK = 0x20,
};

// ===== 명령 타입 =====
enum class CmdType : uint8_t {
  NOP       = 0,
  PLAY      = 1,
  STOP_ALL  = 2,
  STOP_LEFT = 3,  // 현재 런타임 API 제한으로 STOP_ALL로 대체됨
  STOP_RIGHT= 4,  // 현재 런타임 API 제한으로 STOP_ALL로 대체됨
};

// ===== 큐 아이템 =====
struct VendorCmd {
  CmdType  cmd;        // 0..4
  uint8_t  flags;      // 위 플래그 비트마스크
  uint8_t  patternId;  // LRA 패턴
  uint8_t  strengthL;  // 0..255
  uint8_t  strengthR;  // 0..255
  uint16_t durMs;      // 재생 시간
  uint8_t  repeat;     // 반복 횟수-1 (0이면 1회)
  uint16_t gapMs;      // 반복 간격
  uint8_t  priority;   // 0/1/2 (2 + exclusive 시 선점)
};

// 시작/중지
void begin();
void stop();

// 큐 투입
bool enqueue(const VendorCmd& v);

} // namespace VendorWorker
