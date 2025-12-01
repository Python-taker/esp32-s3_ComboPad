#pragma once
//
// Log.h — 비트마스크 로그 레벨 + 헬퍼
//
#include <Arduino.h>
#include <stdarg.h>
#include <stdint.h>

namespace Log {

enum Cat : uint32_t {
  USB      = 1u << 0,
  HAPTICS  = 1u << 1,
  IMU      = 1u << 2,
  FACTORY  = 1u << 3,
  INPUT    = 1u << 4,
  CONFIG   = 1u << 5,
  VENDOR   = 1u << 6,
  ALL      = 0xFFFFFFFFu
};

// 전역 마스크(런타임에 변경 가능)
extern volatile uint32_t g_mask;

// 설정/조회
void setMask(uint32_t m);
uint32_t getMask();

// 저수준 포맷 로그
void vprint(uint32_t cat, const char* fmt, va_list ap);
void print(uint32_t cat, const char* fmt, ...) __attribute__((format(printf,2,3)));

// 짧은 매크로
#define LOGC(cat, fmt, ...) do { if (Log::getMask() & (cat)) Log::print((cat), "[%s] " fmt, #cat, ##__VA_ARGS__); } while(0)

} // namespace Log
