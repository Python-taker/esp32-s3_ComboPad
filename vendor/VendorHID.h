#pragma once
//
// VendorHID.h — Vendor HID 스펙/파서 + TinyUSB 콜백 + 시리얼 백엔드
//  - OUTPUT(ID=2): 하프틱 실행 명령을 VendorWorker 큐로 위임
//  - FEATURE(ID=3): 정책/전역 Enable 및 저장/로드(있으면) 처리
//  - INPUT(ID=1): 상태 요약(간단)
//
//  시리얼 백엔드:
//    "hid2 cmd flags pattern sL sR dur repeat gap prio"
//    "hid3 op key v0 v1"
//

#include <Arduino.h>
#include <stdint.h>

namespace VendorHID {

// Report IDs
inline constexpr uint8_t RID_INPUT  = 0x01;
inline constexpr uint8_t RID_OUTPUT = 0x02;
inline constexpr uint8_t RID_FEATURE= 0x03;

// 파서/콜백 초기화(워커도 내부에서 시작)
void begin();

// 메인 CLI에서 전달하는 시리얼 라인을 처리하려면 호출
// 처리했으면 true, 아니면 false(메인 CLI가 다른 명령으로 해석)
bool tryHandleSerialLine(const String& line);

} // namespace VendorHID
