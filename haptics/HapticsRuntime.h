#pragma once
//
// HapticsRuntime.h — 하프틱 런타임(큐/태스크/DRV2605L/I2C 뮤텍스)
//  - 비동기 실행(TaskHaptics)
//  - ERM PWM 구동 / LRA(Drv2605) 구동
//  - 마스터 enable 스위치
//  - I2C 공유용 내부 뮤텍스 제공
//

#include <Arduino.h>
#include <stdint.h>
#include "HapticsPolicy.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// HAL (핀/ I2C / 핀 준비)
#include "../hal/HAL.h"

// Adafruit DRV2605L
#include <Adafruit_DRV2605.h>

namespace HapticsRuntime {

// ====== 초기화/상태 ======
void begin();               // 큐/태스크/DRV2605L/I2C mutex 초기화
void setEnabled(bool en);
bool isEnabled();

// ====== I2C mutex(공유용) ======
SemaphoreHandle_t i2cMutex();
void i2cLock();
void i2cUnlock();

// ====== 공개 API ======
bool ErmPlay(HapticsPolicy::ErmDir dir, uint32_t ms, uint16_t duty);
bool LraPlay(uint32_t ms, uint8_t effect);
void stopAll();

// ====== 상태 조회 ======
bool getErmFuse(float &loadL, float &loadR, long &cooldownLeftMs);

// 내부 테스트/디버그용(선택적)
bool lraReady();

} // namespace HapticsRuntime
