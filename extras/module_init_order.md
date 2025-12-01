# module_init_order.md

> 목적: 부팅 시 **안전하고 재현 가능한 초기화 순서**를 정의합니다. 모듈 간 의존성을 명시하고, 장애 허용/리트라이/로그 전략을 통일합니다.

## TL;DR (순서 요약)

1. **Serial** (115200) — 조기 로그 확보
2. **Log::init()** — 임시 마스크(부트 최소 로그)
3. **HAL::init()** — 핀/I2C/ADC/Touch 준비
4. **USBDevices::init()** — USB HID 래퍼 준비
5. **HapticsPolicy::init()** — 정책(퓨즈/하한/폴백) 초기화(상태 0)
6. **HapticsRuntime::init()** — I2C mutex, DRV2605L 탐색 및 모드 설정, 큐/태스크 시작
7. **VendorWorker::init()** — VendorCmd 전용 워커 태스크 시작
8. **VendorHID::init()** — TinyUSB 콜백 등록, 시리얼 백엔드 파서 등록
9. **IMU::init()** — WHO_AM_I 확인(0x69), 태스크 2개(ax/ay/az, 리액트) 시작(옵션)
10. **ConfigStore::load()** — NVS/버전/마이그레이션
11. **applyConfigToRuntime()** — 로그 마스크, 입력 파라미터, 하프틱 마스터/정책 반영
12. **RuntimeInput::init()** — 파이프라인 내부 상태 초기화
13. **FactoryTests::init()** — 테이블 준비(인터랙티브 진입은 오케스트라에서)
14. **showReadyBlink()** — 현재 모드(R/G) 짧은 점등

## 의존성 메모

* `HapticsRuntime`는 `HAL`(핀/I2C)과 `HapticsPolicy`에 의존.
* `VendorHID`는 `VendorWorker`가 먼저 살아 있어야 큐 인입이 안전.
* `applyConfigToRuntime`는 `ConfigStore::load` 이후 한 번만 호출.
* `RuntimeInput`은 USB와 하프틱이 준비된 후 시작(피드백/입력 동기화).

## 장애 허용/리트라이 규칙

* DRV2605L 미탐지: `HapticsRuntime`는 **ERM 폴백** 모드로 동작(정책의 effectToDuty 매핑 사용).
* IMU 미탐지: 입력/하프틱은 계속 동작. `IMU::isReady()==false`일 때 리액트 태스크는 noop.
* Vendor 큐 포화: `VendorHID`는 드롭+경고 로그, 메인 실행은 지속.

## 타임라인 & 우선순위(FreeRTOS)

* **HapticsRuntime** 태스크: prio 3, 코어1
* **VendorWorker** 태스크: prio 2, 코어1
* **IMU** 태스크: prio 1, 코어0 (샘플링/리액트 각각)
* 메인 루프는 5ms 휴식(모듈 내부 타이밍 우선)
