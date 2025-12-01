#pragma once
//
// BuildOpts.h — 전역 빌드 스위치/상수
//

// NVS 네임스페이스
#ifndef CFG_NVS_NAMESPACE
#define CFG_NVS_NAMESPACE "S3Combo"
#endif

// 기본 로그 마스크(부트 시 적용)
#ifndef CFG_DEFAULT_LOG_MASK
#define CFG_DEFAULT_LOG_MASK 0x0000003F  // 하위 주요 카테고리 ON
#endif

// 컴파일러 경고 강화(가능한 경우)
#if defined(__GNUC__)
  #pragma GCC diagnostic error "-Wall"
  #pragma GCC diagnostic error "-Wextra"
  #pragma GCC diagnostic error "-Wshadow"
  #pragma GCC diagnostic error "-Wdouble-promotion"
  #pragma GCC diagnostic error "-Wformat"
#endif
