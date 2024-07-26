#pragma once
#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <intrin.h>
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__)

struct cpu_regs
{
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
};

union cpu_info
{
    struct cpu_regs s;
    unsigned int a[4];
};

#if defined __has_builtin
#if __has_builtin(__builtin_cpu_supports)
#define BUILTIN_CPU_SUPPORTS
#endif
#endif

#if !defined(BUILTIN_CPU_SUPPORTS)
static inline struct cpu_regs cpu_id(unsigned int i)
{
    union cpu_info regs;
#if defined(_WIN32)
    __cpuid((int*)regs.a, (int)i);
#elif defined(__cpuid)
    __cpuid(i, regs.s.eax, regs.s.ebx, regs.s.ecx, regs.s.edx);
#else
    /* ECX is set to zero for CPUID function 4 */
    __asm__ __volatile__("cpuid" : "=a" (regs.s.eax), "=b" (regs.s.ebx), "=c" (regs.s.ecx), "=d" (regs.s.edx) : "a" (i), "c" (0));
#endif
    return regs.s;
}
#endif

static inline bool supports_avx()
{
#if defined(BUILTIN_CPU_SUPPORTS)
    return __builtin_cpu_supports("avx");
#else
    struct cpu_regs regs = cpu_id(1);
    return (regs.ecx & (1 << 28)) != 0 && (regs.ecx & (1 << 27)) != 0 && (regs.ecx & (1 << 26)) != 0;
#endif
}

static inline bool supports_avx2()
{
#if defined(BUILTIN_CPU_SUPPORTS)
    return __builtin_cpu_supports("avx2");
#else
    struct cpu_regs regs = cpu_id(7);
    return (regs.ebx & (1 << 5)) != 0;
#endif
}

static inline bool check_xcr0_ymm()
{
    uint32_t xcr0;
#if defined(_MSC_VER)
    xcr0 = (uint32_t)_xgetbv(0);  /* min VS2010 SP1 compiler is required */
#else
    __asm__ __volatile__("xgetbv" : "=a" (xcr0) : "c" (0) : "%edx");
#endif
    return ((xcr0 & 6) == 6); /* checking if xmm and ymm state are enabled in XCR0 */
}

#else

static inline bool supports_avx()
{
    return false;
}

static inline bool supports_avx2()
{
    return false;
}

static inline bool check_xcr0_ymm()
{
    return false;
}

#endif
