#pragma once

#ifndef HOT_FUNC
#define HOT_FUNC
#endif

#ifndef COLD_FUNC
#define COLD_FUNC
#endif

#ifndef FORCE_INLINE
#define FORCE_INLINE
#endif

#ifndef ENABLE_BRANCH_HINTS
  #define UNLIKELY(x) (x)
  #define LIKELY(x) (x)
#else
  #define UNLIKELY(x) __builtin_expect(!!(x), 0)
  #define LIKELY(x) __builtin_expect(!!(x), 1)
#endif

#ifndef ENABLE_RESTRICT
  #define RESTRICT
#else
  #define RESTRICT __restrict__
#endif

#ifndef ENABLE_PREFETCH
  #define PREFETCH(addr)
#else
  #define PREFETCH(addr) __builtin_prefetch((addr), 0, 3)
#endif
