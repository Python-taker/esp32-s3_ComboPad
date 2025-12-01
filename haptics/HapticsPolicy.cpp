#include "HapticsPolicy.h"
#include <math.h>

namespace HapticsPolicy {

// ===== 정책 내부 상태 =====
static uint8_t s_ermMinPct = 50; // UX 하한 %

// ----- ERM fuse 상수(윈도우/에너지/쿨다운/제한) -----
static constexpr uint32_t ERM_WINDOW_MS     = 10000;  // 10s
static constexpr float    ERM_BUDGET_SOFT   = 4.0f;   // soft 시작
static constexpr float    ERM_BUDGET_HARD   = 8.0f;   // hard 컷
static constexpr uint32_t ERM_COOLDOWN_MS   = 3000;   // hard 후 쿨다운
static constexpr uint16_t ERM_CAP1_DUTY     = 1023;   // <4
static constexpr uint16_t ERM_CAP2_DUTY     = 800;    // 4~6
static constexpr uint16_t ERM_CAP3_DUTY     = 600;    // 6~8
static constexpr uint32_t ERM_SOFT_MAX_MS   = 800;    // soft 시 duration 상한
static constexpr uint16_t ERM_MIN_DUTY_UX   = 512;    // UX 하한(duty)

struct Fuse {
  float    loadL = 0.0f;
  float    loadR = 0.0f;
  uint32_t lastUpdateMs = 0;
  uint32_t cooldownUntil = 0;
};
static Fuse s_fuse;

// ====== 공개 구현 ======
void init() {
  s_ermMinPct = 50;
  s_fuse = Fuse{};
}

void setErmMinPct(uint8_t pct) {
  if (pct > 100) pct = 100;
  s_ermMinPct = pct;
}
uint8_t getErmMinPct() { return s_ermMinPct; }

uint16_t pctToDuty(uint8_t pct) {
  if (pct > 100) pct = 100;
  uint32_t d = (uint32_t)pct * 1023u / 100u;
  return (d > 1023u) ? 1023u : static_cast<uint16_t>(d);
}
uint32_t clampMs(uint32_t ms) {
  constexpr uint32_t HAPTICS_MAX_MS = 5000;
  return (ms > HAPTICS_MAX_MS) ? HAPTICS_MAX_MS : ms;
}
uint16_t clampDuty(uint16_t d) {
  return (d > 1023u) ? 1023u : d;
}

// 내부: 지수 감쇠(슬라이딩 윈도우 근사)
static void fuseDecay(Fuse &f, uint32_t nowMs) {
  if (f.lastUpdateMs == 0) { f.lastUpdateMs = nowMs; return; }
  uint32_t dt = nowMs - f.lastUpdateMs;
  f.lastUpdateMs = nowMs;
  if (dt == 0) return;
  float k = static_cast<float>(dt) / static_cast<float>(ERM_WINDOW_MS);
  if (k > 1.0f) k = 1.0f;
  auto decayOne = [&](float &L){
    if (L <= 0.0f) { L = 0.0f; return; }
    L -= L * k; if (L < 0.0f) L = 0.0f;
  };
  decayOne(f.loadL);
  decayOne(f.loadR);
}
static uint16_t capFromLoad(float load) {
  if (load < ERM_BUDGET_SOFT) return ERM_CAP1_DUTY;
  if (load < 6.0f)            return ERM_CAP2_DUTY;
  if (load < ERM_BUDGET_HARD) return ERM_CAP3_DUTY;
  return 0; // hard cut
}

bool fuseCheckAndAdjust(ErmDir dir, uint32_t nowMs, uint32_t &ms, uint16_t &duty) {
  fuseDecay(s_fuse, nowMs);

  if (nowMs < s_fuse.cooldownUntil) {
    // 하드 컷 쿨다운: 실행 거부
    return false;
  }
  // 참조 부하 선택
  float ref = 0.0f;
  switch (dir) {
    case ErmDir::LEFT:  ref = s_fuse.loadL; break;
    case ErmDir::RIGHT: ref = s_fuse.loadR; break;
    case ErmDir::BOTH:  ref = (s_fuse.loadL > s_fuse.loadR) ? s_fuse.loadL : s_fuse.loadR; break;
  }
  uint16_t cap = capFromLoad(ref);
  if (cap == 0) {
    // 하드 컷 진입
    s_fuse.cooldownUntil = nowMs + ERM_COOLDOWN_MS;
    return false;
  }

  // soft 영역: duty/시간 제한
  if (ref >= ERM_BUDGET_SOFT) {
    if (cap < ERM_MIN_DUTY_UX) {
      // UX 유지 못하는 경우 mute 처리(거부)
      return false;
    }
    if (duty > cap) duty = cap;
    if (ms > ERM_SOFT_MAX_MS) ms = ERM_SOFT_MAX_MS;
  }
  // UX 하한 강제
  uint16_t minDuty = pctToDuty(s_ermMinPct);
  if (duty < minDuty) duty = minDuty;

  // 최종 clamp
  duty = clampDuty(duty);
  ms   = clampMs(ms);
  return true;
}

void fuseAccumulate(ErmDir dir, uint32_t ms, uint16_t duty) {
  float dn = static_cast<float>(duty) / 1023.0f;
  float e  = (dn * dn) * (static_cast<float>(ms) / 1000.0f); // 대략적인 에너지
  switch (dir) {
    case ErmDir::LEFT:  s_fuse.loadL += e; break;
    case ErmDir::RIGHT: s_fuse.loadR += e; break;
    case ErmDir::BOTH:  s_fuse.loadL += e; s_fuse.loadR += e; break;
  }
  float hard = ERM_BUDGET_HARD * 1.5f;
  if (s_fuse.loadL > hard) s_fuse.loadL = hard;
  if (s_fuse.loadR > hard) s_fuse.loadR = hard;
}

void fuseGetLoads(float &loadL, float &loadR, uint32_t nowMs, long &cooldownLeftMs) {
  fuseDecay(s_fuse, nowMs);
  loadL = s_fuse.loadL;
  loadR = s_fuse.loadR;
  cooldownLeftMs = (nowMs < s_fuse.cooldownUntil) ?
                   static_cast<long>(s_fuse.cooldownUntil - nowMs) : 0L;
}

uint16_t effectToDuty(uint8_t effect) {
  uint16_t duty;
  switch (effect) {
    case 1:   duty = 920; break;  // strong click
    case 3:   duty = 650; break;  // light click
    case 10:  duty = 900; break;  // Y-high
    case 11:  duty = 760; break;  // Y-low
    case 47:  duty = 900; break;  // X-high
    case 51:  duty = 620; break;  // X-low
    default:  duty = 700; break;  // default medium
  }
  uint16_t minDuty = pctToDuty(s_ermMinPct);
  if (duty < minDuty) duty = minDuty;
  if (duty > 1023) duty = 1023;
  return duty;
}

} // namespace HapticsPolicy
