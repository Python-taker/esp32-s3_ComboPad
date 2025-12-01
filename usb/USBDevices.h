#pragma once
//
// USBDevices.h — USB HID (Mouse/Keyboard/Gamepad) 얇은 래퍼
// - 상위(오케스트라)는 이 API만 사용
// - 내부 구현은 Arduino ESP32-S3 TinyUSB 백엔드(USBHID*)에 위임
//

#include <Arduino.h>

namespace USBDev {

// ---- 초기화/수명주기 ----
void begin(const char* product,
           const char* manufacturer,
           const char* serial);
bool ready();                    // USB.begin() 완료/에넘 OK 추정(간단 헬퍼)
void end();                      // 필요시 USB 종료(일반적으론 사용 안 함)

// ---- Mouse ----
void mouseMove(int x, int y, int wheel = 0); // 상대 이동 + 휠
void mouseClickLeft();
void mouseClickRight();
void mousePress(uint8_t buttons);            // MOUSE_LEFT 등 조합
void mouseRelease(uint8_t buttons);
void mouseReleaseAll();

// ---- Keyboard ----
void keyTap(uint8_t keycode);                // 단일 키 탭
void keyCombo(uint8_t mod1, uint8_t key);    // 예) CTRL + '+'
void keyPress(uint8_t keycode);
void keyRelease(uint8_t keycode);
void keyReleaseAll();

// ---- Gamepad (v3.x style) ----
// X,Y,RX,RY: -127..+127
// btns: 비트필드 (A=bit0, B=bit1, X=bit2, Y=bit3, L3=bit8, R3=bit9 등 상위 레이어에서 정의)
void gamepadSend(int8_t X, int8_t Y, int8_t RX, int8_t RY, uint32_t btns);

// 중립 리포트(초기화 직후, 재보정 완료 등에서 호출)
void gamepadNeutral();

// (선택) 전송 간격(밀리초) 힌트: 상위에서 rate-limit할 때 쓸 수 있음
uint32_t msSinceLastGamepadSend();

} // namespace USBDev
