// -*- c++ -*-
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2001 Tom Barry.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//	This file is subject to the terms of the GNU General Public License as
//	published by the Free Software Foundation.  A copy of this license is
//	included with this software distribution in the file COPYING.  If you
//	do not have a copy, you may obtain a copy by writing to the Free
//	Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//	This software is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details
//
/////////////////////////////////////////////////////////////////////////////

#include "x86-64_macros.inc"
#include "mangle.h"

#if !defined(MASKS_DEFINED)
#define MASKS_DEFINED
static const int64_t __attribute__((__used__)) YMask        = 0x00ff00ff00ff00ffull; // to keep only luma
static const int64_t __attribute__((__used__)) UVMask       = 0xff00ff00ff00ff00ull; // to keep only chroma
static const int64_t __attribute__((__used__)) ShiftMask    = 0xfefffefffefffeffull; // to avoid shifting chroma to luma
static const int64_t __attribute__((__used__)) QW256        = 0x0100010001000100ull; // 4 256's

static int64_t __attribute__((__used__)) MaxComb;
static int64_t __attribute__((__used__)) MotionThreshold;
static int64_t __attribute__((__used__)) MotionSense;
static int64_t __attribute__((__used__)) QW256B;

#endif

static void FUNCT_NAME(uint8_t *output, int outstride,
                  unsigned char* cur, unsigned char* last, 
                  int bottom_field, int second_field, int width, int height )
{
    int64_t i;
    int stride = (width*2);
    int InfoIsOdd = bottom_field;

    int Line;
    long LoopCtr;
    long oldbx = 0;
    unsigned int Pitch = stride*2;
    int FieldHeight = height / 2;

    unsigned char* L1;					// ptr to Line1, of 3
    unsigned char* L2;					// ptr to Line2, the weave line
    unsigned char* L3;					// ptr to Line3

    unsigned char* L2P;					// ptr to prev Line2
    unsigned char* Dest = output;

    int64_t LastAvg=0;			//interp value from left qword

    // Set up our two parms that are actually evaluated for each pixel
    i=GreedyMaxComb;
    MaxComb = i << 56 | i << 48 | i << 40 | i << 32 | i << 24 | i << 16 | i << 8 | i;

    i = GreedyMotionThreshold;		// scale to range of 0-257
    MotionThreshold = i << 48 | i << 32 | i << 16 | i | UVMask;

    i = GreedyMotionSense;		// scale to range of 0-257
    MotionSense = i << 48 | i << 32 | i << 16 | i;
    
    i = 0xffffffff - 256;
    QW256B =  i << 48 |  i << 32 | i << 16 | i;  // save a couple instr on PMINSW instruct.

    // copy first even line no matter what, and the first odd line if we're
    // processing an EVEN field. (note diff from other deint rtns.)
    if ( second_field ) 
    {
        L1 = cur;
        L2 = cur;
        L2P = last;
    } 
    else 
    {
        L1 = last;
        L2 = cur;
        L2P = last;
    }

    if ( InfoIsOdd ) 
    {
        L1 += 0;
        L2 += stride;
        L3 = L1 + Pitch;
        L2P += stride;

        // copy first even line
        memcpy(Dest, L1, stride);
        Dest += outstride;
    } 
    else 
    {
        // copy first even line
        memcpy(Dest, L2, stride);
        Dest += outstride;

        L1 += stride;
        L2 += Pitch;
        L3 = L1 + Pitch;
        L2P += Pitch;

        // then first odd line
        memcpy(Dest, L1, stride);
        Dest += outstride;
    }

    for (Line = 0; Line < (FieldHeight - 1); ++Line) 
    {
        LoopCtr = stride / 8 - 1; // there are LineLength / 8 qwords per line but do 1 less, adj at end of loop

/* Hans-Dieter Kosch writes:
 *
 * >  The older compilers do not understand the syntax
 * >  __asm__ ( "command %[name0]" : : [name0] "x"(arg0) )
 * >  They only understand
 * >  __asm__ ( "command %0" : : "x"(arg0) )
 *
 * now we define the arguments to make the asm code less ugly.
 */
#ifndef asmLastAvg
#define asmLastAvg      "%0"
#define asmL1           "%1"
#define asmL3           "%2"
#define asmL2P          "%3"
#define asmL2           "%4"
#define asmDest         "%5"
#define asmLoopCtr      "%6"
#define asmoldbx        "%7"
#endif

        // For ease of reading, the comments below assume that we're operating on an odd
        // field (i.e., that InfoIsOdd is true).  Assume the obvious for even lines..
        __asm__ __volatile__
            (
             // save ebx (-fPIC)
	     MOVX" %%"XBX", "asmoldbx"\n\t"

             MOVX"  "asmL1",          %%"XAX"\n\t"
             LEAX"  8(%%"XAX"),     %%"XBX"\n\t"    // next qword needed by DJR
             MOVX"  "asmL3",          %%"XCX"\n\t"
             SUBX"  %%"XAX",        %%"XCX"\n\t"    // carry L3 addr as an offset
             MOVX"  "asmL2P",         %%"XDX"\n\t"
             MOVX"  "asmL2",          %%"XSI"\n\t"
             MOVX"  "asmDest",        %%"XDI"\n\t"    // DL1 if Odd or DL2 if Even

             ".align 8\n\t"
             "1:\n\t"

             "movq  (%%"XSI"),      %%mm0\n\t"      // L2 - the newest weave pixel value
             "movq  (%%"XAX"),      %%mm1\n\t"      // L1 - the top pixel
             "movq  (%%"XDX"),      %%mm2\n\t"      // L2P - the prev weave pixel
             "movq  (%%"XAX", %%"XCX"), %%mm3\n\t"  // L3, next odd row
             "movq  %%mm1,          %%mm6\n\t"      // L1 - get simple single pixel interp
             //	pavgb   mm6, mm3                    // use macro below
             V_PAVGB ("%%mm6", "%%mm3", "%%mm4", GREEDYH_MANGLE(ShiftMask))

             // DJR - Diagonal Jaggie Reduction
             // In the event that we are going to use an average (Bob) pixel we do not want a jagged
             // stair step effect.  To combat this we avg in the 2 horizontally adjacen pixels into the
             // interpolated Bob mix. This will do horizontal smoothing for only the Bob'd pixels.

             "movq  "asmLastAvg",   %%mm4\n\t"      // the bob value from prev qword in row
             "movq  %%mm6,          "asmLastAvg"\n\t" // save for next pass
             "psrlq $48,            %%mm4\n\t"      // right justify 1 pixel
             "movq  %%mm6,          %%mm7\n\t"      // copy of simple bob pixel
             "psllq $16,            %%mm7\n\t"      // left justify 3 pixels
             "por   %%mm7,          %%mm4\n\t"      // and combine

             "movq  (%%"XBX"),      %%mm5\n\t"      // next horiz qword from L1
             //			pavgb   mm5, qword ptr[ebx+ecx] // next horiz qword from L3, use macro below
             V_PAVGB ("%%mm5", "(%%"XBX",%%"XCX")", "%%mm7", GREEDYH_MANGLE(ShiftMask))
             "psllq $48,            %%mm5\n\t"      // left just 1 pixel
             "movq  %%mm6,          %%mm7\n\t"      // another copy of simple bob pixel
             "psrlq $16,            %%mm7\n\t"      // right just 3 pixels
             "por   %%mm7,          %%mm5\n\t"      // combine
             //			pavgb	mm4, mm5			// avg of forward and prev by 1 pixel, use macro
             V_PAVGB ("%%mm4", "%%mm5", "%%mm5", GREEDYH_MANGLE(ShiftMask))   // mm5 gets modified if MMX
             //			pavgb	mm6, mm4			// avg of center and surround interp vals, use macro
             V_PAVGB ("%%mm6", "%%mm4", "%%mm7", GREEDYH_MANGLE(ShiftMask))

             // Don't do any more averaging than needed for mmx. It hurts performance and causes rounding errors.
#ifndef IS_MMX
             //          pavgb	mm4, mm6			// 1/4 center, 3/4 adjacent
             V_PAVGB ("%%mm4", "%%mm6", "%%mm7", GREEDYH_MANGLE(ShiftMask))
             //    		pavgb	mm6, mm4			// 3/8 center, 5/8 adjacent
             V_PAVGB ("%%mm6", "%%mm4", "%%mm7", GREEDYH_MANGLE(ShiftMask))
#endif

             // get abs value of possible L2 comb
             "movq    %%mm6,        %%mm4\n\t"      // work copy of interp val
             "movq    %%mm2,        %%mm7\n\t"      // L2
             "psubusb %%mm4,        %%mm7\n\t"      // L2 - avg
             "movq    %%mm4,        %%mm5\n\t"      // avg
             "psubusb %%mm2,        %%mm5\n\t"      // avg - L2
             "por     %%mm7,        %%mm5\n\t"      // abs(avg-L2)

             // get abs value of possible L2P comb
             "movq    %%mm0,        %%mm7\n\t"      // L2P
             "psubusb %%mm4,        %%mm7\n\t"      // L2P - avg
             "psubusb %%mm0,        %%mm4\n\t"      // avg - L2P
             "por     %%mm7,        %%mm4\n\t"      // abs(avg-L2P)

             // use L2 or L2P depending upon which makes smaller comb
             "psubusb %%mm5,        %%mm4\n\t"      // see if it goes to zero
             "psubusb %%mm5,        %%mm5\n\t"      // 0
             "pcmpeqb %%mm5,        %%mm4\n\t"      // if (mm4=0) then FF else 0
             "pcmpeqb %%mm4,        %%mm5\n\t"      // opposite of mm4

             // if Comb(L2P) <= Comb(L2) then mm4=ff, mm5=0 else mm4=0, mm5 = 55
             "pand    %%mm2,        %%mm5\n\t"      // use L2 if mm5 == ff, else 0
             "pand    %%mm0,        %%mm4\n\t"      // use L2P if mm4 = ff, else 0
             "por     %%mm5,        %%mm4\n\t"      // may the best win

             // Inventory: at this point we have the following values:
             // mm0 = L2P (or L2)
             // mm1 = L1
             // mm2 = L2 (or L2P)
             // mm3 = L3
             // mm4 = the best of L2,L2P weave pixel, base upon comb
             // mm6 = the avg interpolated value, if we need to use it

             // Let's measure movement, as how much the weave pixel has changed
             "movq    %%mm2,        %%mm7\n\t"
             "psubusb %%mm0,        %%mm2\n\t"
             "psubusb %%mm7,        %%mm0\n\t"
             "por     %%mm2,        %%mm0\n\t"      // abs value of change, used later

             // Now lets clip our chosen value to be not outside of the range
             // of the high/low range L1-L3 by more than MaxComb.
             // This allows some comb but limits the damages and also allows more
             // detail than a boring oversmoothed clip.
             "movq    %%mm1,        %%mm2\n\t"      // copy L1
             //	pmaxub mm2, mm3                     // use macro
             V_PMAXUB ("%%mm2", "%%mm3")            // now = Max(L1,L3)
             "movq    %%mm1,        %%mm5\n\t"      // copy L1
             // pminub	mm5, mm3                    // now = Min(L1,L3), use macro
             V_PMINUB ("%%mm5", "%%mm3", "%%mm7")
             // allow the value to be above the high or below the low by amt of MaxComb
             "psubusb "GREEDYH_MANGLE(MaxComb)", %%mm5\n\t"      // lower min by diff
             "paddusb "GREEDYH_MANGLE(MaxComb)", %%mm2\n\t"      // increase max by diff
             // pmaxub	mm4, mm5                    // now = Max(best,Min(L1,L3) use macro
             V_PMAXUB ("%%mm4", "%%mm5")
             // pminub	mm4, mm2                    // now = Min( Max(best, Min(L1,L3), L2 )=L2 clipped
             V_PMINUB ("%%mm4", "%%mm2", "%%mm7")

             // Blend weave pixel with bob pixel, depending on motion val in mm0
             "psubusb "GREEDYH_MANGLE(MotionThreshold)", %%mm0\n\t"// test Threshold, clear chroma change >>>??
             "pmullw  "GREEDYH_MANGLE(MotionSense)", %%mm0\n\t"    // mul by user factor, keep low 16 bits
             "movq   "GREEDYH_MANGLE(QW256)", %%mm7\n\t"
#ifdef IS_SSE
             "pminsw  %%mm7,        %%mm0\n\t"      // max = 256
#else
             "paddusw "GREEDYH_MANGLE(QW256B)", %%mm0\n\t"      // add, may sat at fff..
             "psubusw "GREEDYH_MANGLE(QW256B)", %%mm0\n\t"      // now = Min(L1,256)
#endif
             "psubusw %%mm0,        %%mm7\n\t"      // so the 2 sum to 256, weighted avg
             "movq    %%mm4,        %%mm2\n\t"      // save weave chroma info before trashing
             "pand   "GREEDYH_MANGLE(YMask)", %%mm4\n\t"      // keep only luma from calc'd value
             "pmullw  %%mm7,        %%mm4\n\t"      // use more weave for less motion
             "pand   "GREEDYH_MANGLE(YMask)", %%mm6\n\t"      // keep only luma from calc'd value
             "pmullw  %%mm0,        %%mm6\n\t"      // use more bob for large motion
             "paddusw %%mm6,        %%mm4\n\t"      // combine
             "psrlw   $8,           %%mm4\n\t"      // div by 256 to get weighted avg

             // chroma comes from weave pixel
             "pand   "GREEDYH_MANGLE(UVMask)", %%mm2\n\t"      // keep chroma
             "por     %%mm4,        %%mm2\n\t"      // and combine

             V_MOVNTQ ("(%%"XDI")", "%%mm2")        // move in our clipped best, use macro

             // bump ptrs and loop
             LEAX"    8(%%"XAX"),   %%"XAX"\n\t"
             LEAX"    8(%%"XBX"),   %%"XBX"\n\t"
             LEAX"    8(%%"XDX"),   %%"XDX"\n\t"
             LEAX"    8(%%"XDI"),   %%"XDI"\n\t"
             LEAX"    8(%%"XSI"),   %%"XSI"\n\t"
             DECX"    "asmLoopCtr"\n\t"
             "jg      1b\n\t"                       // loop if not to last line
                                                    // note P-III default assumes backward branches taken
             "jl      1f\n\t"                       // done
             MOVX"    %%"XAX",      %%"XBX"\n\t"  // sharpness lookahead 1 byte only, be wrong on 1
             "jmp     1b\n\t"

             "1:\n\t"
	     MOVX" "asmoldbx", %%"XBX"\n\t"

             : /* no outputs */

             : "m"(LastAvg),
               "m"(L1),
               "m"(L3),
               "m"(L2P),
               "m"(L2),
               "m"(Dest),
               "m"(LoopCtr),
               "m"(oldbx)

             : XAX, XCX, XDX, XSI, XDI,
#if ARCH_X86_32
               "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)",
#elif ARCH_X86_64
/* the following clobber list causes trouble for gcc 2.95. it shouldn't be
 * an issue as, afaik, mmx registers map to the existing fp registers.
 */
               "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
#endif
               "memory", "cc"
            );

        Dest += outstride;
        memcpy(Dest, L3, stride);
        Dest += outstride;

        L1  += Pitch;
        L2  += Pitch;
        L3  += Pitch;
        L2P += Pitch;
    }

    if (InfoIsOdd) 
    {
        memcpy(Dest, L2, stride);
    }

    // clear out the MMX registers ready for doing floating point again
#if HAVE_MMX
    __asm__ __volatile__ ("emms\n\t");
#endif
}
