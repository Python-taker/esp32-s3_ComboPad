#pragma once
//
// IMU.h — LSM6DS3TR-C 드라이버(+ 옵션: IMU 기반 하프틱 리액트 엔진)
//  - begin()만 호출하면 ACC 104Hz/±2g로 구동, 읽기 태스크가 주기적으로 갱신
//  - getAccel()로 최신 가속도(g 단위) 조회
//  - enableHapticReact(false)로 제스처 하프틱 비활성화 가능
//

#include <Arduino.h>
#include <stdint.h>

namespace IMU {

// ---- 라이프사이클 ----
void begin();                 // I2C init(이미 HAL이 수행), WHO_AM_I 확인, 태스크 시작
bool isReady();               // WHO_AM_I 확인 성공여부

// ---- 데이터 접근 ----
void getAccel(float& ax, float& ay, float& az);   // 최근 샘플(g)

// |a| (magnitude) 편의 함수
float accelMagnitude();

// ---- 하프틱 리액트 엔진(옵션) ----
// 기본값: enabled(true). 필요 시 런타임 토글
void enableHapticReact(bool en);
bool isHapticReactEnabled();

// 파라미터(원하면 나중에 CLI로 연결)
struct ReactParams {
  // 히스테리시스 임계값
  float th1_on  = 0.50f;
  float th1_off = 0.45f;
  float th2_on  = 0.75f;
  float th2_off = 0.70f;

  // LRA 반복/버스트/대기
  uint8_t  maxBursts    = 5;
  uint32_t repeatMs     = 300;   // 동일 상태 반복 최소 간격
  uint32_t lraRetrigMs  = 300;   // 내부 재트리거 간격(안정)
  // ERM 에스컬레이션
  uint32_t ermSingleMs  = 1500;  // X/Y 단일 에스컬레이션
  uint32_t ermBothMs    = 5000;  // XY 동시 에스컬레이션
  uint16_t ermEscDuty   = 700;   // 0..1023 (정책 하한에 의해 상향될 수 있음)
};

// 파라미터 설정/조회
void setReactParams(const ReactParams& p);
ReactParams getReactParams();

} // namespace IMU
