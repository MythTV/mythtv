;******************************************************************************
;* MMX optimized DSP utils
;* Copyright (c) 2008 Loren Merritt
;*
;* This file is part of FFmpeg.
;*
;* FFmpeg is free software; you can redistribute it and/or
;* modify it under the terms of the GNU Lesser General Public
;* License as published by the Free Software Foundation; either
;* version 2.1 of the License, or (at your option) any later version.
;*
;* FFmpeg is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;* Lesser General Public License for more details.
;*
;* You should have received a copy of the GNU Lesser General Public
;* License along with FFmpeg; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
;******************************************************************************

%include "libavutil/x86/x86inc.asm"
%include "x86util.asm"

SECTION_RODATA
pb_f: times 16 db 15
pb_zzzzzzzz77777777: times 8 db -1
pb_7: times 8 db 7
pb_zzzz3333zzzzbbbb: db -1,-1,-1,-1,3,3,3,3,-1,-1,-1,-1,11,11,11,11
pb_zz11zz55zz99zzdd: db -1,-1,1,1,-1,-1,5,5,-1,-1,9,9,-1,-1,13,13
pb_revwords: db 14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1
pd_16384: times 4 dd 16384
pb_bswap32: db 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12

SECTION_TEXT

%macro SCALARPRODUCT 1
; int scalarproduct_int16(int16_t *v1, int16_t *v2, int order)
cglobal scalarproduct_int16_%1, 3,3,3, v1, v2, order
    shl orderq, 1
    add v1q, orderq
    add v2q, orderq
    neg orderq
    pxor    m2, m2
.loop:
    movu    m0, [v1q + orderq]
    movu    m1, [v1q + orderq + mmsize]
    pmaddwd m0, [v2q + orderq]
    pmaddwd m1, [v2q + orderq + mmsize]
    paddd   m2, m0
    paddd   m2, m1
    add     orderq, mmsize*2
    jl .loop
%if mmsize == 16
    movhlps m0, m2
    paddd   m2, m0
    pshuflw m0, m2, 0x4e
%else
    pshufw  m0, m2, 0x4e
%endif
    paddd   m2, m0
    movd   eax, m2
    RET

; int scalarproduct_and_madd_int16(int16_t *v1, int16_t *v2, int16_t *v3, int order, int mul)
cglobal scalarproduct_and_madd_int16_%1, 4,4,8, v1, v2, v3, order, mul
    shl orderq, 1
    movd    m7, mulm
%if mmsize == 16
    pshuflw m7, m7, 0
    punpcklqdq m7, m7
%else
    pshufw  m7, m7, 0
%endif
    pxor    m6, m6
    add v1q, orderq
    add v2q, orderq
    add v3q, orderq
    neg orderq
.loop:
    movu    m0, [v2q + orderq]
    movu    m1, [v2q + orderq + mmsize]
    mova    m4, [v1q + orderq]
    mova    m5, [v1q + orderq + mmsize]
    movu    m2, [v3q + orderq]
    movu    m3, [v3q + orderq + mmsize]
    pmaddwd m0, m4
    pmaddwd m1, m5
    pmullw  m2, m7
    pmullw  m3, m7
    paddd   m6, m0
    paddd   m6, m1
    paddw   m2, m4
    paddw   m3, m5
    mova    [v1q + orderq], m2
    mova    [v1q + orderq + mmsize], m3
    add     orderq, mmsize*2
    jl .loop
%if mmsize == 16
    movhlps m0, m6
    paddd   m6, m0
    pshuflw m0, m6, 0x4e
%else
    pshufw  m0, m6, 0x4e
%endif
    paddd   m6, m0
    movd   eax, m6
    RET
%endmacro

INIT_MMX
SCALARPRODUCT mmx2
INIT_XMM
SCALARPRODUCT sse2

%macro SCALARPRODUCT_LOOP 1
align 16
.loop%1:
    sub     orderq, mmsize*2
%if %1
    mova    m1, m4
    mova    m4, [v2q + orderq]
    mova    m0, [v2q + orderq + mmsize]
    palignr m1, m0, %1
    palignr m0, m4, %1
    mova    m3, m5
    mova    m5, [v3q + orderq]
    mova    m2, [v3q + orderq + mmsize]
    palignr m3, m2, %1
    palignr m2, m5, %1
%else
    mova    m0, [v2q + orderq]
    mova    m1, [v2q + orderq + mmsize]
    mova    m2, [v3q + orderq]
    mova    m3, [v3q + orderq + mmsize]
%endif
    %define t0  [v1q + orderq]
    %define t1  [v1q + orderq + mmsize]
%if ARCH_X86_64
    mova    m8, t0
    mova    m9, t1
    %define t0  m8
    %define t1  m9
%endif
    pmaddwd m0, t0
    pmaddwd m1, t1
    pmullw  m2, m7
    pmullw  m3, m7
    paddw   m2, t0
    paddw   m3, t1
    paddd   m6, m0
    paddd   m6, m1
    mova    [v1q + orderq], m2
    mova    [v1q + orderq + mmsize], m3
    jg .loop%1
%if %1
    jmp .end
%endif
%endmacro

; int scalarproduct_and_madd_int16(int16_t *v1, int16_t *v2, int16_t *v3, int order, int mul)
cglobal scalarproduct_and_madd_int16_ssse3, 4,5,10, v1, v2, v3, order, mul
    shl orderq, 1
    movd    m7, mulm
    pshuflw m7, m7, 0
    punpcklqdq m7, m7
    pxor    m6, m6
    mov    r4d, v2d
    and    r4d, 15
    and    v2q, ~15
    and    v3q, ~15
    mova    m4, [v2q + orderq]
    mova    m5, [v3q + orderq]
    ; linear is faster than branch tree or jump table, because the branches taken are cyclic (i.e. predictable)
    cmp    r4d, 0
    je .loop0
    cmp    r4d, 2
    je .loop2
    cmp    r4d, 4
    je .loop4
    cmp    r4d, 6
    je .loop6
    cmp    r4d, 8
    je .loop8
    cmp    r4d, 10
    je .loop10
    cmp    r4d, 12
    je .loop12
SCALARPRODUCT_LOOP 14
SCALARPRODUCT_LOOP 12
SCALARPRODUCT_LOOP 10
SCALARPRODUCT_LOOP 8
SCALARPRODUCT_LOOP 6
SCALARPRODUCT_LOOP 4
SCALARPRODUCT_LOOP 2
SCALARPRODUCT_LOOP 0
.end:
    movhlps m0, m6
    paddd   m6, m0
    pshuflw m0, m6, 0x4e
    paddd   m6, m0
    movd   eax, m6
    RET


;-----------------------------------------------------------------------------
; void ff_apply_window_int16(int16_t *output, const int16_t *input,
;                            const int16_t *window, unsigned int len)
;-----------------------------------------------------------------------------

%macro REVERSE_WORDS_MMXEXT 1-2
    pshufw   %1, %1, 0x1B
%endmacro

%macro REVERSE_WORDS_SSE2 1-2
    pshuflw  %1, %1, 0x1B
    pshufhw  %1, %1, 0x1B
    pshufd   %1, %1, 0x4E
%endmacro

%macro REVERSE_WORDS_SSSE3 2
    pshufb  %1, %2
%endmacro

; dst = (dst * src) >> 15
; pmulhw cuts off the bottom bit, so we have to lshift by 1 and add it back
; in from the pmullw result.
%macro MUL16FIXED_MMXEXT 3 ; dst, src, temp
    mova    %3, %1
    pmulhw  %1, %2
    pmullw  %3, %2
    psrlw   %3, 15
    psllw   %1, 1
    por     %1, %3
%endmacro

; dst = ((dst * src) + (1<<14)) >> 15
%macro MUL16FIXED_SSSE3 3 ; dst, src, unused
    pmulhrsw   %1, %2
%endmacro

%macro APPLY_WINDOW_INT16 3 ; %1=instruction set, %2=mmxext/sse2 bit exact version, %3=has_ssse3
cglobal apply_window_int16_%1, 4,5,6, output, input, window, offset, offset2
    lea     offset2q, [offsetq-mmsize]
%if %2
    mova          m5, [pd_16384]
%elifidn %1, ssse3
    mova          m5, [pb_revwords]
    ALIGN 16
%endif
.loop:
%if %2
    ; This version expands 16-bit to 32-bit, multiplies by the window,
    ; adds 16384 for rounding, right shifts 15, then repacks back to words to
    ; save to the output. The window is reversed for the second half.
    mova          m3, [windowq+offset2q]
    mova          m4, [ inputq+offset2q]
    pxor          m0, m0
    punpcklwd     m0, m3
    punpcklwd     m1, m4
    pmaddwd       m0, m1
    paddd         m0, m5
    psrad         m0, 15
    pxor          m2, m2
    punpckhwd     m2, m3
    punpckhwd     m1, m4
    pmaddwd       m2, m1
    paddd         m2, m5
    psrad         m2, 15
    packssdw      m0, m2
    mova  [outputq+offset2q], m0
    REVERSE_WORDS m3
    mova          m4, [ inputq+offsetq]
    pxor          m0, m0
    punpcklwd     m0, m3
    punpcklwd     m1, m4
    pmaddwd       m0, m1
    paddd         m0, m5
    psrad         m0, 15
    pxor          m2, m2
    punpckhwd     m2, m3
    punpckhwd     m1, m4
    pmaddwd       m2, m1
    paddd         m2, m5
    psrad         m2, 15
    packssdw      m0, m2
    mova  [outputq+offsetq], m0
%elif %3
    ; This version does the 16x16->16 multiplication in-place without expanding
    ; to 32-bit. The ssse3 version is bit-identical.
    mova          m0, [windowq+offset2q]
    mova          m1, [ inputq+offset2q]
    pmulhrsw      m1, m0
    REVERSE_WORDS m0, m5
    pmulhrsw      m0, [ inputq+offsetq ]
    mova  [outputq+offset2q], m1
    mova  [outputq+offsetq ], m0
%else
    ; This version does the 16x16->16 multiplication in-place without expanding
    ; to 32-bit. The mmxext and sse2 versions do not use rounding, and
    ; therefore are not bit-identical to the C version.
    mova          m0, [windowq+offset2q]
    mova          m1, [ inputq+offset2q]
    mova          m2, [ inputq+offsetq ]
    MUL16FIXED    m1, m0, m3
    REVERSE_WORDS m0
    MUL16FIXED    m2, m0, m3
    mova  [outputq+offset2q], m1
    mova  [outputq+offsetq ], m2
%endif
    add      offsetd, mmsize
    sub     offset2d, mmsize
    jae .loop
    REP_RET
%endmacro

INIT_MMX
%define REVERSE_WORDS REVERSE_WORDS_MMXEXT
%define MUL16FIXED MUL16FIXED_MMXEXT
APPLY_WINDOW_INT16 mmxext,     0, 0
APPLY_WINDOW_INT16 mmxext_ba,  1, 0
INIT_XMM
%define REVERSE_WORDS REVERSE_WORDS_SSE2
APPLY_WINDOW_INT16 sse2,       0, 0
APPLY_WINDOW_INT16 sse2_ba,    1, 0
APPLY_WINDOW_INT16 ssse3_atom, 0, 1
%define REVERSE_WORDS REVERSE_WORDS_SSSE3
APPLY_WINDOW_INT16 ssse3,      0, 1


; void add_hfyu_median_prediction_mmx2(uint8_t *dst, const uint8_t *top, const uint8_t *diff, int w, int *left, int *left_top)
cglobal add_hfyu_median_prediction_mmx2, 6,6,0, dst, top, diff, w, left, left_top
    movq    mm0, [topq]
    movq    mm2, mm0
    movd    mm4, [left_topq]
    psllq   mm2, 8
    movq    mm1, mm0
    por     mm4, mm2
    movd    mm3, [leftq]
    psubb   mm0, mm4 ; t-tl
    add    dstq, wq
    add    topq, wq
    add   diffq, wq
    neg      wq
    jmp .skip
.loop:
    movq    mm4, [topq+wq]
    movq    mm0, mm4
    psllq   mm4, 8
    por     mm4, mm1
    movq    mm1, mm0 ; t
    psubb   mm0, mm4 ; t-tl
.skip:
    movq    mm2, [diffq+wq]
%assign i 0
%rep 8
    movq    mm4, mm0
    paddb   mm4, mm3 ; t-tl+l
    movq    mm5, mm3
    pmaxub  mm3, mm1
    pminub  mm5, mm1
    pminub  mm3, mm4
    pmaxub  mm3, mm5 ; median
    paddb   mm3, mm2 ; +residual
%if i==0
    movq    mm7, mm3
    psllq   mm7, 56
%else
    movq    mm6, mm3
    psrlq   mm7, 8
    psllq   mm6, 56
    por     mm7, mm6
%endif
%if i<7
    psrlq   mm0, 8
    psrlq   mm1, 8
    psrlq   mm2, 8
%endif
%assign i i+1
%endrep
    movq [dstq+wq], mm7
    add      wq, 8
    jl .loop
    movzx   r2d, byte [dstq-1]
    mov [leftq], r2d
    movzx   r2d, byte [topq-1]
    mov [left_topq], r2d
    RET


%macro ADD_HFYU_LEFT_LOOP 1 ; %1 = is_aligned
    add     srcq, wq
    add     dstq, wq
    neg     wq
%%.loop:
    mova    m1, [srcq+wq]
    mova    m2, m1
    psllw   m1, 8
    paddb   m1, m2
    mova    m2, m1
    pshufb  m1, m3
    paddb   m1, m2
    pshufb  m0, m5
    mova    m2, m1
    pshufb  m1, m4
    paddb   m1, m2
%if mmsize == 16
    mova    m2, m1
    pshufb  m1, m6
    paddb   m1, m2
%endif
    paddb   m0, m1
%if %1
    mova    [dstq+wq], m0
%else
    movq    [dstq+wq], m0
    movhps  [dstq+wq+8], m0
%endif
    add     wq, mmsize
    jl %%.loop
    mov     eax, mmsize-1
    sub     eax, wd
    movd    m1, eax
    pshufb  m0, m1
    movd    eax, m0
    RET
%endmacro

; int add_hfyu_left_prediction(uint8_t *dst, const uint8_t *src, int w, int left)
INIT_MMX
cglobal add_hfyu_left_prediction_ssse3, 3,3,7, dst, src, w, left
.skip_prologue:
    mova    m5, [pb_7]
    mova    m4, [pb_zzzz3333zzzzbbbb]
    mova    m3, [pb_zz11zz55zz99zzdd]
    movd    m0, leftm
    psllq   m0, 56
    ADD_HFYU_LEFT_LOOP 1

INIT_XMM
cglobal add_hfyu_left_prediction_sse4, 3,3,7, dst, src, w, left
    mova    m5, [pb_f]
    mova    m6, [pb_zzzzzzzz77777777]
    mova    m4, [pb_zzzz3333zzzzbbbb]
    mova    m3, [pb_zz11zz55zz99zzdd]
    movd    m0, leftm
    pslldq  m0, 15
    test    srcq, 15
    jnz add_hfyu_left_prediction_ssse3.skip_prologue
    test    dstq, 15
    jnz .unaligned
    ADD_HFYU_LEFT_LOOP 1
.unaligned:
    ADD_HFYU_LEFT_LOOP 0


; float scalarproduct_float_sse(const float *v1, const float *v2, int len)
cglobal scalarproduct_float_sse, 3,3,2, v1, v2, offset
    neg offsetq
    shl offsetq, 2
    sub v1q, offsetq
    sub v2q, offsetq
    xorps xmm0, xmm0
    .loop:
        movaps   xmm1, [v1q+offsetq]
        mulps    xmm1, [v2q+offsetq]
        addps    xmm0, xmm1
        add      offsetq, 16
        js       .loop
    movhlps xmm1, xmm0
    addps   xmm0, xmm1
    movss   xmm1, xmm0
    shufps  xmm0, xmm0, 1
    addss   xmm0, xmm1
%if ARCH_X86_64 == 0
    movss   r0m,  xmm0
    fld     dword r0m
%endif
    RET

; extern void ff_emu_edge_core(uint8_t *buf, const uint8_t *src, x86_reg linesize,
;                              x86_reg start_y, x86_reg end_y, x86_reg block_h,
;                              x86_reg start_x, x86_reg end_x, x86_reg block_w);
;
; The actual function itself is below. It basically wraps a very simple
; w = end_x - start_x
; if (w) {
;   if (w > 22) {
;     jump to the slow loop functions
;   } else {
;     jump to the fast loop functions
;   }
; }
;
; ... and then the same for left/right extend also. See below for loop
; function implementations. Fast are fixed-width, slow is variable-width

%macro EMU_EDGE_FUNC 0
%if ARCH_X86_64
%define w_reg r7
cglobal emu_edge_core, 6, 9, 1
    mov         r8, r5          ; save block_h
%else
%define w_reg r6
cglobal emu_edge_core, 2, 7, 0
    mov         r4, r4m         ; end_y
    mov         r5, r5m         ; block_h
%endif

    ; start with vertical extend (top/bottom) and body pixel copy
    mov      w_reg, r7m
    sub      w_reg, r6m         ; w = start_x - end_x
    sub         r5, r4
%if ARCH_X86_64
    sub         r4, r3
%else
    sub         r4, dword r3m
%endif
    cmp      w_reg, 22
    jg .slow_v_extend_loop
%if ARCH_X86_32
    mov         r2, r2m         ; linesize
%endif
    sal      w_reg, 7           ; w * 128
%ifdef PIC
    lea        rax, [.emuedge_v_extend_1 - (.emuedge_v_extend_2 - .emuedge_v_extend_1)]
    add      w_reg, rax
%else
    lea      w_reg, [.emuedge_v_extend_1 - (.emuedge_v_extend_2 - .emuedge_v_extend_1)+w_reg]
%endif
    call     w_reg              ; fast top extend, body copy and bottom extend
.v_extend_end:

    ; horizontal extend (left/right)
    mov      w_reg, r6m         ; start_x
    sub         r0, w_reg
%if ARCH_X86_64
    mov         r3, r0          ; backup of buf+block_h*linesize
    mov         r5, r8
%else
    mov        r0m, r0          ; backup of buf+block_h*linesize
    mov         r5, r5m
%endif
    test     w_reg, w_reg
    jz .right_extend
    cmp      w_reg, 22
    jg .slow_left_extend_loop
    mov         r1, w_reg
    dec      w_reg
    ; FIXME we can do a if size == 1 here if that makes any speed difference, test me
    sar      w_reg, 1
    sal      w_reg, 6
    ; r0=buf+block_h*linesize,r7(64)/r6(32)=start_x offset for funcs
    ; r6(rax)/r3(ebx)=val,r2=linesize,r1=start_x,r5=block_h
%ifdef PIC
    lea        rax, [.emuedge_extend_left_2]
    add      w_reg, rax
%else
    lea      w_reg, [.emuedge_extend_left_2+w_reg]
%endif
    call     w_reg

    ; now r3(64)/r0(32)=buf,r2=linesize,r8/r5=block_h,r6/r3=val, r7/r6=end_x, r1=block_w
.right_extend:
%if ARCH_X86_32
    mov         r0, r0m
    mov         r5, r5m
%endif
    mov      w_reg, r7m         ; end_x
    mov         r1, r8m         ; block_w
    mov         r4, r1
    sub         r1, w_reg
    jz .h_extend_end            ; if (end_x == block_w) goto h_extend_end
    cmp         r1, 22
    jg .slow_right_extend_loop
    dec         r1
    ; FIXME we can do a if size == 1 here if that makes any speed difference, test me
    sar         r1, 1
    sal         r1, 6
%ifdef PIC
    lea        rax, [.emuedge_extend_right_2]
    add         r1, rax
%else
    lea         r1, [.emuedge_extend_right_2+r1]
%endif
    call        r1
.h_extend_end:
    RET

%if ARCH_X86_64
%define vall  al
%define valh  ah
%define valw  ax
%define valw2 r7w
%define valw3 r3w
%if WIN64
%define valw4 r7w
%else ; unix64
%define valw4 r3w
%endif
%define vald eax
%else
%define vall  bl
%define valh  bh
%define valw  bx
%define valw2 r6w
%define valw3 valw2
%define valw4 valw3
%define vald ebx
%define stack_offset 0x14
%endif

%endmacro

; macro to read/write a horizontal number of pixels (%2) to/from registers
; on x86-64, - fills xmm0-15 for consecutive sets of 16 pixels
;            - if (%2 & 15 == 8) fills the last 8 bytes into rax
;            - else if (%2 & 8)  fills 8 bytes into mm0
;            - if (%2 & 7 == 4)  fills the last 4 bytes into rax
;            - else if (%2 & 4)  fills 4 bytes into mm0-1
;            - if (%2 & 3 == 3)  fills 2 bytes into r7/r3, and 1 into eax
;              (note that we're using r3 for body/bottom because it's a shorter
;               opcode, and then the loop fits in 128 bytes)
;            - else              fills remaining bytes into rax
; on x86-32, - fills mm0-7 for consecutive sets of 8 pixels
;            - if (%2 & 7 == 4)  fills 4 bytes into ebx
;            - else if (%2 & 4)  fills 4 bytes into mm0-7
;            - if (%2 & 3 == 3)  fills 2 bytes into r6, and 1 into ebx
;            - else              fills remaining bytes into ebx
; writing data out is in the same way
%macro READ_NUM_BYTES 2
%assign %%src_off 0 ; offset in source buffer
%assign %%smidx   0 ; mmx register idx
%assign %%sxidx   0 ; xmm register idx

%if cpuflag(sse)
%rep %2/16
    movups xmm %+ %%sxidx, [r1+%%src_off]
%assign %%src_off %%src_off+16
%assign %%sxidx   %%sxidx+1
%endrep ; %2/16
%endif

%if ARCH_X86_64
%if (%2-%%src_off) == 8
    mov           rax, [r1+%%src_off]
%assign %%src_off %%src_off+8
%endif ; (%2-%%src_off) == 8
%endif ; x86-64

%rep (%2-%%src_off)/8
    movq    mm %+ %%smidx, [r1+%%src_off]
%assign %%src_off %%src_off+8
%assign %%smidx   %%smidx+1
%endrep ; (%2-%%dst_off)/8

%if (%2-%%src_off) == 4
    mov          vald, [r1+%%src_off]
%elif (%2-%%src_off) & 4
    movd    mm %+ %%smidx, [r1+%%src_off]
%assign %%src_off %%src_off+4
%endif ; (%2-%%src_off) ==/& 4

%if (%2-%%src_off) == 1
    mov          vall, [r1+%%src_off]
%elif (%2-%%src_off) == 2
    mov          valw, [r1+%%src_off]
%elif (%2-%%src_off) == 3
%ifidn %1, top
    mov         valw2, [r1+%%src_off]
%elifidn %1, body
    mov         valw3, [r1+%%src_off]
%elifidn %1, bottom
    mov         valw4, [r1+%%src_off]
%endif ; %1 ==/!= top
    mov          vall, [r1+%%src_off+2]
%endif ; (%2-%%src_off) == 1/2/3
%endmacro ; READ_NUM_BYTES

%macro WRITE_NUM_BYTES 2
%assign %%dst_off 0 ; offset in destination buffer
%assign %%dmidx   0 ; mmx register idx
%assign %%dxidx   0 ; xmm register idx

%if cpuflag(sse)
%rep %2/16
    movups [r0+%%dst_off], xmm %+ %%dxidx
%assign %%dst_off %%dst_off+16
%assign %%dxidx   %%dxidx+1
%endrep ; %2/16
%endif

%if ARCH_X86_64
%if (%2-%%dst_off) == 8
    mov    [r0+%%dst_off], rax
%assign %%dst_off %%dst_off+8
%endif ; (%2-%%dst_off) == 8
%endif ; x86-64

%rep (%2-%%dst_off)/8
    movq   [r0+%%dst_off], mm %+ %%dmidx
%assign %%dst_off %%dst_off+8
%assign %%dmidx   %%dmidx+1
%endrep ; (%2-%%dst_off)/8

%if (%2-%%dst_off) == 4
    mov    [r0+%%dst_off], vald
%elif (%2-%%dst_off) & 4
    movd   [r0+%%dst_off], mm %+ %%dmidx
%assign %%dst_off %%dst_off+4
%endif ; (%2-%%dst_off) ==/& 4

%if (%2-%%dst_off) == 1
    mov    [r0+%%dst_off], vall
%elif (%2-%%dst_off) == 2
    mov    [r0+%%dst_off], valw
%elif (%2-%%dst_off) == 3
%ifidn %1, top
    mov    [r0+%%dst_off], valw2
%elifidn %1, body
    mov    [r0+%%dst_off], valw3
%elifidn %1, bottom
    mov    [r0+%%dst_off], valw4
%endif ; %1 ==/!= top
    mov  [r0+%%dst_off+2], vall
%endif ; (%2-%%dst_off) == 1/2/3
%endmacro ; WRITE_NUM_BYTES

; vertical top/bottom extend and body copy fast loops
; these are function pointers to set-width line copy functions, i.e.
; they read a fixed number of pixels into set registers, and write
; those out into the destination buffer
; r0=buf,r1=src,r2=linesize,r3(64)/r3m(32)=start_x,r4=end_y,r5=block_h
; r6(eax/64)/r3(ebx/32)=val_reg
%macro VERTICAL_EXTEND 0
%assign %%n 1
%rep 22
ALIGN 128
.emuedge_v_extend_ %+ %%n:
    ; extend pixels above body
%if ARCH_X86_64
    test           r3 , r3                   ; if (!start_y)
    jz .emuedge_copy_body_ %+ %%n %+ _loop   ;   goto body
%else ; ARCH_X86_32
    cmp      dword r3m, 0
    je .emuedge_copy_body_ %+ %%n %+ _loop
%endif ; ARCH_X86_64/32
    READ_NUM_BYTES  top,    %%n              ; read bytes
.emuedge_extend_top_ %+ %%n %+ _loop:        ; do {
    WRITE_NUM_BYTES top,    %%n              ;   write bytes
    add            r0 , r2                   ;   dst += linesize
%if ARCH_X86_64
    dec            r3d
%else ; ARCH_X86_32
    dec      dword r3m
%endif ; ARCH_X86_64/32
    jnz .emuedge_extend_top_ %+ %%n %+ _loop ; } while (--start_y)

    ; copy body pixels
.emuedge_copy_body_ %+ %%n %+ _loop:         ; do {
    READ_NUM_BYTES  body,   %%n              ;   read bytes
    WRITE_NUM_BYTES body,   %%n              ;   write bytes
    add            r0 , r2                   ;   dst += linesize
    add            r1 , r2                   ;   src += linesize
    dec            r4d
    jnz .emuedge_copy_body_ %+ %%n %+ _loop  ; } while (--end_y)

    ; copy bottom pixels
    test           r5 , r5                   ; if (!block_h)
    jz .emuedge_v_extend_end_ %+ %%n         ;   goto end
    sub            r1 , r2                   ; src -= linesize
    READ_NUM_BYTES  bottom, %%n              ; read bytes
.emuedge_extend_bottom_ %+ %%n %+ _loop:     ; do {
    WRITE_NUM_BYTES bottom, %%n              ;   write bytes
    add            r0 , r2                   ;   dst += linesize
    dec            r5d
    jnz .emuedge_extend_bottom_ %+ %%n %+ _loop ; } while (--block_h)

.emuedge_v_extend_end_ %+ %%n:
%if ARCH_X86_64
    ret
%else ; ARCH_X86_32
    rep ret
%endif ; ARCH_X86_64/32
%assign %%n %%n+1
%endrep
%endmacro VERTICAL_EXTEND

; left/right (horizontal) fast extend functions
; these are essentially identical to the vertical extend ones above,
; just left/right separated because number of pixels to extend is
; obviously not the same on both sides.
; for reading, pixels are placed in eax (x86-64) or ebx (x86-64) in the
; lowest two bytes of the register (so val*0x0101), and are splatted
; into each byte of mm0 as well if n_pixels >= 8

%macro READ_V_PIXEL 2
    mov        vall, %2
    mov        valh, vall
%if %1 >= 8
    movd        mm0, vald
%if cpuflag(mmx2)
    pshufw      mm0, mm0, 0
%else ; mmx
    punpcklwd   mm0, mm0
    punpckldq   mm0, mm0
%endif ; sse
%endif ; %1 >= 8
%endmacro

%macro WRITE_V_PIXEL 2
%assign %%dst_off 0
%rep %1/8
    movq [%2+%%dst_off], mm0
%assign %%dst_off %%dst_off+8
%endrep
%if %1 & 4
%if %1 >= 8
    movd [%2+%%dst_off], mm0
%else ; %1 < 8
    mov  [%2+%%dst_off]  , valw
    mov  [%2+%%dst_off+2], valw
%endif ; %1 >=/< 8
%assign %%dst_off %%dst_off+4
%endif ; %1 & 4
%if %1&2
    mov  [%2+%%dst_off], valw
%endif ; %1 & 2
%endmacro

; r0=buf+block_h*linesize, r1=start_x, r2=linesize, r5=block_h, r6/r3=val
%macro LEFT_EXTEND 0
%assign %%n 2
%rep 11
ALIGN 64
.emuedge_extend_left_ %+ %%n:          ; do {
    sub         r0, r2                 ;   dst -= linesize
    READ_V_PIXEL  %%n, [r0+r1]         ;   read pixels
    WRITE_V_PIXEL %%n, r0              ;   write pixels
    dec         r5
    jnz .emuedge_extend_left_ %+ %%n   ; } while (--block_h)
%if ARCH_X86_64
    ret
%else ; ARCH_X86_32
    rep ret
%endif ; ARCH_X86_64/32
%assign %%n %%n+2
%endrep
%endmacro ; LEFT_EXTEND

; r3/r0=buf+block_h*linesize, r2=linesize, r8/r5=block_h, r0/r6=end_x, r6/r3=val
%macro RIGHT_EXTEND 0
%assign %%n 2
%rep 11
ALIGN 64
.emuedge_extend_right_ %+ %%n:          ; do {
%if ARCH_X86_64
    sub        r3, r2                   ;   dst -= linesize
    READ_V_PIXEL  %%n, [r3+w_reg-1]     ;   read pixels
    WRITE_V_PIXEL %%n, r3+r4-%%n        ;   write pixels
    dec       r8
%else ; ARCH_X86_32
    sub        r0, r2                   ;   dst -= linesize
    READ_V_PIXEL  %%n, [r0+w_reg-1]     ;   read pixels
    WRITE_V_PIXEL %%n, r0+r4-%%n        ;   write pixels
    dec     r5
%endif ; ARCH_X86_64/32
    jnz .emuedge_extend_right_ %+ %%n   ; } while (--block_h)
%if ARCH_X86_64
    ret
%else ; ARCH_X86_32
    rep ret
%endif ; ARCH_X86_64/32
%assign %%n %%n+2
%endrep

%if ARCH_X86_32
%define stack_offset 0x10
%endif
%endmacro ; RIGHT_EXTEND

; below follow the "slow" copy/extend functions, these act on a non-fixed
; width specified in a register, and run a loop to copy the full amount
; of bytes. They are optimized for copying of large amounts of pixels per
; line, so they unconditionally splat data into mm registers to copy 8
; bytes per loop iteration. It could be considered to use xmm for x86-64
; also, but I haven't optimized this as much (i.e. FIXME)
%macro V_COPY_NPX 4-5
%if %0 == 4
    test     w_reg, %4
    jz .%1_skip_%4_px
%else ; %0 == 5
.%1_%4_px_loop:
%endif
    %3          %2, [r1+cnt_reg]
    %3 [r0+cnt_reg], %2
    add    cnt_reg, %4
%if %0 == 5
    sub      w_reg, %4
    test     w_reg, %5
    jnz .%1_%4_px_loop
%endif
.%1_skip_%4_px:
%endmacro

%macro V_COPY_ROW 2
%ifidn %1, bottom
    sub         r1, linesize
%endif
.%1_copy_loop:
    xor    cnt_reg, cnt_reg
%if notcpuflag(sse)
%define linesize r2m
    V_COPY_NPX %1,  mm0, movq,    8, 0xFFFFFFF8
%else ; sse
    V_COPY_NPX %1, xmm0, movups, 16, 0xFFFFFFF0
%if ARCH_X86_64
%define linesize r2
    V_COPY_NPX %1, rax , mov,     8
%else ; ARCH_X86_32
%define linesize r2m
    V_COPY_NPX %1,  mm0, movq,    8
%endif ; ARCH_X86_64/32
%endif ; sse
    V_COPY_NPX %1, vald, mov,     4
    V_COPY_NPX %1, valw, mov,     2
    V_COPY_NPX %1, vall, mov,     1
    mov      w_reg, cnt_reg
%ifidn %1, body
    add         r1, linesize
%endif
    add         r0, linesize
    dec         %2
    jnz .%1_copy_loop
%endmacro

%macro SLOW_V_EXTEND 0
.slow_v_extend_loop:
; r0=buf,r1=src,r2(64)/r2m(32)=linesize,r3(64)/r3m(32)=start_x,r4=end_y,r5=block_h
; r8(64)/r3(later-64)/r2(32)=cnt_reg,r6(64)/r3(32)=val_reg,r7(64)/r6(32)=w=end_x-start_x
%if ARCH_X86_64
    push        r8              ; save old value of block_h
    test        r3, r3
%define cnt_reg r8
    jz .do_body_copy            ; if (!start_y) goto do_body_copy
    V_COPY_ROW top, r3
%else
    cmp  dword r3m, 0
%define cnt_reg r2
    je .do_body_copy            ; if (!start_y) goto do_body_copy
    V_COPY_ROW top, dword r3m
%endif

.do_body_copy:
    V_COPY_ROW body, r4

%if ARCH_X86_64
    pop         r8              ; restore old value of block_h
%define cnt_reg r3
%endif
    test        r5, r5
%if ARCH_X86_64
    jz .v_extend_end
%else
    jz .skip_bottom_extend
%endif
    V_COPY_ROW bottom, r5
%if ARCH_X86_32
.skip_bottom_extend:
    mov         r2, r2m
%endif
    jmp .v_extend_end
%endmacro

%macro SLOW_LEFT_EXTEND 0
.slow_left_extend_loop:
; r0=buf+block_h*linesize,r2=linesize,r6(64)/r3(32)=val,r5=block_h,r4=cntr,r7/r6=start_x
    mov         r4, 8
    sub         r0, linesize
    READ_V_PIXEL 8, [r0+w_reg]
.left_extend_8px_loop:
    movq [r0+r4-8], mm0
    add         r4, 8
    cmp         r4, w_reg
    jle .left_extend_8px_loop
    sub         r4, 8
    cmp         r4, w_reg
    jge .left_extend_loop_end
.left_extend_2px_loop:
    mov    [r0+r4], valw
    add         r4, 2
    cmp         r4, w_reg
    jl .left_extend_2px_loop
.left_extend_loop_end:
    dec         r5
    jnz .slow_left_extend_loop
%if ARCH_X86_32
    mov         r2, r2m
%endif
    jmp .right_extend
%endmacro

%macro SLOW_RIGHT_EXTEND 0
.slow_right_extend_loop:
; r3(64)/r0(32)=buf+block_h*linesize,r2=linesize,r4=block_w,r8(64)/r5(32)=block_h,
; r7(64)/r6(32)=end_x,r6/r3=val,r1=cntr
%if ARCH_X86_64
%define buf_reg r3
%define bh_reg r8
%else
%define buf_reg r0
%define bh_reg r5
%endif
    lea         r1, [r4-8]
    sub    buf_reg, linesize
    READ_V_PIXEL 8, [buf_reg+w_reg-1]
.right_extend_8px_loop:
    movq [buf_reg+r1], mm0
    sub         r1, 8
    cmp         r1, w_reg
    jge .right_extend_8px_loop
    add         r1, 8
    cmp         r1, w_reg
    je .right_extend_loop_end
.right_extend_2px_loop:
    sub         r1, 2
    mov [buf_reg+r1], valw
    cmp         r1, w_reg
    jg .right_extend_2px_loop
.right_extend_loop_end:
    dec         bh_reg
    jnz .slow_right_extend_loop
    jmp .h_extend_end
%endmacro

%macro emu_edge 1
INIT_XMM %1
EMU_EDGE_FUNC
VERTICAL_EXTEND
LEFT_EXTEND
RIGHT_EXTEND
SLOW_V_EXTEND
SLOW_LEFT_EXTEND
SLOW_RIGHT_EXTEND
%endmacro

emu_edge sse
%if ARCH_X86_32
emu_edge mmx
%endif

;-----------------------------------------------------------------------------
; void ff_vector_clip_int32(int32_t *dst, const int32_t *src, int32_t min,
;                           int32_t max, unsigned int len)
;-----------------------------------------------------------------------------

; %1 = number of xmm registers used
; %2 = number of inline load/process/store loops per asm loop
; %3 = process 4*mmsize (%3=0) or 8*mmsize (%3=1) bytes per loop
; %4 = CLIPD function takes min/max as float instead of int (CLIPD_SSE2)
; %5 = suffix
%macro VECTOR_CLIP_INT32 4-5
cglobal vector_clip_int32%5, 5,5,%1, dst, src, min, max, len
%if %4
    cvtsi2ss  m4, minm
    cvtsi2ss  m5, maxm
%else
    movd      m4, minm
    movd      m5, maxm
%endif
    SPLATD    m4
    SPLATD    m5
.loop:
%assign %%i 1
%rep %2
    mova      m0,  [srcq+mmsize*0*%%i]
    mova      m1,  [srcq+mmsize*1*%%i]
    mova      m2,  [srcq+mmsize*2*%%i]
    mova      m3,  [srcq+mmsize*3*%%i]
%if %3
    mova      m7,  [srcq+mmsize*4*%%i]
    mova      m8,  [srcq+mmsize*5*%%i]
    mova      m9,  [srcq+mmsize*6*%%i]
    mova      m10, [srcq+mmsize*7*%%i]
%endif
    CLIPD  m0,  m4, m5, m6
    CLIPD  m1,  m4, m5, m6
    CLIPD  m2,  m4, m5, m6
    CLIPD  m3,  m4, m5, m6
%if %3
    CLIPD  m7,  m4, m5, m6
    CLIPD  m8,  m4, m5, m6
    CLIPD  m9,  m4, m5, m6
    CLIPD  m10, m4, m5, m6
%endif
    mova  [dstq+mmsize*0*%%i], m0
    mova  [dstq+mmsize*1*%%i], m1
    mova  [dstq+mmsize*2*%%i], m2
    mova  [dstq+mmsize*3*%%i], m3
%if %3
    mova  [dstq+mmsize*4*%%i], m7
    mova  [dstq+mmsize*5*%%i], m8
    mova  [dstq+mmsize*6*%%i], m9
    mova  [dstq+mmsize*7*%%i], m10
%endif
%assign %%i %%i+1
%endrep
    add     srcq, mmsize*4*(%2+%3)
    add     dstq, mmsize*4*(%2+%3)
    sub     lend, mmsize*(%2+%3)
    jg .loop
    REP_RET
%endmacro

INIT_MMX mmx
%define SPLATD SPLATD_MMX
%define CLIPD CLIPD_MMX
VECTOR_CLIP_INT32 0, 1, 0, 0
INIT_XMM sse2
%define SPLATD SPLATD_SSE2
VECTOR_CLIP_INT32 6, 1, 0, 0, _int
%define CLIPD CLIPD_SSE2
VECTOR_CLIP_INT32 6, 2, 0, 1
INIT_XMM sse4
%define CLIPD CLIPD_SSE41
%ifdef m8
VECTOR_CLIP_INT32 11, 1, 1, 0
%else
VECTOR_CLIP_INT32 6, 1, 0, 0
%endif

;-----------------------------------------------------------------------------
; void vector_fmul(float *dst, const float *src0, const float *src1, int len)
;-----------------------------------------------------------------------------
%macro VECTOR_FMUL 0
cglobal vector_fmul, 4,4,2, dst, src0, src1, len
    lea       lenq, [lend*4 - 2*mmsize]
ALIGN 16
.loop
    mova      m0,   [src0q + lenq]
    mova      m1,   [src0q + lenq + mmsize]
    mulps     m0, m0, [src1q + lenq]
    mulps     m1, m1, [src1q + lenq + mmsize]
    mova      [dstq + lenq], m0
    mova      [dstq + lenq + mmsize], m1

    sub       lenq, 2*mmsize
    jge       .loop
%if mmsize == 32
    vzeroupper
    RET
%else
    REP_RET
%endif
%endmacro

INIT_XMM sse
VECTOR_FMUL
%if HAVE_AVX
INIT_YMM avx
VECTOR_FMUL
%endif

;-----------------------------------------------------------------------------
; void vector_fmul_reverse(float *dst, const float *src0, const float *src1,
;                          int len)
;-----------------------------------------------------------------------------
%macro VECTOR_FMUL_REVERSE 0
cglobal vector_fmul_reverse, 4,4,2, dst, src0, src1, len
    lea       lenq, [lend*4 - 2*mmsize]
ALIGN 16
.loop
%if cpuflag(avx)
    vmovaps     xmm0, [src1q + 16]
    vinsertf128 m0, m0, [src1q], 1
    vshufps     m0, m0, m0, q0123
    vmovaps     xmm1, [src1q + mmsize + 16]
    vinsertf128 m1, m1, [src1q + mmsize], 1
    vshufps     m1, m1, m1, q0123
%else
    mova    m0, [src1q]
    mova    m1, [src1q + mmsize]
    shufps  m0, m0, q0123
    shufps  m1, m1, q0123
%endif
    mulps   m0, m0, [src0q + lenq + mmsize]
    mulps   m1, m1, [src0q + lenq]
    mova    [dstq + lenq + mmsize], m0
    mova    [dstq + lenq], m1
    add     src1q, 2*mmsize
    sub     lenq,  2*mmsize
    jge     .loop
%if mmsize == 32
    vzeroupper
    RET
%else
    REP_RET
%endif
%endmacro

INIT_XMM sse
VECTOR_FMUL_REVERSE
%if HAVE_AVX
INIT_YMM avx
VECTOR_FMUL_REVERSE
%endif

;-----------------------------------------------------------------------------
; vector_fmul_add(float *dst, const float *src0, const float *src1,
;                 const float *src2, int len)
;-----------------------------------------------------------------------------
%macro VECTOR_FMUL_ADD 0
cglobal vector_fmul_add, 5,5,2, dst, src0, src1, src2, len
    lea       lenq, [lend*4 - 2*mmsize]
ALIGN 16
.loop
    mova    m0,   [src0q + lenq]
    mova    m1,   [src0q + lenq + mmsize]
    mulps   m0, m0, [src1q + lenq]
    mulps   m1, m1, [src1q + lenq + mmsize]
    addps   m0, m0, [src2q + lenq]
    addps   m1, m1, [src2q + lenq + mmsize]
    mova    [dstq + lenq], m0
    mova    [dstq + lenq + mmsize], m1

    sub     lenq,   2*mmsize
    jge     .loop
%if mmsize == 32
    vzeroupper
    RET
%else
    REP_RET
%endif
%endmacro

INIT_XMM sse
VECTOR_FMUL_ADD
%if HAVE_AVX
INIT_YMM avx
VECTOR_FMUL_ADD
%endif

;-----------------------------------------------------------------------------
; void ff_butterflies_float_interleave(float *dst, const float *src0,
;                                      const float *src1, int len);
;-----------------------------------------------------------------------------

%macro BUTTERFLIES_FLOAT_INTERLEAVE 0
cglobal butterflies_float_interleave, 4,4,3, dst, src0, src1, len
%if ARCH_X86_64
    movsxd    lenq, lend
%endif
    test      lenq, lenq
    jz .end
    shl       lenq, 2
    lea      src0q, [src0q +   lenq]
    lea      src1q, [src1q +   lenq]
    lea       dstq, [ dstq + 2*lenq]
    neg       lenq
.loop:
    mova        m0, [src0q + lenq]
    mova        m1, [src1q + lenq]
    subps       m2, m0, m1
    addps       m0, m0, m1
    unpcklps    m1, m0, m2
    unpckhps    m0, m0, m2
%if cpuflag(avx)
    vextractf128 [dstq + 2*lenq     ], m1, 0
    vextractf128 [dstq + 2*lenq + 16], m0, 0
    vextractf128 [dstq + 2*lenq + 32], m1, 1
    vextractf128 [dstq + 2*lenq + 48], m0, 1
%else
    mova [dstq + 2*lenq         ], m1
    mova [dstq + 2*lenq + mmsize], m0
%endif
    add       lenq, mmsize
    jl .loop
%if mmsize == 32
    vzeroupper
    RET
%endif
.end:
    REP_RET
%endmacro

INIT_XMM sse
BUTTERFLIES_FLOAT_INTERLEAVE
%if HAVE_AVX
INIT_YMM avx
BUTTERFLIES_FLOAT_INTERLEAVE
%endif

INIT_XMM sse2
; %1 = aligned/unaligned
%macro BSWAP_LOOPS_SSE2  1
    mov      r3, r2
    sar      r2, 3
    jz       .left4_%1
.loop8_%1:
    mov%1    m0, [r1 +  0]
    mov%1    m1, [r1 + 16]
    pshuflw  m0, m0, 10110001b
    pshuflw  m1, m1, 10110001b
    pshufhw  m0, m0, 10110001b
    pshufhw  m1, m1, 10110001b
    mova     m2, m0
    mova     m3, m1
    psllw    m0, 8
    psllw    m1, 8
    psrlw    m2, 8
    psrlw    m3, 8
    por      m2, m0
    por      m3, m1
    mova     [r0 +  0], m2
    mova     [r0 + 16], m3
    add      r1, 32
    add      r0, 32
    dec      r2
    jnz      .loop8_%1
.left4_%1:
    mov      r2, r3
    and      r3, 4
    jz       .left
    mov%1    m0, [r1]
    pshuflw  m0, m0, 10110001b
    pshufhw  m0, m0, 10110001b
    mova     m2, m0
    psllw    m0, 8
    psrlw    m2, 8
    por      m2, m0
    mova     [r0], m2
    add      r1, 16
    add      r0, 16
%endmacro

; void bswap_buf(uint32_t *dst, const uint32_t *src, int w);
cglobal bswap32_buf, 3,4,5
    mov      r3, r1
    and      r3, 15
    jz       .start_align
    BSWAP_LOOPS_SSE2  u
    jmp      .left
.start_align:
    BSWAP_LOOPS_SSE2  a
.left:
    and      r2, 3
    jz       .end
.loop2:
    mov      r3d, [r1]
    bswap    r3d
    mov      [r0], r3d
    add      r1, 4
    add      r0, 4
    dec      r2
    jnz      .loop2
.end
    RET

; %1 = aligned/unaligned
%macro BSWAP_LOOPS_SSSE3  1
    mov      r3, r2
    sar      r2, 3
    jz       .left4_%1
.loop8_%1:
    mov%1    m0, [r1 +  0]
    mov%1    m1, [r1 + 16]
    pshufb   m0, m2
    pshufb   m1, m2
    mova     [r0 +  0], m0
    mova     [r0 + 16], m1
    add      r0, 32
    add      r1, 32
    dec      r2
    jnz      .loop8_%1
.left4_%1:
    mov      r2, r3
    and      r3, 4
    jz       .left2
    mov%1    m0, [r1]
    pshufb   m0, m2
    mova     [r0], m0
    add      r1, 16
    add      r0, 16
%endmacro

INIT_XMM ssse3
; void bswap_buf(uint32_t *dst, const uint32_t *src, int w);
cglobal bswap32_buf, 3,4,3
    mov      r3, r1
    mova     m2, [pb_bswap32]
    and      r3, 15
    jz       .start_align
    BSWAP_LOOPS_SSSE3  u
    jmp      .left2
.start_align:
    BSWAP_LOOPS_SSSE3  a
.left2:
    mov      r3, r2
    and      r2, 2
    jz       .left1
    movq     m0, [r1]
    pshufb   m0, m2
    movq     [r0], m0
    add      r1, 8
    add      r0, 8
.left1:
    and      r3, 1
    jz       .end
    mov      r2d, [r1]
    bswap    r2d
    mov      [r0], r2d
.end:
    RET

