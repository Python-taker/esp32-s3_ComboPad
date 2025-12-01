#include "IMU.h"
#include "../hal/HAL.h"
#include "../haptics/HapticsRuntime.h"
#include "../haptics/HapticsPolicy.h"
#include "../core/Log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

using HAL::i2c;

namespace {

// -------- LSM6DS3TR-C 레지스터 --------
constexpr uint8_t I2C_ADDR      = 0x6A;
constexpr uint8_t REG_WHO_AM_I  = 0x0F;
constexpr uint8_t REG_CTRL1_XL  = 0x10;
constexpr uint8_t REG_CTRL2_G   = 0x11;
constexpr uint8_t REG_CTRL3_C   = 0x12;
constexpr uint8_t REG_OUTX_L_XL = 0x28;

// 스케일: DATASHEET (±2g, 16-bit)
constexpr float   G_PER_LSB     = 0.000061f; // ≈ 2g/32768

// 상태
TaskHandle_t  s_taskRead     = nullptr;
TaskHandle_t  s_taskReact    = nullptr;
SemaphoreHandle_t s_i2cMtx   = nullptr;

volatile float s_ax = 0.f, s_ay = 0.f, s_az = 0.f;
volatile bool  s_ready = false;

IMU::ReactParams s_params{};
volatile bool s_reactEnabled = true;

// ---- 안전한 I2C 락 래퍼 ----
struct I2CScope {
  I2CScope()  { xSemaphoreTake(s_i2cMtx, portMAX_DELAY); }
  ~I2CScope() { xSemaphoreGive(s_i2cMtx); }
};

inline bool wr1(uint8_t reg, uint8_t val) {
  I2CScope lk;
  auto& W = i2c();
  W.beginTransmission(I2C_ADDR);
  W.write(reg);
  W.write(val);
  return (W.endTransmission() == 0);
}

inline bool rdN(uint8_t reg, uint8_t* buf, size_t n) {
  I2CScope lk;
  auto& W = i2c();
  W.beginTransmission(I2C_ADDR);
  W.write(reg);
  if (W.endTransmission(false) != 0) return false;
  if (W.requestFrom(I2C_ADDR, (uint8_t)n) != (int)n) return false;
  for (size_t i=0;i<n;i++) buf[i] = W.read();
  return true;
}

inline int16_t i16(uint8_t lo, uint8_t hi) {
  return (int16_t)((hi<<8) | lo);
}

// ---- 초기화 ----
bool imuInit() {
  uint8_t who = 0;
  if (!rdN(REG_WHO_AM_I, &who, 1)) {
    LOGW("IMU", "WHO_AM_I read failed");
    return false;
  }
  LOGI("IMU", "WHO_AM_I=0x%02X (exp 0x69)", who);

  // BDU=1, IF_INC=1
  wr1(REG_CTRL3_C, 0x44);
  // ACC=104Hz, ±2g
  wr1(REG_CTRL1_XL, 0x40);
  // GYR=104Hz, 245 dps (사용 안 함)
  wr1(REG_CTRL2_G, 0x40);

  return (who == 0x69);
}

// ---- 리딩 태스크 ----
void taskRead(void*) {
  uint8_t abuf[6];
  for(;;){
    if (rdN(REG_OUTX_L_XL, abuf, 6)) {
      const int16_t x = i16(abuf[0], abuf[1]);
      const int16_t y = i16(abuf[2], abuf[3]);
      const int16_t z = i16(abuf[4], abuf[5]);
      s_ax = x * G_PER_LSB;
      s_ay = y * G_PER_LSB;
      s_az = z * G_PER_LSB;
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz 샘플링이면 충분
  }
}

// ---- 하프틱 리액트 태스크 ----
// 상태 머신 + 히스테리시스
void taskReact(void*) {
  uint8_t  stateX=0, stateY=0, stateXY=0; // 0=off,1=low,2=high
  uint32_t lastPlayX=0, lastPlayY=0, lastPlayXY=0;
  uint8_t  cntX=0, cntY=0, cntXY=0;
  uint32_t lraInhibitUntil = 0;

  for(;;){
    if (!s_reactEnabled || !HapticsRuntime::isEnabled()) {
      vTaskDelay(pdMS_TO_TICKS(25));
      continue;
    }

    // 스냅샷
    const float ax = fabsf(s_ax), ay = fabsf(s_ay);
    const uint32_t now = millis();

    // X
    if (stateX == 0)              { if (ax >= s_params.th1_on)  stateX = 1; }
    else if (stateX == 1)         { if (ax >= s_params.th2_on)  stateX = 2; else if (ax < s_params.th1_off) stateX = 0; }
    else /*stateX==2*/            { if (ax < s_params.th2_off)  stateX = 1; }

    // Y
    if (stateY == 0)              { if (ay >= s_params.th1_on)  stateY = 1; }
    else if (stateY == 1)         { if (ay >= s_params.th2_on)  stateY = 2; else if (ay < s_params.th1_off) stateY = 0; }
    else /*stateY==2*/            { if (ay < s_params.th2_off)  stateY = 1; }

    // XY 동시
    const bool xy_low_on   = (ax>=s_params.th1_on && ay>=s_params.th1_on);
    const bool xy_high_on  = (ax>=s_params.th2_on && ay>=s_params.th2_on);
    const bool xy_low_off  = (ax<s_params.th1_off || ay<s_params.th1_off);
    const bool xy_high_off = (ax<s_params.th2_off || ay<s_params.th2_off);
    if (stateXY == 0)             { if (xy_low_on)  stateXY = 1; }
    else if (stateXY == 1)        { if (xy_high_on) stateXY = 2; else if (xy_low_off)  stateXY = 0; }
    else /*stateXY==2*/           { if (xy_high_off)stateXY = 1; }

    // 카운터 리셋
    if (ax < s_params.th1_off) cntX = 0;
    if (ay < s_params.th1_off) cntY = 0;
    if (ax < s_params.th1_off && ay < s_params.th1_off) cntXY = 0;

    const bool lraAllowed = (now >= lraInhibitUntil) && HapticsRuntime::isEnabled();

    // XY 우선
    if (stateXY) {
      if (cntXY >= s_params.maxBursts) {
        HapticsRuntime::ErmPlay(HapticsPolicy::ErmDir::BOTH, s_params.ermBothMs, s_params.ermEscDuty);
        lraInhibitUntil = now + s_params.ermBothMs;
        cntXY = s_params.maxBursts;
      } else if (lraAllowed && (now - lastPlayXY >= s_params.repeatMs)) {
        const uint8_t eff = (stateXY == 2) ? 1 : 3; // 강/약
        if (HapticsRuntime::LraPlay(60, eff)) {
          lastPlayXY = now;
          cntXY++;
        }
      }
    } else {
      // X
      if (stateX) {
        if (cntX >= s_params.maxBursts) {
          HapticsRuntime::ErmPlay(HapticsPolicy::ErmDir::LEFT, s_params.ermSingleMs, s_params.ermEscDuty);
          lraInhibitUntil = now + s_params.ermSingleMs;
          cntX = s_params.maxBursts;
        } else if (lraAllowed && (now - lastPlayX >= s_params.repeatMs)) {
          const uint8_t eff = (stateX == 2) ? 47 : 51;
          if (HapticsRuntime::LraPlay(60, eff)) {
            lastPlayX = now;
            cntX++;
          }
        }
      }
      // Y
      if (stateY) {
        if (cntY >= s_params.maxBursts) {
          HapticsRuntime::ErmPlay(HapticsPolicy::ErmDir::RIGHT, s_params.ermSingleMs, s_params.ermEscDuty);
          lraInhibitUntil = now + s_params.ermSingleMs;
          cntY = s_params.maxBursts;
        } else if (lraAllowed && (now - lastPlayY >= s_params.repeatMs)) {
          const uint8_t eff = (stateY == 2) ? 10 : 11;
          if (HapticsRuntime::LraPlay(60, eff)) {
            lastPlayY = now;
            cntY++;
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

} // namespace

// ====== Public API ======
namespace IMU {

void begin() {
  if (!s_i2cMtx) s_i2cMtx = xSemaphoreCreateMutex();

  s_ready = imuInit();
  if (s_ready) {
    if (!s_taskRead)  xTaskCreatePinnedToCore(taskRead,  "IMURead",  4096, nullptr, 1, &s_taskRead, 0);
    if (!s_taskReact) xTaskCreatePinnedToCore(taskReact, "IMUReact", 4096, nullptr, 1, &s_taskReact, 1);
    LOGI("IMU", "LSM6DS3TR-C ready");
  } else {
    LOGW("IMU", "Not detected at 0x6A. Continue without IMU.");
  }
}

bool isReady() {
  return s_ready;
}

void getAccel(float& ax, float& ay, float& az) {
  ax = s_ax; ay = s_ay; az = s_az;
}

float accelMagnitude() {
  const float ax = s_ax, ay = s_ay, az = s_az;
  return sqrtf(ax*ax + ay*ay + az*az);
}

void enableHapticReact(bool en) {
  s_reactEnabled = en;
}

bool isHapticReactEnabled() {
  return s_reactEnabled;
}

void setReactParams(const ReactParams& p) {
  s_params = p;
}

ReactParams getReactParams() {
  return s_params;
}

} // namespace IMU
