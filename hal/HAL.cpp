#include "HAL.h"
#include <mpr121.h>  // CapaTouch (MPR121)

using namespace HAL;

// 내부: 전역 객체(라이브러리 특성상 전역 인스턴스 사용)
static MPR121 CapaTouch;

// ========== 내부 헬퍼 ==========
static inline void cfgInputPullup(int pin)  { pinMode(pin, INPUT_PULLUP); }
static inline void cfgOutput(int pin)       { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }

namespace {
  // 아날로그 해상도/어텐 설정값
  constexpr uint8_t kAdcBits = 12; // 0..4095
}

// ========== 초기화 ==========
void HAL::init() {
  // LEDs
  cfgOutput(Pin::LED_R);
  cfgOutput(Pin::LED_G);
  cfgOutput(Pin::LED_B);
  ledAllOff();

  // Touch digital
  pinMode(Pin::TOUCH_DIGITAL, Const::TOUCH_ACTIVE_HIGH ? INPUT : INPUT);

  // Buttons (pull-up)
  cfgInputPullup(Pin::BTN_A);
  cfgInputPullup(Pin::BTN_B);
  cfgInputPullup(Pin::BTN_X);
  cfgInputPullup(Pin::BTN_Y);
  cfgInputPullup(Pin::JS_L_SW);
  cfgInputPullup(Pin::JS_R_SW);

  // I2C (공유)
  Wire.begin(Pin::SDA, Pin::SCL, 100000);

  // CapaTouch 시작 (실패해도 계속 진행; 상위에서 graceful degrade)
  CapaTouch.begin();

  // ADC
  configureAdc();

  // PWM 핀 준비(실제 attach는 하프틱 런타임이 담당)
  preparePwmPins();
}

// ========== LED ==========
void HAL::ledR(bool on){ digitalWrite(Pin::LED_R, on ? HIGH : LOW); }
void HAL::ledG(bool on){ digitalWrite(Pin::LED_G, on ? HIGH : LOW); }
void HAL::ledB(bool on){ digitalWrite(Pin::LED_B, on ? HIGH : LOW); }
void HAL::ledAllOff(){ ledR(false); ledG(false); ledB(false); }

// ========== 버튼/터치 ==========
bool HAL::pressed(Button b) {
  switch(b){
    case Button::A:    return digitalRead(Pin::BTN_A)   == LOW;
    case Button::B:    return digitalRead(Pin::BTN_B)   == LOW;
    case Button::X:    return digitalRead(Pin::BTN_X)   == LOW;
    case Button::Y:    return digitalRead(Pin::BTN_Y)   == LOW;
    case Button::L3:   return digitalRead(Pin::JS_L_SW) == LOW;
    case Button::R3:   return digitalRead(Pin::JS_R_SW) == LOW;
    case Button::TouchDigital:
      return readTouchDigital();
  }
  return false;
}

bool HAL::readTouchDigital() {
  int v = digitalRead(Pin::TOUCH_DIGITAL);
  return Const::TOUCH_ACTIVE_HIGH ? (v == HIGH) : (v == LOW);
}

// ========== 아날로그 ==========
int HAL::readSliderRaw() {
  return analogRead(Pin::SLIDER);
}

void HAL::readSticksRaw(SticksRaw& s) {
  s.lx = analogRead(Pin::JS_L_X);
  s.ly = analogRead(Pin::JS_L_Y);
  s.rx = analogRead(Pin::JS_R_X);
  s.ry = analogRead(Pin::JS_R_Y);
}

// ========== CapaTouch ==========
bool HAL::touchGetCoord(TouchPt& out) {
  // CapaTouch.getX()/getY()는 유효 좌표가 아닐 때 보통 0 또는 범위 밖 값을 리턴
  int x = CapaTouch.getX();
  int y = CapaTouch.getY();

  if (x >= Const::CX_MIN && x <= Const::CX_MAX &&
      y >= Const::CY_MIN && y <= Const::CY_MAX) {
    // 기존 구현처럼 X축 반전(터치패드 좌우 방향 맞춤)
    x = (Const::CX_MAX + 1) - x;
    out.x = x;
    out.y = y;
    return true;
  }
  return false;
}

// ========== I2C ==========
TwoWire& HAL::i2c() {
  return Wire;
}

// ========== PWM/ADC 준비 ==========
void HAL::preparePwmPins() {
  // 하프틱 런타임에서 ledcAttachChannel()을 호출할 예정이라 여기서는 출력만 LOW로 정리
  pinMode(Pin::MOTOR_LEFT,  OUTPUT);  digitalWrite(Pin::MOTOR_LEFT,  LOW);
  pinMode(Pin::MOTOR_RIGHT, OUTPUT);  digitalWrite(Pin::MOTOR_RIGHT, LOW);
}

void HAL::configureAdc() {
  analogReadResolution(kAdcBits);
  // 보드 아날로그 레이아웃에 맞춰 어텐 설정(11dB = 넓은 입력 범위)
  analogSetPinAttenuation(Pin::JS_L_X, ADC_11db);
  analogSetPinAttenuation(Pin::JS_L_Y, ADC_11db);
  analogSetPinAttenuation(Pin::JS_R_X, ADC_11db);
  analogSetPinAttenuation(Pin::JS_R_Y, ADC_11db);
  analogSetPinAttenuation(Pin::SLIDER, ADC_11db);
}
