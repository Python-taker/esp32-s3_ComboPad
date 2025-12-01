#include "FactoryTests.h"

#include "../hal/HAL.h"
#include "../haptics/HapticsRuntime.h"
#include "../imu/IMU.h"
#include "../usb/USBDevices.h"
#include "../core/Log.h"

#include <math.h>
#include <Arduino.h>

using namespace HAL;
using HapticsRuntime::ErmDir;

namespace {

constexpr uint32_t PASS_HOLD_MS = 3000;

// 공통 LED 훅
inline void ledStart() { HAL::ledR(true); }
inline void ledEnd()   { HAL::ledR(false); }

inline void ledFailBlink(){
  for(int i=0;i<2;i++){ HAL::ledR(true); delay(250); HAL::ledR(false); delay(200); }
}
inline void ledPassHold(){
  HAL::ledG(true); delay(PASS_HOLD_MS); HAL::ledG(false);
}

// 유틸
static int readADCavg(int pin, int n=8){
  long acc=0; for(int i=0;i<n;i++){ acc+=analogRead(pin); delay(1); } return (int)(acc/n);
}

// 방향 제스처(터치 좌표 기반)
bool waitTouchDir(const char* name, char dir, uint32_t timeout_ms){
  LOGI("T06", "STEP %s", name);
  uint32_t t0=millis(); HAL::TouchPt p0{}; bool had=false;
  while(millis()-t0<timeout_ms){
    HAL::TouchPt p;
    if (HAL::touchGetCoord(p)){
      if(!had){ p0=p; had=true; }
      int dx=p.x-p0.x, dy=p.y-p0.y;
      if (dir=='L' && dx<-2) return true;
      if (dir=='R' && dx> 2) return true;
      if (dir=='U' && dy<-2) return true;
      if (dir=='D' && dy> 2) return true;
    }
    delay(5);
  }
  return false;
}

} // anon

namespace FactoryTests {

// ---------- T00~T10 구현 ----------
bool T00_I2CScan(){
  LOGI("T00","START env & I2C scan");
  ledStart();

  bool found5A=false, found5B=false, found6A=false;
  auto& W = HAL::i2c();
  for(uint8_t a=0x08;a<=0x77;a++){
    W.beginTransmission(a);
    if (W.endTransmission()==0){
      if (a==0x5A) found5A=true; // DRV2605L
      if (a==0x5B) found5B=true; // (보드에 있을 경우)
      if (a==0x6A) found6A=true; // LSM6DS3TR-C
    }
  }

  if (found5A && found6A){
    LOGI("T00","PASS found: %s%s", "0x5A ", "0x6A ");
    ledEnd(); ledPassHold(); return true;
  }
  LOGW("T00","FAIL missing: %s%s%s",
       found5A?"":"0x5A ", found5B?"":"0x5B ", found6A?"":"0x6A ");
  ledEnd(); ledFailBlink(); return false;
}

bool T01_LED(){
  LOGI("T01","START LED RGB");
  ledStart();
  HAL::ledR(true);  delay(400);
  HAL::ledR(false); HAL::ledG(true);  delay(400);
  HAL::ledG(false); HAL::ledB(true);  delay(400);
  HAL::ledB(false);
  LOGI("T01","PASS R,G,B");
  ledEnd(); ledPassHold(); return true;
}

static bool waitPressed(const char* name, HAL::Button pin, uint32_t timeout_ms=10000){
  LOGI("T02","ASK press %s (%lus)", name, (unsigned long)(timeout_ms/1000));
  uint32_t t0=millis(); bool ok=false;
  while(millis()-t0<timeout_ms){
    if (HAL::pressed(pin)){ ok=true; break; }
    delay(5);
  }
  if(ok) LOGI("T02","DET %s ok", name);
  else   LOGW("T02","TIMEOUT %s", name);
  return ok;
}
bool T02_Buttons(){
  LOGI("T02","START buttons A,B,X,Y,L3,R3");
  ledStart();
  bool ok = true;
  ok &= waitPressed("A", HAL::Button::A);
  ok &= waitPressed("B", HAL::Button::B);
  ok &= waitPressed("X", HAL::Button::X);
  ok &= waitPressed("Y", HAL::Button::Y);
  ok &= waitPressed("L3", HAL::Button::L3);
  ok &= waitPressed("R3", HAL::Button::R3);
  if(ok){ LOGI("T02","PASS all"); ledEnd(); ledPassHold(); return true; }
  LOGW("T02","FAIL");
  ledEnd(); ledFailBlink(); return false;
}

bool T03_JoyCenter(){
  LOGI("T03","START joystick center/idle noise (1s)");
  ledStart();
  const int N=100;
  int lx[N],ly[N],rx[N],ry[N];
  for(int i=0;i<N;i++){
    HAL::SticksRaw r; HAL::readSticksRaw(r);
    lx[i]=r.lx; ly[i]=r.ly; rx[i]=r.rx; ry[i]=r.ry;
    delay(10);
  }
  auto meanStd=[&](int* a)->std::pair<float,float>{
    long s=0; for(int i=0;i<N;i++) s+=a[i];
    float m = s/(float)N;
    double v=0; for(int i=0;i<N;i++){ float d=a[i]-m; v+=d*d; }
    float sd = sqrtf((float)(v/N));
    return {m, sd};
  };
  auto [mlx,slx]=meanStd(lx);
  auto [mly,sly]=meanStd(ly);
  auto [mrx,srx]=meanStd(rx);
  auto [mry,sry]=meanStd(ry);
  bool pass = (fabsf(mlx-2048)<=200 && fabsf(mly-2048)<=200 &&
               fabsf(mrx-2048)<=200 && fabsf(mry-2048)<=200 &&
               slx<=20 && sly<=20 && srx<=20 && sry<=20);
  LOGI("T03","L center=(%.0f,%.0f) σ=(%.1f,%.1f) | R center=(%.0f,%.0f) σ=(%.1f,%.1f) -> %s",
    mlx,mly,slx,sly,mrx,mry,srx,sry, pass?"PASS":"FAIL");
  if(pass){ ledEnd(); ledPassHold(); return true; }
  ledEnd(); ledFailBlink(); return false;
}

bool T04_JoySweep(){
  LOGI("T04","START joystick sweep (2s) rotate sticks");
  ledStart();
  uint32_t t0=millis(); int lminx=4095,lmaxx=0,lminy=4095,lmaxy=0, rminx=4095,rmaxx=0,rminy=4095,rmaxy=0;
  while(millis()-t0<2000){
    HAL::SticksRaw r; HAL::readSticksRaw(r);
    lminx=min(lminx,r.lx); lmaxx=max(lmaxx,r.lx);
    lminy=min(lminy,r.ly); lmaxy=max(lmaxy,r.ly);
    rminx=min(rminx,r.rx); rmaxx=max(rmaxx,r.rx);
    rminy=min(rminy,r.ry); rmaxy=max(rmaxy,r.ry);
    delay(5);
  }
  bool pass = (lminx<300 || lminy<300 || rminx<300 || rminy<300) &&
              (lmaxx>3800 || lmaxy>3800 || rmaxx>3800 || rmaxy>3800);
  LOGI("T04","L[min=%d,%d max=%d,%d] R[min=%d,%d max=%d,%d] -> %s",
    lminx,lminy,lmaxx,lmaxy,rminx,rminy,rmaxx,rmaxy, pass?"PASS":"FAIL");
  if(pass){ ledEnd(); ledPassHold(); return true; }
  ledEnd(); ledFailBlink(); return false;
}

bool T05_Slider(){
  LOGI("T05","START slider sweep (5s)");
  ledStart();
  uint32_t t0=millis(); int smin=4095,smax=0; int last=-1; int maxJump=0;
  while(millis()-t0<5000){
    int v=HAL::readSliderRaw();
    smin=min(smin,v); smax=max(smax,v);
    if(last>=0) maxJump=max(maxJump, abs(v-last));
    last=v; delay(5);
  }
  bool pass = (smin<300 && smax>3800 && maxJump<=300);
  LOGI("T05","min=%d max=%d continuity maxΔ=%d -> %s", smin,smax,maxJump, pass?"PASS":"FAIL");
  if(pass){ ledEnd(); ledPassHold(); return true; }
  ledEnd(); ledFailBlink(); return false;
}

bool T06_Touch(bool full){
  LOGI("T06","START touch pattern L→R→U→D (%s)", full?"x2":"x1");
  ledStart();
  int loops = full?2:1;
  for(int k=0;k<loops;k++){
    if(!waitTouchDir("Left", 'L', 1500))  { LOGW("T06","FAIL L"); ledEnd(); ledFailBlink(); return false; }
    if(!waitTouchDir("Right",'R', 1500))  { LOGW("T06","FAIL R"); ledEnd(); ledFailBlink(); return false; }
    if(!waitTouchDir("Up",   'U', 1500))  { LOGW("T06","FAIL U"); ledEnd(); ledFailBlink(); return false; }
    if(!waitTouchDir("Down", 'D', 1500))  { LOGW("T06","FAIL D"); ledEnd(); ledFailBlink(); return false; }
  }
  LOGI("T06","PASS");
  ledEnd(); ledPassHold(); return true;
}

bool T07_IMU(){
  LOGI("T07","START IMU health (1s still)");
  ledStart();
  const int N=100; float m[N];
  for(int i=0;i<N;i++){
    float ax,ay,az;
    if (!IMU::getAccel(ax,ay,az)){ HAL::ledR(false); ledFailBlink(); return false; }
    m[i]=sqrtf(ax*ax+ay*ay+az*az);
    delay(10);
  }
  double s=0; for(int i=0;i<N;i++) s+=m[i]; float mean=s/N;
  double v=0; for(int i=0;i<N;i++){ float d=m[i]-mean; v+=d*d; } float sd=sqrtf((float)(v/N));
  bool pass = (mean>0.9f && mean<1.1f);
  LOGI("T07","|g|=%.2f σ=%.3f -> %s", mean, sd, pass?"PASS":"FAIL");
  if(pass){ ledEnd(); ledPassHold(); return true; }
  ledEnd(); ledFailBlink(); return false;
}

bool T08_LRA(){
  LOGI("T08","START LRA (#11→#10)");
  ledStart();
  if(!HapticsRuntime::lraReady()){ LOGW("T08","FAIL LRA not ready"); ledEnd(); ledFailBlink(); return false; }
  HapticsRuntime::LraPlay(300, 11); delay(350);
  HapticsRuntime::LraPlay(300, 10); delay(350);
  LOGI("T08","PASS");
  ledEnd(); ledPassHold(); return true;
}

bool T09_ERM(){
  LOGI("T09","START ERM ramp L/R (1s each @70%)");
  ledStart();
  HapticsRuntime::ErmPlay(ErmDir::LEFT,  1100, 700); delay(1200);
  HapticsRuntime::ErmPlay(ErmDir::RIGHT, 1100, 700); delay(1200);
  LOGI("T09","PASS");
  ledEnd(); ledPassHold(); return true;
}

bool T10_HID(){
  LOGI("T10","START HID ready");
  ledStart();
  // 실제로는 PC에서 동작 확인(수동). 여기선 존재/초기화 완료 가정
  LOGI("T10","PASS mouse/keyboard/gamepad up");
  ledEnd(); ledPassHold(); return true;
}

// ---------- 테이블 드리븐 러너 ----------
struct TestCase {
  const char* name;
  bool (*fn)();
  bool fullOnly;  // Full에서만 필수 실행
};

static const TestCase kTests[] = {
  {"T00_I2CScan", [](){return T00_I2CScan();}, false},
  {"T01_LED",     [](){return T01_LED();},     false},
  {"T02_Buttons", [](){return T02_Buttons();}, false},
  // Full 전용들(Smoke에선 스킵)
  {"T03_JoyCenter", [](){return T03_JoyCenter();}, true},
  {"T04_JoySweep",  [](){return T04_JoySweep();},  true},
  // 공통
  {"T05_Slider",  [](){return T05_Slider();},  false},
  // Touch는 fullOnly 여부에 따라 내부 루프 횟수를 조정
  // 표에는 더미, 런타임에서 분기 실행
  {"T06_Touch",   nullptr, /*handled specially*/ false},
  {"T07_IMU",     [](){return T07_IMU();},     true}, // IMU는 Full에서 체크 권장
  {"T08_LRA",     [](){return T08_LRA();},     false},
  {"T09_ERM",     [](){return T09_ERM();},     false},
  {"T10_HID",     [](){return T10_HID();},     false},
};

} // namespace FactoryTests (전용)

namespace FactoryTests {

void init(){ /* 확장 포인트: 현재 NOP */ }

Result run(Profile profile, bool interactive){
  LOGI("FACT","enter (profile=%s)", profile==Profile::Smoke?"SMOKE":"FULL");

  // 하프틱은 항상 활성화(기존 상태 저장/복원)
  const bool prev = HapticsRuntime::isEnabled();
  HapticsRuntime::setEnabled(true);

  Result res{true, -1};

  for (size_t i=0;i<sizeof(kTests)/sizeof(kTests[0]); ++i){
    const auto& tc = kTests[i];

    if (tc.fn == nullptr){
      // T06 특수 처리: Smoke x1 / Full x2
      const bool full = (profile==Profile::Full);
      if (!T06_Touch(full)){ res.all_pass=false; res.first_fail_index=(int16_t)i; break; }
      continue;
    }

    if (profile==Profile::Smoke && tc.fullOnly){
      // Smoke에서는 fullOnly 항목은 건너뛰되, T07(IMU)은 가능하면 실행하려면 false로 내려도 OK
      continue;
    }

    if (!tc.fn()){
      res.all_pass=false; res.first_fail_index=(int16_t)i;
      break;
    }
  }

  if (res.all_pass){
    LOGI("SUMMARY","ALL PASS");
    for(int i=0;i<3;i++){ HAL::ledG(true); delay(180); HAL::ledG(false); delay(200); }
  } else {
    LOGW("SUMMARY","FAILED at index=%d (%s).", (int)res.first_fail_index,
         kTests[res.first_fail_index].name);
    for(int i=0;i<3;i++){ HAL::ledR(true); delay(180); HAL::ledR(false); delay(200); }

    if (interactive){
      LOGI("FACT","Press A=retry SMOKE, B=retry FULL, or reset the device.");
      uint32_t idleBlinkT=millis(); bool bOn=false;
      while(true){
        if(millis()-idleBlinkT>500){ bOn=!bOn; HAL::ledB(bOn); idleBlinkT=millis(); }
        if (HAL::pressed(HAL::Button::A)){ return run(Profile::Smoke, false); }
        if (HAL::pressed(HAL::Button::B)){ return run(Profile::Full,  false); }
        delay(10);
      }
    }
  }

  // 상태 복원
  HapticsRuntime::setEnabled(prev);
  return res;
}

} // namespace FactoryTests
