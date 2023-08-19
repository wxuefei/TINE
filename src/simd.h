#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <immintrin.h>

#define PREFETCHNTA(mem)       _mm_prefetch((char const*)(mem), _MM_HINT_NTA)
#define MOVDQA_LOAD(mem)       _mm_load_si128((__m128i const*)(mem))
#define MOVDQA_STORE(mem, reg) _mm_store_si128((__m128i*)(mem), (__m128i)(reg))
#define MOVDQU_LOAD(mem)       _mm_loadu_si128((__m128i const*)(mem))
#define MOVDQU_STORE(mem, reg) _mm_storeu_si128((__m128i*)(mem), (__m128i)(reg))

#ifdef __cplusplus
}
#endif
