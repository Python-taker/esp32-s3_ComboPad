#pragma once
//
// ConfigStore.h — NVS 저장/로드 + 버전/마이그레이션 + 런타임 적용 훅
//
#include <Arduino.h>
#include <stdint.h>

namespace ConfigStore {

// 초기 모드(슬라이더)
enum : uint8_t { SL_WHEEL = 0, SL_ZOOM = 1 };

// 외부 런타임 반영 훅(오케스트라/런타임이 구현)
struct IRuntimeHooks {
  virtual ~IRuntimeHooks() = default;
  virtual void onCursorGain(float) = 0;
  virtual void onSliderParams(int thresh, int zstep, int wstep) = 0;
  virtual void onInitialMode(uint8_t mode) = 0;
  virtual void onHapticsEnable(bool en) = 0;
  virtual void onErmMinPct(uint8_t pct) = 0;
  virtual void onLogMask(uint32_t mask) = 0;
};

// 설정 구조체
struct Config {
  // 버전/예약
  uint16_t version = 0;

  // 커서/입력 파라미터
  float   cursor_gain   = 18.0f;
  int     slider_thresh = 25;
  int     zoom_step_dv  = 150;
  int     wheel_step_dv = 40;
  uint8_t initial_mode  = SL_WHEEL;

  // 하프틱
  bool    haptics_on    = true;
  uint8_t erm_min_pct   = 50;        // ERM 최소 듀티 %

  // 로그
  uint32_t log_mask     = 0;

  // 확장 여지
  uint8_t _reserved[7]  = {0};
};

// NVS 키 문자열(공개: CLI/툴과 공유할 수 있게)
extern const char* KEY_VER;
extern const char* KEY_GAIN;
extern const char* KEY_SLTH;
extern const char* KEY_ZSTEP;
extern const char* KEY_WSTEP;
extern const char* KEY_MODE;
extern const char* KEY_HAPT;     // on/off
extern const char* KEY_ERMPCT;   // erm_min_pct
extern const char* KEY_LOGMASK;  // log mask

// 전역 상태
void setHooks(IRuntimeHooks* hooks);

// 로드/세이브/리셋/표시
bool load(Config& out);          // NVS→RAM, 버전 체크/마이그레이션 내장
bool save(const Config& in);     // RAM→NVS
void reset(Config& out);         // defaults로 되돌림(세이브는 아님)
void show(const Config& cfg);    // Serial로 보기 좋게 출력

// 버전 마이그레이션(필요 시 확장)
bool migrateIfNeeded(Config& cfg, uint16_t storedVer);

// 런타임 반영(훅 호출)
void applyToRuntime(const Config& cfg);

} // namespace ConfigStore
