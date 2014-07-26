////////////////////////////////////////////////////////////////////////////////
///
/// gcc version of the x86 CPU detect routine.
///
/// This file is to be compiled on any platform with the GNU C compiler.
/// Compiler. Please see 'cpu_detect_x86_win.cpp' for the x86 Windows version 
/// of this file.
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date$
// File revision : $Revision$
//
// $Id$
//
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
////////////////////////////////////////////////////////////////////////////////

#include <stdexcept>
#include <string>
#include "cpu_detect.h"

#ifndef __GNUC__
#error wrong platform - this source code file is for the GNU C compiler.
#endif

#include "config.h"

using namespace std;

#include <stdio.h>
//////////////////////////////////////////////////////////////////////////////
//
// processor instructions extension detection routines
//
//////////////////////////////////////////////////////////////////////////////


// Flag variable indicating whick ISA extensions are disabled (for debugging)
static uint _dwDisabledISA = 0x00;      // 0xffffffff; //<- use this to disable all extensions

// Disables given set of instruction extensions. See SUPPORT_... defines.
void disableExtensions(uint dwDisableMask)
{
    _dwDisabledISA = dwDisableMask;
}

#if ARCH_X86

#if ARCH_X86_64
#  define REG_b "rbx"
#  define REG_S "rsi"
#else
#  define REG_b "ebx"
#  define REG_S "esi"
#endif

/* ebx saving is necessary for PIC. gcc seems unable to see it alone */
#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("mov %%"REG_b", %%"REG_S"\n\t"\
         "cpuid\n\t"\
         "xchg %%"REG_b", %%"REG_S\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

/* Function to test if multimedia instructions are supported...  */
static int mm_support(void)
{
    int rval = 0;
    int eax, ebx, ecx, edx;
    int max_std_level, max_ext_level, std_caps=0, ext_caps=0;
    long a, c;

    __asm__ __volatile__ (
                          /* See if CPUID instruction is supported ... */
                          /* ... Get copies of EFLAGS into eax and ecx */
                          "pushf\n\t"
                          "pop %0\n\t"
                          "mov %0, %1\n\t"

                          /* ... Toggle the ID bit in one copy and store */
                          /*     to the EFLAGS reg */
                          "xor $0x200000, %0\n\t"
                          "push %0\n\t"
                          "popf\n\t"

                          /* ... Get the (hopefully modified) EFLAGS */
                          "pushf\n\t"
                          "pop %0\n\t"
                          : "=a" (a), "=c" (c)
                          :
                          : "cc"
                          );

    if (a == c)
        return 0; /* CPUID not supported */

    cpuid(0, max_std_level, ebx, ecx, edx);

    if(max_std_level >= 1){
        cpuid(1, eax, ebx, ecx, std_caps);
        if (std_caps & (1<<23))
            rval |= MM_MMX;
        if (std_caps & (1<<25))
            rval |= MM_MMXEXT | MM_SSE;
        if (std_caps & (1<<26))
            rval |= MM_SSE2;
        if (ecx & 1)
            rval |= MM_SSE3;
        if (ecx & 0x00000200 )
            rval |= MM_SSSE3;
        if (ecx & 0x00080000 )
            rval |= MM_SSE4;
        if (ecx & 0x00100000 )
            rval |= MM_SSE42;
    }

    cpuid(0x80000000, max_ext_level, ebx, ecx, edx);

    if(max_ext_level >= 0x80000001){
        cpuid(0x80000001, eax, ebx, ecx, ext_caps);
        if (ext_caps & (1<<31))
            rval |= MM_3DNOW;
        if (ext_caps & (1<<30))
            rval |= MM_3DNOWEXT;
        if (ext_caps & (1<<23))
            rval |= MM_MMX;
    }

    cpuid(0, eax, ebx, ecx, edx);
    if (       ebx == 0x68747541 &&
               edx == 0x69746e65 &&
               ecx == 0x444d4163) {
        /* AMD */
        if(ext_caps & (1<<22))
            rval |= MM_MMXEXT;
    } else if (ebx == 0x746e6543 &&
               edx == 0x48727561 &&
               ecx == 0x736c7561) {  /*  "CentaurHauls" */
        /* VIA C3 */
        if(ext_caps & (1<<24))
          rval |= MM_MMXEXT;
    } else if (ebx == 0x69727943 &&
               edx == 0x736e4978 &&
               ecx == 0x64616574) {
        /* Cyrix Section */
        /* See if extended CPUID level 80000001 is supported */
        /* The value of CPUID/80000001 for the 6x86MX is undefined
           according to the Cyrix CPU Detection Guide (Preliminary
           Rev. 1.01 table 1), so we'll check the value of eax for
           CPUID/0 to see if standard CPUID level 2 is supported.
           According to the table, the only CPU which supports level
           2 is also the only one which supports extended CPUID levels.
        */
        if (eax < 2)
            return rval;
        if (ext_caps & (1<<24))
            rval |= MM_MMXEXT;
    }
#if 0
   av_log(NULL, AV_LOG_DEBUG, "%s%s%s%s%s%s\n",
        (rval&MM_MMX) ? "MMX ":"",
        (rval&MM_MMXEXT) ? "MMX2 ":"",
        (rval&MM_SSE) ? "SSE ":"",
        (rval&MM_SSE2) ? "SSE2 ":"",
        (rval&MM_3DNOW) ? "3DNow ":"",
        (rval&MM_3DNOWEXT) ? "3DNowExt ":"");
#endif
    return rval;
}
#endif

/// Checks which instruction set extensions are supported by the CPU.
uint detectCPUextensions(void)
{
#if !ARCH_X86
    return 0; // always disable extensions on non-x86 platforms.
#else
    return mm_support() & ~_dwDisabledISA;
#endif
}
