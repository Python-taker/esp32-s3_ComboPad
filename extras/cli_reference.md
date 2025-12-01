# cli_reference.md

> 목적: 시리얼 CLI 명령 레퍼런스(펌웨어 내 **MainCLI** 담당). 모듈 별 디버깅/설정 작업을 통일합니다.

## 공통

* 대소문자 구분 없음, 라인 단위 파싱
* 공백은 1개 이상 허용, 인자 부족 시 usage 출력

## 설정

* `cfg show` — 현재 설정 출력
* `cfg load` — NVS에서 설정 로드(버전 확인/마이그레이션 포함)
* `cfg save` — 현재 설정을 NVS 저장
* `cfg reset` — 기본값으로 되돌림(메모리 상, 저장 안 함)
* `cfg set <key> <val>` — 개별 설정 변경 + 즉시 적용

  * `gain` (float) — 터치패드→마우스 게인
  * `slth` (int) — 슬라이더 임계치
  * `zstep` (int) — 줌 스텝 ΔV
  * `wstep` (int) — 휠 스텝 ΔV
  * `mode`  (wheel|zoom|0|1) — 초기 모드

## 하프틱 운영

* `haptics on|off` — 마스터 스위치
* `hap min <pct>` — ERM 최소 듀티 %, 0..100(미저장은 `hap save` 전까지)
* `hap save` / `hap load` — 하프틱 관련 저장/로드(실제로는 ConfigStore 위임)
* `hap enable|disable` — 즉시 enable/disable + 상태 로그

## 로깅

* `log set <mask>` — 로그 마스크 설정(16진/10진 모두 허용)

  * 비트 예시: 0x01 CORE, 0x02 USB, 0x04 HAPTICS, 0x08 VENDOR, 0x10 IMU, 0x20 INPUT, 0x40 FACTORY … (BuildOpts/Log에 정의)

## ERM Fuse 상태

* `erm load` — loadL/loadR 및 cooldown 남은 시간(ms) 출력

## 팩토리/스모크

* `factory smoke` — 스모크 프로파일 실행(대화형/리부트 포함)
* `factory full`  — 풀 프로파일 실행(대화형/리부트 포함)

## Vendor HID (시리얼 백엔드)

> 실제 스펙/파서는 **vendor_hid_spec.md** 참고. 여기서는 개발 편의를 위해 동일 포맷을 시리얼로도 주입.

* `hid2 <cmd> <flags_hex> <pattern> <sL> <sR> <durMs> <repeat> <gapMs> <prio>`

  * 예: `hid2 1 0x1e 11 0 0 300 1 200 2`
* `hid3 <op> <key> <v0> <v1>` — FEATURE 동작

  * 예: `hid3 1 1 50 0`  → DUTY_MIN_%=50 설정(미저장)

## 권장 워크플로

1. `cfg load` → 2) `cfg show` → 3) 일부 튜닝(`cfg set …`, `hap min …`) → 4) `cfg save`

## 주의

* 일부 명령은 즉시 런타임 반영되며, 저장하려면 `cfg save`를 별도 실행해야 합니다.
* 위험: 과도한 ERM 연속 구동은 Fuse hard-cut을 유발할 수 있습니다. 테스트는 LRA 우선으로 수행하세요.
