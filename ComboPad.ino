//
// ComboPad.ino — 오케스트라(setup/loop 전담)
// - 모듈 초기화 순서 통합
// - 설정(NVS) 로드→적용
// - 부팅 팩토리 진입 윈도우 처리
// - 런타임 입력/CLI 폴링
//

#include <Arduino.h>

#include "core/BuildOpts.h"
#include "core/Version.h"
#include "core/Log.h"
#include "core/ConfigStore.h"
#include "core/MainCLI.h"

#include "hal/HAL.h"
#include "usb/USBDevices.h"

#include "haptics/HapticsPolicy.h"
#include "haptics/HapticsRuntime.h"

#include "vendor/VendorWorker.h"
#include "vendor/VendorHID.h"

#include "imu/IMU.h"

#include "input/RuntimeInput.h"
#include "factory/FactoryTests.h"

// ==============================
// USB 제품 문자열(필요에 맞게 조정)
// ==============================
static const char* kUsbProduct      = "S3 ComboPad";
static const char* kUsbManufacturer = "My Studio";
static const char* kUsbSerial       = "S3-COMBO-0007";

// ==============================
// 팩토리 모드 진입 파라미터
// ==============================
static constexpr uint32_t FACTORY_WINDOW_MS = 10000;  // 부팅 후 10초 내
static constexpr uint32_t FACTORY_HOLD_MS   = 2500;   // L3+R3+A 유지

static bool   g_factoryChecked = false;
static uint32_t g_bootStartMs  = 0;
static uint32_t g_factoryHoldStart = 0;

// ==============================
// 내부 함수 원형
// ==============================
static void applyConfigToRuntime();          // NVS 구성 → 런타임 모듈 반영
static void checkFactoryEntryDuringBoot();   // 부팅 윈도우 내 공장 모드 진입
static void showReadyBlink();                // 준비 표시(현재 모드 LED 짧은 점등)

// ==============================
// setup
// ==============================
void setup() {
  // 시리얼 먼저 열고 아주 이른 로그도 받을 수 있게 함
  Serial.begin(115200);
  delay(10);

  // 1) 로깅 초기화(최소한의 마스크로 시작) — 실제 마스크는 NVS 로드 후 반영
  Log::init();
  LOGI("BOOT", "==== ComboPad boot ====");
  LOGI("BOOT", "FW %s | CFG %u", Version::kFirmwareStr, Version::kConfigVersion);

  // 2) HAL (핀/I2C/ADC/터치)
  HAL::init();

  // 3) USB 장치 래퍼
  USBDevices::init(kUsbProduct, kUsbManufacturer, kUsbSerial);

  // 4) 하프틱(정책 → 런타임 순으로)
  HapticsPolicy::init();
  HapticsRuntime::init();        // DRV2605L, 큐/태스크, I2C mutex 등

  // 5) 벤더(워커 → HID 파서 순으로)
  VendorWorker::init();          // 반복/선점/지연 처리 전용 태스크
  VendorHID::init();             // TinyUSB 콜백 등록 + 시리얼 백엔드 파서

  // 6) IMU (가능 시 시작)
  IMU::init();

  // 7) 설정(NVS) → 로드 → 런타임 반영
  ConfigStore::load();           // 내부적으로 버전 확인/마이그레이션 처리
  applyConfigToRuntime();

  // 8) 입력 엔진(터치/슬라이더/게임패드 파이프라인 묶음)
  RuntimeInput::init();

  // 9) 팩토리/스모크 테스트 모듈
  FactoryTests::init();

  // 10) 사용자 피드백(현재 모드 등의 짧은 LED 안내)
  showReadyBlink();

  // 부팅 타임스탬프
  g_bootStartMs  = millis();
  g_factoryChecked = false;

  LOGI("BOOT", "ready (USB/Haptics/Vendor/IMU/Input/NVS up)");
}

// ==============================
// loop
// ==============================
void loop() {
  // 1) 부팅 윈도우 내 팩토리 진입 감시
  checkFactoryEntryDuringBoot();

  // 2) CLI 폴링(설정/로그/팩토리 명령 수용)
  MainCLI::poll();   // 논블로킹

  // 3) 런타임 입력 처리(터치→마우스, 슬라이더→휠/줌, 게임패드, 제스처)
  RuntimeInput::tick(millis());

  // 4) 필요 시 여유 딜레이 (주 타이밍은 각 모듈 내부에서 조정)
  delay(5);
}

// ==============================
// 구성 적용(설정 → 런타임)
// ==============================
static void applyConfigToRuntime() {
  const auto &cfg = ConfigStore::get();

  // 로그 마스크
  Log::setMask(cfg.log_mask);

  // 커서/슬라이더 파라미터/모드 — 입력 엔진에 전달
  RuntimeInput::setCursorGain(cfg.cursor_gain);
  RuntimeInput::setSliderParams(cfg.slider_thresh, cfg.zoom_step_dv, cfg.wheel_step_dv);
  RuntimeInput::setInitialSliderMode(cfg.initial_mode); // 0: wheel, 1: zoom

  // 하프틱 마스터 ON/OFF
  HapticsRuntime::setEnabled(cfg.haptics_on);

  // 하프틱 정책 파라미터(ERM 최소 듀티 % 등)
  HapticsPolicy::setErmMinPct(cfg.erm_min_pct);

  LOGI("CFG",
       "applied: gain=%.2f slth=%d zstep=%d wstep=%d mode=%s haptics=%s ermMin=%u%% log=0x%08lx",
       cfg.cursor_gain, cfg.slider_thresh, cfg.zoom_step_dv, cfg.wheel_step_dv,
       (cfg.initial_mode==1 ? "zoom" : "wheel"),
       (cfg.haptics_on ? "on" : "off"), cfg.erm_min_pct, (unsigned long)cfg.log_mask);
}

// ==============================
// 준비 LED 피드백
// ==============================
static void showReadyBlink() {
  // 현재 모드(초기값)에 따라 R(zoom)/G(wheel) 짧게 표시
  const auto &cfg = ConfigStore::get();
  if (cfg.initial_mode == 1) { // zoom
    HAL::ledR(true); delay(250); HAL::ledR(false);
  } else {
    HAL::ledG(true); delay(250); HAL::ledG(false);
  }
}

// ==============================
// 부팅 윈도우 내 팩토리 진입
//  - L3 + R3 + A 를 2.5s 이상 유지
//  - 해제 직후 3초 이내에 A=Smoke, B=Full 선택
// ==============================
static void checkFactoryEntryDuringBoot() {
  if (g_factoryChecked) return;

  const uint32_t now = millis();
  if ((now - g_bootStartMs) > FACTORY_WINDOW_MS) {
    g_factoryChecked = true;
    return;
  }

  const bool l3 = HAL::pressed(HAL::Button::L3);
  const bool r3 = HAL::pressed(HAL::Button::R3);
  const bool a  = HAL::pressed(HAL::Button::A);

  static bool comboActive = false;

  if (l3 && r3 && a) {
    if (!comboActive) {
      comboActive = true;
      g_factoryHoldStart = now;
    } else if ((now - g_factoryHoldStart) >= FACTORY_HOLD_MS) {
      LOGI("FACT", "boot-combo detected (L3+R3+A). Select profile: A=SMOKE, B=FULL (3s)");
      // 3초간 선택 대기
      const uint32_t selT = millis();
      while (millis() - selT < 3000) {
        if (HAL::pressed(HAL::Button::A)) {
          FactoryTests::run(FactoryTests::Profile::Smoke, /*interactive=*/true);
          g_factoryChecked = true;
          return;
        }
        if (HAL::pressed(HAL::Button::B)) {
          FactoryTests::run(FactoryTests::Profile::Full,  /*interactive=*/true);
          g_factoryChecked = true;
          return;
        }
        // 파란 LED 하트비트
        static uint32_t hb=0; static bool on=false;
        if (millis() - hb > 500) { on = !on; HAL::ledB(on); hb = millis(); }
        delay(5);
      }
      // 타임아웃이면 Smoke 기본 선택
      FactoryTests::run(FactoryTests::Profile::Smoke, /*interactive=*/true);
      g_factoryChecked = true;
      return;
    }
  } else {
    comboActive = false;
    g_factoryHoldStart = 0;
  }
}
