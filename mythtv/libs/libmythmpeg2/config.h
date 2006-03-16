/* Include config options from main configure script.         */
#include "../../config.h"

/* just uses ARCH_X86                                         */
#ifdef ARCH_X86_64
#define ARCH_X86
#endif

/* Enable CPU-specific optimizations.                         */
#define ACCEL_DETECT

/* The __builtin_expect function is used where available      */
/* (GCC 2.96.x and above).                                    */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 95)
#define HAVE_BUILTIN_EXPECT
#endif

/* Translate avcodec's ARCH_POWERPC into mpeg2's ARCH_PPC.    */
#ifdef ARCH_POWERPC
#define ARCH_PPC
#endif

/* Some detection code needs to know the return type of a     */
/* signal handler.                                            */
#define RETSIGTYPE void

/* Set the maximum alignment for variables.                   */
#define ATTRIBUTE_ALIGNED_MAX 64
