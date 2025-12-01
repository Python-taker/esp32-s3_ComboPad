#include "GamepadPipeline.h"
#include "../hal/HAL.h"
#include "../usb/USBDevices.h"
#include "../haptics/HapticsRuntime.h"
#include "../core/ConfigStore.h"
#include "../core/Log.h"

namespace {

struct JoyCalib {
  int minX=300, centerX=2048, maxX=3800;
  int minY=300, centerY=2048, maxY=3800;
};

struct StickState {
  float xEma=0.f, yEma=0.f;
};

struct State {
  JoyCalib L, R;
  StickState Ls, Rs;
  uint32_t lastSend=0;
  int8_t lastX=0, lastY=0, lastRX=0, lastRY=0;
  uint32_t lastBtns=0;
  uint32_t l3r3PressMs=0;
} S;

constexpr uint32_t SEND_INTERVAL_MS = 5;

constexpr float JOY_DEADZONE  = 0.15f;
constexpr float JOY_GAMMA     = 1.4f;
constexpr float JOY_EMA_ALPHA = 0.25f;

// 축 반전(필요하면 Config로 이동 가능)
constexpr bool INVERT_LX = true;
constexpr bool INVERT_LY = false;
constexpr bool INVERT_RX = true;
constexpr bool INVERT_RY = false;

float normalizeAxis(int raw, int minV, int cenV, int maxV){
  if (raw < minV) raw = minV;
  if (raw > maxV) raw = maxV;
  float halfSpan = (float)(maxV - minV) * 0.5f;
  float mid      = (float)cenV;
  float val      = ((float)raw - mid) / halfSpan;   // -1..+1
  if (val >  1.f) val =  1.f;
  if (val < -1.f) val = -1.f;
  return val;
}

void shapeStick(float& x, float& y){
  float r = sqrtf(x*x + y*y);
  if (r < JOY_DEADZONE){ x=0.f; y=0.f; return; }
  float s = (r - JOY_DEADZONE) / (1.f - JOY_DEADZONE);
  if (s < 0.f) s = 0.f; if (s > 1.f) s = 1.f;
  float k = (r > 0.f) ? (s / r) : 0.f;
  x *= k; y *= k;
  auto curve = [](float v){
    float a = fabsf(v);
    float o = powf(a, JOY_GAMMA);
    return (v >= 0.f) ? o : -o;
  };
  x = curve(x); y = curve(y);
  const float rr = sqrtf(x*x + y*y);
  if (rr > 1.f){ x/=rr; y/=rr; }
}

inline void ema2(float a, float inX, float inY, float& sX, float& sY){
  sX = sX + a * (inX - sX);
  sY = sY + a * (inY - sY);
}

inline int8_t toI8(float v){
  if (v >  1.f) v =  1.f;
  if (v < -1.f) v = -1.f;
  return (int8_t)lroundf(v * 127.0f);
}

void sendIfChanged(int8_t X,int8_t Y,int8_t RX,int8_t RY,uint32_t btns,uint32_t now){
  const bool moved = (X!=S.lastX)||(Y!=S.lastY)||(RX!=S.lastRX)||(RY!=S.lastRY)||(btns!=S.lastBtns);
  if (!moved) return;
  if (now - S.lastSend < SEND_INTERVAL_MS) return;

  USBDevices::gamepadSend(X, Y, RX, RY, btns);

  S.lastX=X; S.lastY=Y; S.lastRX=RX; S.lastRY=RY; S.lastBtns=btns;
  S.lastSend = now;
}

} // anon

namespace Gamepad {

void init(){
  S = State{};
}

void recalibrateCenters(bool blinkRed, uint16_t finishGreenMs){
  LOGI("GP", "center calibration start");
  uint32_t lastToggle = millis();
  bool redOn = false;

  const int N = 100;
  const uint16_t GAP_MS = 20;
  const uint16_t BLINK_MS = 500;

  long sumLX=0, sumLY=0, sumRX=0, sumRY=0;
  for(int i=0;i<N;i++){
    HAL::SticksRaw r; HAL::readSticksRaw(r);
    sumLX += r.lx; sumLY += r.ly; sumRX += r.rx; sumRY += r.ry;

    if (blinkRed){
      if (millis() - lastToggle >= BLINK_MS){
        redOn = !redOn; HAL::ledR(redOn); lastToggle = millis();
      }
    }
    delay(GAP_MS);
  }

  S.L.centerX = (int)(sumLX / N);
  S.L.centerY = (int)(sumLY / N);
  S.R.centerX = (int)(sumRX / N);
  S.R.centerY = (int)(sumRY / N);

  HAL::ledR(false);
  if (finishGreenMs){
    HAL::ledG(true);
    HapticsRuntime::LraPlay(220, 10);
    delay(finishGreenMs);
    HAL::ledG(false);
  }

  USBDevices::gamepadSend(0,0,0,0,0); // 중립 리포트
  S.lastX=S.lastY=S.lastRX=S.lastRY=0; S.lastBtns=0; S.lastSend=millis();

  LOGI("GP", "centers L(%d,%d) R(%d,%d)", S.L.centerX, S.L.centerY, S.R.centerX, S.R.centerY);
}

void tick(uint32_t now_ms){
  // L3+R3 1초 → 재보정
  const bool l3 = HAL::pressed(HAL::Button::L3);
  const bool r3 = HAL::pressed(HAL::Button::R3);
  if (l3 && r3){
    if (!S.l3r3PressMs) S.l3r3PressMs = now_ms;
    if (now_ms - S.l3r3PressMs > 1000){
      recalibrateCenters(true, 600);
      S.l3r3PressMs = now_ms; // 반복 방지 최소 지연
    }
  } else {
    S.l3r3PressMs = 0;
  }

  HAL::SticksRaw raw; HAL::readSticksRaw(raw);

  float nlx = normalizeAxis(raw.lx, S.L.minX,S.L.centerX,S.L.maxX);
  float nly = normalizeAxis(raw.ly, S.L.minY,S.L.centerY,S.L.maxY);
  float nrx = normalizeAxis(raw.rx, S.R.minX,S.R.centerX,S.R.maxX);
  float nry = normalizeAxis(raw.ry, S.R.minY,S.R.centerY,S.R.maxY);

  // 원래 구현처럼 XY swap
  { float t=nlx; nlx=nly; nly=t; }
  { float t=nrx; nrx=nry; nry=t; }

  if (INVERT_LX) nlx = -nlx;
  if (INVERT_LY) nly = -nly;
  if (INVERT_RX) nrx = -nrx;
  if (INVERT_RY) nry = -nry;

  ema2(JOY_EMA_ALPHA, nlx, nly, S.Ls.xEma, S.Ls.yEma);
  ema2(JOY_EMA_ALPHA, nrx, nry, S.Rs.xEma, S.Rs.yEma);

  float fx=S.Ls.xEma, fy=S.Ls.yEma;
  float gx=S.Rs.xEma, gy=S.Rs.yEma;
  shapeStick(fx, fy);
  shapeStick(gx, gy);

  const int8_t X  = toI8(fx);
  const int8_t Y  = toI8(fy);
  const int8_t RX = toI8(gx);
  const int8_t RY = toI8(gy);

  uint32_t btns = 0;
  if (HAL::pressed(HAL::Button::A))  btns |= (1u<<0);
  if (HAL::pressed(HAL::Button::B))  btns |= (1u<<1);
  if (HAL::pressed(HAL::Button::X))  btns |= (1u<<2);
  if (HAL::pressed(HAL::Button::Y))  btns |= (1u<<3);
  if (HAL::pressed(HAL::Button::L3)) btns |= (1u<<8);
  if (HAL::pressed(HAL::Button::R3)) btns |= (1u<<9);

  sendIfChanged(X, Y, RX, RY, btns, now_ms);
}

} // namespace Gamepad
