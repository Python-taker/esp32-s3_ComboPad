#include "ConfigStore.h"
#include "BuildOpts.h"
#include "Version.h"
#include "Log.h"

#include <Preferences.h>

namespace ConfigStore {

const char* KEY_VER     = "cfg.ver";
const char* KEY_GAIN    = "gain";
const char* KEY_SLTH    = "slth";
const char* KEY_ZSTEP   = "zstep";
const char* KEY_WSTEP   = "wstep";
const char* KEY_MODE    = "mode";
const char* KEY_HAPT    = "hap";
const char* KEY_ERMPCT  = "ermpct";
const char* KEY_LOGMASK = "logmask";

static IRuntimeHooks* s_hooks = nullptr;

void setHooks(IRuntimeHooks* hooks){ s_hooks = hooks; }

// 기본값 채우기
static void fillDefaults(Config& c){
  c.version       = CFG_SCHEMA_VERSION;
  c.cursor_gain   = 18.0f;
  c.slider_thresh = 25;
  c.zoom_step_dv  = 150;
  c.wheel_step_dv = 40;
  c.initial_mode  = SL_WHEEL;
  c.haptics_on    = true;
  c.erm_min_pct   = 50;
  c.log_mask      = CFG_DEFAULT_LOG_MASK;
}

bool migrateIfNeeded(Config& cfg, uint16_t storedVer){
  // 현재는 스키마 1 → (미래에 2,3...) 로 확장
  if (storedVer == CFG_SCHEMA_VERSION) return true;

  // 간단 정책: 더 낮은 버전이면 기본값으로 리셋 후, 일부 필드만 가져오기 등
  LOGC(CONFIG, "Config migration required: stored=%u → current=%u (reset to defaults)",
       (unsigned)storedVer, (unsigned)CFG_SCHEMA_VERSION);
  Config def; fillDefaults(def);

  // 예: v0 → v1 이면 log_mask 필드가 새로 생겼다 가정하고 안전히 기본값
  // 필요 시 기존 NVS 키 값을 다시 읽어 부분 이식 가능. 지금은 단순 리셋.
  cfg = def;
  return true;
}

bool load(Config& out){
  Preferences prefs;
  if (!prefs.begin(CFG_NVS_NAMESPACE, /*readOnly=*/true)){
    LOGC(CONFIG, "[NVS] open(read) failed");
    fillDefaults(out);
    return false;
  }

  // 버전 키가 없으면 완전 신규 → 기본값
  if (!prefs.isKey(KEY_VER)){
    LOGC(CONFIG, "[NVS] no keys, using defaults");
    fillDefaults(out);
    prefs.end();
    return true;
  }

  // 버전 먼저
  uint16_t ver = prefs.getUShort(KEY_VER, 0);
  // 본문
  Config c;
  c.version       = ver;
  c.cursor_gain   = prefs.getFloat(KEY_GAIN,    18.0f);
  c.slider_thresh = prefs.getInt(KEY_SLTH,      25);
  c.zoom_step_dv  = prefs.getInt(KEY_ZSTEP,     150);
  c.wheel_step_dv = prefs.getInt(KEY_WSTEP,     40);
  c.initial_mode  = (uint8_t)prefs.getUChar(KEY_MODE,   SL_WHEEL);
  c.haptics_on    = prefs.getBool(KEY_HAPT,     true);
  c.erm_min_pct   = prefs.getUChar(KEY_ERMPCT,  50);
  c.log_mask      = prefs.getULong(KEY_LOGMASK, CFG_DEFAULT_LOG_MASK);
  prefs.end();

  // 마이그레이션
  if (!migrateIfNeeded(c, ver)){
    // 실패 시 안전하게 defaults 로
    fillDefaults(out);
    return false;
  }

  // 보정: 범위 클램프(미래에 잘못된 값 방어)
  if (c.erm_min_pct > 100) c.erm_min_pct = 100;
  if (c.initial_mode != SL_WHEEL && c.initial_mode != SL_ZOOM) c.initial_mode = SL_WHEEL;

  out = c;
  LOGC(CONFIG, "[NVS] loaded (ver=%u)", (unsigned)out.version);
  return true;
}

bool save(const Config& in){
  Preferences prefs;
  if (!prefs.begin(CFG_NVS_NAMESPACE, /*readOnly=*/false)){
    LOGC(CONFIG, "[NVS] open(write) failed");
    return false;
  }

  // 현재 스키마 버전으로 강제 저장
  prefs.putUShort(KEY_VER,     CFG_SCHEMA_VERSION);
  prefs.putFloat (KEY_GAIN,    in.cursor_gain);
  prefs.putInt   (KEY_SLTH,    in.slider_thresh);
  prefs.putInt   (KEY_ZSTEP,   in.zoom_step_dv);
  prefs.putInt   (KEY_WSTEP,   in.wheel_step_dv);
  prefs.putUChar (KEY_MODE,    in.initial_mode);
  prefs.putBool  (KEY_HAPT,    in.haptics_on);
  prefs.putUChar (KEY_ERMPCT,  in.erm_min_pct);
  prefs.putULong (KEY_LOGMASK, in.log_mask);

  prefs.end();
  LOGC(CONFIG, "[NVS] saved");
  return true;
}

void reset(Config& out){
  fillDefaults(out);
  LOGC(CONFIG, "[CFG] reset to defaults (not saved yet)");
}

void show(const Config& c){
  LOGC(CONFIG, "ver=%u gain=%.2f slth=%d zstep=%d wstep=%d mode=%s hap=%s ermMin=%u%% logmask=0x%08lX",
       (unsigned)c.version, c.cursor_gain, c.slider_thresh, c.zoom_step_dv, c.wheel_step_dv,
       (c.initial_mode==SL_ZOOM?"zoom":"wheel"),
       (c.haptics_on?"on":"off"), c.erm_min_pct, (unsigned long)c.log_mask);
}

void applyToRuntime(const Config& c){
  if (!s_hooks){
    LOGC(CONFIG, "[CFG] applyToRuntime skipped (no hooks)");
    return;
  }
  s_hooks->onCursorGain(c.cursor_gain);
  s_hooks->onSliderParams(c.slider_thresh, c.zoom_step_dv, c.wheel_step_dv);
  s_hooks->onInitialMode(c.initial_mode);
  s_hooks->onHapticsEnable(c.haptics_on);
  s_hooks->onErmMinPct(c.erm_min_pct);
  s_hooks->onLogMask(c.log_mask);
  LOGC(CONFIG, "[CFG] applied to runtime");
}

} // namespace ConfigStore
