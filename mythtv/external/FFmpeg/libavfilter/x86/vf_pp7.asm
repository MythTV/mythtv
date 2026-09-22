;*****************************************************************************
;* x86-optimized functions for pp7 filter
;*
;* Copyright (c) 2005 Michael Niedermayer <michaelni@gmx.at>
;*
;* This file is part of FFmpeg.
;*
;* FFmpeg is free software; you can redistribute it and/or modify
;* it under the terms of the GNU General Public License as published by
;* the Free Software Foundation; either version 2 of the License, or
;* (at your option) any later version.
;*
;* FFmpeg is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;* GNU General Public License for more details.
;*
;* You should have received a copy of the GNU General Public License along
;* with FFmpeg; if not, write to the Free Software Foundation, Inc.,
;* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
;******************************************************************************

%include "libavutil/x86/x86util.asm"

SECTION .text

INIT_XMM sse2
;void ff_pp7_dctB_sse2(int16_t *dst, const int16_t *src)
cglobal pp7_dctB, 2, 2, 6, dst, src
    movq         m0, [srcq+8*0]
    movq         m5, [srcq+8*6]
    movq         m3, [srcq+8*3]
    movq         m1, [srcq+8*1]
    movq         m4, [srcq+8*5]
    movq         m2, [srcq+8*2]
    paddw        m0, m5
    movq         m5, [srcq+8*4]
    paddw        m3, m3
    paddw        m1, m4
    paddw        m2, m5

    SUMSUB_BA     w, 0, 3, 4
    SUMSUB_BA     w, 1, 2, 5

    SUMSUB_BA     w, 1, 0, 4
    movq     [dstq], m1
    paddw        m4, m2, m3
    paddw        m2, m2
    movq [dstq+8*2], m0
    paddw        m4, m3
    psubw        m3, m2
    movq [dstq+8*1], m4
    movq [dstq+8*3], m3
    RET
