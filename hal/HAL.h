#pragma once
//
// HAL.h — 보드 의존부(핀·LED·버튼·슬라이더·스틱·터치·I2C 초기화)
// - 하드웨어 접근은 전부 여기로 모읍니다.
// - 상위 레이어는 HAL API만 사용하세요.
//

#include <Arduino.h>
#include <Wire.h>

// 전방 선언(터치 좌표용)
namespace HAL {
struct TouchPt { int x; int y; };
}

// ========= 보드 핀맵 =========
// (보드가 바뀌면 이 섹션만 수정)
namespace HAL::Pin {
  // I2C
  inline constexpr int SDA = 4;
  inline constexpr int SCL = 5;

  // LEDs
  inline constexpr int LED_R = 16;
  inline constexpr int LED_G = 17;
  inline constexpr int LED_B = 18;

  // Touch (디지털 탭 감지)
  inline constexpr int TOUCH_DIGITAL = 2; // ACTIVE_HIGH

  // Slider (ADC)
  inline constexpr int SLIDER = 15;

  // ERM Motors (PWM 채널 핀)
  inline constexpr int MOTOR_LEFT  = 6;
  inline constexpr int MOTOR_RIGHT = 7;

  // Joysticks
  inline constexpr int JS_L_X  = 9;    // ADC1
  inline constexpr int JS_L_Y  = 10;   // ADC1
  inline constexpr int JS_L_SW = 11;   // digital (pull-up)
  inline constexpr int JS_R_X  = 12;   // ADC2
  inline constexpr int JS_R_Y  = 13;   // ADC2
  inline constexpr int JS_R_SW = 14;   // digital (pull-up)

  // ABXY buttons (pull-up)
  inline constexpr int BTN_A = 39;
  inline constexpr int BTN_B = 40;
  inline constexpr int BTN_X = 41;
  inline constexpr int BTN_Y = 42;
}

// ========= 상수/설정 =========
namespace HAL::Const {
  // Touch active level
  inline constexpr bool TOUCH_ACTIVE_HIGH = true;

  // CapaTouch 좌표 유효 범위 (MPR121)
  inline constexpr int CX_MIN = 1, CX_MAX = 9;
  inline constexpr int CY_MIN = 1, CY_MAX = 13;
}

// ========= 버튼/입력 열거 =========
namespace HAL {
enum class Button : uint8_t {
  A, B, X, Y, L3, R3, TouchDigital
};

struct SticksRaw {
  int lx, ly, rx, ry; // 0..4095 (12-bit)
};
} // namespace HAL

// ========= API =========
namespace HAL {

// 초기화 (핀모드, I2C, ADC 해상도/어텐 설정, 터치칩 시작 등)
void init();

// 주기 호출 필요 없음. (타이머/IRQ 사용 안 함) — 필요 시 tick() 추가 가능
// void tick(uint32_t now_ms);

// LED
void ledR(bool on);
void ledG(bool on);
void ledB(bool on);
void ledAllOff();

// 버튼/터치
bool pressed(Button b);          // 풀업 기준: LOW = pressed
bool readTouchDigital();         // 단순 디지털 탭 입력

// 아날로그 입력
int  readSliderRaw();            // 0..4095
void readSticksRaw(SticksRaw& s);// 각 축 0..4095

// CapaTouch (MPR121) 좌표 — 좌표가 유효하면 true
bool touchGetCoord(TouchPt& out);

// I2C 접근자 (공유 Wire)
TwoWire& i2c();

// PWM 채널 준비(ERM 등에서 사용) — 실제 attach는 상위(하프틱 런타임)에서 수행
void preparePwmPins();

// ADC 공통 설정(필요 시 재호출 가능)
void configureAdc();

} // namespace HAL
