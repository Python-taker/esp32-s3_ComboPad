#include "VendorHID.h"
#include "VendorWorker.h"
#include "../haptics/HapticsRuntime.h"
#include "../haptics/HapticsPolicy.h"

// TinyUSB (콜백 심볼만 필요)
extern "C" {
  #include "tusb.h"
  uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                                 hid_report_type_t report_type,
                                 uint8_t* buffer, uint16_t reqlen);
  void     tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                                 hid_report_type_t report_type,
                                 uint8_t const* buffer, uint16_t bufsize);
}

namespace {

// (선택) ConfigStore와 느슨 연결: 약한 심볼로 SAVE/LOAD/RESET 지원
extern "C" void CoreCfg_Save()  __attribute__((weak));
extern "C" void CoreCfg_Load()  __attribute__((weak));
extern "C" void CoreCfg_Reset() __attribute__((weak));

static inline void cfgSaveIfAvailable(){ if (CoreCfg_Save)  CoreCfg_Save();  }
static inline void cfgLoadIfAvailable(){ if (CoreCfg_Load)  CoreCfg_Load();  }
static inline void cfgResetIfAvailable(){if (CoreCfg_Reset) CoreCfg_Reset(); }

// ===== FEATURE 키 정의 =====
enum : uint8_t {
  FEAT_DUTY_MIN_PCT   = 1,
  FEAT_LRA_LIBRARY    = 2, // read-only: 6
  FEAT_BURST_MAX      = 3, // (옵션) 구현 생략
  FEAT_LRA_RETRIGGER  = 4, // (옵션) 구현 생략
  FEAT_GLOBAL_ENABLE  = 5,
};

// ====== INPUT 리포트 생성(간단 요약 64바이트) ======
static void fillInputReport(uint8_t* buf, uint16_t len) {
  if (len < 64) return;
  memset(buf, 0, len);
  buf[0] = VendorHID::RID_INPUT;

  // status 비트:
  // bit0 enabled, bit1 lraReady
  uint8_t status = 0;
  if (HapticsRuntime::isEnabled()) status |= 0x01;
  if (HapticsRuntime::lraReady())  status |= 0x02;
  buf[1] = status;

  // 간단 버전 문자열
  const char* fw = "1.0.0";
  memcpy(buf + 8, fw, strlen(fw));
}

// ====== OUTPUT 파서 → 워커 큐 ======
static void handleOutput(const uint8_t* b, uint16_t n) {
  if (n < 12) return;
  VendorWorker::VendorCmd v{};
  v.cmd       = static_cast<VendorWorker::CmdType>(b[1]);
  v.flags     = b[2];
  v.patternId = b[3];
  v.strengthL = b[4];
  v.strengthR = b[5];
  v.durMs     = (uint16_t)(b[6] | (b[7] << 8));
  v.repeat    = b[8];
  v.gapMs     = (uint16_t)(b[9] | (b[10] << 8));
  v.priority  = b[11];
  VendorWorker::enqueue(v);
}

// ====== FEATURE 처리 ======
static void handleFeature(const uint8_t* b, uint16_t n) {
  if (n < 5) return;
  uint8_t op  = b[1];
  uint8_t key = b[2];
  uint8_t v0  = b[3];
  uint8_t v1  = b[4];
  (void)v1;

  switch (op) {
    case 0: { // GET
      switch (key) {
        case FEAT_DUTY_MIN_PCT: {
          // 값을 즉시 시리얼로 돌려주지는 않음(호스트가 FEATURE GET 메카니즘 선택)
          // 필요 시 별도 IN 전송 경로 구현
        } break;
        case FEAT_GLOBAL_ENABLE: {
          // 동상
        } break;
        default: break;
      }
    } break;

    case 1: { // SET
      switch (key) {
        case FEAT_DUTY_MIN_PCT: {
          if (v0 > 100) v0 = 100;
          HapticsPolicy::setErmMinPct(v0);
        } break;
        case FEAT_GLOBAL_ENABLE: {
          HapticsRuntime::setEnabled(v0 != 0);
        } break;
        default: /* 미구현 */ break;
      }
    } break;

    case 2: cfgSaveIfAvailable();  break; // SAVE
    case 3: cfgLoadIfAvailable();  break; // LOAD
    case 4: cfgResetIfAvailable(); break; // RESET
    default: break;
  }
}

} // namespace

namespace VendorHID {

void begin() {
  VendorWorker::begin();
}

bool tryHandleSerialLine(const String& line) {
  if (!line.startsWith("hid2 ") && !line.startsWith("hid3 ")) return false;

  if (line.startsWith("hid2 ")) {
    // usage: hid2 cmd flags pattern sL sR dur repeat gap prio
    VendorWorker::VendorCmd v{};
    unsigned int cmd, flags, pattern, sL, sR, dur, repeat, gap, prio;
    int n = sscanf(line.c_str() + 5, "%u %x %u %u %u %u %u %u %u",
                   &cmd,&flags,&pattern,&sL,&sR,&dur,&repeat,&gap,&prio);
    if (n >= 6) {
      v.cmd       = static_cast<VendorWorker::CmdType>(cmd);
      v.flags     = (uint8_t)flags;
      v.patternId = (uint8_t)pattern;
      v.strengthL = (uint8_t)sL;
      v.strengthR = (uint8_t)sR;
      v.durMs     = (uint16_t)dur;
      v.repeat    = (uint8_t)repeat;
      v.gapMs     = (uint16_t)gap;
      v.priority  = (uint8_t)prio;
      VendorWorker::enqueue(v);
      Serial.println("[HID2] enqueued");
    } else {
      Serial.println("[HID2] usage: hid2 cmd flags pattern sL sR dur repeat gap prio");
    }
    return true;
  }

  if (line.startsWith("hid3 ")) {
    // usage: hid3 op key v0 v1
    unsigned int op, key, v0, v1;
    int n = sscanf(line.c_str() + 5, "%u %u %u %u", &op, &key, &v0, &v1);
    if (n >= 2) {
      uint8_t buf[5] = { VendorHID::RID_FEATURE, (uint8_t)op, (uint8_t)key, (uint8_t)v0, (uint8_t)v1 };
      handleFeature(buf, sizeof(buf));
      Serial.println("[HID3] feature handled");
    } else {
      Serial.println("[HID3] usage: hid3 op key v0 v1");
    }
    return true;
  }

  return false;
}

} // namespace VendorHID

// ===== TinyUSB 콜백들 =====
extern "C" uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                                          hid_report_type_t report_type,
                                          uint8_t* buffer, uint16_t reqlen)
{
  (void)itf; (void)report_type;
  if (report_id == VendorHID::RID_INPUT) {
    fillInputReport(buffer, reqlen);
    return (reqlen < 64) ? reqlen : 64;
  }
  return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                                      hid_report_type_t report_type,
                                      uint8_t const* buffer, uint16_t bufsize)
{
  (void)itf;
  if (bufsize < 2) return;

  if (report_type == HID_REPORT_TYPE_OUTPUT &&
      report_id  == VendorHID::RID_OUTPUT)
  {
    handleOutput(buffer, bufsize);
  }
  else if (report_type == HID_REPORT_TYPE_FEATURE &&
           report_id  == VendorHID::RID_FEATURE)
  {
    handleFeature(buffer, bufsize);
  }
}
