#include "Log.h"
#include "BuildOpts.h"

namespace Log {

volatile uint32_t g_mask = CFG_DEFAULT_LOG_MASK;

void setMask(uint32_t m){ g_mask = m; }
uint32_t getMask(){ return g_mask; }

void vprint(uint32_t, const char* fmt, va_list ap){
  // 카테고리 프리픽스는 상위에서 넣어준다(매크로로)
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  Serial.println(buf);
}

void print(uint32_t cat, const char* fmt, ...){
  if (!(g_mask & cat)) return;
  va_list ap; va_start(ap, fmt);
  vprint(cat, fmt, ap);
  va_end(ap);
}

} // namespace Log
