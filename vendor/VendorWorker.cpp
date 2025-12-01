#include "VendorWorker.h"
#include "../haptics/HapticsRuntime.h"
#include "../haptics/HapticsPolicy.h"
#include "../hal/HAL.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace {

using VendorWorker::VendorCmd;
using CmdType = VendorWorker::CmdType;
using HapticsPolicy::ErmDir;

static QueueHandle_t s_q = nullptr;
static TaskHandle_t  s_task = nullptr;

static inline bool sideL(uint8_t f){ return f & VendorWorker::FLAG_SIDE_L; }
static inline bool sideR(uint8_t f){ return f & VendorWorker::FLAG_SIDE_R; }
static inline bool useLRA(uint8_t f){ return f & VendorWorker::FLAG_USE_LRA; }
static inline bool useERM(uint8_t f){ return f & VendorWorker::FLAG_USE_ERM; }
static inline bool exclusiveFlag(uint8_t f){ return f & VendorWorker::FLAG_EXCLUSIVE; }
static inline bool allowFallback(uint8_t f){ return f & VendorWorker::FLAG_ALLOW_FALLBACK; }

static void playOnce(const VendorCmd& v) {
  // LRA 우선
  if (useLRA(v.flags)) {
    if (HapticsRuntime::lraReady()) {
      HapticsRuntime::LraPlay(v.durMs, v.patternId);
      return;
    }
    // LRA 불가 → 폴백 허용 or ERM 플래그 있으면 ERM로 전환
    if (!(allowFallback(v.flags) || useERM(v.flags))) return;
  }

  // ERM 경로
  // strength 0..255 → duty 0..1023
  auto toDuty = [](uint8_t s)->uint16_t {
    return static_cast<uint16_t>((static_cast<uint32_t>(s) * 1023u) / 255u);
  };
  uint16_t dL = toDuty(v.strengthL);
  uint16_t dR = toDuty(v.strengthR);

  // UX 하한 보장(정책값)
  const uint16_t minDuty = HapticsPolicy::pctToDuty(HapticsPolicy::getErmMinPct());
  if (dL && dL < minDuty) dL = minDuty;
  if (dR && dR < minDuty) dR = minDuty;

  if (sideL(v.flags) && sideR(v.flags)) {
    // 양쪽 요청은 둘 중 큰 값으로 BOTH 구동
    HapticsRuntime::ErmPlay(ErmDir::BOTH, v.durMs, (dL > dR ? dL : dR));
  } else if (sideL(v.flags)) {
    HapticsRuntime::ErmPlay(ErmDir::LEFT, v.durMs, dL);
  } else if (sideR(v.flags)) {
    HapticsRuntime::ErmPlay(ErmDir::RIGHT, v.durMs, dR);
  }
}

static void taskWorker(void*) {
  for(;;) {
    VendorCmd v;
    if (xQueueReceive(s_q, &v, portMAX_DELAY) != pdTRUE) continue;

    // 우선순위 2 + exclusive → 선점
    if (v.priority >= 2 && exclusiveFlag(v.flags)) {
      HapticsRuntime::stopAll();
    }

    switch (v.cmd) {
      case CmdType::STOP_ALL:
        HapticsRuntime::stopAll();
        break;
      case CmdType::STOP_LEFT:
      case CmdType::STOP_RIGHT:
        // 현재 런타임 API에 개별 stop이 없어 STOP_ALL로 처리
        HapticsRuntime::stopAll();
        break;
      case CmdType::PLAY: {
        uint16_t gap = (v.gapMs < 50) ? 50 : v.gapMs;
        uint8_t  times = (v.repeat == 0) ? 1 : (uint8_t)(v.repeat + 1);
        if (times > 11) times = 11;

        for (uint8_t i = 0; i < times; ++i) {
          if (!HapticsRuntime::isEnabled()) break;
          playOnce(v);
          vTaskDelay(pdMS_TO_TICKS(gap));
        }
      } break;
      case CmdType::NOP:
      default:
        break;
    }
  }
}

} // namespace

namespace VendorWorker {

void begin() {
  if (!s_q)    s_q = xQueueCreate(10, sizeof(VendorCmd));
  if (!s_task) xTaskCreatePinnedToCore(taskWorker, "VendorWorker", 4096, nullptr, 2, &s_task, 1);
}

void stop() {
  // 태스크 종료/큐 해제는 생략(런타임 상시 구동)
}

bool enqueue(const VendorCmd& v) {
  if (!s_q) return false;
  return (xQueueSend(s_q, &v, 0) == pdTRUE);
}

} // namespace VendorWorker
