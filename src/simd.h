#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <emmintrin.h>
#include <immintrin.h>

#define PREFETCHT0(mem)        _mm_prefetch((char const*)(mem), _MM_HINT_T0)
#define PREFETCHNTA(mem)       _mm_prefetch((char const*)(mem), _MM_HINT_NTA)
#define MOVDQA_LOAD(mem)       _mm_load_si128((__m128i const*)(mem))
#define MOVDQA_STORE(mem, reg) _mm_store_si128((__m128i*)(mem), (__m128i)(reg))
#define MOVDQU_LOAD(mem)       _mm_loadu_si128((__m128i const*)(mem))
#define MOVDQU_STORE(mem, reg) _mm_storeu_si128((__m128i*)(mem), (__m128i)(reg))
// long long: stupid workaround because intel intrinsics wont
// provide __int64 anymore. not sure if this is even portable
#define MOVNTI_64(mem, i) _mm_stream_si64((long long*)(mem), (long)(i))
#define MOVNTI_32(mem, i) _mm_stream_si32((int*)(mem), (int)(i))
#define MOVNTDQ_STORE(mem, reg) \
  _mm_stream_si128((__m128i*)(mem), (__m128i)(reg))

#ifdef __cplusplus
}
#endif
