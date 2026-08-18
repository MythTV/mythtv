#ifndef LIBMYTHTV_SSE2_H
#define LIBMYTHTV_SSE2_H

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
#include <QtProcessorDetection>
#endif

#ifdef Q_PROCESSOR_X86
// Check cpuid for SSE2 support on x86 / x86_64
inline bool sse2_check()
{
#ifdef Q_PROCESSOR_X86_64
    return true;
#else
    static int has_sse2 = -1;
    if (has_sse2 != -1)
        return (bool)has_sse2;
    __asm__(
        // -fPIC - we may not clobber ebx/rbx
        "push       %%ebx               \n\t"
        "mov        $1, %%eax           \n\t"
        "cpuid                          \n\t"
        "and        $0x4000000, %%edx   \n\t"
        "shr        $26, %%edx          \n\t"
        "pop        %%ebx               \n\t"
        :"=d"(has_sse2)
        ::"%eax","%ecx"
    );
    return (bool)has_sse2;
#endif
}
#endif //Q_PROCESSOR_X86

#endif // LIBMYTHTV_SSE2_H
