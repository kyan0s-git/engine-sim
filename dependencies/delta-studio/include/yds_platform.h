#ifndef YDS_PLATFORM_H
#define YDS_PLATFORM_H

// Portability helpers so that code originally written against MSVC builds
// with GCC and Clang on Linux and macOS.

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef _MSC_VER
#include <malloc.h>

inline void *ysAlignedAlloc(size_t size, size_t alignment) {
    return _aligned_malloc(size, alignment);
}

inline void ysAlignedFree(void *p) {
    _aligned_free(p);
}

#else

#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

#ifndef __int64
#define __int64 long long
#endif

inline void *ysAlignedAlloc(size_t size, size_t alignment) {
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    void *p = nullptr;
    if (posix_memalign(&p, alignment, (size == 0) ? alignment : size) != 0) return nullptr;
    return p;
}

inline void ysAlignedFree(void *p) {
    free(p);
}

// Bounds-checked string helpers (subset of MSVC's Annex K functions)
inline int strcpy_s(char *dest, size_t size, const char *src) {
    if (dest == nullptr || size == 0) return 22;
    if (src == nullptr) { dest[0] = '\0'; return 22; }
    std::snprintf(dest, size, "%s", src);
    return 0;
}

template <size_t N>
inline int strcpy_s(char (&dest)[N], const char *src) {
    return strcpy_s(dest, N, src);
}

inline int strcat_s(char *dest, size_t size, const char *src) {
    if (dest == nullptr || size == 0 || src == nullptr) return 22;
    const size_t len = strnlen(dest, size);
    if (len >= size) return 22;
    std::snprintf(dest + len, size - len, "%s", src);
    return 0;
}

template <size_t N>
inline int strcat_s(char (&dest)[N], const char *src) {
    return strcat_s(dest, N, src);
}

inline int vsprintf_s(char *buffer, size_t size, const char *format, va_list args) {
    return std::vsnprintf(buffer, size, format, args);
}

inline int sprintf_s(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int result = std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

template <size_t N>
inline int sprintf_s(char (&buffer)[N], const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int result = std::vsnprintf(buffer, N, format, args);
    va_end(args);
    return result;
}

inline int fopen_s(FILE **file, const char *filename, const char *mode) {
    if (file == nullptr) return 22;
    *file = std::fopen(filename, mode);
    return (*file == nullptr) ? 2 : 0;
}

inline int localtime_s(struct tm *result, const time_t *time) {
    return (localtime_r(time, result) == nullptr) ? 22 : 0;
}

#endif /* _MSC_VER */

#endif /* YDS_PLATFORM_H */
