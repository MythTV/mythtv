/* Spatial/temporal denoising filter shamelessly swiped from Mplayer
   CPU detection borrowed from linearblend filter
   Mplayer filter code Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
   MythTV adaptation and MMX optimization by Andrew Mahone <andrewmahone@eml.cc>
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

#define LowPass(Prev, Curr, Coef) (Curr + Coef[Prev - Curr])

#define ABS(A) ( (A) > 0 ? (A) : -(A) )

#ifdef TIME_FILTER
#include <sys/time.h>
#ifndef TIME_INTERVAL
#define TIME_INTERVAL 300
#endif /* undef TIME_INTERVAL */
#endif /* TIME_FILTER */

#include "mmx.h"

static const mmx_t mz = { 0x0LL };

#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

#define MM_MMX    0x0001        /* standard MMX */
#define MM_3DNOW  0x0004        /* AMD 3DNOW */
#define MM_MMXEXT 0x0002        /* SSE integer functions or AMD MMX ext */
#define MM_SSE    0x0008        /* SSE functions */
#define MM_SSE2   0x0010        /* PIV SSE2 functions */

/* function to determin cacheline size, it's not perfect because some flakey
   procs make an accurate test complicated, but it should be good enough
*/
int
cacheline_size (void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid (0x80000000, eax, ebx, ecx, edx)
    if (eax > 0x80000004)
    {
        cpuid (0x80000005, eax, ebx, ecx, edx);
        return (ecx & 0xff);
    }
    else
        return 32;
}

/* Function to test if multimedia instructions are supported...  */
int
mm_support (void)
{
    int rval;
    int eax, ebx, ecx, edx;

    __asm__ __volatile__ (
                             /* See if CPUID instruction is supported ... */
                             /* ... Get copies of EFLAGS into eax and ecx */
                             "pushf\n\t" "popl %0\n\t" "movl %0, %1\n\t"
                             /* ... Toggle the ID bit in one copy and store */
                             /*     to the EFLAGS reg */
                             "xorl $0x200000, %0\n\t" "push %0\n\t" "popf\n\t"
                             /* ... Get the (hopefully modified) EFLAGS */
                             "pushf\n\t"
                             "popl %0\n\t":"=a" (eax), "=c" (ecx)::"cc");

    if (eax == ecx)
        return 0;               /* CPUID not supported */

    cpuid (0, eax, ebx, ecx, edx);

    if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e)
    {

        /* intel */
      inteltest:
        cpuid (1, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x02000000)
            rval |= MM_MMXEXT | MM_SSE;
        if (edx & 0x04000000)
            rval |= MM_SSE2;
        return rval;
    }
    else if (ebx == 0x68747541 && edx == 0x69746e65 && ecx == 0x444d4163)
    {
        /* AMD */
        cpuid (0x80000000, eax, ebx, ecx, edx);
        if ((unsigned) eax < 0x80000001)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x80000000)
            rval |= MM_3DNOW;
        if (edx & 0x00400000)
            rval |= MM_MMXEXT;
        return rval;
    }
    else if (ebx == 0x746e6543 && edx == 0x48727561 && ecx == 0x736c7561)
    {                           /*  "CentaurHauls" */
        /* VIA C3 */
        cpuid (0x80000000, eax, ebx, ecx, edx);
        if ((unsigned) eax < 0x80000001)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        rval = 0;
        if (edx & (1 << 31))
            rval |= MM_3DNOW;
        if (edx & (1 << 23))
            rval |= MM_MMX;
        if (edx & (1 << 24))
            rval |= MM_MMXEXT;
        return rval;
    }
    else if (ebx == 0x69727943 && edx == 0x736e4978 && ecx == 0x64616574)
    {
        /* Cyrix Section */
        /* See if extended CPUID level 80000001 is supported */
        /* The value of CPUID/80000001 for the 6x86MX is undefined
           according to the Cyrix CPU Detection Guide (Preliminary
           Rev. 1.01 table 1), so we'll check the value of eax for
           CPUID/0 to see if standard CPUID level 2 is supported.
           According to the table, the only CPU which supports level
           2 is also the only one which supports extended CPUID levels.
         */
        if (eax != 2)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        if ((eax & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (eax & 0x01000000)
            rval |= MM_MMXEXT;
        return rval;
    }
    else
    {
        return 0;
    }
}

typedef struct ThisFilter
{
    VideoFilter vf;

    int width;
    int height;
    int uoff;
    int voff;
    int cwidth;
    int cheight;
    int first;
    int mm_flags;
    void (*filtfunc)(uint8_t*, uint8_t*, uint8_t*, int, int, uint8_t*,
                     uint8_t*, uint8_t*);
    uint8_t *line;
    uint8_t *prev;
    uint8_t coefs[4][512];
#ifdef TIME_FILTER
    int frames;
    double seconds;
#endif /* TIME_FILTER */
} ThisFilter;

static void
PrecalcCoefs (uint8_t * Ct, double Dist25)
{
    int i;
    double Gamma, Simil, C;

    Gamma = log (0.25) / log (1.0 - Dist25 / 255.0);

    for (i = -256; i <= 255; i++)
    {
        Simil = 1.0 - ABS (i) / 255.0;
        C = pow (Simil, Gamma) * (double) i;
        Ct[256 + i] = (C < 0) ? (C - 0.5) : (C + 0.5);
    }
}


static void
denoise (uint8_t * Frame,
         uint8_t * FramePrev,
         uint8_t * Line,
         int W, int H,
         uint8_t * Horizontal, uint8_t * Vertical, uint8_t * Temporal)
{
    uint8_t prev;
    int X, Y;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;

    prev = Line[0] = Frame[0];
    Frame[0] = LowPass (FramePrev[0], Frame[1], Temporal);
    for (X = 1; X < W; X++)
    {
        prev = LowPass (prev, Frame[X], Horizontal);
        Line[X] = prev;
        Frame[X] = LowPass (FramePrev[X], prev, Temporal);
    }

    for (Y = 1; Y < H; Y++)
    {
        LineCur += W;
        LinePrev += W;
        prev = LineCur[0];
        Line[0] = LowPass (Line[0], prev, Vertical);
        LineCur[0] = LowPass (LinePrev[0], Line[0], Temporal);
        for (X = 1; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Horizontal);
            Line[X] = LowPass (Line[X], prev, Vertical);
            LineCur[X] = LowPass (LinePrev[X], Line[X], Temporal);
        }
    }
}

static void
denoise64 (uint8_t * Frame,
            uint8_t * FramePrev,
            uint8_t * Line,
            int W, int H,
            uint8_t * Horizontal, uint8_t * Vertical, uint8_t * Temporal)
{
    uint8_t prev;
    int X, X2, Y;
    uint8_t *BlockCur, *BlockUp, *BlockPrev;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;
    uint8_t cbuf[128];
    int16_t *wbuf = (int16_t *) cbuf;

    BlockUp = Line;
    BlockCur = LineCur;
    BlockPrev = LinePrev;
    prev = Frame[0];
    prefetcht0(BlockCur[0]);
    for (X = 0; X < W - 63; X += 64)
    {
        prefetcht0(BlockCur[64]);
        for (X2 = 0; X2 < 64; X2++)
            prev = BlockCur[X2] = LowPass(prev, BlockCur[X2], Horizontal);

        movq_m2r (BlockCur[0], mm0);
        movq_m2r (BlockCur[0], mm1);
        movq_m2r (BlockCur[8], mm2);
        movq_m2r (BlockCur[8], mm3);
        movq_m2r (BlockPrev[0], mm4);
        movq_m2r (BlockPrev[0], mm5);
        movq_m2r (BlockPrev[8], mm6);
        movq_m2r (BlockPrev[8], mm7);
        movq_r2m (mm0, BlockUp[0]);
        movq_r2m (mm2, BlockUp[8]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[0]);
        movq_r2m (mm5, cbuf[8]);
        movq_r2m (mm6, cbuf[16]);
        movq_r2m (mm7, cbuf[24]);

        movq_m2r (BlockCur[16], mm0);
        movq_m2r (BlockCur[16], mm1);
        movq_m2r (BlockCur[24], mm2);
        movq_m2r (BlockCur[24], mm3);
        movq_m2r (BlockPrev[16], mm4);
        movq_m2r (BlockPrev[16], mm5);
        movq_m2r (BlockPrev[24], mm6);
        movq_m2r (BlockPrev[24], mm7);
        movq_r2m (mm0, BlockUp[16]);
        movq_r2m (mm2, BlockUp[24]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[32]);
        movq_r2m (mm5, cbuf[40]);
        movq_r2m (mm6, cbuf[48]);
        movq_r2m (mm7, cbuf[56]);

        movq_m2r (BlockCur[32], mm0);
        movq_m2r (BlockCur[32], mm1);
        movq_m2r (BlockCur[40], mm2);
        movq_m2r (BlockCur[40], mm3);
        movq_m2r (BlockPrev[32], mm4);
        movq_m2r (BlockPrev[32], mm5);
        movq_m2r (BlockPrev[40], mm6);
        movq_m2r (BlockPrev[40], mm7);
        movq_r2m (mm0, BlockUp[32]);
        movq_r2m (mm2, BlockUp[40]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[64]);
        movq_r2m (mm5, cbuf[72]);
        movq_r2m (mm6, cbuf[80]);
        movq_r2m (mm7, cbuf[88]);

        movq_m2r (BlockCur[48], mm0);
        movq_m2r (BlockCur[48], mm1);
        movq_m2r (BlockCur[56], mm2);
        movq_m2r (BlockCur[56], mm3);
        movq_m2r (BlockPrev[48], mm4);
        movq_m2r (BlockPrev[48], mm5);
        movq_m2r (BlockPrev[56], mm6);
        movq_m2r (BlockPrev[56], mm7);
        movq_r2m (mm0, BlockUp[48]);
        movq_r2m (mm2, BlockUp[56]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[96]);
        movq_r2m (mm5, cbuf[104]);
        movq_r2m (mm6, cbuf[112]);
        movq_r2m (mm7, cbuf[120]);

        cbuf[0] = Temporal[wbuf[0]];
        cbuf[1] = Temporal[wbuf[1]];
        cbuf[2] = Temporal[wbuf[2]];
        cbuf[3] = Temporal[wbuf[3]];
        cbuf[4] = Temporal[wbuf[4]];
        cbuf[5] = Temporal[wbuf[5]];
        cbuf[6] = Temporal[wbuf[6]];
        cbuf[7] = Temporal[wbuf[7]];
        cbuf[8] = Temporal[wbuf[8]];
        cbuf[9] = Temporal[wbuf[9]];
        cbuf[10] = Temporal[wbuf[10]];
        cbuf[11] = Temporal[wbuf[11]];
        cbuf[12] = Temporal[wbuf[12]];
        cbuf[13] = Temporal[wbuf[13]];
        cbuf[14] = Temporal[wbuf[14]];
        cbuf[15] = Temporal[wbuf[15]];
        cbuf[16] = Temporal[wbuf[16]];
        cbuf[17] = Temporal[wbuf[17]];
        cbuf[18] = Temporal[wbuf[18]];
        cbuf[19] = Temporal[wbuf[19]];
        cbuf[20] = Temporal[wbuf[20]];
        cbuf[21] = Temporal[wbuf[21]];
        cbuf[22] = Temporal[wbuf[22]];
        cbuf[23] = Temporal[wbuf[23]];
        cbuf[24] = Temporal[wbuf[24]];
        cbuf[25] = Temporal[wbuf[25]];
        cbuf[26] = Temporal[wbuf[26]];
        cbuf[27] = Temporal[wbuf[27]];
        cbuf[28] = Temporal[wbuf[28]];
        cbuf[29] = Temporal[wbuf[29]];
        cbuf[30] = Temporal[wbuf[30]];
        cbuf[31] = Temporal[wbuf[31]];
        cbuf[32] = Temporal[wbuf[32]];
        cbuf[33] = Temporal[wbuf[33]];
        cbuf[34] = Temporal[wbuf[34]];
        cbuf[35] = Temporal[wbuf[35]];
        cbuf[36] = Temporal[wbuf[36]];
        cbuf[37] = Temporal[wbuf[37]];
        cbuf[38] = Temporal[wbuf[38]];
        cbuf[39] = Temporal[wbuf[39]];
        cbuf[40] = Temporal[wbuf[40]];
        cbuf[41] = Temporal[wbuf[41]];
        cbuf[42] = Temporal[wbuf[42]];
        cbuf[43] = Temporal[wbuf[43]];
        cbuf[44] = Temporal[wbuf[44]];
        cbuf[45] = Temporal[wbuf[45]];
        cbuf[46] = Temporal[wbuf[46]];
        cbuf[47] = Temporal[wbuf[47]];
        cbuf[48] = Temporal[wbuf[48]];
        cbuf[49] = Temporal[wbuf[49]];
        cbuf[50] = Temporal[wbuf[50]];
        cbuf[51] = Temporal[wbuf[51]];
        cbuf[52] = Temporal[wbuf[52]];
        cbuf[53] = Temporal[wbuf[53]];
        cbuf[54] = Temporal[wbuf[54]];
        cbuf[55] = Temporal[wbuf[55]];
        cbuf[56] = Temporal[wbuf[56]];
        cbuf[57] = Temporal[wbuf[57]];
        cbuf[58] = Temporal[wbuf[58]];
        cbuf[59] = Temporal[wbuf[59]];
        cbuf[60] = Temporal[wbuf[60]];
        cbuf[61] = Temporal[wbuf[61]];
        cbuf[62] = Temporal[wbuf[62]];
        cbuf[63] = Temporal[wbuf[63]];

        movq_m2r (cbuf[0], mm0);
        movq_m2r (cbuf[8], mm1);
        movq_m2r (cbuf[16], mm2);
        movq_m2r (cbuf[24], mm3);
        movq_m2r (cbuf[32], mm4);
        movq_m2r (cbuf[40], mm5);
        movq_m2r (cbuf[48], mm6);
        movq_m2r (cbuf[56], mm7);
        paddb_m2r (BlockCur[0], mm0);
        paddb_m2r (BlockCur[8], mm1);
        paddb_m2r (BlockCur[16], mm2);
        paddb_m2r (BlockCur[24], mm3);
        paddb_m2r (BlockCur[32], mm4);
        paddb_m2r (BlockCur[40], mm5);
        paddb_m2r (BlockCur[48], mm6);
        paddb_m2r (BlockCur[56], mm7);
        movq_r2m (mm0, BlockCur[0]);
        movq_r2m (mm1, BlockCur[8]);
        movq_r2m (mm2, BlockCur[16]);
        movq_r2m (mm3, BlockCur[24]);
        movq_r2m (mm4, BlockCur[32]);
        movq_r2m (mm5, BlockCur[40]);
        movq_r2m (mm6, BlockCur[48]);
        movq_r2m (mm7, BlockCur[56]);
        movq_r2m (mm0, BlockPrev[0]);
        movq_r2m (mm1, BlockPrev[8]);
        movq_r2m (mm2, BlockPrev[16]);
        movq_r2m (mm3, BlockPrev[24]);
        movq_r2m (mm4, BlockPrev[32]);
        movq_r2m (mm5, BlockPrev[40]);
        movq_r2m (mm6, BlockPrev[48]);
        movq_r2m (mm7, BlockPrev[56]);

        BlockPrev += 64;
        BlockCur += 64;
        BlockUp += 64;
    }
/* tried smaller unrolled MMX loops, they were always slower, so we finish any
   leftover with integer ops
*/
    for (/*X*/; X < W ; X++)
    {
        Line[X] = prev = LowPass(prev, LineCur[X], Horizontal);
        LineCur[X] = LinePrev[X] = LowPass(LinePrev[X], prev, Temporal);
    }

    for (Y = 0; Y < H; Y++)
    {
        LineCur += W;
        LinePrev += W;
        BlockUp = Line;
        BlockCur = LineCur;
        BlockPrev = LinePrev;
        prev = BlockCur[0];
/* this loop was slower when unrolled to 64 bytes, even with 64-byte
   cachelines, and the extra prefetches are cheaper than testing to
   prefetch every other line
*/
        for (X = 0; X < W - 31; X += 32)
        {
            prefetcht0 (BlockCur[32]);
            for (X2 = 0; X2 < 32; X2++)
                prev = BlockCur[X2] = LowPass(prev, BlockCur[X2], Horizontal);

            movq_m2r (BlockCur[0], mm0);
            movq_m2r (BlockCur[0], mm1);
            movq_m2r (BlockCur[8], mm2);
            movq_m2r (BlockCur[8], mm3);
            movq_m2r (BlockUp[0], mm4);
            movq_m2r (BlockUp[0], mm5);
            movq_m2r (BlockUp[8], mm6);
            movq_m2r (BlockUp[8], mm7);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[0]);
            movq_r2m (mm5, cbuf[8]);
            movq_r2m (mm6, cbuf[16]);
            movq_r2m (mm7, cbuf[24]);

            movq_m2r (BlockCur[16], mm0);
            movq_m2r (BlockCur[16], mm1);
            movq_m2r (BlockCur[24], mm2);
            movq_m2r (BlockCur[24], mm3);
            movq_m2r (BlockUp[16], mm4);
            movq_m2r (BlockUp[16], mm5);
            movq_m2r (BlockUp[24], mm6);
            movq_m2r (BlockUp[24], mm7);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[32]);
            movq_r2m (mm5, cbuf[40]);
            movq_r2m (mm6, cbuf[48]);
            movq_r2m (mm7, cbuf[56]);

            cbuf[0] = Vertical[wbuf[0]];
            cbuf[1] = Vertical[wbuf[1]];
            cbuf[2] = Vertical[wbuf[2]];
            cbuf[3] = Vertical[wbuf[3]];
            cbuf[4] = Vertical[wbuf[4]];
            cbuf[5] = Vertical[wbuf[5]];
            cbuf[6] = Vertical[wbuf[6]];
            cbuf[7] = Vertical[wbuf[7]];
            cbuf[8] = Vertical[wbuf[8]];
            cbuf[9] = Vertical[wbuf[9]];
            cbuf[10] = Vertical[wbuf[10]];
            cbuf[11] = Vertical[wbuf[11]];
            cbuf[12] = Vertical[wbuf[12]];
            cbuf[13] = Vertical[wbuf[13]];
            cbuf[14] = Vertical[wbuf[14]];
            cbuf[15] = Vertical[wbuf[15]];
            cbuf[16] = Vertical[wbuf[16]];
            cbuf[17] = Vertical[wbuf[17]];
            cbuf[18] = Vertical[wbuf[18]];
            cbuf[19] = Vertical[wbuf[19]];
            cbuf[20] = Vertical[wbuf[20]];
            cbuf[21] = Vertical[wbuf[21]];
            cbuf[22] = Vertical[wbuf[22]];
            cbuf[23] = Vertical[wbuf[23]];
            cbuf[24] = Vertical[wbuf[24]];
            cbuf[25] = Vertical[wbuf[25]];
            cbuf[26] = Vertical[wbuf[26]];
            cbuf[27] = Vertical[wbuf[27]];
            cbuf[28] = Vertical[wbuf[28]];
            cbuf[29] = Vertical[wbuf[29]];
            cbuf[30] = Vertical[wbuf[30]];
            cbuf[31] = Vertical[wbuf[31]];

            movq_m2r (cbuf[0], mm0);
            movq_m2r (cbuf[8], mm2);
            paddb_m2r (BlockCur[0], mm0);
            paddb_m2r (BlockCur[8], mm2);
            movq_m2r (BlockPrev[0], mm4);
            movq_m2r (BlockPrev[0], mm5);
            movq_m2r (BlockPrev[8], mm6);
            movq_m2r (BlockPrev[8], mm7);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2m (mm0, BlockUp[0]);
            movq_r2m (mm2, BlockUp[8]);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[64]);
            movq_r2m (mm5, cbuf[72]);
            movq_r2m (mm6, cbuf[80]);
            movq_r2m (mm7, cbuf[88]);

            movq_m2r (cbuf[16], mm0);
            movq_m2r (cbuf[24], mm2);
            paddb_m2r (BlockCur[16], mm0);
            paddb_m2r (BlockCur[24], mm2);
            movq_m2r (BlockPrev[16], mm4);
            movq_m2r (BlockPrev[16], mm5);
            movq_m2r (BlockPrev[24], mm6);
            movq_m2r (BlockPrev[24], mm7);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2m (mm0, BlockUp[16]);
            movq_r2m (mm2, BlockUp[24]);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[96]);
            movq_r2m (mm5, cbuf[104]);
            movq_r2m (mm6, cbuf[112]);
            movq_r2m (mm7, cbuf[120]);


            cbuf[0] = Temporal[wbuf[32]];
            cbuf[1] = Temporal[wbuf[33]];
            cbuf[2] = Temporal[wbuf[34]];
            cbuf[3] = Temporal[wbuf[35]];
            cbuf[4] = Temporal[wbuf[36]];
            cbuf[5] = Temporal[wbuf[37]];
            cbuf[6] = Temporal[wbuf[38]];
            cbuf[7] = Temporal[wbuf[39]];
            cbuf[8] = Temporal[wbuf[40]];
            cbuf[9] = Temporal[wbuf[41]];
            cbuf[10] = Temporal[wbuf[42]];
            cbuf[11] = Temporal[wbuf[43]];
            cbuf[12] = Temporal[wbuf[44]];
            cbuf[13] = Temporal[wbuf[45]];
            cbuf[14] = Temporal[wbuf[46]];
            cbuf[15] = Temporal[wbuf[47]];
            cbuf[16] = Temporal[wbuf[48]];
            cbuf[17] = Temporal[wbuf[49]];
            cbuf[18] = Temporal[wbuf[50]];
            cbuf[19] = Temporal[wbuf[51]];
            cbuf[20] = Temporal[wbuf[52]];
            cbuf[21] = Temporal[wbuf[53]];
            cbuf[22] = Temporal[wbuf[54]];
            cbuf[23] = Temporal[wbuf[55]];
            cbuf[24] = Temporal[wbuf[56]];
            cbuf[25] = Temporal[wbuf[57]];
            cbuf[26] = Temporal[wbuf[58]];
            cbuf[27] = Temporal[wbuf[59]];
            cbuf[28] = Temporal[wbuf[60]];
            cbuf[29] = Temporal[wbuf[61]];
            cbuf[30] = Temporal[wbuf[62]];
            cbuf[31] = Temporal[wbuf[63]];

            movq_m2r (cbuf[0], mm0);
            movq_m2r (cbuf[8], mm1);
            movq_m2r (cbuf[16], mm2);
            movq_m2r (cbuf[24], mm3);
            paddb_m2r (BlockUp[0], mm0);
            paddb_m2r (BlockUp[8], mm1);
            paddb_m2r (BlockUp[16], mm2);
            paddb_m2r (BlockUp[24], mm3);
            movq_r2m (mm0, BlockCur[0]);
            movq_r2m (mm1, BlockCur[8]);
            movq_r2m (mm2, BlockCur[16]);
            movq_r2m (mm3, BlockCur[24]);
            movq_r2m (mm0, BlockPrev[0]);
            movq_r2m (mm1, BlockPrev[8]);
            movq_r2m (mm2, BlockPrev[16]);
            movq_r2m (mm3, BlockPrev[24]);

            BlockPrev += 32;
            BlockCur += 32;
            BlockUp += 32;
        }
        for (/*X*/; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Horizontal);
            Line[X] = LowPass (Line[X], prev, Vertical);
            LinePrev[X] = LineCur[X] =
                LowPass (LinePrev[X], Line[X], Temporal);
        }
    }
}

static void
denoise32 (uint8_t * Frame,
            uint8_t * FramePrev,
            uint8_t * Line,
            int W, int H,
            uint8_t * Horizontal, uint8_t * Vertical, uint8_t * Temporal)
{
    uint8_t prev;
    int X, X2, Y;
    uint8_t *BlockCur, *BlockUp, *BlockPrev;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;
    uint8_t cbuf[128];
    int16_t *wbuf = (int16_t *) cbuf;

    BlockUp = Line;
    BlockCur = LineCur;
    BlockPrev = LinePrev;
    prev = Frame[0];

    prefetcht0 (BlockCur[0]);
    for (X = 0; X < W - 31; X += 32)
    {
        prefetcht0 (BlockCur[32]);
        for (X2 = 0; X2 < 32; X2++)
            prev = BlockCur[X2] = LowPass(prev, BlockCur[X2], Horizontal);

        movq_m2r (BlockCur[0], mm0);
        movq_m2r (BlockCur[0], mm1);
        movq_m2r (BlockCur[8], mm2);
        movq_m2r (BlockCur[8], mm3);
        movq_m2r (BlockPrev[0], mm4);
        movq_m2r (BlockPrev[0], mm5);
        movq_m2r (BlockPrev[8], mm6);
        movq_m2r (BlockPrev[8], mm7);
        movq_r2m (mm0, BlockUp[0]);
        movq_r2m (mm2, BlockUp[8]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[0]);
        movq_r2m (mm5, cbuf[8]);
        movq_r2m (mm6, cbuf[16]);
        movq_r2m (mm7, cbuf[24]);

        movq_m2r (BlockCur[16], mm0);
        movq_m2r (BlockCur[16], mm1);
        movq_m2r (BlockCur[24], mm2);
        movq_m2r (BlockCur[24], mm3);
        movq_m2r (BlockPrev[16], mm4);
        movq_m2r (BlockPrev[16], mm5);
        movq_m2r (BlockPrev[24], mm6);
        movq_m2r (BlockPrev[24], mm7);
        movq_r2m (mm0, BlockUp[16]);
        movq_r2m (mm2, BlockUp[24]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[32]);
        movq_r2m (mm5, cbuf[40]);
        movq_r2m (mm6, cbuf[48]);
        movq_r2m (mm7, cbuf[56]);

        cbuf[0] = Temporal[wbuf[0]];
        cbuf[1] = Temporal[wbuf[1]];
        cbuf[2] = Temporal[wbuf[2]];
        cbuf[3] = Temporal[wbuf[3]];
        cbuf[4] = Temporal[wbuf[4]];
        cbuf[5] = Temporal[wbuf[5]];
        cbuf[6] = Temporal[wbuf[6]];
        cbuf[7] = Temporal[wbuf[7]];
        cbuf[8] = Temporal[wbuf[8]];
        cbuf[9] = Temporal[wbuf[9]];
        cbuf[10] = Temporal[wbuf[10]];
        cbuf[11] = Temporal[wbuf[11]];
        cbuf[12] = Temporal[wbuf[12]];
        cbuf[13] = Temporal[wbuf[13]];
        cbuf[14] = Temporal[wbuf[14]];
        cbuf[15] = Temporal[wbuf[15]];
        cbuf[16] = Temporal[wbuf[16]];
        cbuf[17] = Temporal[wbuf[17]];
        cbuf[18] = Temporal[wbuf[18]];
        cbuf[19] = Temporal[wbuf[19]];
        cbuf[20] = Temporal[wbuf[20]];
        cbuf[21] = Temporal[wbuf[21]];
        cbuf[22] = Temporal[wbuf[22]];
        cbuf[23] = Temporal[wbuf[23]];
        cbuf[24] = Temporal[wbuf[24]];
        cbuf[25] = Temporal[wbuf[25]];
        cbuf[26] = Temporal[wbuf[26]];
        cbuf[27] = Temporal[wbuf[27]];
        cbuf[28] = Temporal[wbuf[28]];
        cbuf[29] = Temporal[wbuf[29]];
        cbuf[30] = Temporal[wbuf[30]];
        cbuf[31] = Temporal[wbuf[31]];

        movq_m2r (cbuf[0], mm0);
        movq_m2r (cbuf[8], mm1);
        movq_m2r (cbuf[16], mm2);
        movq_m2r (cbuf[24], mm3);
        paddb_m2r (BlockCur[0], mm0);
        paddb_m2r (BlockCur[8], mm1);
        paddb_m2r (BlockCur[16], mm2);
        paddb_m2r (BlockCur[24], mm3);
        movq_r2m (mm0, BlockCur[0]);
        movq_r2m (mm1, BlockCur[8]);
        movq_r2m (mm2, BlockCur[16]);
        movq_r2m (mm3, BlockCur[24]);
        movq_r2m (mm0, BlockPrev[0]);
        movq_r2m (mm1, BlockPrev[8]);
        movq_r2m (mm2, BlockPrev[16]);
        movq_r2m (mm3, BlockPrev[24]);

        BlockPrev += 32;
        BlockCur += 32;
        BlockUp += 32;
    }
    for (/*X*/; X < W ; X++)
    {
        Line[X] = prev = LowPass(prev, LineCur[X], Horizontal);
        LineCur[X] = LinePrev[X] = LowPass(LinePrev[X], prev, Temporal);
    }

    for (Y = 0; Y < H; Y++)
    {
        LineCur += W;
        LinePrev += W;
        BlockUp = Line;
        BlockCur = LineCur;
        BlockPrev = LinePrev;
        prev = BlockCur[0];
        for (X = 0; X < W - 31; X += 32)
        {
            prefetcht0 (BlockCur[32]);
            for (X2 = 0; X2 < 32; X2++)
                prev = BlockCur[X2] = LowPass(prev, BlockCur[X2], Horizontal);

            movq_m2r (BlockCur[0], mm0);
            movq_m2r (BlockCur[0], mm1);
            movq_m2r (BlockCur[8], mm2);
            movq_m2r (BlockCur[8], mm3);
            movq_m2r (BlockUp[0], mm4);
            movq_m2r (BlockUp[0], mm5);
            movq_m2r (BlockUp[8], mm6);
            movq_m2r (BlockUp[8], mm7);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[0]);
            movq_r2m (mm5, cbuf[8]);
            movq_r2m (mm6, cbuf[16]);
            movq_r2m (mm7, cbuf[24]);

            movq_m2r (BlockCur[16], mm0);
            movq_m2r (BlockCur[16], mm1);
            movq_m2r (BlockCur[24], mm2);
            movq_m2r (BlockCur[24], mm3);
            movq_m2r (BlockUp[16], mm4);
            movq_m2r (BlockUp[16], mm5);
            movq_m2r (BlockUp[24], mm6);
            movq_m2r (BlockUp[24], mm7);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[32]);
            movq_r2m (mm5, cbuf[40]);
            movq_r2m (mm6, cbuf[48]);
            movq_r2m (mm7, cbuf[56]);

            cbuf[0] = Vertical[wbuf[0]];
            cbuf[1] = Vertical[wbuf[1]];
            cbuf[2] = Vertical[wbuf[2]];
            cbuf[3] = Vertical[wbuf[3]];
            cbuf[4] = Vertical[wbuf[4]];
            cbuf[5] = Vertical[wbuf[5]];
            cbuf[6] = Vertical[wbuf[6]];
            cbuf[7] = Vertical[wbuf[7]];
            cbuf[8] = Vertical[wbuf[8]];
            cbuf[9] = Vertical[wbuf[9]];
            cbuf[10] = Vertical[wbuf[10]];
            cbuf[11] = Vertical[wbuf[11]];
            cbuf[12] = Vertical[wbuf[12]];
            cbuf[13] = Vertical[wbuf[13]];
            cbuf[14] = Vertical[wbuf[14]];
            cbuf[15] = Vertical[wbuf[15]];
            cbuf[16] = Vertical[wbuf[16]];
            cbuf[17] = Vertical[wbuf[17]];
            cbuf[18] = Vertical[wbuf[18]];
            cbuf[19] = Vertical[wbuf[19]];
            cbuf[20] = Vertical[wbuf[20]];
            cbuf[21] = Vertical[wbuf[21]];
            cbuf[22] = Vertical[wbuf[22]];
            cbuf[23] = Vertical[wbuf[23]];
            cbuf[24] = Vertical[wbuf[24]];
            cbuf[25] = Vertical[wbuf[25]];
            cbuf[26] = Vertical[wbuf[26]];
            cbuf[27] = Vertical[wbuf[27]];
            cbuf[28] = Vertical[wbuf[28]];
            cbuf[29] = Vertical[wbuf[29]];
            cbuf[30] = Vertical[wbuf[30]];
            cbuf[31] = Vertical[wbuf[31]];

            movq_m2r (cbuf[0], mm0);
            movq_m2r (cbuf[8], mm2);
            paddb_m2r (BlockCur[0], mm0);
            paddb_m2r (BlockCur[8], mm2);
            movq_m2r (BlockPrev[0], mm4);
            movq_m2r (BlockPrev[0], mm5);
            movq_m2r (BlockPrev[8], mm6);
            movq_m2r (BlockPrev[8], mm7);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2m (mm0, BlockUp[0]);
            movq_r2m (mm2, BlockUp[8]);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[64]);
            movq_r2m (mm5, cbuf[72]);
            movq_r2m (mm6, cbuf[80]);
            movq_r2m (mm7, cbuf[88]);

            movq_m2r (cbuf[16], mm0);
            movq_m2r (cbuf[24], mm2);
            paddb_m2r (BlockCur[16], mm0);
            paddb_m2r (BlockCur[24], mm2);
            movq_m2r (BlockPrev[16], mm4);
            movq_m2r (BlockPrev[16], mm5);
            movq_m2r (BlockPrev[24], mm6);
            movq_m2r (BlockPrev[24], mm7);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2m (mm0, BlockUp[16]);
            movq_r2m (mm2, BlockUp[24]);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[96]);
            movq_r2m (mm5, cbuf[104]);
            movq_r2m (mm6, cbuf[112]);
            movq_r2m (mm7, cbuf[120]);


            cbuf[0] = Temporal[wbuf[32]];
            cbuf[1] = Temporal[wbuf[33]];
            cbuf[2] = Temporal[wbuf[34]];
            cbuf[3] = Temporal[wbuf[35]];
            cbuf[4] = Temporal[wbuf[36]];
            cbuf[5] = Temporal[wbuf[37]];
            cbuf[6] = Temporal[wbuf[38]];
            cbuf[7] = Temporal[wbuf[39]];
            cbuf[8] = Temporal[wbuf[40]];
            cbuf[9] = Temporal[wbuf[41]];
            cbuf[10] = Temporal[wbuf[42]];
            cbuf[11] = Temporal[wbuf[43]];
            cbuf[12] = Temporal[wbuf[44]];
            cbuf[13] = Temporal[wbuf[45]];
            cbuf[14] = Temporal[wbuf[46]];
            cbuf[15] = Temporal[wbuf[47]];
            cbuf[16] = Temporal[wbuf[48]];
            cbuf[17] = Temporal[wbuf[49]];
            cbuf[18] = Temporal[wbuf[50]];
            cbuf[19] = Temporal[wbuf[51]];
            cbuf[20] = Temporal[wbuf[52]];
            cbuf[21] = Temporal[wbuf[53]];
            cbuf[22] = Temporal[wbuf[54]];
            cbuf[23] = Temporal[wbuf[55]];
            cbuf[24] = Temporal[wbuf[56]];
            cbuf[25] = Temporal[wbuf[57]];
            cbuf[26] = Temporal[wbuf[58]];
            cbuf[27] = Temporal[wbuf[59]];
            cbuf[28] = Temporal[wbuf[60]];
            cbuf[29] = Temporal[wbuf[61]];
            cbuf[30] = Temporal[wbuf[62]];
            cbuf[31] = Temporal[wbuf[63]];

            movq_m2r (cbuf[0], mm0);
            movq_m2r (cbuf[8], mm1);
            movq_m2r (cbuf[16], mm2);
            movq_m2r (cbuf[24], mm3);
            paddb_m2r (BlockUp[0], mm0);
            paddb_m2r (BlockUp[8], mm1);
            paddb_m2r (BlockUp[16], mm2);
            paddb_m2r (BlockUp[24], mm3);
            movq_r2m (mm0, BlockCur[0]);
            movq_r2m (mm1, BlockCur[8]);
            movq_r2m (mm2, BlockCur[16]);
            movq_r2m (mm3, BlockCur[24]);
            movq_r2m (mm0, BlockPrev[0]);
            movq_r2m (mm1, BlockPrev[8]);
            movq_r2m (mm2, BlockPrev[16]);
            movq_r2m (mm3, BlockPrev[24]);

            BlockPrev += 32;
            BlockCur += 32;
            BlockUp += 32;
        }
        for (/*X*/; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Horizontal);
            Line[X] = LowPass (Line[X], prev, Vertical);
            LinePrev[X] = LineCur[X] =
                LowPass (LinePrev[X], Line[X], Temporal);
        }
    }
}

/* if you don't have prefetch, you probably won't benefit from the longer
   unrolled loops, so the plain MMX version is denoise32 - prefetch
*/
static void
denoiseMMX (uint8_t * Frame, uint8_t * FramePrev, uint8_t * Line, int W, int H,
            uint8_t * Horizontal, uint8_t * Vertical, uint8_t * Temporal)
{
    uint8_t prev;
    int X, X2, Y;
    uint8_t *BlockCur, *BlockUp, *BlockPrev;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;
    uint8_t cbuf[128];
    int16_t *wbuf = (int16_t *) cbuf;

    BlockUp = Line;
    BlockCur = LineCur;
    BlockPrev = LinePrev;
    prev = Frame[0];

    prefetcht0 (BlockCur[0]);
    for (X = 0; X < W - 31; X += 32)
    {
        for (X2 = 0; X2 < 32; X2++)
            prev = BlockCur[X2] = LowPass(prev, BlockCur[X2], Horizontal);

        movq_m2r (BlockCur[0], mm0);
        movq_m2r (BlockCur[0], mm1);
        movq_m2r (BlockCur[8], mm2);
        movq_m2r (BlockCur[8], mm3);
        movq_m2r (BlockPrev[0], mm4);
        movq_m2r (BlockPrev[0], mm5);
        movq_m2r (BlockPrev[8], mm6);
        movq_m2r (BlockPrev[8], mm7);
        movq_r2m (mm0, BlockUp[0]);
        movq_r2m (mm2, BlockUp[8]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[0]);
        movq_r2m (mm5, cbuf[8]);
        movq_r2m (mm6, cbuf[16]);
        movq_r2m (mm7, cbuf[24]);

        movq_m2r (BlockCur[16], mm0);
        movq_m2r (BlockCur[16], mm1);
        movq_m2r (BlockCur[24], mm2);
        movq_m2r (BlockCur[24], mm3);
        movq_m2r (BlockPrev[16], mm4);
        movq_m2r (BlockPrev[16], mm5);
        movq_m2r (BlockPrev[24], mm6);
        movq_m2r (BlockPrev[24], mm7);
        movq_r2m (mm0, BlockUp[16]);
        movq_r2m (mm2, BlockUp[24]);
        punpcklbw_m2r (mz, mm0);
        punpckhbw_m2r (mz, mm1);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        punpcklbw_m2r (mz, mm4);
        punpckhbw_m2r (mz, mm5);
        punpcklbw_m2r (mz, mm6);
        punpckhbw_m2r (mz, mm7);
        psubw_r2r (mm0, mm4);
        psubw_r2r (mm1, mm5);
        psubw_r2r (mm2, mm6);
        psubw_r2r (mm3, mm7);
        movq_r2m (mm4, cbuf[32]);
        movq_r2m (mm5, cbuf[40]);
        movq_r2m (mm6, cbuf[48]);
        movq_r2m (mm7, cbuf[56]);

        cbuf[0] = Temporal[wbuf[0]];
        cbuf[1] = Temporal[wbuf[1]];
        cbuf[2] = Temporal[wbuf[2]];
        cbuf[3] = Temporal[wbuf[3]];
        cbuf[4] = Temporal[wbuf[4]];
        cbuf[5] = Temporal[wbuf[5]];
        cbuf[6] = Temporal[wbuf[6]];
        cbuf[7] = Temporal[wbuf[7]];
        cbuf[8] = Temporal[wbuf[8]];
        cbuf[9] = Temporal[wbuf[9]];
        cbuf[10] = Temporal[wbuf[10]];
        cbuf[11] = Temporal[wbuf[11]];
        cbuf[12] = Temporal[wbuf[12]];
        cbuf[13] = Temporal[wbuf[13]];
        cbuf[14] = Temporal[wbuf[14]];
        cbuf[15] = Temporal[wbuf[15]];
        cbuf[16] = Temporal[wbuf[16]];
        cbuf[17] = Temporal[wbuf[17]];
        cbuf[18] = Temporal[wbuf[18]];
        cbuf[19] = Temporal[wbuf[19]];
        cbuf[20] = Temporal[wbuf[20]];
        cbuf[21] = Temporal[wbuf[21]];
        cbuf[22] = Temporal[wbuf[22]];
        cbuf[23] = Temporal[wbuf[23]];
        cbuf[24] = Temporal[wbuf[24]];
        cbuf[25] = Temporal[wbuf[25]];
        cbuf[26] = Temporal[wbuf[26]];
        cbuf[27] = Temporal[wbuf[27]];
        cbuf[28] = Temporal[wbuf[28]];
        cbuf[29] = Temporal[wbuf[29]];
        cbuf[30] = Temporal[wbuf[30]];
        cbuf[31] = Temporal[wbuf[31]];

        movq_m2r (cbuf[0], mm0);
        movq_m2r (cbuf[8], mm1);
        movq_m2r (cbuf[16], mm2);
        movq_m2r (cbuf[24], mm3);
        paddb_m2r (BlockCur[0], mm0);
        paddb_m2r (BlockCur[8], mm1);
        paddb_m2r (BlockCur[16], mm2);
        paddb_m2r (BlockCur[24], mm3);
        movq_r2m (mm0, BlockCur[0]);
        movq_r2m (mm1, BlockCur[8]);
        movq_r2m (mm2, BlockCur[16]);
        movq_r2m (mm3, BlockCur[24]);
        movq_r2m (mm0, BlockPrev[0]);
        movq_r2m (mm1, BlockPrev[8]);
        movq_r2m (mm2, BlockPrev[16]);
        movq_r2m (mm3, BlockPrev[24]);

        BlockPrev += 32;
        BlockCur += 32;
        BlockUp += 32;
    }
    for (/*X*/; X < W ; X++)
    {
        Line[X] = prev = LowPass(prev, LineCur[X], Horizontal);
        LineCur[X] = LinePrev[X] = LowPass(LinePrev[X], prev, Temporal);
    }

    for (Y = 0; Y < H; Y++)
    {
        LineCur += W;
        LinePrev += W;
        BlockUp = Line;
        BlockCur = LineCur;
        BlockPrev = LinePrev;
        prev = BlockCur[0];
        for (X = 0; X < W - 31; X += 32)
        {
            for (X2 = 0; X2 < 32; X2++)
                prev = BlockCur[X2] = LowPass(prev, BlockCur[X2], Horizontal);

            movq_m2r (BlockCur[0], mm0);
            movq_m2r (BlockCur[0], mm1);
            movq_m2r (BlockCur[8], mm2);
            movq_m2r (BlockCur[8], mm3);
            movq_m2r (BlockUp[0], mm4);
            movq_m2r (BlockUp[0], mm5);
            movq_m2r (BlockUp[8], mm6);
            movq_m2r (BlockUp[8], mm7);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[0]);
            movq_r2m (mm5, cbuf[8]);
            movq_r2m (mm6, cbuf[16]);
            movq_r2m (mm7, cbuf[24]);

            movq_m2r (BlockCur[16], mm0);
            movq_m2r (BlockCur[16], mm1);
            movq_m2r (BlockCur[24], mm2);
            movq_m2r (BlockCur[24], mm3);
            movq_m2r (BlockUp[16], mm4);
            movq_m2r (BlockUp[16], mm5);
            movq_m2r (BlockUp[24], mm6);
            movq_m2r (BlockUp[24], mm7);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[32]);
            movq_r2m (mm5, cbuf[40]);
            movq_r2m (mm6, cbuf[48]);
            movq_r2m (mm7, cbuf[56]);

            cbuf[0] = Vertical[wbuf[0]];
            cbuf[1] = Vertical[wbuf[1]];
            cbuf[2] = Vertical[wbuf[2]];
            cbuf[3] = Vertical[wbuf[3]];
            cbuf[4] = Vertical[wbuf[4]];
            cbuf[5] = Vertical[wbuf[5]];
            cbuf[6] = Vertical[wbuf[6]];
            cbuf[7] = Vertical[wbuf[7]];
            cbuf[8] = Vertical[wbuf[8]];
            cbuf[9] = Vertical[wbuf[9]];
            cbuf[10] = Vertical[wbuf[10]];
            cbuf[11] = Vertical[wbuf[11]];
            cbuf[12] = Vertical[wbuf[12]];
            cbuf[13] = Vertical[wbuf[13]];
            cbuf[14] = Vertical[wbuf[14]];
            cbuf[15] = Vertical[wbuf[15]];
            cbuf[16] = Vertical[wbuf[16]];
            cbuf[17] = Vertical[wbuf[17]];
            cbuf[18] = Vertical[wbuf[18]];
            cbuf[19] = Vertical[wbuf[19]];
            cbuf[20] = Vertical[wbuf[20]];
            cbuf[21] = Vertical[wbuf[21]];
            cbuf[22] = Vertical[wbuf[22]];
            cbuf[23] = Vertical[wbuf[23]];
            cbuf[24] = Vertical[wbuf[24]];
            cbuf[25] = Vertical[wbuf[25]];
            cbuf[26] = Vertical[wbuf[26]];
            cbuf[27] = Vertical[wbuf[27]];
            cbuf[28] = Vertical[wbuf[28]];
            cbuf[29] = Vertical[wbuf[29]];
            cbuf[30] = Vertical[wbuf[30]];
            cbuf[31] = Vertical[wbuf[31]];

            movq_m2r (cbuf[0], mm0);
            movq_m2r (cbuf[8], mm2);
            paddb_m2r (BlockCur[0], mm0);
            paddb_m2r (BlockCur[8], mm2);
            movq_m2r (BlockPrev[0], mm4);
            movq_m2r (BlockPrev[0], mm5);
            movq_m2r (BlockPrev[8], mm6);
            movq_m2r (BlockPrev[8], mm7);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2m (mm0, BlockUp[0]);
            movq_r2m (mm2, BlockUp[8]);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[64]);
            movq_r2m (mm5, cbuf[72]);
            movq_r2m (mm6, cbuf[80]);
            movq_r2m (mm7, cbuf[88]);

            movq_m2r (cbuf[16], mm0);
            movq_m2r (cbuf[24], mm2);
            paddb_m2r (BlockCur[16], mm0);
            paddb_m2r (BlockCur[24], mm2);
            movq_m2r (BlockPrev[16], mm4);
            movq_m2r (BlockPrev[16], mm5);
            movq_m2r (BlockPrev[24], mm6);
            movq_m2r (BlockPrev[24], mm7);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2m (mm0, BlockUp[16]);
            movq_r2m (mm2, BlockUp[24]);
            punpcklbw_m2r (mz, mm0);
            punpckhbw_m2r (mz, mm1);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm3);
            punpcklbw_m2r (mz, mm4);
            punpckhbw_m2r (mz, mm5);
            punpcklbw_m2r (mz, mm6);
            punpckhbw_m2r (mz, mm7);
            psubw_r2r (mm0, mm4);
            psubw_r2r (mm1, mm5);
            psubw_r2r (mm2, mm6);
            psubw_r2r (mm3, mm7);
            movq_r2m (mm4, cbuf[96]);
            movq_r2m (mm5, cbuf[104]);
            movq_r2m (mm6, cbuf[112]);
            movq_r2m (mm7, cbuf[120]);


            cbuf[0] = Temporal[wbuf[32]];
            cbuf[1] = Temporal[wbuf[33]];
            cbuf[2] = Temporal[wbuf[34]];
            cbuf[3] = Temporal[wbuf[35]];
            cbuf[4] = Temporal[wbuf[36]];
            cbuf[5] = Temporal[wbuf[37]];
            cbuf[6] = Temporal[wbuf[38]];
            cbuf[7] = Temporal[wbuf[39]];
            cbuf[8] = Temporal[wbuf[40]];
            cbuf[9] = Temporal[wbuf[41]];
            cbuf[10] = Temporal[wbuf[42]];
            cbuf[11] = Temporal[wbuf[43]];
            cbuf[12] = Temporal[wbuf[44]];
            cbuf[13] = Temporal[wbuf[45]];
            cbuf[14] = Temporal[wbuf[46]];
            cbuf[15] = Temporal[wbuf[47]];
            cbuf[16] = Temporal[wbuf[48]];
            cbuf[17] = Temporal[wbuf[49]];
            cbuf[18] = Temporal[wbuf[50]];
            cbuf[19] = Temporal[wbuf[51]];
            cbuf[20] = Temporal[wbuf[52]];
            cbuf[21] = Temporal[wbuf[53]];
            cbuf[22] = Temporal[wbuf[54]];
            cbuf[23] = Temporal[wbuf[55]];
            cbuf[24] = Temporal[wbuf[56]];
            cbuf[25] = Temporal[wbuf[57]];
            cbuf[26] = Temporal[wbuf[58]];
            cbuf[27] = Temporal[wbuf[59]];
            cbuf[28] = Temporal[wbuf[60]];
            cbuf[29] = Temporal[wbuf[61]];
            cbuf[30] = Temporal[wbuf[62]];
            cbuf[31] = Temporal[wbuf[63]];

            movq_m2r (cbuf[0], mm0);
            movq_m2r (cbuf[8], mm1);
            movq_m2r (cbuf[16], mm2);
            movq_m2r (cbuf[24], mm3);
            paddb_m2r (BlockUp[0], mm0);
            paddb_m2r (BlockUp[8], mm1);
            paddb_m2r (BlockUp[16], mm2);
            paddb_m2r (BlockUp[24], mm3);
            movq_r2m (mm0, BlockCur[0]);
            movq_r2m (mm1, BlockCur[8]);
            movq_r2m (mm2, BlockCur[16]);
            movq_r2m (mm3, BlockCur[24]);
            movq_r2m (mm0, BlockPrev[0]);
            movq_r2m (mm1, BlockPrev[8]);
            movq_r2m (mm2, BlockPrev[16]);
            movq_r2m (mm3, BlockPrev[24]);

            BlockPrev += 32;
            BlockCur += 32;
            BlockUp += 32;
        }
        for (/*X*/; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Horizontal);
            Line[X] = LowPass (Line[X], prev, Vertical);
            LinePrev[X] = LineCur[X] =
                LowPass (LinePrev[X], Line[X], Temporal);
        }
    }
}

static int
denoise3DFilter (VideoFilter * f, VideoFrame * frame)
{
    int width = frame->width;
    int height = frame->height;
    uint8_t *yuvptr = frame->buf;
    ThisFilter *filter = (ThisFilter *) f;

    if (filter->first)
    {
        memcpy(filter->prev, yuvptr, frame->size);
        filter->first = 0;
    }

#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */
    (filter->filtfunc) (yuvptr, filter->prev, filter->line, width, height,
                        filter->coefs[0] + 256, filter->coefs[0] + 256,
                        filter->coefs[1] + 256);
    (filter->filtfunc) (yuvptr + filter->uoff, filter->prev + filter->uoff,
                        filter->line, filter->cwidth, filter->cheight,
                        filter->coefs[2] + 256, filter->coefs[2] + 256,
                        filter->coefs[3] + 256);
    (filter->filtfunc) (yuvptr + filter->voff, filter->prev + filter->voff,
                        filter->line, filter->cwidth, filter->cheight,
                        filter->coefs[2] + 256, filter->coefs[2] + 256,
                        filter->coefs[3] + 256);
    if (filter->mm_flags & MM_MMX)
        emms();
    else
        memcpy (filter->prev, yuvptr, frame->size);
#ifdef TIME_FILTER
    struct timeval t2;
    gettimeofday (&t2, NULL);
    filter->seconds +=
        (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) * .000001;
    filter->frames = (filter->frames + 1) % TIME_INTERVAL;
    if (filter->frames == 0)
    {
        fprintf (stderr,
                 "Denoise3D: filter timed at %3f frames/sec for %dx%d\n",
                 TIME_INTERVAL / filter->seconds, filter->width,
                 filter->height);
        filter->seconds = 0;
    }
#endif /* TIME_FILTER */
    return 0;
}

void
Denoise3DFilterCleanup (VideoFilter * filter)
{
    free (((ThisFilter *)filter)->prev);
    free (((ThisFilter *)filter)->line);
}

VideoFilter *
NewDenoise3DFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                    int *width, int *height, char *options)
{
    double LumSpac, LumTmp, ChromSpac, ChromTmp;
    double Param1, Param2, Param3;
    ThisFilter *filter;
    if (inpixfmt != FMT_YV12)
        return NULL;
    if (outpixfmt != FMT_YV12)
        return NULL;
    filter = malloc (sizeof (ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "Denoise3D: failed to allocate memory for filter\n");
        return NULL;
    }

    filter->line = (uint8_t *) malloc (*width);
    if (filter->line == NULL )
    {
        fprintf (stderr, "Denoise3D: failed to allocate line buffer\n");
        free (filter);
        return NULL;
    }
    filter->prev = (uint8_t *) malloc (*width * *height * 3 / 2);
    if (filter->prev == NULL )
    {
        fprintf (stderr, "Denoise3D: failed to allocate frame buffer\n");
        free (filter->line);
        free (filter);
        return NULL;
    }
    filter->width = *width;
    filter->height = *height;
    filter->uoff = *width * *height;
    filter->voff = *width * *height * 5 / 4;
    filter->cwidth = *width / 2;
    filter->cheight = *height / 2;

    filter->mm_flags = mm_support();
    if (filter->mm_flags & (MM_3DNOW | MM_SSE))
    {
        if (cacheline_size() >= 64)
            filter->filtfunc = &denoise64;
        else
            filter->filtfunc = &denoise32;
    }
    else if (filter->mm_flags & MM_MMX)
    {
        filter->filtfunc = &denoiseMMX;
    }
    else
    {
        filter->filtfunc = &denoise;
    }
    filter->vf.filter = &denoise3DFilter;
    filter->vf.cleanup = &Denoise3DFilterCleanup;
#ifdef TIME_FILTER
    filter->seconds = 0;
    filter->frames = 0;
#endif /* TIME_FILTER */
    filter->first = 1;

    if (options)
    {
        switch (sscanf (options, "%lf:%lf:%lf", &Param1, &Param2, &Param3))
        {
        case 0:
            LumSpac = PARAM1_DEFAULT;
            LumTmp = PARAM3_DEFAULT;
            ChromSpac = PARAM2_DEFAULT;
            break;

        case 1:
            LumSpac = Param1;
            LumTmp = PARAM3_DEFAULT * Param1 / PARAM1_DEFAULT;
            ChromSpac = PARAM2_DEFAULT * Param1 / PARAM1_DEFAULT;
            break;

        case 2:
            LumSpac = Param1;
            LumTmp = PARAM3_DEFAULT * Param1 / PARAM1_DEFAULT;
            ChromSpac = Param2;
            break;

        case 3:
            LumSpac = Param1;
            LumTmp = Param3;
            ChromSpac = Param2;
            break;

        default:
            LumSpac = PARAM1_DEFAULT;
            LumTmp = PARAM3_DEFAULT;
            ChromSpac = PARAM2_DEFAULT;
        }
    }
    else
    {
        LumSpac = PARAM1_DEFAULT;
        LumTmp = PARAM3_DEFAULT;
        ChromSpac = PARAM2_DEFAULT;
    }

    ChromTmp = LumTmp * ChromSpac / LumSpac;
    PrecalcCoefs (filter->coefs[0], LumSpac);
    PrecalcCoefs (filter->coefs[1], LumTmp);
    PrecalcCoefs (filter->coefs[2], ChromSpac);
    PrecalcCoefs (filter->coefs[3], ChromTmp);
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

FilterInfo filter_table[] = 
{
    {
        symbol:     "NewDenoise3DFilter",
        name:       "denoise3d",
        descript:   "removes noise with a spatial and temporal low-pass filter",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
