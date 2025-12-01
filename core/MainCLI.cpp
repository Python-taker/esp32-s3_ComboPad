#include "MainCLI.h"

#include "Log.h"
#include "BuildOpts.h"
#include "Version.h"

#include "../haptics/HapticsRuntime.h"
#include "../haptics/HapticsPolicy.h"
#include "../factory/FactoryTests.h"

using namespace ConfigStore;

namespace MainCLI {

static Config* s_cfg = nullptr;

// ---- 유틸 ----
static String toLowerTrim(String s) {
  s.trim();
  s.toLowerCase();
  return s;
}
static bool parseUint32(const String& s, uint32_t& out) {
  char* end = nullptr;
  // 0x...도 허용
  long v = strtoul(s.c_str(), &end, 0);
  if (end == nullptr || *end != '\0') return false;
  if (v < 0) return false;
  out = static_cast<uint32_t>(v);
  return true;
}
static bool parseInt(const String& s, int& out) {
  char* end = nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (end == nullptr || *end != '\0') return false;
  out = static_cast<int>(v);
  return true;
}
static bool parseFloat(const String& s, float& out) {
  char* end = nullptr;
  float v = strtof(s.c_str(), &end);
  if (end == nullptr || *end != '\0') return false;
  out = v;
  return true;
}
static void printOk(const char* msg){ Serial.println(msg); }
static void printErr(const char* msg){ Serial.println(msg); }

// ---- 공개 API ----
void begin(Config* cfg) {
  s_cfg = cfg;
}

void help() {
  Serial.println(F("[CLI] commands:"));
  Serial.println(F("  cfg show|load|save|reset"));
  Serial.println(F("  cfg set gain <float> | slth <int> | zstep <int> | wstep <int> | mode <wheel|zoom|0|1>"));
  Serial.println(F("  haptics on|off"));
  Serial.println(F("  hap min <pct 0..100>"));
  Serial.println(F("  log set <mask(0x..|dec)>"));
  Serial.println(F("  erm load               (ERM fuse loads & cooldown)"));
  Serial.println(F("  factory smoke|full"));
  Serial.println(F("  help"));
}

void poll() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line = toLowerTrim(line);
  if (line.length() == 0) return;

  if (!s_cfg) {
    LOGC(CONFIG, "[CLI] no Config* bound. call MainCLI::begin(&cfg) first.");
    return;
  }

  // ---- cfg show ----
  if (line == "cfg show") {
    ConfigStore::show(*s_cfg);
    return;
  }

  // ---- cfg load ----
  if (line == "cfg load") {
    Config tmp;
    if (ConfigStore::load(tmp)) {
      *s_cfg = tmp;
      ConfigStore::applyToRuntime(*s_cfg);
      printOk("[CLI] cfg loaded & applied");
    } else {
      printErr("[CLI] cfg load failed (defaults may be in use)");
    }
    return;
  }

  // ---- cfg save ----
  if (line == "cfg save") {
    if (ConfigStore::save(*s_cfg)) {
      printOk("[CLI] cfg saved");
    } else {
      printErr("[CLI] cfg save failed");
    }
    return;
  }

  // ---- cfg reset ----
  if (line == "cfg reset") {
    ConfigStore::reset(*s_cfg);
    ConfigStore::applyToRuntime(*s_cfg);
    printOk("[CLI] cfg reset to defaults (not saved)");
    return;
  }

  // ---- cfg set <key> <val> ----
  if (line.startsWith("cfg set ")) {
    // key와 value 분리
    int sp = line.indexOf(' ', 8);
    if (sp < 0) { printErr("[CLI] usage: cfg set <key> <value>"); return; }
    String key = line.substring(8, sp); key.trim();
    String val = line.substring(sp+1); val.trim();

    if (key == "gain") {
      float f;
      if (!parseFloat(val, f)) { printErr("[CLI] gain must be a float"); return; }
      s_cfg->cursor_gain = f;
      ConfigStore::applyToRuntime(*s_cfg);
      Serial.printf("[CLI] gain=%.2f\n", s_cfg->cursor_gain);
      return;
    }
    if (key == "slth") {
      int v;
      if (!parseInt(val, v)) { printErr("[CLI] slth must be an int"); return; }
      s_cfg->slider_thresh = v;
      ConfigStore::applyToRuntime(*s_cfg);
      Serial.printf("[CLI] slth=%d\n", s_cfg->slider_thresh);
      return;
    }
    if (key == "zstep") {
      int v;
      if (!parseInt(val, v)) { printErr("[CLI] zstep must be an int"); return; }
      s_cfg->zoom_step_dv = v;
      ConfigStore::applyToRuntime(*s_cfg);
      Serial.printf("[CLI] zstep=%d\n", s_cfg->zoom_step_dv);
      return;
    }
    if (key == "wstep") {
      int v;
      if (!parseInt(val, v)) { printErr("[CLI] wstep must be an int"); return; }
      s_cfg->wheel_step_dv = v;
      ConfigStore::applyToRuntime(*s_cfg);
      Serial.printf("[CLI] wstep=%d\n", s_cfg->wheel_step_dv);
      return;
    }
    if (key == "mode") {
      uint8_t m = (val=="zoom" || val=="1") ? SL_ZOOM :
                  (val=="wheel"|| val=="0") ? SL_WHEEL : 255;
      if (m == 255) { printErr("[CLI] mode must be wheel|zoom|0|1"); return; }
      s_cfg->initial_mode = m;
      ConfigStore::applyToRuntime(*s_cfg);
      Serial.printf("[CLI] mode=%s\n", (s_cfg->initial_mode==SL_ZOOM?"zoom":"wheel"));
      return;
    }

    printErr("[CLI] unknown key (gain|slth|zstep|wstep|mode)");
    return;
  }

  // ---- haptics on/off ----
  if (line == "haptics on") {
    s_cfg->haptics_on = true;
    HapticsRuntime::setEnabled(true);
    Serial.println("[CLI] haptics ON (not saved)");
    return;
  }
  if (line == "haptics off") {
    s_cfg->haptics_on = false;
    HapticsRuntime::setEnabled(false);
    Serial.println("[CLI] haptics OFF (not saved)");
    return;
  }

  // ---- hap min <pct> ----
  if (line.startsWith("hap min ")) {
    String v = line.substring(8); v.trim();
    int pct;
    if (!parseInt(v, pct)) { printErr("[CLI] hap min requires integer 0..100"); return; }
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    s_cfg->erm_min_pct = static_cast<uint8_t>(pct);
    HapticsPolicy::setErmMinPct(s_cfg->erm_min_pct);
    Serial.printf("[CLI] ERM min duty = %u%% (not saved)\n", s_cfg->erm_min_pct);
    return;
  }

  // ---- log set <mask> ----
  if (line.startsWith("log set ")) {
    String m = line.substring(8); m.trim();
    uint32_t mask;
    if (!parseUint32(m, mask)) { printErr("[CLI] mask must be decimal or 0x.. hex"); return; }
    s_cfg->log_mask = mask;
    Log::setMask(mask);
    Serial.printf("[CLI] log mask = 0x%08lX (not saved)\n", (unsigned long)mask);
    return;
  }

  // ---- erm load ----
  if (line == "erm load") {
    float l=0.f, r=0.f; long cd=0;
    if (HapticsRuntime::getErmFuse(l, r, cd)) {
      Serial.printf("[ERM] loadL=%.2f loadR=%.2f (cooldown=%ldms left)\n", l, r, cd);
    } else {
      printErr("[ERM] fuse info not available");
    }
    return;
  }

  // ---- factory smoke|full ----
  if (line == "factory smoke") {
    FactoryTests::runFactory(FactoryTests::Profile::SMOKE);
    return;
  }
  if (line == "factory full") {
    FactoryTests::runFactory(FactoryTests::Profile::FULL);
    return;
  }

  // ---- help ----
  if (line == "help" || line == "?") {
    help();
    return;
  }

  // 알려지지 않은 명령
  Serial.println("[CLI] unknown command. type 'help' for list.");
}

} // namespace MainCLI
