////////////////////////////////////////////////////////////////////////////////
///
/// gcc version of the MMX optimized routines. All MMX optimized functions
/// have been gathered into this single source code file, regardless to their 
/// class or original source code file, in order to ease porting the library
/// to other compiler and processor platforms.
///
/// This file is to be compiled on any platform with the GNU C compiler.
/// Compiler. Please see 'mmx_win.cpp' for the x86 Windows version of this
/// file.
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai @ iki.fi
/// SoundTouch WWW: http://www.iki.fi/oparviai/soundtouch
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

#include "STTypes.h"
using namespace soundtouch;

#ifdef ALLOW_MMX
#include <stdexcept>
#include <string>
#include <climits>

// define USE_GCC_INTRINSICS to use gcc 3.x intrinsics instead of our mmx.h
//#define USE_GCC_INTRINSICS

#ifdef USE_GCC_INTRINSICS
#  include <mmintrin.h>
#  define SI(A,B...) A
#  define GI(X...) X
#else
#  include "x86/mmx.h"
#  define _mm_empty() __asm__ __volatile__ ("emms")
#  define __m64 mmx_t
#  define SI(A,B...) B
#  define GI(X...)
#endif

#include "cpu_detect.h"
#include "TDStretch.h"

// MMX routines available only with integer sample type    

//////////////////////////////////////////////////////////////////////////////
//
// Implementation of MMX optimized functions of class 'TDStretch'
//
//////////////////////////////////////////////////////////////////////////////

// these are declared in 'TDStretch.cpp'
extern int scanOffsets[4][24];

// Calculates cross correlation of two buffers
long TDStretchMMX::calcCrossCorrStereo(const short *pV1, const short *pV2) const
{
    uint tmp; 
    uint counter = (overlapLength>>3)-1;             // load counter to counter = overlapLength / 8 - 1
    __m64 *pv1=(__m64*)pV1, *pv2=(__m64*)pV2;
    GI(__m64 m0, m1, m2, m3, m4, m5); // temporaries
    uint shift = overlapDividerBits;

    // prepare to the first round by loading 
    SI(m1 = pv1[0],             movq_a2r(0, pv1, mm1)); // load m1 = pv1[0]
    SI(m2 = pv1[1],             movq_a2r(8, pv1, mm2)); // load m2 = pv1[1]
    SI(m0 = _mm_setzero_si64(), pxor_r2r(mm0, mm0));    // clear m0
    SI(m5 = _mm_cvtsi32_si64(shift),movd_v2r(shift, mm5));   // shift in 64bit reg

    do {
        // Calculate cross-correlation between the tempOffset and tmpbid_buffer.
        // Process 4 parallel batches of 2 * stereo samples each during one
        // round to improve CPU-level parallellization.
        SI(m1 = _mm_madd_pi16(m1, pv2[0]),pmaddwd_a2r(0, pv2, mm1)); // multiply-add m1 = m1 * pv2[0]
        SI(m3 = pv1[2],                   movq_a2r(16, pv1, mm3));   // load mm3 = pv1[2]
        SI(m2 = _mm_madd_pi16(m2, pv2[1]),pmaddwd_a2r(8, pv2, mm2)); // multiply-add m2 = m2 * pv2[1]
        SI(m4 = pv1[3],                   movq_a2r(24, pv1, mm4));   // load mm4 = pv1[3]
        SI(m3 = _mm_madd_pi16(m3, pv2[2]),pmaddwd_a2r(16, pv2, mm3));// multiply-add m3 = m3 * pv2[2]
        SI(m2 = _mm_add_pi32(m2, m1),     paddd_r2r(mm1, mm2));      // add m2 += m1
        SI(m4 = _mm_madd_pi16(m4, pv2[3]),pmaddwd_a2r(24, pv2, mm4));// multiply-add m4 = m4 * pv2[3]
        SI(m1 = pv1[4],                   movq_a2r(32, pv1, mm1));   // mm1 = pv1[0] for next round
        SI(m2 = _mm_srai_pi32(m2, m5),    psrad_r2r(mm5, mm2));      // m2 >>= shift (mm5)
        pv1 += 4;                                                    // increment first pointer
        SI(m3 = _mm_add_pi32(m3, m4),     paddd_r2r(mm4, mm3));      // m3 += m4
        SI(m0 = _mm_add_pi32(m0, m2),     paddd_r2r(mm2, mm0));      // m0 += m2
        SI(m2 = pv1[1],                   movq_a2r(8, pv1, mm2));    // mm2 = pv1[1] for next round
        SI(m3 = _mm_srai_pi32(m3, m5),    psrad_r2r(mm5, mm3));    // m3 >>= shift (mm5)
        pv2 += 4;                                                    // increment second pointer
        SI(m0 = _mm_add_pi32(m0, m3),     paddd_r2r(mm3, mm0));      // add m0 += m3
    } while ((--counter)!=0);

    // Finalize the last partial loop:
    SI(m1 = _mm_madd_pi16(m1, pv2[0]), pmaddwd_a2r(0, pv2, mm1));
    SI(m3 = pv1[2],                    movq_a2r(16, pv1, mm3));
    SI(m2 = _mm_madd_pi16(m2, pv2[1]), pmaddwd_a2r(8, pv2, mm2));
    SI(m4 = pv1[3],                    movq_a2r(24, pv1, mm4));
    SI(m3 = _mm_madd_pi16(m3, pv2[2]), pmaddwd_a2r(16, pv2, mm3));
    SI(m2 = _mm_add_pi32(m2, m1),      paddd_r2r(mm1, mm2));
    SI(m4 = _mm_madd_pi16(m4, pv2[3]), pmaddwd_a2r(24, pv2, mm4));
    SI(m2 = _mm_srai_pi32(m2, m5),     psrad_r2r(mm5, mm2));
    SI(m3 = _mm_add_pi32(m3, m4),      paddd_r2r(mm4, mm3));
    SI(m0 = _mm_add_pi32(m0, m2),      paddd_r2r(mm2, mm0));
    SI(m3 = _mm_srai_pi32(m3, m5),     psrad_r2r(mm5, mm3));
    SI(m0 = _mm_add_pi32(m0, m3),      paddd_r2r(mm3, mm0));

    // copy hi-dword of mm0 to lo-dword of mm1, then sum mm0+mm1
    // and finally return the result
    SI(m1 = m0,                        movq_r2r(mm0, mm1));
    SI(m1 = _mm_srli_si64(m1, 32),     psrld_i2r(32, mm1));
    SI(m0 = _mm_add_pi32(m0, m1),      paddd_r2r(mm1, mm0));
    SI(tmp = _mm_cvtsi64_si32(m0),     movd_r2m(mm0, tmp));
    return tmp;
}

#ifdef USE_MULTI_MMX
// Calculates cross correlation of two buffers
long TDStretchMMX::calcCrossCorrMulti(const short *pV1, const short *pV2) const
{
    //static const unsigned long long int mm_half __attribute__ ((aligned(8))) = 0xffffffffULL;
    static const __m64 mm_mask[4][8] __attribute__ ((aligned(8))) = {
        {
            // even bit
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0,
            0,
            0,
            0
        },
        {
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0x0000ffffffffffffULL,
            0,
            0,
            0,
            0
        },
        {
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0x00000000ffffffffULL,
            0,
            0,
            0,
            0
        },
        {
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0xffffffffffffffffULL,
            0x000000000000ffffULL,
            0,
            0,
            0,
            0
        }
    };
    uint tmp; 
    uint adjustedOverlapLength = overlapLength*channels;
    uint counter = ((adjustedOverlapLength+15)>>4)-1;    // load counter to counter = overlapLength / 8 - 1
    uint remainder = (16-adjustedOverlapLength)&0xf;     // since there are 1/3 sample per 1/2 quadword

    __m64 *ph = (__m64*)&mm_mask[remainder&3][remainder>>2];
    __m64 *pv1=(__m64*)pV1, *pv2=(__m64*)pV2;
    GI(__m64 m0, m1, m2, m3, m4, m5, m6); // temporaries
    uint shift = overlapDividerBits;

    // prepare to the first round by loading 
    SI(m1 = pv1[0],             movq_a2r(0, pv1, mm1)); // load m1 = pv1[0]
    SI(m2 = pv1[1],             movq_a2r(8, pv1, mm2)); // load m2 = pv1[1]
    SI(m0 = _mm_setzero_si64(), pxor_r2r(mm0, mm0));    // clear m0
    SI(m5 = _mm_cvtsi32_si64(shift),movd_v2r(shift, mm5));   // shift in 64bit reg

    do {
        // Calculate cross-correlation between the tempOffset and tmpbid_buffer.
        // Process 4 parallel batches of 2 * stereo samples each during one
        // round to improve CPU-level parallellization.
        SI(m1 = _mm_madd_pi16(m1, pv2[0]),pmaddwd_a2r(0, pv2, mm1)); // multiply-add m1 = m1 * pv2[0]
        SI(m3 = pv1[2],                   movq_a2r(16, pv1, mm3));   // load mm3 = pv1[2]
        SI(m2 = _mm_madd_pi16(m2, pv2[1]),pmaddwd_a2r(8, pv2, mm2)); // multiply-add m2 = m2 * pv2[1]
        SI(m4 = pv1[3],                   movq_a2r(24, pv1, mm4));   // load mm4 = pv1[3]
        SI(m3 = _mm_madd_pi16(m3, pv2[2]),pmaddwd_a2r(16, pv2, mm3));// multiply-add m3 = m3 * pv2[2]
        SI(m2 = _mm_add_pi32(m2, m1),     paddd_r2r(mm1, mm2));      // add m2 += m1
        SI(m4 = _mm_madd_pi16(m4, pv2[3]),pmaddwd_a2r(24, pv2, mm4));// multiply-add m4 = m4 * pv2[3]
        SI(m1 = pv1[4],                   movq_a2r(32, pv1, mm1));   // mm1 = pv1[0] for next round
        SI(m2 = _mm_srai_pi32(m2, m5),    psrad_r2r(mm5, mm2));      // m2 >>= shift (mm5)
        pv1 += 4;                                                    // increment first pointer
        SI(m3 = _mm_add_pi32(m3, m4),     paddd_r2r(mm4, mm3));      // m3 += m4
        SI(m0 = _mm_add_pi32(m0, m2),     paddd_r2r(mm2, mm0));      // m0 += m2
        SI(m2 = pv1[1],                   movq_a2r(8, pv1, mm2));    // mm2 = pv1[1] for next round
        SI(m3 = _mm_srai_pi32(m3, m5),    psrad_r2r(mm5, mm3));    // m3 >>= shift (mm5)
        pv2 += 4;                                                    // increment second pointer
        SI(m0 = _mm_add_pi32(m0, m3),     paddd_r2r(mm3, mm0));      // add m0 += m3
    } while ((--counter)!=0);

    SI(m6 = ph[0], movq_a2r(0, ph, mm6));
    // Finalize the last partial loop:
    SI(m1 = _mm_madd_pi16(m1, pv2[0]), pmaddwd_a2r(0, pv2, mm1));
    SI(m1 = _mm_and_si64(m1, m6),      pand_r2r(mm6, mm1));
    SI(m3 = pv1[2],                    movq_a2r(16, pv1, mm3));
    SI(m6 = ph[1], movq_a2r(8, ph, mm6));
    SI(m2 = _mm_madd_pi16(m2, pv2[1]), pmaddwd_a2r(8, pv2, mm2));
    SI(m2 = _mm_and_si64(m2, m6),      pand_r2r(mm6, mm2));
    SI(m4 = pv1[3],                    movq_a2r(24, pv1, mm4));
    SI(m6 = ph[2], movq_a2r(16, ph, mm6));
    SI(m3 = _mm_madd_pi16(m3, pv2[2]), pmaddwd_a2r(16, pv2, mm3));
    SI(m3 = _mm_and_si64(m3, m6),      pand_r2r(mm6, mm3));
    SI(m2 = _mm_add_pi32(m2, m1),      paddd_r2r(mm1, mm2));
    SI(m6 = ph[3], movq_a2r(24, ph, mm6));
    SI(m4 = _mm_madd_pi16(m4, pv2[3]), pmaddwd_a2r(24, pv2, mm4));
    SI(m4 = _mm_and_si64(m4, m6),      pand_r2r(mm6, mm4));
    SI(m2 = _mm_srai_pi32(m2, m5),     psrad_r2r(mm5, mm2));
    SI(m3 = _mm_add_pi32(m3, m4),      paddd_r2r(mm4, mm3));
    SI(m0 = _mm_add_pi32(m0, m2),      paddd_r2r(mm2, mm0));
    SI(m3 = _mm_srai_pi32(m3, m5),     psrad_r2r(mm5, mm3));
    SI(m0 = _mm_add_pi32(m0, m3),      paddd_r2r(mm3, mm0));

    // copy hi-dword of mm0 to lo-dword of mm1, then sum mm0+mm1
    // and finally return the result
    SI(m1 = m0,                        movq_r2r(mm0, mm1));
    SI(m1 = _mm_srli_si64(m1, 32),     psrld_i2r(32, mm1));
    SI(m0 = _mm_add_pi32(m0, m1),      paddd_r2r(mm1, mm0));
    SI(tmp = _mm_cvtsi64_si32(m0),     movd_r2m(mm0, tmp));
    return tmp;
}
#endif

void TDStretchMMX::clearCrossCorrState()
{
    _mm_empty();
}

// MMX-optimized version of the function overlapStereo
void TDStretchMMX::overlapStereo(short *output, const short *input) const
{
    _mm_empty();
    uint shift = overlapDividerBits;
    uint counter = overlapLength>>2;                 // counter = overlapLength / 4
    __m64 *inPtr = (__m64*) input;                   // load address of inputBuffer
    __m64 *midPtr = (__m64*) pMidBuffer;             // load address of midBuffer
    __m64 *outPtr = ((__m64*) output)-2;             // load address of outputBuffer
    GI(__m64 m0, m1, m2, m3, m4, m5, m6, m7);        // temporaries

    // load mixing value adder to mm5
    uint tmp0 = 0x0002fffe;                                      // tmp0 = 0x0002 fffe
    SI(m5 = _mm_cvtsi32_si64(tmp0),    movd_v2r(tmp0, mm5));     // mm5 = 0x0000 0000 0002 fffe
    SI(m5 = _mm_unpacklo_pi32(m5,m5),  punpckldq_r2r(mm5, mm5)); // mm5 = 0x0002 fffe 0002 fffe
    // load sliding mixing value counter to mm6
    SI(m6 = _mm_cvtsi32_si64(overlapLength), movd_v2r(overlapLength, mm6));
    SI(m6 = _mm_unpacklo_pi32(m6, m6), punpckldq_r2r(mm6, mm6)); // mm6 = 0x0000 OVL_ 0000 OVL_
    // load sliding mixing value counter to mm7
    uint tmp1 = (overlapLength-1)|0x00010000;                    // tmp1 = 0x0001 overlapLength-1
    SI(m7 = _mm_cvtsi32_si64(tmp1),    movd_v2r(tmp1, mm7));     // mm7 = 0x0000 0000 0001 01ff
    SI(m7 = _mm_unpacklo_pi32(m7, m7), punpckldq_r2r(mm7, mm7)); // mm7 = 0x0001 01ff 0001 01ff

    do {
        // Process two parallel batches of 2+2 stereo samples during each round 
        // to improve CPU-level parallellization.
        //
        // Load [midPtr] into m0 and m1
        // Load [inPtr] into m3
        // unpack words of m0, m1 and m3 into m0 and m1
        // multiply-add m0*m6 and m1*m7, store results into m0 and m1
        // divide m0 and m1 by 512 (=right-shift by overlapDividerBits)
        // pack the result into m0 and store into [edx]
        //
        // Load [midPtr+8] into m2 and m3
        // Load [inPtr+8] into m4
        // unpack words of m2, m3 and m4 into m2 and m3
        // multiply-add m2*m6 and m3*m7, store results into m2 and m3
        // divide m2 and m3 by 512 (=right-shift by overlapDividerBits)
        // pack the result into m2 and store into [edx+8]
        SI(m0 = midPtr[0],                movq_a2r(0, midPtr, mm0));// mm0 = m1l m1r m0l m0r
        outPtr += 2;
        SI(m3 = inPtr[0],                 movq_a2r(0, inPtr, mm3)); // mm3 = i1l i1r i0l i0r
        SI(m1 = m0,                       movq_r2r(mm0, mm1));      // mm1 = m1l m1r m0l m0r
        SI(m2 = midPtr[1],                movq_a2r(8, midPtr, mm2));// mm2 = m3l m3r m2l m2r
        SI(m0 = _mm_unpacklo_pi16(m0, m3),punpcklwd_r2r(mm3, mm0)); // mm0 = i0l m0l i0r m0r
        midPtr += 2;
        SI(m4 = inPtr[1],                 movq_a2r(8, inPtr, mm4)); // mm4 = i3l i3r i2l i2r
        SI(m1 = _mm_unpackhi_pi16(m1, m3),punpckhwd_r2r(mm3, mm1)); // mm1 = i1l m1l i1r m1r
        inPtr+=2;
        SI(m3 = m2,                       movq_r2r(mm2, mm3));      // mm3 = m3l m3r m2l m2r
        SI(m2 = _mm_unpacklo_pi16(m2, m4),punpcklwd_r2r(mm4, mm2)); // mm2 = i2l m2l i2r m2r
        // mm0 = i0l*m63+m0l*m62 i0r*m61+m0r*m60
        SI(m0 = _mm_madd_pi16(m0, m6),    pmaddwd_r2r(mm6, mm0));
        SI(m3 = _mm_unpackhi_pi16(m3, m4),punpckhwd_r2r(mm4, mm3)); // mm3 = i3l m3l i3r m3r
        SI(m4 = _mm_cvtsi32_si64(shift),  movd_v2r(shift, mm4));    // mm4 = shift
        // mm1 = i1l*m73+m1l*m72 i1r*m71+m1r*m70
        SI(m1 = _mm_madd_pi16(m1, m7),    pmaddwd_r2r(mm7, mm1));
        SI(m6 = _mm_add_pi16(m6, m5),     paddw_r2r(mm5, mm6));
        SI(m7 = _mm_add_pi16(m7, m5),     paddw_r2r(mm5, mm7));
        SI(m0 = _mm_srai_pi32(m0, m4),    psrad_r2r(mm4, mm0));    // mm0 >>= shift
        // mm2 = i2l*m63+m2l*m62 i2r*m61+m2r*m60
        SI(m2 = _mm_madd_pi16(m2, m6),    pmaddwd_r2r(mm6, mm2));
        SI(m1 = _mm_srai_pi32(m1, m4),    psrad_r2r(mm4, mm1));    // mm1 >>= shift
        // mm3 = i3l*m73+m3l*m72 i3r*m71+m3r*m70
        SI(m3 = _mm_madd_pi16(m3, m7),    pmaddwd_r2r(mm7, mm3));
        SI(m2 = _mm_srai_pi32(m2, m4),    psrad_r2r(mm4, mm2));    // mm2 >>= shift
        SI(m0 = _mm_packs_pi32(m0, m1),   packssdw_r2r(mm1, mm0)); // mm0 = mm1h mm1l mm0h mm0l
        SI(m3 = _mm_srai_pi32(m3, m4),    psrad_r2r(mm4, mm3));    // mm3 >>= shift
        SI(m6 = _mm_add_pi16(m6, m5),     paddw_r2r(mm5, mm6));
        SI(m2 = _mm_packs_pi32(m2, m3),   packssdw_r2r(mm3, mm2)); // mm2 = mm2h mm2l mm3h mm3l
        SI(m7 = _mm_add_pi16(m7, m5),     paddw_r2r(mm5, mm7));
        SI(outPtr[0] = m0,                movq_r2a(mm0, 0, outPtr));
        SI(outPtr[1] = m2,                movq_r2a(mm2, 8, outPtr));
    } while ((--counter)!=0);
    _mm_empty();
}

#if 0
// MMX-optimized version of the function overlapMulti
void TDStretchMMX::overlapMulti(short *output, const short *input) const
{
    _mm_empty();
    uint shift = overlapDividerBits;
    uint counter = overlapLength>>2;                 // counter = overlapLength / 4
    __m64 *inPtr = (__m64*) input;                   // load address of inputBuffer
    __m64 *midPtr = (__m64*) pMidBuffer;             // load address of midBuffer
    __m64 *outPtr = ((__m64*) output)-2;             // load address of outputBuffer
    GI(__m64 m0, m1, m2, m3, m4, m5, m6, m7);        // temporaries

    // load mixing value adder to mm5
    uint tmp0 = 0x0002fffe;                                      // tmp0 = 0x0002 fffe
    SI(m5 = _mm_cvtsi32_si64(tmp0),    movd_v2r(tmp0, mm5));     // mm5 = 0x0000 0000 0002 fffe
    SI(m5 = _mm_unpacklo_pi32(m5,m5),  punpckldq_r2r(mm5, mm5)); // mm5 = 0x0002 fffe 0002 fffe
    // load sliding mixing value counter to mm6
    SI(m6 = _mm_cvtsi32_si64(overlapLength), movd_v2r(overlapLength, mm6));
    SI(m6 = _mm_unpacklo_pi32(m6, m6), punpckldq_r2r(mm6, mm6)); // mm6 = 0x0000 OVL_ 0000 OVL_
    // load sliding mixing value counter to mm7
    uint tmp1 = (overlapLength-1)|0x00010000;                    // tmp1 = 0x0001 overlapLength-1
    SI(m7 = _mm_cvtsi32_si64(tmp1),    movd_v2r(tmp1, mm7));     // mm7 = 0x0000 0000 0001 01ff
    SI(m7 = _mm_unpacklo_pi32(m7, m7), punpckldq_r2r(mm7, mm7)); // mm7 = 0x0001 01ff 0001 01ff

    do {
        // Process two parallel batches of 2+2 stereo samples during each round 
        // to improve CPU-level parallellization.
        //
        // Load [midPtr] into m0 and m1
        // Load [inPtr] into m3
        // unpack words of m0, m1 and m3 into m0 and m1
        // multiply-add m0*m6 and m1*m7, store results into m0 and m1
        // divide m0 and m1 by 512 (=right-shift by overlapDividerBits)
        // pack the result into m0 and store into [edx]
        //
        // Load [midPtr+8] into m2 and m3
        // Load [inPtr+8] into m4
        // unpack words of m2, m3 and m4 into m2 and m3
        // multiply-add m2*m6 and m3*m7, store results into m2 and m3
        // divide m2 and m3 by 512 (=right-shift by overlapDividerBits)
        // pack the result into m2 and store into [edx+8]
        SI(m0 = midPtr[0],                movq_a2r(0, midPtr, mm0));// mm0 = m1l m1r m0l m0r
        outPtr += 2;
        SI(m3 = inPtr[0],                 movq_a2r(0, inPtr, mm3)); // mm3 = i1l i1r i0l i0r
        SI(m1 = m0,                       movq_r2r(mm0, mm1));      // mm1 = m1l m1r m0l m0r
        SI(m2 = midPtr[1],                movq_a2r(8, midPtr, mm2));// mm2 = m3l m3r m2l m2r
        SI(m0 = _mm_unpacklo_pi16(m0, m3),punpcklwd_r2r(mm3, mm0)); // mm0 = i0l m0l i0r m0r
        midPtr += 2;
        SI(m4 = inPtr[1],                 movq_a2r(8, inPtr, mm4)); // mm4 = i3l i3r i2l i2r
        SI(m1 = _mm_unpackhi_pi16(m1, m3),punpckhwd_r2r(mm3, mm1)); // mm1 = i1l m1l i1r m1r
        inPtr+=2;
        SI(m3 = m2,                       movq_r2r(mm2, mm3));      // mm3 = m3l m3r m2l m2r
        SI(m2 = _mm_unpacklo_pi16(m2, m4),punpcklwd_r2r(mm4, mm2)); // mm2 = i2l m2l i2r m2r
        // mm0 = i0l*m63+m0l*m62 i0r*m61+m0r*m60
        SI(m0 = _mm_madd_pi16(m0, m6),    pmaddwd_r2r(mm6, mm0));
        SI(m3 = _mm_unpackhi_pi16(m3, m4),punpckhwd_r2r(mm4, mm3)); // mm3 = i3l m3l i3r m3r
        SI(m4 = _mm_cvtsi32_si64(shift),  movd_v2r(shift, mm4));    // mm4 = shift
        // mm1 = i1l*m73+m1l*m72 i1r*m71+m1r*m70
        SI(m1 = _mm_madd_pi16(m1, m7),    pmaddwd_r2r(mm7, mm1));
        SI(m6 = _mm_add_pi16(m6, m5),     paddw_r2r(mm5, mm6));
        SI(m7 = _mm_add_pi16(m7, m5),     paddw_r2r(mm5, mm7));
        SI(m0 = _mm_srai_pi32(m0, m4),    psrad_r2r(mm4, mm0));    // mm0 >>= shift
        // mm2 = i2l*m63+m2l*m62 i2r*m61+m2r*m60
        SI(m2 = _mm_madd_pi16(m2, m6),    pmaddwd_r2r(mm6, mm2));
        SI(m1 = _mm_srai_pi32(m1, m4),    psrad_r2r(mm4, mm1));    // mm1 >>= shift
        // mm3 = i3l*m73+m3l*m72 i3r*m71+m3r*m70
        SI(m3 = _mm_madd_pi16(m3, m7),    pmaddwd_r2r(mm7, mm3));
        SI(m2 = _mm_srai_pi32(m2, m4),    psrad_r2r(mm4, mm2));    // mm2 >>= shift
        SI(m0 = _mm_packs_pi32(m0, m1),   packssdw_r2r(mm1, mm0)); // mm0 = mm1h mm1l mm0h mm0l
        SI(m3 = _mm_srai_pi32(m3, m4),    psrad_r2r(mm4, mm3));    // mm3 >>= shift
        SI(m6 = _mm_add_pi16(m6, m5),     paddw_r2r(mm5, mm6));
        SI(m2 = _mm_packs_pi32(m2, m3),   packssdw_r2r(mm3, mm2)); // mm2 = mm2h mm2l mm3h mm3l
        SI(m7 = _mm_add_pi16(m7, m5),     paddw_r2r(mm5, mm7));
        SI(outPtr[0] = m0,                movq_r2a(mm0, 0, outPtr));
        SI(outPtr[1] = m2,                movq_r2a(mm2, 8, outPtr));
    } while ((--counter)!=0);
    _mm_empty();
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// implementation of MMX optimized functions of class 'FIRFilter'
//
//////////////////////////////////////////////////////////////////////////////

#include "FIRFilter.h"
FIRFilterMMX::FIRFilterMMX() : FIRFilter()
{
    filterCoeffsUnalign = NULL;
}


FIRFilterMMX::~FIRFilterMMX()
{
    delete[] filterCoeffsUnalign;
}

// (overloaded) Calculates filter coefficients for MMX routine
void FIRFilterMMX::setCoefficients(const short *coeffs, uint newLength, uint uResultDivFactor)
{
    uint i;
    FIRFilter::setCoefficients(coeffs, newLength, uResultDivFactor);

    // Ensure that filter coeffs array is aligned to 16-byte boundary
    delete[] filterCoeffsUnalign;
    filterCoeffsUnalign = new short[2 * newLength + 8];
    filterCoeffsAlign = (short *)(((ulong)filterCoeffsUnalign + 15) & -16);

    // rearrange the filter coefficients for mmx routines 
    for (i = 0;i < length; i += 4) 
    {
        filterCoeffsAlign[2 * i + 0] = coeffs[i + 0];
        filterCoeffsAlign[2 * i + 1] = coeffs[i + 2];
        filterCoeffsAlign[2 * i + 2] = coeffs[i + 0];
        filterCoeffsAlign[2 * i + 3] = coeffs[i + 2];

        filterCoeffsAlign[2 * i + 4] = coeffs[i + 1];
        filterCoeffsAlign[2 * i + 5] = coeffs[i + 3];
        filterCoeffsAlign[2 * i + 6] = coeffs[i + 1];
        filterCoeffsAlign[2 * i + 7] = coeffs[i + 3];
    }
}



// mmx-optimized version of the filter routine for stereo sound
uint FIRFilterMMX::evaluateFilterStereo(short *dest, const short *src, const uint numSamples) const
{
    _mm_empty();
    __m64 *inPtr  = (__m64*)src;
    __m64 *outPtr = ((__m64*)dest) - 1;
    uint counter  = (numSamples - length) >> 1;
    GI(__m64 m0, m1, m2, m3, m4, m5, m6, m7);

    do {
        __m64 *filterInPtr = inPtr;                         // Load pointer to samples
        __m64 *filterPtr =  (__m64*)filterCoeffsAlign;      // Load pointer to filter coefficients
        uint filterCounter = lengthDiv8;                    // Load filter length/8 to filterCounter

        SI(m0 = _mm_setzero_si64(), pxor_r2r(mm0, mm0));    // zero sums
        SI(m1 = filterInPtr[0], movq_a2r(0, filterInPtr, mm1)); // mm1 = l1 r1 l0 r0
        SI(m7 = _mm_setzero_si64(), pxor_r2r(mm7, mm7));    // zero sums

        do {
            SI(m2 = filterInPtr[1],        movq_a2r(8, filterInPtr, mm2)); // mm2 = l3 r3 l2 r2
            SI(m4 = m1,                    movq_r2r(mm1, mm4));            // mm4 = l1 r1 l0 r0
            SI(m3 = filterInPtr[2],        movq_a2r(16, filterInPtr, mm3));// mm3 = l5 r5 l4 r4
            SI(m1 = _mm_unpackhi_pi16(m1, m2), punpckhwd_r2r(mm2, mm1));   // mm1 = l3 l1 r3 r1
            SI(m6 = m2,                    movq_r2r(mm2, mm6));            // mm6 = l3 r3 l2 r2
            SI(m4 = _mm_unpacklo_pi16(m4, m2), punpcklwd_r2r(mm2, mm4));   // mm4 = l2 l0 r2 r0
            SI(m2 = filterPtr[0],          movq_a2r(0, filterPtr, mm2));   // mm2 = f2 f0 f2 f0
            SI(m5 = m1,                    movq_r2r(mm1, mm5));            // mm5 = l3 l1 r3 r1
            SI(m6 = _mm_unpacklo_pi16(m6, m3), punpcklwd_r2r(mm3, mm6));   // mm6 = l4 l2 r4 r2
            SI(m4 = _mm_madd_pi16(m4, m2), pmaddwd_r2r(mm2, mm4));  // mm4 = l2*f2+l0*f0 r2*f2+r0*f0
            SI(m5 = _mm_madd_pi16(m5, m2), pmaddwd_r2r(mm2, mm5));  // mm5 = l3*f2+l1*f0 r3*f2+l1*f0
            SI(m2 = filterPtr[1],          movq_a2r(8, filterPtr, mm2));   // mm2 = f3 f1 f3 f1
            SI(m0 = _mm_add_pi32(m0, m4),  paddd_r2r(mm4, mm0));           // mm0 += s02*f02
            SI(m4 = m3,                    movq_r2r(mm3, mm4));            // mm4 = l1 r1 l0 r0
            SI(m1 = _mm_madd_pi16(m1, m2), pmaddwd_r2r(mm2, mm1)); // mm1 = l3*f3+l1*f1 r3*f3+l1*f1
            SI(m7 = _mm_add_pi32(m7, m5),  paddd_r2r(mm5, mm7));           // mm7 += s13*f02
            SI(m6 = _mm_madd_pi16(m6, m2), pmaddwd_r2r(mm2, mm6)); // mm6 = l4*f3+l2*f1 r4*f3+f4*f1
            SI(m2 = filterInPtr[3],        movq_a2r(24, filterInPtr, mm2));// mm2 = l3 r3 l2 r2
            SI(m0 = _mm_add_pi32(m0, m1),  paddd_r2r(mm1, mm0));           // mm0 += s31*f31
            SI(m1 = filterInPtr[4],        movq_a2r(32, filterInPtr, mm1));// mm1 = l5 r5 l4 r4
            SI(m7 = _mm_add_pi32(m7, m6),  paddd_r2r(mm6, mm7));           // mm7 += s42*f31
            SI(m3 = _mm_unpackhi_pi16(m3, m2), punpckhwd_r2r(mm2, mm3));   // mm3 = l3 l1 r3 r1
            SI(m6 = m2,                    movq_r2r(mm2, mm6));            // mm6 = l3 r3 l2 r2
            SI(m4 = _mm_unpackhi_pi16(m4, m2), punpcklwd_r2r(mm2, mm4));   // mm4 = l2 l0 r2 r0
            SI(m2 = filterPtr[2],          movq_a2r(16, filterInPtr, mm2));// mm2 = f2 f0 f2 f0
            SI(m5 = m3,                    movq_r2r(mm3, mm5));            // mm5 = l3 l1 r3 r1
            SI(m6 = _mm_unpackhi_pi16(m6, m1), punpcklwd_r2r(mm1, mm6));   // mm6 = l4 l2 r4 r2
            filterPtr += 4;
            SI(m4 = _mm_madd_pi16(m4, m2), pmaddwd_r2r(mm2, mm4)); // mm4 = l2*f2+l0*f0 r2*f2+r0*f0
            filterInPtr += 4;
            SI(m5 = _mm_madd_pi16(m5, m2), pmaddwd_r2r(mm2, mm5)); // mm5 = l3*f2+l1*f0 r3*f2+l1*f0
            SI(m2 = filterPtr[-1],         movq_a2r(-8, filterPtr, mm2));  // mm2 = f3 f1 f3 f1
            SI(m0 = _mm_add_pi32(m0, m4),  paddd_r2r(mm4, mm0));           // mm0 += s02*f02
            SI(m3 = _mm_madd_pi16(m3, m2), pmaddwd_r2r(mm2, mm3)); // mm3 = l3*f3+l1*f1 r3*f3+l1*f1
            SI(m7 = _mm_add_pi32(m7, m5),  paddd_r2r(mm5, mm7));           // mm7 += s13*f02
            SI(m6 = _mm_madd_pi16(m6, m2), pmaddwd_r2r(mm2, mm6)); // mm6 = l4*f3+l2*f1 r4*f3+f4*f1
            SI(m0 = _mm_add_pi32(m0, m3),  paddd_r2r(mm3, mm0));           // mm0 += s31*f31
            SI(m7 = _mm_add_pi32(m7, m6),  paddd_r2r(mm6, mm7));           // mm7 += s42*f31
        } while ((--filterCounter)!=0);

        SI(m4 = _mm_cvtsi32_si64(resultDivFactor),  movd_v2r(resultDivFactor, mm4));    // mm4 = shift
        // Divide mm0 by 8192 (= right-shift by 13)
        SI(m0 = _mm_srai_pi32(m0, m4), psrad_r2r(mm4, mm0));
        outPtr++;
        // Divide mm7 by 8192 (= right-shift by 13)
        SI(m7 = _mm_srai_pi32(m7, m4), psrad_r2r(mm4, mm7));
        inPtr++;
        // pack and store to [outPtr]
        SI(m0 = _mm_packs_pi32(m0, m7), packssdw_r2r(mm7, mm0));
        SI(*outPtr = m0,                movq_r2a(mm0, 0, outPtr));
    } while ((--counter)!=0);

    _mm_empty();
    return (numSamples & 0xfffffffe) - length;
}

#endif // ALLOW_MMX
