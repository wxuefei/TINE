#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <immintrin.h>

#define PREFETCHT0(mem)        _mm_prefetch((char const*)(mem), _MM_HINT_T0)
#define PREFETCHNTA(mem)       _mm_prefetch((char const*)(mem), _MM_HINT_NTA)
#define MOVUPS_LOAD(mem)       _mm_loadu_ps((float const*)(mem))
#define MOVUPS_STORE(mem, reg) _mm_storeu_ps((float*)(mem), (__m128)(reg))
#define MOVDQU_LOAD(mem)       _mm_loadu_si128((__m128i const*)(mem))
#define MOVDQU_STORE(mem, reg) _mm_storeu_si128((__m128i*)(mem), (__m128i)(reg))

#ifdef __cplusplus
}
#endif
