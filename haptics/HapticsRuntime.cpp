#include "HapticsRuntime.h"
#include <Wire.h>

namespace {

// ===== 보드 의존 파라미터는 HAL에서 가져옴 =====
using HAL::Pin;
using HapticsPolicy::ErmDir;

// 런타임 플래그/상태
static bool s_enabled = true;

// I2C mutex (DRV2605 접근 보호용)
static SemaphoreHandle_t s_i2cMutex = nullptr;

// DRV2605
static Adafruit_DRV2605 s_drv;
static bool s_lraReady = false;

// ERM 구동 상태(간단 플래그)
static volatile bool s_ermRunningL = false;
static volatile bool s_ermRunningR = false;

// 하프틱 명령
enum class HCmdType : uint8_t { ERM, LRA };

struct HCmdERM {
  ErmDir   dir;
  uint16_t duty;
  uint32_t ms;
};
struct HCmdLRA {
  uint8_t  effect;
  uint32_t ms;
};
struct HCmd {
  HCmdType type;
  union { HCmdERM erm; HCmdLRA lra; } u;
};

// 큐/태스크 핸들
static QueueHandle_t  s_qHaptics  = nullptr;
static TaskHandle_t   s_taskHapt  = nullptr;

// PWM 파라미터(필요 시 HAL로 승격 가능)
static constexpr int ERM_PWM_FREQ     = 1000; // Hz
static constexpr int ERM_PWM_RES_BITS = 10;   // 0..1023

// 유틸
inline void ermWrite(bool left, uint16_t duty) {
  // Arduino-ESP32 v3.x 전용 API: ledcAttach(pin,freq,res), ledcWrite(pin, duty)
  ledcWrite(left ? Pin::MOTOR_LEFT : Pin::MOTOR_RIGHT, duty);
  if (left) s_ermRunningL = (duty > 0); else s_ermRunningR = (duty > 0);
}
inline void ermStopAll() {
  ledcWrite(Pin::MOTOR_LEFT,  0);
  ledcWrite(Pin::MOTOR_RIGHT, 0);
  s_ermRunningL = s_ermRunningR = false;
}

void taskHaptics(void*) {
  for(;;) {
    HCmd cmd;
    if (xQueueReceive(s_qHaptics, &cmd, portMAX_DELAY) != pdTRUE) continue;

    if (!s_enabled) {
      // disable 중이면 명령 drop
      continue;
    }

    if (cmd.type == HCmdType::ERM) {
      uint32_t dur = cmd.u.erm.ms;
      uint16_t dt  = cmd.u.erm.duty;
      ErmDir   dir = cmd.u.erm.dir;

      // 정책 적용(퓨즈/하한/클램프)
      const uint32_t now = millis();
      if (!HapticsPolicy::fuseCheckAndAdjust(dir, now, dur, dt)) {
        // soft mute 또는 hard cooldown → 실행 거부
        continue;
      }

      // 실행
      const uint32_t t0 = millis();
      switch (dir) {
        case ErmDir::LEFT:  ermWrite(true, dt);  ermWrite(false, 0);  break;
        case ErmDir::RIGHT: ermWrite(true, 0);   ermWrite(false, dt); break;
        case ErmDir::BOTH:  ermWrite(true, dt);  ermWrite(false, dt); break;
      }
      while (millis() - t0 < dur) { vTaskDelay(pdMS_TO_TICKS(5)); }
      if (dir == ErmDir::LEFT)       ermWrite(true, 0);
      else if (dir == ErmDir::RIGHT) ermWrite(false, 0);
      else                           ermStopAll();

      HapticsPolicy::fuseAccumulate(dir, dur, dt);
    }
    else if (cmd.type == HCmdType::LRA) {
      if (!s_lraReady) continue;

      const uint32_t t0  = millis();
      const uint32_t dur = HapticsPolicy::clampMs(cmd.u.lra.ms);
      const uint8_t  eff = cmd.u.lra.effect;

      constexpr uint32_t LRA_RETRIGGER_MS = 300; // 재트리거 템포

      while (millis() - t0 < dur) {
        HapticsRuntime::i2cLock();
        s_drv.setWaveform(0, eff);
        s_drv.setWaveform(1, 0);
        s_drv.go();
        HapticsRuntime::i2cUnlock();

        const uint32_t remain = dur - (millis() - t0);
        const uint32_t waitMs = (remain < LRA_RETRIGGER_MS) ? remain : LRA_RETRIGGER_MS;
        vTaskDelay(pdMS_TO_TICKS(waitMs));
      }
    }
  }
}

} // namespace (anonymous)

// ====== 공개 구현 ======
namespace HapticsRuntime {

void begin() {
  // 정책 초기화
  HapticsPolicy::init();

  // HAL 쪽 핀 준비(핀모드/LOW 정리)
  HAL::preparePwmPins();

  // 내부 I2C mutex
  s_i2cMutex = xSemaphoreCreateMutex();

  // ERM PWM attach (보드 핀은 HAL::Pin 사용)
  ledcAttach(Pin::MOTOR_LEFT,  ERM_PWM_FREQ, ERM_PWM_RES_BITS);
  ledcAttach(Pin::MOTOR_RIGHT, ERM_PWM_FREQ, ERM_PWM_RES_BITS);
  ermStopAll();

  // DRV2605L init (HAL의 Wire 공유)
  i2cLock();
  bool ok = s_drv.begin(&HAL::i2c()); // 오버로드: TwoWire* 사용 가능(Adafruit lib 최신)
  if (!ok) {
    // 일부 버전은 begin(twi) 오버로드가 없어 기본 begin() 필요
    ok = s_drv.begin();
  }
  if (ok) {
    s_drv.useLRA();
    s_drv.selectLibrary(6);
    s_drv.setMode(DRV2605_MODE_INTTRIG);
  }
  i2cUnlock();
  s_lraReady = ok;

  // 큐/태스크
  s_qHaptics = xQueueCreate(16, sizeof(HCmd));
  xTaskCreatePinnedToCore(taskHaptics, "Haptics", 4096, nullptr, 3, &s_taskHapt, 1);
}

void setEnabled(bool en) { s_enabled = en; }
bool isEnabled() { return s_enabled; }

SemaphoreHandle_t i2cMutex() { return s_i2cMutex; }
void i2cLock()   { if (s_i2cMutex) xSemaphoreTake(s_i2cMutex, portMAX_DELAY); }
void i2cUnlock() { if (s_i2cMutex) xSemaphoreGive(s_i2cMutex); }

bool ErmPlay(ErmDir dir, uint32_t ms, uint16_t duty) {
  if (!s_enabled) return false;
  HCmd c; c.type = HCmdType::ERM;
  c.u.erm.dir  = dir;
  c.u.erm.ms   = HapticsPolicy::clampMs(ms);
  // UX 하한(정책도 최종 보정하지만, 큐 진입 전 1차 보정)
  const uint16_t minDuty = HapticsPolicy::pctToDuty(HapticsPolicy::getErmMinPct());
  if (duty < minDuty) duty = minDuty;
  c.u.erm.duty = HapticsPolicy::clampDuty(duty);
  return (xQueueSend(s_qHaptics, &c, 0) == pdTRUE);
}

bool LraPlay(uint32_t ms, uint8_t effect) {
  if (!s_enabled) return false;
  if (!s_lraReady) {
    // LRA 불가 시 ERM 폴백
    const uint16_t duty = HapticsPolicy::effectToDuty(effect);
    return ErmPlay(ErmDir::BOTH, HapticsPolicy::clampMs(ms), duty);
  }
  HCmd c; c.type = HCmdType::LRA;
  c.u.lra.ms = HapticsPolicy::clampMs(ms);
  c.u.lra.effect = effect;
  return (xQueueSend(s_qHaptics, &c, 0) == pdTRUE);
}

void stopAll() {
  // 큐 flush는 하지 않음(필요시 xQueueReset 고려)
  ermStopAll();
}

bool getErmFuse(float &loadL, float &loadR, long &cooldownLeftMs) {
  const uint32_t now = millis();
  HapticsPolicy::fuseGetLoads(loadL, loadR, now, cooldownLeftMs);
  return true;
}

bool lraReady() { return s_lraReady; }

} // namespace HapticsRuntime
