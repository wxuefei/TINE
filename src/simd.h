#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <immintrin.h>

#define PREFETCHT0(mem)      _mm_prefetch((mem), _MM_HINT_T0)
#define MOVUPS_GET(mem)      _mm_loadu_ps((float const*)(mem))
#define MOVUPS_PUT(mem, reg) _mm_storeu_ps((float*)(mem), (__m128)(reg))

#ifdef __cplusplus
}
#endif
