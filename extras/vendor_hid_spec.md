# vendor_hid_spec.md

> 목적: PC툴/호스트가 Vendor HID 인터페이스를 통해 **하프틱 제어/설정**을 수행하기 위한 스펙.

## 인터페이스 개요

* **Usage Page**: 0xFF00 (Vendor-defined)
* **Reports**:

  * **INPUT (ID=1)**: 상태 조회
  * **OUTPUT (ID=2)**: 하프틱 재생/정지 명령 (비블로킹, 워커가 반복/간격 처리)
  * **FEATURE (ID=3)**: 설정 Get/Set/Save/Load/Reset
* **엔디안**: multi-byte는 **Little Endian**
* **Max report size**: 64 bytes (고정 프레임, 나머지는 0 패딩)

## REPORT LAYOUTS

### INPUT — ID=1 (Device → Host)

| Byte | Name          | Desc                                                                                |
| ---: | ------------- | ----------------------------------------------------------------------------------- |
|    0 | Report ID     | 0x01                                                                                |
|    1 | status        | bit0=hapticsOn, bit1=LRA-ready, bit2=ERM-active, bit3=queue-busy, bit4=lastError!=0 |
|    2 | ermActiveMask | bit0=Left, bit1=Right                                                               |
|    3 | lastError     | 0=OK, 1=I2C, 2=QueueFull, 3=FuseCut 등                                               |
|  8.. | fwVersion[?]  | ASCII, NUL 미보장(호스트는 길이 체크)                                                          |

### OUTPUT — ID=2 (Host → Device)

| Byte | Name      | Desc                                                       |
| ---: | --------- | ---------------------------------------------------------- |
|    0 | Report ID | 0x02                                                       |
|    1 | cmd       | 0=NOP, 1=PLAY, 2=STOP_ALL, 3=STOP_LEFT, 4=STOP_RIGHT       |
|    2 | flags     | b0=LRA, b1=ERM, b2=L, b3=R, b4=exclusive, b5=allowFallback |
|    3 | pattern   | LRA 패턴(라이브러리 인덱스)                                          |
|    4 | sL        | 0..255(ERM Left 강도)                                        |
|    5 | sR        | 0..255(ERM Right 강도)                                       |
|  6-7 | durMs     | 실행 시간(ms, LE)                                              |
|    8 | repeat    | 0=1회, n= (n+1)회 반복(최대 11회)                                 |
| 9-10 | gapMs     | 반복 간격(ms, LE), 최소 50ms 적용                                  |
|   11 | priority  | 0=LOW, 1=NORMAL, 2=HIGH(선점 허용 시 stopAll)                   |

> **동작 규칙**
>
> * 콜백은 **즉시 리턴**: 파싱→`VendorWorker` 큐 enqueue만 수행(비블로킹)
> * `exclusive && priority>=HIGH`일 때 플레이 직전 **모든 하프틱 정지**
> * LRA 사용 불가 시 `allowFallback` 또는 `ERM flag`가 켜져 있으면 ERM 경로로 폴백

### FEATURE — ID=3 (Host ↔ Device)

| Byte | Name      | Desc                                            |
| ---: | --------- | ----------------------------------------------- |
|    0 | Report ID | 0x03                                            |
|    1 | op        | 0=GET, 1=SET, 2=SAVE, 3=LOAD, 4=RESET           |
|    2 | key       | 1=DUTY_MIN_%, 5=GLOBAL_ENABLE, 2=LRA_LIB(읽기만) 등 |
|    3 | v0        | 값(주로 0..100)                                    |
|    4 | v1        | 예약                                              |

## 예시(OUTPUT)

* **양쪽 ERM 70%, 1.1s, 1회**

```
ID=2, cmd=1, flags=0b00001110 (ERM+L+R), sL=179, sR=179, dur=1100, repeat=0, gap=0, prio=1
```

* **LRA eff#11, 300ms, 2회, gap 200ms, High+exclusive**

```
ID=2, cmd=1, flags=0b00010001 (LRA+exclusive), pattern=11, dur=300, repeat=1, gap=200, prio=2
```

## 리턴/에러 정책

* 큐 만재: 드롭 + `[VENDOR] queue full`
* LRA 미준비: `allowFallback` 미설정이면 무시(로그 only)
* Fuse hard-cut 중: 명령 거부(LED 패턴 + 로그)
