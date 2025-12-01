# ComboPad 개발 가이드 (ESP32‑S3)

ESP32‑S3(N16R8) 기반 **ComboPad** 프로젝트의 전체 구조, 초기화 순서, 빌드/플래싱 방법, 주의사항, 팁을 한 문서에 정리했습니다. 본 문서는 아래 리포 구조를 기준으로 작성되었습니다.

---

## 📑 목차

* [프로젝트 개요](#-프로젝트-개요)
* [파일/폴더 구조](#-파일폴더-구조)
* [빠른 실행(First Flash)](#-빠른-실행first-flash)
* [개발 환경 요구사항](#-개발-환경-요구사항)
* [빌드 설정 체크리스트](#-빌드-설정-체크리스트)
* [모듈 초기화 순서](#-모듈-초기화-순서)
* [CLI 요약](#-cli-요약)
* [Vendor HID 요약](#-vendor-hid-요약)
* [입력 파이프라인(터치/슬라이더/게임패드)](#-입력-파이프라인터치슬라이더게임패드)
* [하프틱(정책·런타임) 요약](#-하프틱정책런타임-요약)
* [IMU 요약](#-imu-요약)
* [자주 겪는 이슈 & 주의사항](#-자주-겪는-이슈--주의사항)
* [필수 점검(Bring‑up Checklist)](#-필수-점검bringup-checklist)
* [부품 비용(참고)](#-부품-비용참고)
* [참고 자료/링크](#-참고-자료링크)

---

## 🚀 프로젝트 개요

* **목표**: 하나의 펌웨어로 Gamepad + Mouse + Keyboard + Vendor HID + 하프틱(ERM/LRA) 제어를 통합.
* **핵심 철학**: `ComboPad.ino`는 **오케스트라**만 담당(setup/loop). 모든 기능은 **모듈로 분리**.
* **테스트 가능성**: 정책/수학(곡선/데드존/정규화/퓨즈 카운터)은 PC 유닛테스트 허용 구조(매크로 전환) 지향.

---

## 🗂 파일/폴더 구조

```
/ComboPad
  ├─ ComboPad.ino
  ├─ README.md
  ├─ .clang-format
  ├─ core/
  │   ├─ BuildOpts.h
  │   ├─ Version.h
  │   ├─ Log.h / Log.cpp
  │   ├─ ConfigStore.h / ConfigStore.cpp
  │   ├─ MainCLI.h / MainCLI.cpp
  ├─ hal/
  │   ├─ HAL.h / HAL.cpp
  ├─ usb/
  │   ├─ USBDevices.h / USBDevices.cpp
  ├─ haptics/
  │   ├─ HapticsPolicy.h / HapticsPolicy.cpp
  │   ├─ HapticsRuntime.h / HapticsRuntime.cpp
  ├─ vendor/
  │   ├─ VendorHID.h / VendorHID.cpp
  │   ├─ VendorWorker.h / VendorWorker.cpp
  ├─ input/
  │   ├─ RuntimeInput.h / RuntimeInput.cpp
  │   ├─ TouchPadPipeline.h / TouchPadPipeline.cpp
  │   ├─ SliderPipeline.h / SliderPipeline.cpp
  │   ├─ GamepadPipeline.h / GamepadPipeline.cpp
  │   ├─ GestureEngine.h / GestureEngine.cpp
  ├─ imu/
  │   ├─ IMU.h / IMU.cpp
  ├─ factory/
  │   ├─ FactoryTests.h / FactoryTests.cpp
  └─ extras/
      ├─ module_init_order.md
      ├─ vendor_hid_spec.md
      └─ cli_reference.md
```

> 구조가 커지더라도, **모듈명/책임**이 한 눈에 들어오도록 폴더 단위로 정리합니다.

---

## ⚡ 빠른 실행(First Flash)

1. **보드 설정(Arduino IDE)**

* Board: *ESP32S3 Dev Module*
* USB CDC On Boot: **Enabled**
* USB Mode: **USB‑OTG (TinyUSB)**
* Upload Mode: **USB‑OTG CDC (TinyUSB)**
* PSRAM: *OPI PSRAM (if N16R8)*
* Flash Size: *16MB* / Partition: *default*

2. **USB 장치 문자열(선택)**

```cpp
#include <USB.h>
void setup(){
  USB.productName("S3 ComboPad");
  USB.manufacturerName("My Studio");
  USB.serialNumber("S3-COMBO-0007");
  USB.begin();
  /* ... */
}
```

> 장치 이름을 OS 수준에서 고정하려면 코어 수정도 가능(자세한 내용은 하단 참고/주의사항).

3. **플래싱 & 초기 동작**

* 연결: USB‑C 커넥터를 **아래로 두고 왼쪽 포트** 사용.
* 시리얼 모니터 115200bps → 부팅 로그 확인.
* `help` 또는 `cfg show` 등 CLI 명령으로 상태 점검.

---

## 🧰 개발 환경 요구사항

* Arduino‑ESP32 v3.x (TinyUSB 포함)
* 사용 라이브러리 예: `Adafruit_DRV2605`, `mpr121`(DFRobot 버전), `Adafruit Unified Sensor` 등
* C++17 빌드(ESP32 코어 기본)

> **ADC2 주의**: Wi‑Fi 활성 동작 시 ADC2(GPIO 11~20) 읽기가 불안정할 수 있음. 슬라이더/오른쪽 스틱은 ADC2 계열이므로 현장 옵션화 권장.

---

## 🧪 빌드 설정 체크리스트

* `.clang-format` 배포(팀 일관 포맷)
* `core/BuildOpts.h` : 로그/ASSERT/테스트 플래그
* `core/Version.h` : 펌웨어/설정 버전 동기화
* `core/ConfigStore.*` : NVS 네임스페이스/키/마이그레이션 루틴 점검
* USB Mode = TinyUSB, CDC On Boot = Enabled 확인

---

## 🧬 모듈 초기화 순서

1. **Log** → 최소한의 로깅 준비
2. **HAL** → 핀모드, I2C, ADC 해상도/어텐, PWM 핀 준비
3. **ConfigStore** → NVS 로드/버전 확인/마이그레이션 → `cfgApplyToRuntime()`
4. **USBDevices** → Composite HID 래퍼 시작
5. **HapticsRuntime** → DRV2605L/I2C뮤텍스/큐/태스크 준비
6. **HapticsPolicy** → 하한%, 폴백, 퓨즈 파라미터 주입
7. **VendorWorker/HID** → 파서/워커 태스크, TinyUSB 콜백 등록
8. **IMU**(선택) → 센서 시작/폴링 태스크
9. **RuntimeInput** → Touch/Slider/Gamepad 파이프라인 바인딩 & 시작
10. **FactoryTests**(옵션) → 엔트리만 준비(진입 시 실행)

자세한 순서는 `extras/module_init_order.md` 참고.

---

## ⌨️ CLI 요약

* `cfg show|load|save|reset` : 설정 조회/로드/저장/초기화
* `cfg set <key> <val>` : 설정 키 값 변경 (예: `cursor.gain`, `slider.step`, `erm.minpct`, `log.mask`)
* `haptics on|off` : 마스터 스위치 / `hap min <pct>` : ERM 최소 듀티 %
* `log set <mask>` : 하위시스템별 로그 비트마스크
* `erm load` : ERM 퓨즈/쿨다운 조회
* `factory smoke|full` : 스모크/풀 테스트 진입

세부 명세는 `extras/cli_reference.md` 참고.

---

## 🧾 Vendor HID 요약

* `vendor/VendorHID.*` : **리포트 스펙/파서 + TinyUSB 콜백** (비블로킹, 워커 큐에 enqueue)
* `vendor/VendorWorker.*` : **선점/반복/딜레이 스케줄링**(repeat, priority, cancel)
* 시리얼 백엔드: `hid2 ...`, `hid3 ...` 텍스트 명령으로 동일 동작 유도(PC툴 연동 편의)

리포트 맵은 `extras/vendor_hid_spec.md` 참고.

---

## 🖱 입력 파이프라인(터치/슬라이더/게임패드)

* **TouchPadPipeline**: MPR121 좌표 → 상대 마우스 이동(게인/데드존), 탭/더블탭 → 클릭
* **SliderPipeline**: 모드별 휠/줌 변환(누적 스텝), 터치 중 억제, 동작 시 R/G 인디케이터 점등
* **GamepadPipeline**: ADC → 정규화 → XY 스왑/반전 → EMA → 데드존/감마 → int8 → v3.x HID 전송(변화시에만)
* **GestureEngine**: (예) 터치+ABXY 3초 → 하프틱 토글, 터치 2.5~6.2초 홀드→모드 토글, L3+R3 1초→센터 재보정, 부팅윈도우 L3+R3+A 2.5초→Factory 진입 등

---

## 💥 하프틱(정책·런타임) 요약

* **Policy**: `pctToDuty`, `clampMs/Duty`, `fuseCheckAndAdjust`, `fuseAccumulate`, `effectToDuty(LRA→ERM 폴백)`
* **Runtime**: 큐/태스크, `ErmPlay/LraPlay/stopAllHapticsNow()`, I2C mutex, DRV2605L 초기화/동작, 마스터 enable
* **설정 연계**: `cfgApplyToRuntime()`에서 `HapticsRuntime::setEnabled()`, `HapticsPolicy::setErmMinPct()` 등 반영

---

## 🧭 IMU 요약

* `imu/IMU.*` : LSM6DS3TR‑C 초기화 + 주기 폴링 태스크(선택)
* 정상 구동까지 **메인 입력 경로와 분리**(옵션 플래그로 빌드)

---

## ⚠️ 자주 겪는 이슈 & 주의사항

* **MPR121 I2C 주소**: DFRobot 모듈은 `mpr121.h`의 주소를 `0x5B`로 변경 필요.
* **단순 터치 모듈(디지털)**: 하드웨어 특성상 **6.2초 이상 연속 홀드 불가** → 제스처 설계 시 2.5~6.2초 윈도우에서 `RELEASE`를 트리거로 사용.
* **ADC2 주의**: Wi‑Fi 동작과 동시 사용 시 읽기 흔들림 가능(슬라이더/오른스틱). 테스트 시 Wi‑Fi 비활성 또는 필터/히스테리시스 강화.
* **LED 극성**: 기본은 **공통 캐소드(CC)**. 공통 애노드(CA) 사용 시 PWM 반전 필요.
* **TinyUSB 장치명 커스터마이즈**: `USB.productName()` 등으로 장치 문자열 설정 가능. OS 레벨 표시 명칭을 완전 고정하려면 코어 소스(USBHID.cpp) 커스터마이즈가 필요할 수 있음.
* **USB 케이블 방향**: USB‑C를 아래로 두고 **왼쪽 포트** 사용(보드별 편차 가능).

---

## ✅ 필수 점검(Bring‑up Checklist)

* [ ] 부팅 로그 출력 확인(115200bps)
* [ ] `HAL.init()` 후 LED/버튼/ADC 정상
* [ ] `ConfigStore` 로드→버전 일치/마이그레이션/`cfgApplyToRuntime()` 호출됨
* [ ] USB Composite: Gamepad/Mouse/Keyboard 인식(Windows `joy.cpl`/macOS Gamepad Tester)
* [ ] MPR121 좌표 수신/마우스 이동 OK, 탭/더블탭 클릭 OK
* [ ] 슬라이더: Wheel/Zoom 모드 전환(터치 홀드 2.5~6.2s), 인디케이터 R/G 점등
* [ ] Gamepad: 스틱 정규화/데드존/EMA/감마 적용, ABXY/L3/R3 버튼 반영, 변경시에만 전송
* [ ] 하프틱: DRV2605L init, ERM/LRA 동작, 폴백/퓨즈/하한% 정책 적용
* [ ] CLI: 설정/로그/팩토리 진입 명령 동작
* [ ] Factory: `smoke|full` 시나리오 성공

---

## 💰 부품 비용(참고)

> 아래 표는 공유된 구매 링크/단가를 바탕으로 한 **참고 견적**입니다. 재고/환율/배송비에 따라 달라질 수 있습니다.

(원문 표 그대로 유지)

| 부품명                  | 단가     | 개수 | 비용     | 구매 링크 | 부품 정보 |
| -------------------- | ------ | -- | ------ | ----- | ----- |
| ESP32‑S3             | 15,400 | 3  | 46,200 | (링크)  |       |
| 진동모터 실린더형            | 1,320  | 6  | 7,920  | (링크)  |       |
| 진동모터 LRA방식           | 3,300  | 4  | 13,200 | (링크)  |       |
| DRV2605L             | 12,000 | 2  | 24,000 | (링크)  |       |
| LSM6D3TR‑C           | 18,000 | 2  | 36,000 | (링크)  |       |
| 전기 용량성 터치 센서 키트      | 23,100 | 2  | 46,200 | (링크)  |       |
| 디지털 정전식 터치 센서        | 7,700  | 2  | 15,400 | (링크)  |       |
| Li‑Po 배터리(DTP103040) | 4,700  | 3  | 14,100 | (링크)  |       |
| Li‑ion 18650         | 3,300  | 2  | 6,600  | (링크)  |       |
| 배터리 잔량표시기            | 1,320  | 2  | 2,640  | (링크)  |       |
| 18650 홀더             | 220    | 4  | 880    | (링크)  |       |
| HAM4816              | 3,080  | 4  | 12,320 | (링크)  |       |
| USB‑C 브레이크 아웃        | 8,900  | 2  | 17,800 | (링크)  |       |
| 조이스틱 모듈              | 440    | 4  | 1,760  | (링크)  |       |
| 마이크로 택트 4핀           | 220    | 13 | 2,860  | (링크)  |       |
| (이하 공구/패스너/케이블 등)    |        |    |        |       |       |

> 원문 평균 단가/총액: **3,392.67원 × 171개 = 367,460원** (배송비 제외)

---

## 🔗 참고 자료/링크

* `extras/cli_reference.md` : CLI 상세 명세/예시
* `extras/vendor_hid_spec.md` : Vendor HID 리포트/파서 명세
* `extras/module_init_order.md` : 실제 초기화 시퀀스와 의존성
* TinyUSB & Arduino‑ESP32 v3.x Gamepad API 예시(프로젝트 내부 `usb/USBDevices.*` 참고)
* 하드웨어 핀맵은 `hal/HAL.h` 상단 정의(보드 바뀔 때 이 파일만 수정)

---

## 📌 부록: 실전 팁

* **민감도 튜닝**: 터치 `CURSOR_GAIN`, 슬라이더 `ZOOM_STEP_DV/WHEEL_STEP_DV`, 히스테리시스 `SLIDER_THRESH`는 현장 조정값으로 노출.
* **로그 마스크**: 하드웨어별 bitmask(USB/IMU/Haptics/Factory 등)로 현장 디버깅에 유리. `log set <mask>` 지원.
* **정수/고정소수점**: 스틱 곡선/EMA/정규화는 고정소수점 전환 여지 있음(성능/일관성 향상). 지금은 float로 충분.
* **UNIT_TEST 모드**: `#ifdef UNIT_TEST`에서 Wire/DRV2605/USB 더미 어댑터로 치환해 PC 단위 테스트 허용.

---

> 문서/구성은 실제 구현(헤더/CPP)과 동기화되어야 합니다. 변경 시 `extras/*.md`와 CLI 도움말을 함께 업데이트하세요.
