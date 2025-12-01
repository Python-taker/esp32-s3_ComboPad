#include "USBDevices.h"

#include <USB.h>
#include <USBHIDMouse.h>
#include <USBHIDKeyboard.h>
#include <USBHIDGamepad.h>

namespace {
  USBHIDMouse     gMouse;
  USBHIDKeyboard  gKeyboard;
  USBHIDGamepad   gGamepad;

  // 마지막 게임패드 전송 시각
  uint32_t gLastGpSendMs = 0;

  // 중립 상태 캐시(불필요한 중복 전송을 줄이고 싶으면 사용 가능)
  int8_t   gLastX  = 0, gLastY  = 0, gLastRX = 0, gLastRY = 0;
  uint32_t gLastButtons = 0;

  inline bool changed(int8_t X, int8_t Y, int8_t RX, int8_t RY, uint32_t btns){
    return (X!=gLastX) || (Y!=gLastY) || (RX!=gLastRX) || (RY!=gLastRY) || (btns!=gLastButtons);
  }
}

namespace USBDev {

// ---- 초기화/수명주기 ----
void begin(const char* product,
           const char* manufacturer,
           const char* serial)
{
  if (product)      USB.productName(product);
  if (manufacturer) USB.manufacturerName(manufacturer);
  if (serial)       USB.serialNumber(serial);

  USB.begin();
  gMouse.begin();
  gKeyboard.begin();
  gGamepad.begin();

  // 첫 리포트는 중립으로
  gamepadNeutral();
}

bool ready() {
  // TinyUSB에서 별도 “enumerated” 플래그는 없으므로,
  // 일단 USB가 시작되었다고 가정. 필요하면 추가 상태를 여기서 추적.
  return true;
}

void end() {
  // 일반적으로 호출하지 않지만, 구조상 제공
  USB.end();
}

// ---- Mouse ----
void mouseMove(int x, int y, int wheel) {
  gMouse.move(x, y, wheel);
}

void mouseClickLeft()  { gMouse.click(MOUSE_LEFT);  }
void mouseClickRight() { gMouse.click(MOUSE_RIGHT); }

void mousePress(uint8_t buttons)   { gMouse.press(buttons);   }
void mouseRelease(uint8_t buttons) { gMouse.release(buttons); }
void mouseReleaseAll()             { gMouse.releaseAll();     }

// ---- Keyboard ----
void keyTap(uint8_t keycode) {
  gKeyboard.write(keycode);
}

void keyCombo(uint8_t mod1, uint8_t key) {
  gKeyboard.press(mod1);
  gKeyboard.write(key);
  gKeyboard.releaseAll();
}

void keyPress(uint8_t keycode)   { gKeyboard.press(keycode);   }
void keyRelease(uint8_t keycode) { gKeyboard.release(keycode); }
void keyReleaseAll()             { gKeyboard.releaseAll();     }

// ---- Gamepad ----
void gamepadSend(int8_t X, int8_t Y, int8_t RX, int8_t RY, uint32_t btns) {
  if (!changed(X,Y,RX,RY,btns)) {
    return; // 중복 전송 방지(상위에서 주기 제한과 함께 사용 권장)
  }
  // v3.x API 형태: Gamepad.send(X, Y, Z, RZ, RX, RY, hat, buttons)
  // 우리 장치는 Z/RZ를 0, hat=0x08(중립) 고정으로 사용
  gGamepad.send(X, Y, 0, 0, RX, RY, 0x08, btns);

  gLastX = X; gLastY = Y; gLastRX = RX; gLastRY = RY; gLastButtons = btns;
  gLastGpSendMs = millis();
}

void gamepadNeutral() {
  gGamepad.send(0, 0, 0, 0, 0, 0, 0x08, 0);
  gLastX = gLastY = gLastRX = gLastRY = 0;
  gLastButtons = 0;
  gLastGpSendMs = millis();
}

uint32_t msSinceLastGamepadSend() {
  return millis() - gLastGpSendMs;
}

} // namespace USBDev
