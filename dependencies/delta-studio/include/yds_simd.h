#ifndef YDS_SIMD_H
#define YDS_SIMD_H

// Minimal SIMD abstraction for delta's math library.
//
// On x86/x64 the native SSE intrinsics are used. On other architectures
// (e.g. Apple Silicon / ARM64 Linux) a small portable implementation of the
// handful of intrinsics used by the engine is provided instead. The math in
// delta is not on the simulation's hot path so a scalar fallback is fine.

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__) || defined(__SSE__)
#define YDS_SIMD_SSE 1
#include <xmmintrin.h>

inline float ysSimdGet(const __m128 &v, int i) {
    union { __m128 v; float f[4]; } u;
    u.v = v;
    return u.f[i];
}

#else
#define YDS_SIMD_SSE 0

#include <cmath>
#include <cstdint>
#include <cstring>

struct alignas(16) __m128 {
    float f[4];
};

#ifndef _MM_SHUFFLE
#define _MM_SHUFFLE(fp3, fp2, fp1, fp0) (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))
#endif

inline float ysSimdGet(const __m128 &v, int i) {
    return v.f[i];
}

inline __m128 _mm_set_ps(float w, float z, float y, float x) {
    __m128 r = { { x, y, z, w } };
    return r;
}

inline __m128 _mm_add_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) r.f[i] = a.f[i] + b.f[i];
    return r;
}

inline __m128 _mm_sub_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) r.f[i] = a.f[i] - b.f[i];
    return r;
}

inline __m128 _mm_mul_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) r.f[i] = a.f[i] * b.f[i];
    return r;
}

inline __m128 _mm_div_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) r.f[i] = a.f[i] / b.f[i];
    return r;
}

inline __m128 _mm_max_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) r.f[i] = (a.f[i] > b.f[i]) ? a.f[i] : b.f[i];
    return r;
}

inline __m128 _mm_min_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) r.f[i] = (a.f[i] < b.f[i]) ? a.f[i] : b.f[i];
    return r;
}

inline __m128 _mm_sqrt_ps(const __m128 &a) {
    __m128 r;
    for (int i = 0; i < 4; ++i) r.f[i] = std::sqrt(a.f[i]);
    return r;
}

inline __m128 _mm_and_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) {
        uint32_t ua, ub;
        std::memcpy(&ua, &a.f[i], sizeof(float));
        std::memcpy(&ub, &b.f[i], sizeof(float));
        const uint32_t ur = ua & ub;
        std::memcpy(&r.f[i], &ur, sizeof(float));
    }
    return r;
}

inline __m128 _mm_or_ps(const __m128 &a, const __m128 &b) {
    __m128 r;
    for (int i = 0; i < 4; ++i) {
        uint32_t ua, ub;
        std::memcpy(&ua, &a.f[i], sizeof(float));
        std::memcpy(&ub, &b.f[i], sizeof(float));
        const uint32_t ur = ua | ub;
        std::memcpy(&r.f[i], &ur, sizeof(float));
    }
    return r;
}

inline __m128 _mm_shuffle_ps(const __m128 &a, const __m128 &b, unsigned int mask) {
    __m128 r;
    r.f[0] = a.f[(mask >> 0) & 0x3];
    r.f[1] = a.f[(mask >> 2) & 0x3];
    r.f[2] = b.f[(mask >> 4) & 0x3];
    r.f[3] = b.f[(mask >> 6) & 0x3];
    return r;
}

#ifndef _MM_TRANSPOSE4_PS
#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3)                \
    do {                                                         \
        const __m128 _t[4] = { (row0), (row1), (row2), (row3) }; \
        for (int _i = 0; _i < 4; ++_i) {                         \
            (row0).f[_i] = _t[_i].f[0];                          \
            (row1).f[_i] = _t[_i].f[1];                          \
            (row2).f[_i] = _t[_i].f[2];                          \
            (row3).f[_i] = _t[_i].f[3];                          \
        }                                                        \
    } while (0)
#endif

#endif

#endif /* YDS_SIMD_H */
