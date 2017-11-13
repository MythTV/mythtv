/*
   RTjpeg (C) Justin Schoeman 1998 (justin@suntiger.ee.up.ac.za)

   With modifications by:
   (c) 1998, 1999 by Joerg Walter <trouble@moes.pmnet.uni-oldenburg.de>
   and
   (c) 1999 by Wim Taymans <wim.taymans@tvd.be>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "RTjpegN.h"

#ifdef MMX
static mmx_t RTjpeg_ones;
static mmx_t RTjpeg_half;
static mmx_t RTjpeg_C4;
static mmx_t RTjpeg_C6;
static mmx_t RTjpeg_C2mC6;
static mmx_t RTjpeg_C2pC6;
static mmx_t RTjpeg_zero;
#endif

//#define SHOWBLOCK 1
#define BETTERCOMPRESSION 1

static const unsigned char RTjpeg_ZZ[64]={
0,
8, 1,
2, 9, 16,
24, 17, 10, 3,
4, 11, 18, 25, 32,
40, 33, 26, 19, 12, 5,
6, 13, 20, 27, 34, 41, 48,
56, 49, 42, 35, 28, 21, 14, 7,
15, 22, 29, 36, 43, 50, 57,
58, 51, 44, 37, 30, 23,
31, 38, 45, 52, 59,
60, 53, 46, 39,
47, 54, 61,
62, 55,
63 };

static const uint64_t RTjpeg_aan_tab[64]={
4294967296ULL, 5957222912ULL, 5611718144ULL, 5050464768ULL, 4294967296ULL, 3374581504ULL, 2324432128ULL, 1184891264ULL,
5957222912ULL, 8263040512ULL, 7783580160ULL, 7005009920ULL, 5957222912ULL, 4680582144ULL, 3224107520ULL, 1643641088ULL,
5611718144ULL, 7783580160ULL, 7331904512ULL, 6598688768ULL, 5611718144ULL, 4408998912ULL, 3036936960ULL, 1548224000ULL,
5050464768ULL, 7005009920ULL, 6598688768ULL, 5938608128ULL, 5050464768ULL, 3968072960ULL, 2733115392ULL, 1393296000ULL,
4294967296ULL, 5957222912ULL, 5611718144ULL, 5050464768ULL, 4294967296ULL, 3374581504ULL, 2324432128ULL, 1184891264ULL,
3374581504ULL, 4680582144ULL, 4408998912ULL, 3968072960ULL, 3374581504ULL, 2651326208ULL, 1826357504ULL, 931136000ULL,
2324432128ULL, 3224107520ULL, 3036936960ULL, 2733115392ULL, 2324432128ULL, 1826357504ULL, 1258030336ULL, 641204288ULL,
1184891264ULL, 1643641088ULL, 1548224000ULL, 1393296000ULL, 1184891264ULL, 931136000ULL, 641204288ULL, 326894240ULL,
};

static const unsigned char RTjpeg_lum_quant_tbl[64] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
 };

static const unsigned char RTjpeg_chrom_quant_tbl[64] = {
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
 };

#ifdef BETTERCOMPRESSION

/*--------------------------------------------------*/
/*  better encoding, but needs a lot more cpu time  */
/*  seems to be more effective than old method +lzo */
/*  with this encoding lzo isn't efficient anymore  */
/*  there is still more potential for better        */
/*  encoding but that would need even more cputime  */
/*  anyway your mileage may vary                    */
/*                                                  */
/*  written by Martin BIELY and Roman HOCHLEITNER   */
/*--------------------------------------------------*/

/* +++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* Block to Stream (encoding)                         */
/*                                                    */

int RTjpeg::b2s(int16_t *data, int8_t *strm, uint8_t /*bt8*/)
{
 int ci, co=1;
 int16_t ZZvalue;
 unsigned char bitten;
 unsigned char bitoff;

 uint8_t *ustrm = (uint8_t *)strm;
#ifdef SHOWBLOCK

  int ii;
  for (ii=0; ii < 64; ii++) {
    fprintf(stdout, "%d ", data[RTjpeg_ZZ[ii]]);
  }
  fprintf(stdout, "\n\n");

#endif

// *strm++ = 0x10;
// *strm   = 0x00;
//
// return 2;

 // first byte allways written
 ustrm[0]=
      (uint8_t)(data[RTjpeg_ZZ[0]]>254) ? 254:((data[RTjpeg_ZZ[0]]<0)?0:data[RTjpeg_ZZ[0]]);


 ci=63;
 while (data[RTjpeg_ZZ[ci]]==0 && ci>0) ci--;

 bitten = ((unsigned char)ci) << 2;

 if (ci==0) {
   ustrm[1]= bitten;
   co = 2;
   return (int)co;
 }

 /* bitoff=0 because the high 6bit contain first non zero position */
 bitoff = 0;
 co = 1;

 for(; ci>0; ci--) {

   ZZvalue = data[RTjpeg_ZZ[ci]];

   switch(ZZvalue) {
   case 0:
        break;
   case 1:
        bitten |= (0x01<<bitoff);
        break;
   case -1:
        bitten |= (0x03<<bitoff);
        break;
   default:
        bitten |= (0x02<<bitoff);
        goto HERZWEH;
        break;
   }

   if ( bitoff == 0 ) {
      ustrm[co]= bitten;
      bitten = 0;
      bitoff = 8;
      co++;
   } /* "fall through" */
   bitoff-=2;

 }

 /* ci must be 0 */
 if (bitoff != 6) {

      ustrm[co]= bitten;
      co++;

 }
 goto BAUCHWEH;

HERZWEH:
/* ci cannot be 0 */
/* correct bitoff to nibble boundaries */

 switch(bitoff){
 case 4:
 case 6:
   bitoff = 0;
   break;
 case 2:
 case 0:
   ustrm[co]= bitten;
   bitoff = 4;
   co++;
   bitten = 0; // clear half nibble values in bitten
   break;
 default:
   break;
 }

 for(; ci>0; ci--) {

   ZZvalue = data[RTjpeg_ZZ[ci]];

   if ( (ZZvalue > 7) || (ZZvalue < -7) ) {
        bitten |= (0x08<<bitoff);
        goto HIRNWEH;
   }

   bitten |= (ZZvalue&0xf)<<bitoff;

   if ( bitoff == 0 ) {
      ustrm[co]= bitten;
      bitten = 0;
      bitoff = 8;
      co++;
   } /* "fall thru" */
   bitoff-=4;
 }

 /* ci must be 0 */
 if ( bitoff == 0 ) {
    ustrm[co]= bitten;
    co++;
 }
 goto BAUCHWEH;

HIRNWEH:

 ustrm[co]= bitten;
 co++;


 /* bitting is over now we bite */
 for(; ci>0; ci--) {

   ZZvalue = data[RTjpeg_ZZ[ci]];

   if (ZZvalue>0)
   {
     strm[co++]=(int8_t)(ZZvalue>127)?127:ZZvalue;
   }
   else
   {
     strm[co++]=(int8_t)(ZZvalue<-128)?-128:ZZvalue;
   }

 }


BAUCHWEH:
  /* we gotoo much now we are ill */
#ifdef SHOWBLOCK
{
int i;
fprintf(stdout, "\nco = '%d'\n", co);
 for (i=0; i < co+2; i++) {
   fprintf(stdout, "%d ", strm[i]);
 }
fprintf(stdout, "\n\n");
}
#endif

 return (int)co;
}

/* +++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* Stream to Block  (decoding)                        */
/*                                                    */

int RTjpeg::s2b(int16_t *data, int8_t *strm, uint8_t /*bt8*/, int32_t *qtbla)
{
 uint32_t *qtbl = (uint32_t *)qtbla;
 int ci;
 int co;
 int i;
 unsigned char bitten;
 unsigned char bitoff;

 /* first byte always read */
 i=RTjpeg_ZZ[0];
 data[i]=((uint8_t)strm[0])*qtbl[i];

 /* we start at the behind */

 bitten = ((unsigned char)strm[1]) >> 2;
 co = 63;
 for(; co > bitten; co--) {

   data[RTjpeg_ZZ[co]] = 0;

 }

 if (co==0) {
   ci = 2;
   goto AUTOBAHN;
 }

 /* we have to read the last 2 bits of the second byte */
 ci=1;
 bitoff = 0;

 for(; co>0; co--) {

  bitten  = ((unsigned char)strm[ci]) >> bitoff;
  bitten &= 0x03;

  i=RTjpeg_ZZ[co];

  switch( bitten ) {
  case 0x03:
    data[i]= -qtbl[i];
    break;
  case 0x02:
    goto FUSSWEG;
    break;
  case 0x01:
    data[i]= qtbl[i];
    break;
  case 0x00:
    data[i]= 0;
    break;
  default:
    break;
  }

  if ( bitoff == 0 ) {
    bitoff = 8;
    ci++;
  }
  bitoff -= 2;
 }
 /* co is 0 now */
 /* data is written properly */

 /* if bitoff!=6 then ci is the index, but should be the byte count, so we increment by 1 */
 if (bitoff!=6) ci++;

 goto AUTOBAHN;


FUSSWEG:
/* correct bitoff to nibble */
 switch(bitoff){
 case 4:
 case 6:
   bitoff = 0;
   break;
 case 2:
 case 0:
   /* we have to read from the next byte */
   ci++;
   bitoff = 4;
   break;
 default:
   break;
 }

 for(; co>0; co--) {

  bitten  = ((unsigned char)strm[ci]) >> bitoff;
  bitten &= 0x0f;

  i=RTjpeg_ZZ[co];

  if ( bitten == 0x08 ) {
    goto STRASSE;
  }

  /* the compiler cannot do sign extension for signed nibbles */
  if ( bitten & 0x08 ) {
    bitten |= 0xf0;
  }
  /* the unsigned char bitten now is a valid signed char */

  data[i]=((signed char)bitten)*qtbl[i];

  if ( bitoff == 0 ) {
    bitoff = 8;
    ci++;
  }
  bitoff -= 4;
 }
 /* co is 0 */

 /* if bitoff!=4 then ci is the index, but should be the byte count, so we increment by 1 */
 if (bitoff!=4) ci++;

 goto AUTOBAHN;

STRASSE:
  ci++;

 for(; co>0; co--) {
  i=RTjpeg_ZZ[co];
  data[i]=strm[ci++]*qtbl[i];
 }

 /* ci now is the count, because it points to next element => no incrementing */

AUTOBAHN:

#ifdef SHOWBLOCK
fprintf(stdout, "\nci = '%d'\n", ci);
 for (i=0; i < 64; i++) {
   fprintf(stdout, "%d ", data[RTjpeg_ZZ[i]]);
 }
fprintf(stdout, "\n\n");
#endif

 return ci;
}

#else

int RTjpeg::b2s(int16_t *data, int8_t *strm, uint8_t bt8)
{
 register int ci, co=1, tmp;
 register int16_t ZZvalue;

#ifdef SHOWBLOCK

  int ii;
  for (ii=0; ii < 64; ii++) {
    fprintf(stdout, "%d ", data[RTjpeg_ZZ[ii]]);
  }
  fprintf(stdout, "\n\n");

#endif

 (uint8_t)strm[0]=(uint8_t)(data[RTjpeg_ZZ[0]]>254) ? 254:((data[RTjpeg_ZZ[0]]<0)?0:data[RTjpeg_ZZ[0]]);

 for(ci=1; ci<=bt8; ci++)
 {
        ZZvalue = data[RTjpeg_ZZ[ci]];

   if (ZZvalue>0)
        {
     strm[co++]=(int8_t)(ZZvalue>127)?127:ZZvalue;
   }
        else
        {
     strm[co++]=(int8_t)(ZZvalue<-128)?-128:ZZvalue;
   }
 }

 for(; ci<64; ci++)
 {
  ZZvalue = data[RTjpeg_ZZ[ci]];

  if (ZZvalue>0)
  {
   strm[co++]=(int8_t)(ZZvalue>63)?63:ZZvalue;
  }
  else if (ZZvalue<0)
  {
   strm[co++]=(int8_t)(ZZvalue<-64)?-64:ZZvalue;
  }
  else /* compress zeros */
  {
   tmp=ci;
   do
   {
    ci++;
   } while((ci<64)&&(data[RTjpeg_ZZ[ci]]==0));

   strm[co++]=(int8_t)(63+(ci-tmp));
   ci--;
  }
 }
 return (int)co;
}

int RTjpeg::s2b(int16_t *data, int8_t *strm, uint8_t bt8, uint32_t *qtbla)
{
 uint32_t *qtbl = (uint32_t *)qtbla;
 int ci=1, co=1, tmp;
 register int i;

 i=RTjpeg_ZZ[0];
 data[i]=((uint8_t)strm[0])*qtbl[i];

 for(co=1; co<=bt8; co++)
 {
  i=RTjpeg_ZZ[co];
  data[i]=strm[ci++]*qtbl[i];
 }

 for(; co<64; co++)
 {
  if (strm[ci]>63)
  {
   tmp=co+strm[ci]-63;
   for(; co<tmp; co++)data[RTjpeg_ZZ[co]]=0;
   co--;
  } else
  {
   i=RTjpeg_ZZ[co];
   data[i]=strm[ci]*qtbl[i];
  }
  ci++;
 }
 return (int)ci;
}
#endif

#ifdef MMX
void RTjpeg::QuantInit(void)
{
 int i;
 typedef union { int16_t *int16; int32_t *int32; } P16_32;
 P16_32 qtbl;

 qtbl.int32 = lqt;
 for (i = 0; i < 64; i++)
     qtbl.int16[i] = static_cast<int16_t>(lqt[i]);

 qtbl.int32 = cqt;
 for (i = 0; i < 64; i++)
    qtbl.int16[i] = static_cast<int16_t>(cqt[i]);
}

void RTjpeg::Quant(int16_t *block, int32_t *qtbl)
{
 int i;
 mmx_t *bl, *ql;


 ql=(mmx_t *)qtbl;
 bl=(mmx_t *)block;

 movq_m2r(RTjpeg_ones, mm6);
 movq_m2r(RTjpeg_half, mm7);

 for(i=16; i; i--)
 {
  movq_m2r(*(ql++), mm0); /* quant vals (4) */
  movq_m2r(*bl, mm2); /* block vals (4) */
  movq_r2r(mm0, mm1);
  movq_r2r(mm2, mm3);

  punpcklwd_r2r(mm6, mm0); /*           1 qb 1 qa */
  punpckhwd_r2r(mm6, mm1); /* 1 qd 1 qc */

  punpcklwd_r2r(mm7, mm2); /*                   32767 bb 32767 ba */
  punpckhwd_r2r(mm7, mm3); /* 32767 bd 32767 bc */

  pmaddwd_r2r(mm2, mm0); /*                         32767+bb*qb 32767+ba*qa */
  pmaddwd_r2r(mm3, mm1); /* 32767+bd*qd 32767+bc*qc */

  psrad_i2r(16, mm0);
  psrad_i2r(16, mm1);

  packssdw_r2r(mm1, mm0);

  movq_r2m(mm0, *(bl++));
 }
}
#else
void RTjpeg::QuantInit()
{
}

void RTjpeg::Quant(int16_t *block, int32_t *qtbl)
{
 int i;

 for(i=0; i<64; i++)
   block[i]=(int16_t)((block[i]*qtbl[i]+32767)>>16);
}
#endif

/*
 * Perform the forward DCT on one block of samples.
 */
#ifndef MMX
#define FIX_0_382683433  ((int32_t)   98)               /* FIX(0.382683433) */
#define FIX_0_541196100  ((int32_t)  139)               /* FIX(0.541196100) */
#define FIX_0_707106781  ((int32_t)  181)               /* FIX(0.707106781) */
#define FIX_1_306562965  ((int32_t)  334)               /* FIX(1.306562965) */

#define DESCALE10(x) (int16_t)( ((x)+128) >> 8)
#define DESCALE20(x)  (int16_t)(((x)+32768) >> 16)
#define D_MULTIPLY(var,const)  ((int32_t) ((var) * (const)))
#endif

void RTjpeg::DctInit()
{
    int i;

    for(i = 0; i < 64; i++)
    {
        lqt[i] = (((uint64_t)lqt[i] << 32) / RTjpeg_aan_tab[i]);
        cqt[i] = (((uint64_t)cqt[i] << 32) / RTjpeg_aan_tab[i]);
    }
}

void RTjpeg::DctY(uint8_t *idata, int rskip)
{
#ifndef MMX
  int32_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int32_t tmp10, tmp11, tmp12, tmp13;
  int32_t z1, z2, z3, z4, z5, z11, z13;
  uint8_t *idataptr;
  int16_t *odataptr;
  int32_t *wsptr;
  int ctr;


  idataptr = idata;
  wsptr = ws;
  for (ctr = 7; ctr >= 0; ctr--) {
    tmp0 = idataptr[0] + idataptr[7];
    tmp7 = idataptr[0] - idataptr[7];
    tmp1 = idataptr[1] + idataptr[6];
    tmp6 = idataptr[1] - idataptr[6];
    tmp2 = idataptr[2] + idataptr[5];
    tmp5 = idataptr[2] - idataptr[5];
    tmp3 = idataptr[3] + idataptr[4];
    tmp4 = idataptr[3] - idataptr[4];

    tmp10 = (tmp0 + tmp3);      /* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = (tmp1 + tmp2);
    tmp12 = tmp1 - tmp2;

    wsptr[0] = (tmp10 + tmp11)<<8; /* phase 3 */
    wsptr[4] = (tmp10 - tmp11)<<8;

    z1 = D_MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
    wsptr[2] = (tmp13<<8) + z1; /* phase 5 */
    wsptr[6] = (tmp13<<8) - z1;

    tmp10 = tmp4 + tmp5;        /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    z5 = D_MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
    z2 = D_MULTIPLY(tmp10, FIX_0_541196100) + z5; /* c2-c6 */
    z4 = D_MULTIPLY(tmp12, FIX_1_306562965) + z5; /* c2+c6 */
    z3 = D_MULTIPLY(tmp11, FIX_0_707106781); /* c4 */

    z11 = (tmp7<<8) + z3;               /* phase 5 */
    z13 = (tmp7<<8) - z3;

    wsptr[5] = z13 + z2;        /* phase 6 */
    wsptr[3] = z13 - z2;
    wsptr[1] = z11 + z4;
    wsptr[7] = z11 - z4;

    idataptr += rskip<<3;               /* advance pointer to next row */
    wsptr += 8;
  }

  wsptr = ws;
  odataptr = block;
  for (ctr = 7; ctr >= 0; ctr--) {
    tmp0 = wsptr[0] + wsptr[56];
    tmp7 = wsptr[0] - wsptr[56];
    tmp1 = wsptr[8] + wsptr[48];
    tmp6 = wsptr[8] - wsptr[48];
    tmp2 = wsptr[16] + wsptr[40];
    tmp5 = wsptr[16] - wsptr[40];
    tmp3 = wsptr[24] + wsptr[32];
    tmp4 = wsptr[24] - wsptr[32];

    tmp10 = tmp0 + tmp3;        /* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    odataptr[0] = DESCALE10(tmp10 + tmp11); /* phase 3 */
    odataptr[32] = DESCALE10(tmp10 - tmp11);

    z1 = D_MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
    odataptr[16] = DESCALE20((tmp13<<8) + z1); /* phase 5 */
    odataptr[48] = DESCALE20((tmp13<<8) - z1);

    tmp10 = tmp4 + tmp5;        /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    z5 = D_MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
    z2 = D_MULTIPLY(tmp10, FIX_0_541196100) + z5; /* c2-c6 */
    z4 = D_MULTIPLY(tmp12, FIX_1_306562965) + z5; /* c2+c6 */
    z3 = D_MULTIPLY(tmp11, FIX_0_707106781); /* c4 */

    z11 = (tmp7<<8) + z3;               /* phase 5 */
    z13 = (tmp7<<8) - z3;

    odataptr[40] = DESCALE20(z13 + z2); /* phase 6 */
    odataptr[24] = DESCALE20(z13 - z2);
    odataptr[8] = DESCALE20(z11 + z4);
    odataptr[56] = DESCALE20(z11 - z4);

    odataptr++;                 /* advance pointer to next column */
    wsptr++;

  }
#else
  volatile mmx_t tmp6, tmp7;
  mmx_t *dataptr = (mmx_t *)block;
  mmx_t *idata2 = (mmx_t *)idata;


   // first copy the input 8 bit to the destination 16 bits

   movq_m2r(RTjpeg_zero, mm2);

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+1));

        idata2 += rskip;

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr+2));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+3));

        idata2 += rskip;

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr+4));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+5));

        idata2 += rskip;

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr+6));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+7));

        idata2 += rskip;

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr+8));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+9));

        idata2 += rskip;

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr+10));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+11));

        idata2 += rskip;

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr+12));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+13));

        idata2 += rskip;

        movq_m2r(*idata2, mm0);
        movq_r2r(mm0, mm1);

        punpcklbw_r2r(mm2, mm0);
        movq_r2m(mm0, *(dataptr+14));

        punpckhbw_r2r(mm2, mm1);
        movq_r2m(mm1, *(dataptr+15));

/*  Start Transpose to do calculations on rows */

        movq_m2r(*(dataptr+9), mm7);                    // m03:m02|m01:m00 - first line (line 4)and copy into m5

        movq_m2r(*(dataptr+13), mm6);           // m23:m22|m21:m20 - third line (line 6)and copy into m2
        movq_r2r(mm7, mm5);

        punpcklwd_m2r(*(dataptr+11), mm7);      // m11:m01|m10:m00 - interleave first and second lines
        movq_r2r(mm6, mm2);

        punpcklwd_m2r(*(dataptr+15), mm6);  // m31:m21|m30:m20 - interleave third and fourth lines
        movq_r2r(mm7, mm1);

        movq_m2r(*(dataptr+11), mm3);         // m13:m13|m11:m10 - second line
        punpckldq_r2r(mm6, mm7);                                // m30:m20|m10:m00 - interleave to produce result 1

        movq_m2r(*(dataptr+15), mm0);         // m13:m13|m11:m10 - fourth line
        punpckhdq_r2r(mm6, mm1);                                // m31:m21|m11:m01 - interleave to produce result 2

        movq_r2m(mm7,*(dataptr+9));                     // write result 1
        punpckhwd_r2r(mm3, mm5);                                // m13:m03|m12:m02 - interleave first and second lines

        movq_r2m(mm1,*(dataptr+11));                    // write result 2
        punpckhwd_r2r(mm0, mm2);                                // m33:m23|m32:m22 - interleave third and fourth lines

        movq_r2r(mm5, mm1);
        punpckldq_r2r(mm2, mm5);                                // m32:m22|m12:m02 - interleave to produce result 3

        movq_m2r(*(dataptr+1), mm0);                    // m03:m02|m01:m00 - first line, 4x4
        punpckhdq_r2r(mm2, mm1);                                // m33:m23|m13:m03 - interleave to produce result 4

        movq_r2m(mm5,*(dataptr+13));                    // write result 3

        // last 4x4 done

        movq_r2m(mm1, *(dataptr+15));                   // write result 4, last 4x4

        movq_m2r(*(dataptr+5), mm2);                    // m23:m22|m21:m20 - third line
        movq_r2r(mm0, mm6);

        punpcklwd_m2r(*(dataptr+3), mm0);       // m11:m01|m10:m00 - interleave first and second lines
        movq_r2r(mm2, mm7);

        punpcklwd_m2r(*(dataptr+7), mm2);       // m31:m21|m30:m20 - interleave third and fourth lines
        movq_r2r(mm0, mm4);

        //
        movq_m2r(*(dataptr+8), mm1);                    // n03:n02|n01:n00 - first line
        punpckldq_r2r(mm2, mm0);                                // m30:m20|m10:m00 - interleave to produce first result

        movq_m2r(*(dataptr+12), mm3);                   // n23:n22|n21:n20 - third line
        punpckhdq_r2r(mm2, mm4);                                // m31:m21|m11:m01 - interleave to produce second result

        punpckhwd_m2r(*(dataptr+3), mm6);       // m13:m03|m12:m02 - interleave first and second lines
        movq_r2r(mm1, mm2);                     // copy first line

        punpckhwd_m2r(*(dataptr+7), mm7);       // m33:m23|m32:m22 - interleave third and fourth lines
        movq_r2r(mm6, mm5);                                             // copy first intermediate result

        movq_r2m(mm0, *(dataptr+8));                    // write result 1
        punpckhdq_r2r(mm7, mm5);                                // m33:m23|m13:m03 - produce third result

        punpcklwd_m2r(*(dataptr+10), mm1);  // n11:n01|n10:n00 - interleave first and second lines
        movq_r2r(mm3, mm0);                                             // copy third line

        punpckhwd_m2r(*(dataptr+10), mm2);  // n13:n03|n12:n02 - interleave first and second lines

        movq_r2m(mm4, *(dataptr+10));                   // write result 2 out
        punpckldq_r2r(mm7, mm6);                                // m32:m22|m12:m02 - produce fourth result

        punpcklwd_m2r(*(dataptr+14), mm3);  // n31:n21|n30:n20 - interleave third and fourth lines
        movq_r2r(mm1, mm4);

        movq_r2m(mm6, *(dataptr+12));                   // write result 3 out
        punpckldq_r2r(mm3, mm1);                                // n30:n20|n10:n00 - produce first result

        punpckhwd_m2r(*(dataptr+14), mm0);  // n33:n23|n32:n22 - interleave third and fourth lines
        movq_r2r(mm2, mm6);

        movq_r2m(mm5, *(dataptr+14));                   // write result 4 out
        punpckhdq_r2r(mm3, mm4);                                // n31:n21|n11:n01- produce second result

        movq_r2m(mm1, *(dataptr+1));                    // write result 5 out - (first result for other 4 x 4 block)
        punpckldq_r2r(mm0, mm2);                                // n32:n22|n12:n02- produce third result

        movq_r2m(mm4, *(dataptr+3));                    // write result 6 out
        punpckhdq_r2r(mm0, mm6);                                // n33:n23|n13:n03 - produce fourth result

        movq_r2m(mm2, *(dataptr+5));                    // write result 7 out

        movq_m2r(*dataptr, mm0);                                // m03:m02|m01:m00 - first line, first 4x4

        movq_r2m(mm6, *(dataptr+7));                    // write result 8 out


// Do first 4x4 quadrant, which is used in the beginning of the DCT:

        movq_m2r(*(dataptr+4), mm7);                    // m23:m22|m21:m20 - third line
        movq_r2r(mm0, mm2);

        punpcklwd_m2r(*(dataptr+2), mm0);       // m11:m01|m10:m00 - interleave first and second lines
        movq_r2r(mm7, mm4);

        punpcklwd_m2r(*(dataptr+6), mm7);       // m31:m21|m30:m20 - interleave third and fourth lines
        movq_r2r(mm0, mm1);

        movq_m2r(*(dataptr+2), mm6);                    // m13:m12|m11:m10 - second line
        punpckldq_r2r(mm7, mm0);                                // m30:m20|m10:m00 - interleave to produce result 1

        movq_m2r(*(dataptr+6), mm5);                    // m33:m32|m31:m30 - fourth line
        punpckhdq_r2r(mm7, mm1);                                // m31:m21|m11:m01 - interleave to produce result 2

        movq_r2r(mm0, mm7);                                             // write result 1
        punpckhwd_r2r(mm6, mm2);                                // m13:m03|m12:m02 - interleave first and second lines

        psubw_m2r(*(dataptr+14), mm7);          // tmp07=x0-x7  /* Stage 1 */
        movq_r2r(mm1, mm6);                                             // write result 2

        paddw_m2r(*(dataptr+14), mm0);          // tmp00=x0+x7  /* Stage 1 */
        punpckhwd_r2r(mm5, mm4);                        // m33:m23|m32:m22 - interleave third and fourth lines

        paddw_m2r(*(dataptr+12), mm1);          // tmp01=x1+x6  /* Stage 1 */
        movq_r2r(mm2, mm3);                                             // copy first intermediate result

        psubw_m2r(*(dataptr+12), mm6);          // tmp06=x1-x6  /* Stage 1 */
        punpckldq_r2r(mm4, mm2);                                // m32:m22|m12:m02 - interleave to produce result 3

   movq_r2m(mm7, tmp7);
        movq_r2r(mm2, mm5);                                             // write result 3

   movq_r2m(mm6, tmp6);
        punpckhdq_r2r(mm4, mm3);                                // m33:m23|m13:m03 - interleave to produce result 4

        paddw_m2r(*(dataptr+10), mm2);          // tmp02=x2+5 /* Stage 1 */
        movq_r2r(mm3, mm4);                                             // write result 4

/************************************************************************************************
                                        End of Transpose
************************************************************************************************/


   paddw_m2r(*(dataptr+8), mm3);        // tmp03=x3+x4 /* stage 1*/
   movq_r2r(mm0, mm7);

   psubw_m2r(*(dataptr+8), mm4);        // tmp04=x3-x4 /* stage 1*/
   movq_r2r(mm1, mm6);

        paddw_r2r(mm3, mm0);                                    // tmp10 = tmp00 + tmp03 /* even 2 */
        psubw_r2r(mm3, mm7);                                    // tmp13 = tmp00 - tmp03 /* even 2 */

        psubw_r2r(mm2, mm6);                                    // tmp12 = tmp01 - tmp02 /* even 2 */
        paddw_r2r(mm2, mm1);                                    // tmp11 = tmp01 + tmp02 /* even 2 */

   psubw_m2r(*(dataptr+10), mm5);       // tmp05=x2-x5 /* stage 1*/
        paddw_r2r(mm7, mm6);                                            // tmp12 + tmp13

        /* stage 3 */

   movq_m2r(tmp6, mm2);
   movq_r2r(mm0, mm3);

        psllw_i2r(2, mm6);                      // m8 * 2^2
        paddw_r2r(mm1, mm0);

        pmulhw_m2r(RTjpeg_C4, mm6);                     // z1
        psubw_r2r(mm1, mm3);

   movq_r2m(mm0, *dataptr);
   movq_r2r(mm7, mm0);

    /* Odd part */
   movq_r2m(mm3, *(dataptr+8));
        paddw_r2r(mm5, mm4);                                            // tmp10

   movq_m2r(tmp7, mm3);
        paddw_r2r(mm6, mm0);                                            // tmp32

        paddw_r2r(mm2, mm5);                                            // tmp11
        psubw_r2r(mm6, mm7);                                            // tmp33

   movq_r2m(mm0, *(dataptr+4));
        paddw_r2r(mm3, mm2);                                            // tmp12

        /* stage 4 */

   movq_r2m(mm7, *(dataptr+12));
        movq_r2r(mm4, mm1);                                             // copy of tmp10

        psubw_r2r(mm2, mm1);                                            // tmp10 - tmp12
        psllw_i2r(2, mm4);                      // m8 * 2^2

        movq_m2r(RTjpeg_C2mC6, mm0);
        psllw_i2r(2, mm1);

        pmulhw_m2r(RTjpeg_C6, mm1);                     // z5
        psllw_i2r(2, mm2);

        pmulhw_r2r(mm0, mm4);                                   // z5

        /* stage 5 */

        pmulhw_m2r(RTjpeg_C2pC6, mm2);
        psllw_i2r(2, mm5);

        pmulhw_m2r(RTjpeg_C4, mm5);                     // z3
        movq_r2r(mm3, mm0);                                             // copy tmp7

   movq_m2r(*(dataptr+1), mm7);
        paddw_r2r(mm1, mm4);                                            // z2

        paddw_r2r(mm1, mm2);                                            // z4

        paddw_r2r(mm5, mm0);                                            // z11
        psubw_r2r(mm5, mm3);                                            // z13

        /* stage 6 */

        movq_r2r(mm3, mm5);                                             // copy z13
        psubw_r2r(mm4, mm3);                                            // y3=z13 - z2

        paddw_r2r(mm4, mm5);                                            // y5=z13 + z2
        movq_r2r(mm0, mm6);                                             // copy z11

   movq_r2m(mm3, *(dataptr+6));                         //save y3
        psubw_r2r(mm2, mm0);                                            // y7=z11 - z4

   movq_r2m(mm5, *(dataptr+10));                //save y5
        paddw_r2r(mm2, mm6);                                            // y1=z11 + z4

   movq_r2m(mm0, *(dataptr+14));                //save y7

        /************************************************
         *  End of 1st 4 rows
         ************************************************/

   movq_m2r(*(dataptr+3), mm1);                         // load x1   /* stage 1 */
        movq_r2r(mm7, mm0);                                             // copy x0

   movq_r2m(mm6, *(dataptr+2));                         //save y1

   movq_m2r(*(dataptr+5), mm2);                         // load x2   /* stage 1 */
        movq_r2r(mm1, mm6);                                             // copy x1

   paddw_m2r(*(dataptr+15), mm0);               // tmp00 = x0 + x7

   movq_m2r(*(dataptr+7), mm3);                         // load x3   /* stage 1 */
        movq_r2r(mm2, mm5);                                             // copy x2

   psubw_m2r(*(dataptr+15), mm7);               // tmp07 = x0 - x7
        movq_r2r(mm3, mm4);                                             // copy x3

   paddw_m2r(*(dataptr+13), mm1);               // tmp01 = x1 + x6

        movq_r2m(mm7, tmp7);                                            // save tmp07
        movq_r2r(mm0, mm7);                                             // copy tmp00

   psubw_m2r(*(dataptr+13), mm6);               // tmp06 = x1 - x6

   /* stage 2, Even Part */

   paddw_m2r(*(dataptr+9), mm3);                // tmp03 = x3 + x4

        movq_r2m(mm6, tmp6);                                            // save tmp07
        movq_r2r(mm1, mm6);                                             // copy tmp01

   paddw_m2r(*(dataptr+11), mm2);               // tmp02 = x2 + x5
        paddw_r2r(mm3, mm0);                    // tmp10 = tmp00 + tmp03

        psubw_r2r(mm3, mm7);                    // tmp13 = tmp00 - tmp03

   psubw_m2r(*(dataptr+9), mm4);                // tmp04 = x3 - x4
        psubw_r2r(mm2, mm6);                    // tmp12 = tmp01 - tmp02

        paddw_r2r(mm2, mm1);                    // tmp11 = tmp01 + tmp02

   psubw_m2r(*(dataptr+11), mm5);               // tmp05 = x2 - x5
        paddw_r2r(mm7, mm6);                    //  tmp12 + tmp13

   /* stage 3, Even and stage 4 & 5 even */

        movq_m2r(tmp6, mm2);                            // load tmp6
        movq_r2r(mm0, mm3);                                             // copy tmp10

        psllw_i2r(2, mm6);                      // shift z1
        paddw_r2r(mm1, mm0);                                    // y0=tmp10 + tmp11

        pmulhw_m2r(RTjpeg_C4, mm6);             // z1
        psubw_r2r(mm1, mm3);                                    // y4=tmp10 - tmp11

   movq_r2m(mm0, *(dataptr+1));                         //save y0
        movq_r2r(mm7, mm0);                                             // copy tmp13

        /* odd part */

   movq_r2m(mm3, *(dataptr+9));                         //save y4
        paddw_r2r(mm5, mm4);                    // tmp10 = tmp4 + tmp5

        movq_m2r(tmp7, mm3);                            // load tmp7
        paddw_r2r(mm6, mm0);                    // tmp32 = tmp13 + z1

        paddw_r2r(mm2, mm5);                    // tmp11 = tmp5 + tmp6
        psubw_r2r(mm6, mm7);                    // tmp33 = tmp13 - z1

   movq_r2m(mm0, *(dataptr+5));                         //save y2
        paddw_r2r(mm3, mm2);                    // tmp12 = tmp6 + tmp7

        /* stage 4 */

   movq_r2m(mm7, *(dataptr+13));                //save y6
        movq_r2r(mm4, mm1);                                             // copy tmp10

        psubw_r2r(mm2, mm1);                                    // tmp10 - tmp12
        psllw_i2r(2, mm4);                      // shift tmp10

        movq_m2r(RTjpeg_C2mC6, mm0);                    // load C2mC6
        psllw_i2r(2, mm1);                      // shift (tmp10-tmp12)

        pmulhw_m2r(RTjpeg_C6, mm1);             // z5
        psllw_i2r(2, mm5);                      // prepare for multiply

        pmulhw_r2r(mm0, mm4);                                   // multiply by converted real

        /* stage 5 */

        pmulhw_m2r(RTjpeg_C4, mm5);                     // z3
        psllw_i2r(2, mm2);                      // prepare for multiply

        pmulhw_m2r(RTjpeg_C2pC6, mm2);          // multiply
        movq_r2r(mm3, mm0);                                             // copy tmp7

        movq_m2r(*(dataptr+9), mm7);                    // m03:m02|m01:m00 - first line (line 4)and copy into mm7
        paddw_r2r(mm1, mm4);                                            // z2

        paddw_r2r(mm5, mm0);                                            // z11
        psubw_r2r(mm5, mm3);                                            // z13

        /* stage 6 */

        movq_r2r(mm3, mm5);                                             // copy z13
        paddw_r2r(mm1, mm2);                                            // z4

        movq_r2r(mm0, mm6);                                             // copy z11
        psubw_r2r(mm4, mm5);                                            // y3

        paddw_r2r(mm2, mm6);                                            // y1
        paddw_r2r(mm4, mm3);                                            // y5

   movq_r2m(mm5, *(dataptr+7));                         //save y3

   movq_r2m(mm6, *(dataptr+3));                         //save y1
        psubw_r2r(mm2, mm0);                                            // y7

/************************************************************************************************
                                        Start of Transpose
************************************************************************************************/

        movq_m2r(*(dataptr+13), mm6);                   // m23:m22|m21:m20 - third line (line 6)and copy into m2
        movq_r2r(mm7, mm5);                                             // copy first line

        punpcklwd_r2r(mm3, mm7);                                // m11:m01|m10:m00 - interleave first and second lines
        movq_r2r(mm6, mm2);                                             // copy third line

        punpcklwd_r2r(mm0, mm6);                                // m31:m21|m30:m20 - interleave third and fourth lines
        movq_r2r(mm7, mm1);                                             // copy first intermediate result

        punpckldq_r2r(mm6, mm7);                                // m30:m20|m10:m00 - interleave to produce result 1

        punpckhdq_r2r(mm6, mm1);                                // m31:m21|m11:m01 - interleave to produce result 2

        movq_r2m(mm7, *(dataptr+9));                    // write result 1
        punpckhwd_r2r(mm3, mm5);                                // m13:m03|m12:m02 - interleave first and second lines

        movq_r2m(mm1, *(dataptr+11));                   // write result 2
        punpckhwd_r2r(mm0, mm2);                                // m33:m23|m32:m22 - interleave third and fourth lines

        movq_r2r(mm5, mm1);                                             // copy first intermediate result
        punpckldq_r2r(mm2, mm5);                                // m32:m22|m12:m02 - interleave to produce result 3

        movq_m2r(*(dataptr+1), mm0);                    // m03:m02|m01:m00 - first line, 4x4
        punpckhdq_r2r(mm2, mm1);                                // m33:m23|m13:m03 - interleave to produce result 4

        movq_r2m(mm5, *(dataptr+13));                   // write result 3

        /****** last 4x4 done */

        movq_r2m(mm1, *(dataptr+15));                   // write result 4, last 4x4

        movq_m2r(*(dataptr+5), mm2);                    // m23:m22|m21:m20 - third line
        movq_r2r(mm0, mm6);                                             // copy first line

        punpcklwd_m2r(*(dataptr+3), mm0);       // m11:m01|m10:m00 - interleave first and second lines
        movq_r2r(mm2, mm7);                                             // copy third line

        punpcklwd_m2r(*(dataptr+7), mm2);       // m31:m21|m30:m20 - interleave third and fourth lines
        movq_r2r(mm0, mm4);                                             // copy first intermediate result



        movq_m2r(*(dataptr+8), mm1);                    // n03:n02|n01:n00 - first line
        punpckldq_r2r(mm2, mm0);                                // m30:m20|m10:m00 - interleave to produce first result

        movq_m2r(*(dataptr+12), mm3);                   // n23:n22|n21:n20 - third line
        punpckhdq_r2r(mm2, mm4);                                // m31:m21|m11:m01 - interleave to produce second result

        punpckhwd_m2r(*(dataptr+3), mm6);       // m13:m03|m12:m02 - interleave first and second lines
        movq_r2r(mm1, mm2);                                             // copy first line

        punpckhwd_m2r(*(dataptr+7), mm7);       // m33:m23|m32:m22 - interleave third and fourth lines
        movq_r2r(mm6, mm5);                                             // copy first intermediate result

        movq_r2m(mm0, *(dataptr+8));                    // write result 1
        punpckhdq_r2r(mm7, mm5);                                // m33:m23|m13:m03 - produce third result

        punpcklwd_m2r(*(dataptr+10), mm1);  // n11:n01|n10:n00 - interleave first and second lines
        movq_r2r(mm3, mm0);                                             // copy third line

        punpckhwd_m2r(*(dataptr+10), mm2);  // n13:n03|n12:n02 - interleave first and second lines

        movq_r2m(mm4, *(dataptr+10));                   // write result 2 out
        punpckldq_r2r(mm7, mm6);                                // m32:m22|m12:m02 - produce fourth result

        punpcklwd_m2r(*(dataptr+14), mm3);  // n33:n23|n32:n22 - interleave third and fourth lines
        movq_r2r(mm1, mm4);                                             // copy second intermediate result

        movq_r2m(mm6, *(dataptr+12));                   // write result 3 out
        punpckldq_r2r(mm3, mm1);                                //

        punpckhwd_m2r(*(dataptr+14), mm0);  // n33:n23|n32:n22 - interleave third and fourth lines
        movq_r2r(mm2, mm6);                                             // copy second intermediate result

        movq_r2m(mm5, *(dataptr+14));                   // write result 4 out
        punpckhdq_r2r(mm3, mm4);                                // n31:n21|n11:n01- produce second result

        movq_r2m(mm1, *(dataptr+1));                    // write result 5 out - (first result for other 4 x 4 block)
        punpckldq_r2r(mm0, mm2);                                // n32:n22|n12:n02- produce third result

        movq_r2m(mm4, *(dataptr+3));                    // write result 6 out
        punpckhdq_r2r(mm0, mm6);                                // n33:n23|n13:n03 - produce fourth result

        movq_r2m(mm2, *(dataptr+5));                    // write result 7 out

        movq_m2r(*dataptr, mm0);                                // m03:m02|m01:m00 - first line, first 4x4

        movq_r2m(mm6, *(dataptr+7));                    // write result 8 out

// Do first 4x4 quadrant, which is used in the beginning of the DCT:

        movq_m2r(*(dataptr+4), mm7);                    // m23:m22|m21:m20 - third line
        movq_r2r(mm0, mm2);                                             // copy first line

        punpcklwd_m2r(*(dataptr+2), mm0);       // m11:m01|m10:m00 - interleave first and second lines
        movq_r2r(mm7, mm4);                                             // copy third line

        punpcklwd_m2r(*(dataptr+6), mm7);       // m31:m21|m30:m20 - interleave third and fourth lines
        movq_r2r(mm0, mm1);                                             // copy first intermediate result

        movq_m2r(*(dataptr+2), mm6);                    // m13:m12|m11:m10 - second line
        punpckldq_r2r(mm7, mm0);                                // m30:m20|m10:m00 - interleave to produce result 1

        movq_m2r(*(dataptr+6), mm5);                    // m33:m32|m31:m30 - fourth line
        punpckhdq_r2r(mm7, mm1);                                // m31:m21|m11:m01 - interleave to produce result 2

        movq_r2r(mm0, mm7);                                             // write result 1
        punpckhwd_r2r(mm6, mm2);                                // m13:m03|m12:m02 - interleave first and second lines

        psubw_m2r(*(dataptr+14), mm7);          // tmp07=x0-x7  /* Stage 1 */
        movq_r2r(mm1, mm6);                                             // write result 2

        paddw_m2r(*(dataptr+14), mm0);          // tmp00=x0+x7  /* Stage 1 */
        punpckhwd_r2r(mm5, mm4);                        // m33:m23|m32:m22 - interleave third and fourth lines

        paddw_m2r(*(dataptr+12), mm1);          // tmp01=x1+x6  /* Stage 1 */
        movq_r2r(mm2, mm3);                                             // copy first intermediate result

        psubw_m2r(*(dataptr+12), mm6);          // tmp06=x1-x6  /* Stage 1 */
        punpckldq_r2r(mm4, mm2);                                // m32:m22|m12:m02 - interleave to produce result 3

        movq_r2m(mm7, tmp7);                                            // save tmp07
        movq_r2r(mm2, mm5);                                             // write result 3

        movq_r2m(mm6, tmp6);                                            // save tmp06

        punpckhdq_r2r(mm4, mm3);                                // m33:m23|m13:m03 - interleave to produce result 4

        paddw_m2r(*(dataptr+10), mm2);          // tmp02=x2+x5 /* stage 1 */
        movq_r2r(mm3, mm4);                                             // write result 4

/************************************************************************************************
                                        End of Transpose 2
************************************************************************************************/

   paddw_m2r(*(dataptr+8), mm3);        // tmp03=x3+x4 /* stage 1*/
   movq_r2r(mm0, mm7);

   psubw_m2r(*(dataptr+8), mm4);        // tmp04=x3-x4 /* stage 1*/
   movq_r2r(mm1, mm6);

        paddw_r2r(mm3, mm0);                                    // tmp10 = tmp00 + tmp03 /* even 2 */
        psubw_r2r(mm3, mm7);                                    // tmp13 = tmp00 - tmp03 /* even 2 */

        psubw_r2r(mm2, mm6);                                    // tmp12 = tmp01 - tmp02 /* even 2 */
        paddw_r2r(mm2, mm1);                                    // tmp11 = tmp01 + tmp02 /* even 2 */

   psubw_m2r(*(dataptr+10), mm5);       // tmp05=x2-x5 /* stage 1*/
        paddw_r2r(mm7, mm6);                                            // tmp12 + tmp13

        /* stage 3 */

   movq_m2r(tmp6, mm2);
   movq_r2r(mm0, mm3);

        psllw_i2r(2, mm6);                      // m8 * 2^2
        paddw_r2r(mm1, mm0);

        pmulhw_m2r(RTjpeg_C4, mm6);                     // z1
        psubw_r2r(mm1, mm3);

   movq_r2m(mm0, *dataptr);
   movq_r2r(mm7, mm0);

    /* Odd part */
   movq_r2m(mm3, *(dataptr+8));
        paddw_r2r(mm5, mm4);                                            // tmp10

   movq_m2r(tmp7, mm3);
        paddw_r2r(mm6, mm0);                                            // tmp32

        paddw_r2r(mm2, mm5);                                            // tmp11
        psubw_r2r(mm6, mm7);                                            // tmp33

   movq_r2m(mm0, *(dataptr+4));
        paddw_r2r(mm3, mm2);                                            // tmp12

        /* stage 4 */
   movq_r2m(mm7, *(dataptr+12));
        movq_r2r(mm4, mm1);                                             // copy of tmp10

        psubw_r2r(mm2, mm1);                                            // tmp10 - tmp12
        psllw_i2r(2, mm4);                      // m8 * 2^2

        movq_m2r(RTjpeg_C2mC6, mm0);
        psllw_i2r(2, mm1);

        pmulhw_m2r(RTjpeg_C6, mm1);                     // z5
        psllw_i2r(2, mm2);

        pmulhw_r2r(mm0, mm4);                                   // z5

        /* stage 5 */

        pmulhw_m2r(RTjpeg_C2pC6, mm2);
        psllw_i2r(2, mm5);

        pmulhw_m2r(RTjpeg_C4, mm5);                     // z3
        movq_r2r(mm3, mm0);                                             // copy tmp7

   movq_m2r(*(dataptr+1), mm7);
        paddw_r2r(mm1, mm4);                                            // z2

        paddw_r2r(mm1, mm2);                                            // z4

        paddw_r2r(mm5, mm0);                                            // z11
        psubw_r2r(mm5, mm3);                                            // z13

        /* stage 6 */

        movq_r2r(mm3, mm5);                                             // copy z13
        psubw_r2r(mm4, mm3);                                            // y3=z13 - z2

        paddw_r2r(mm4, mm5);                                            // y5=z13 + z2
        movq_r2r(mm0, mm6);                                             // copy z11

   movq_r2m(mm3, *(dataptr+6));                         //save y3
        psubw_r2r(mm2, mm0);                                            // y7=z11 - z4

   movq_r2m(mm5, *(dataptr+10));                //save y5
        paddw_r2r(mm2, mm6);                                            // y1=z11 + z4

   movq_r2m(mm0, *(dataptr+14));                //save y7

        /************************************************
         *  End of 1st 4 rows
         ************************************************/

   movq_m2r(*(dataptr+3), mm1);                         // load x1   /* stage 1 */
        movq_r2r(mm7, mm0);                                             // copy x0

   movq_r2m(mm6, *(dataptr+2));                         //save y1

   movq_m2r(*(dataptr+5), mm2);                         // load x2   /* stage 1 */
        movq_r2r(mm1, mm6);                                             // copy x1

   paddw_m2r(*(dataptr+15), mm0);               // tmp00 = x0 + x7

   movq_m2r(*(dataptr+7), mm3);                         // load x3   /* stage 1 */
        movq_r2r(mm2, mm5);                                             // copy x2

   psubw_m2r(*(dataptr+15), mm7);               // tmp07 = x0 - x7
        movq_r2r(mm3, mm4);                                             // copy x3

   paddw_m2r(*(dataptr+13), mm1);               // tmp01 = x1 + x6

        movq_r2m(mm7, tmp7);                                            // save tmp07
        movq_r2r(mm0, mm7);                                             // copy tmp00

   psubw_m2r(*(dataptr+13), mm6);               // tmp06 = x1 - x6

   /* stage 2, Even Part */

   paddw_m2r(*(dataptr+9), mm3);                // tmp03 = x3 + x4

        movq_r2m(mm6, tmp6);                                            // save tmp07
        movq_r2r(mm1, mm6);                                             // copy tmp01

   paddw_m2r(*(dataptr+11), mm2);               // tmp02 = x2 + x5
        paddw_r2r(mm3, mm0);                    // tmp10 = tmp00 + tmp03

        psubw_r2r(mm3, mm7);                    // tmp13 = tmp00 - tmp03

   psubw_m2r(*(dataptr+9), mm4);                // tmp04 = x3 - x4
        psubw_r2r(mm2, mm6);                    // tmp12 = tmp01 - tmp02

        paddw_r2r(mm2, mm1);                    // tmp11 = tmp01 + tmp02

   psubw_m2r(*(dataptr+11), mm5);               // tmp05 = x2 - x5
        paddw_r2r(mm7, mm6);                    //  tmp12 + tmp13

   /* stage 3, Even and stage 4 & 5 even */

        movq_m2r(tmp6, mm2);                            // load tmp6
        movq_r2r(mm0, mm3);                                             // copy tmp10

        psllw_i2r(2, mm6);                      // shift z1
        paddw_r2r(mm1, mm0);                                    // y0=tmp10 + tmp11

        pmulhw_m2r(RTjpeg_C4, mm6);             // z1
        psubw_r2r(mm1, mm3);                                    // y4=tmp10 - tmp11

   movq_r2m(mm0, *(dataptr+1));                         //save y0
        movq_r2r(mm7, mm0);                                             // copy tmp13

        /* odd part */

   movq_r2m(mm3, *(dataptr+9));                         //save y4
        paddw_r2r(mm5, mm4);                    // tmp10 = tmp4 + tmp5

        movq_m2r(tmp7, mm3);                            // load tmp7
        paddw_r2r(mm6, mm0);                    // tmp32 = tmp13 + z1

        paddw_r2r(mm2, mm5);                    // tmp11 = tmp5 + tmp6
        psubw_r2r(mm6, mm7);                    // tmp33 = tmp13 - z1

   movq_r2m(mm0, *(dataptr+5));                         //save y2
        paddw_r2r(mm3, mm2);                    // tmp12 = tmp6 + tmp7

        /* stage 4 */

   movq_r2m(mm7, *(dataptr+13));                //save y6
        movq_r2r(mm4, mm1);                                             // copy tmp10

        psubw_r2r(mm2, mm1);                                    // tmp10 - tmp12
        psllw_i2r(2, mm4);                      // shift tmp10

        movq_m2r(RTjpeg_C2mC6, mm0);                    // load C2mC6
        psllw_i2r(2, mm1);                      // shift (tmp10-tmp12)

        pmulhw_m2r(RTjpeg_C6, mm1);             // z5
        psllw_i2r(2, mm5);                      // prepare for multiply

        pmulhw_r2r(mm0, mm4);                                   // multiply by converted real

        /* stage 5 */

        pmulhw_m2r(RTjpeg_C4, mm5);                     // z3
        psllw_i2r(2, mm2);                      // prepare for multiply

        pmulhw_m2r(RTjpeg_C2pC6, mm2);          // multiply
        movq_r2r(mm3, mm0);                                             // copy tmp7

        movq_m2r(*(dataptr+9), mm7);                    // m03:m02|m01:m00 - first line (line 4)and copy into mm7
        paddw_r2r(mm1, mm4);                                            // z2

        paddw_r2r(mm5, mm0);                                            // z11
        psubw_r2r(mm5, mm3);                                            // z13

        /* stage 6 */

        movq_r2r(mm3, mm5);                                             // copy z13
        paddw_r2r(mm1, mm2);                                            // z4

        movq_r2r(mm0, mm6);                                             // copy z11
        psubw_r2r(mm4, mm5);                                            // y3

        paddw_r2r(mm2, mm6);                                            // y1
        paddw_r2r(mm4, mm3);                                            // y5

   movq_r2m(mm5, *(dataptr+7));                         //save y3
        psubw_r2r(mm2, mm0);                                            // y�=z11 - z4

   movq_r2m(mm3, *(dataptr+11));                //save y5

   movq_r2m(mm6, *(dataptr+3));                         //save y1

   movq_r2m(mm0, *(dataptr+15));                //save y7


#endif
}

#define FIX_1_082392200  ((int32_t)  277)               /* FIX(1.082392200) */
#define FIX_1_414213562  ((int32_t)  362)               /* FIX(1.414213562) */
#define FIX_1_847759065  ((int32_t)  473)               /* FIX(1.847759065) */
#define FIX_2_613125930  ((int32_t)  669)               /* FIX(2.613125930) */

#define DESCALE(x) (int16_t)( ((x)+4) >> 3)

/* clip yuv to 16..235 (should be 16..240 for cr/cb but ... */

#define RL(x) ((x)>235) ? 235 : (((x)<16) ? 16 : (x))
#define MULTIPLY(var,const)  (((int32_t) ((var) * (const)) + 128)>>8)

void RTjpeg::IdctInit(void)
{
    int i;

    for( i = 0; i < 64; i++)
    {
        liqt[i] = ((uint64_t)liqt[i] * RTjpeg_aan_tab[i]) >> 32;
        ciqt[i] = ((uint64_t)ciqt[i] * RTjpeg_aan_tab[i]) >> 32;
    }
}

void RTjpeg::Idct(uint8_t *odata, int16_t *data, int rskip)
{
#ifdef MMX

static mmx_t fix_141;         fix_141.q = (long long)0x5a825a825a825a82LL;
static mmx_t fix_184n261; fix_184n261.q = (long long)0xcf04cf04cf04cf04LL;
static mmx_t fix_184;         fix_184.q = (long long)0x7641764176417641LL;
static mmx_t fix_n184;       fix_n184.q = (long long)0x896f896f896f896fLL;
static mmx_t fix_108n184; fix_108n184.q = (long long)0xcf04cf04cf04cf04LL;

  mmx_t *wsptr = (mmx_t *)ws;
  mmx_t *dataptr = (mmx_t *)odata;
  mmx_t *idata = (mmx_t *)data;

  rskip = rskip>>3;
/*
 * Perform inverse DCT on one block of coefficients.
 */

    /* Odd part */

        movq_m2r(*(idata+10), mm1);     // load idata[DCTSIZE*5]

        movq_m2r(*(idata+6), mm0);              // load idata[DCTSIZE*3]

        movq_m2r(*(idata+2), mm3);              // load idata[DCTSIZE*1]

        movq_r2r(mm1, mm2);                             // copy tmp6    /* phase 6 */

        movq_m2r(*(idata+14), mm4);     // load idata[DCTSIZE*7]

        paddw_r2r(mm0, mm1);                            // z13 = tmp6 + tmp5;

        psubw_r2r(mm0, mm2);                            // z10 = tmp6 - tmp5

        psllw_i2r(2, mm2);                              // shift z10
        movq_r2r(mm2, mm0);                             // copy z10

        pmulhw_m2r(fix_184n261, mm2);   // MULTIPLY( z12, FIX_1_847759065); /* 2*c2 */
        movq_r2r(mm3, mm5);                             // copy tmp4

        pmulhw_m2r(fix_n184, mm0);              // MULTIPLY(z10, -FIX_1_847759065); /* 2*c2 */
        paddw_r2r(mm4, mm3);                            // z11 = tmp4 + tmp7;

        movq_r2r(mm3, mm6);                             // copy z11                     /* phase 5 */
        psubw_r2r(mm4, mm5);                            // z12 = tmp4 - tmp7;

        psubw_r2r(mm1, mm6);                            // z11-z13
        psllw_i2r(2, mm5);                              //      shift z12

        movq_m2r(*(idata+12), mm4);     // load idata[DCTSIZE*6], even part
        movq_r2r(mm5, mm7);                             //      copy z12

        pmulhw_m2r(fix_108n184, mm5); //        MULT(z12, (FIX_1_08-FIX_1_84)) //- z5; /* 2*(c2-c6) */ even part
        paddw_r2r(mm1, mm3);                            // tmp7 = z11 + z13;

        //ok

    /* Even part */
        pmulhw_m2r(fix_184, mm7);               // MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) //+ z5; /* -2*(c2+c6) */
        psllw_i2r(2, mm6);

        movq_m2r(*(idata+4), mm1);              // load idata[DCTSIZE*2]

        paddw_r2r(mm5, mm0);                            //      tmp10

        paddw_r2r(mm7, mm2);                            // tmp12

        pmulhw_m2r(fix_141, mm6);               // tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */
        psubw_r2r(mm3, mm2);                            // tmp6 = tmp12 - tmp7

        movq_r2r(mm1, mm5);                             // copy tmp1
        paddw_r2r(mm4, mm1);                            // tmp13= tmp1 + tmp3;  /* phases 5-3 */

        psubw_r2r(mm4, mm5);                            // tmp1-tmp3
        psubw_r2r(mm2, mm6);                            // tmp5 = tmp11 - tmp6;

        movq_r2m(mm1, *(wsptr));                // save tmp13 in workspace
        psllw_i2r(2, mm5);      // shift tmp1-tmp3

        movq_m2r(*(idata), mm7);                // load idata[DCTSIZE*0]

        pmulhw_m2r(fix_141, mm5);               // MULTIPLY(tmp1 - tmp3, FIX_1_414213562)
        paddw_r2r(mm6, mm0);                            // tmp4 = tmp10 + tmp5;

        movq_m2r(*(idata+8), mm4);      // load idata[DCTSIZE*4]

        psubw_r2r(mm1, mm5);                            // tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13; /* 2*c4 */

        movq_r2m(mm0, *(wsptr+4));              // save tmp4 in workspace
        movq_r2r(mm7, mm1);                             // copy tmp0    /* phase 3 */

        movq_r2m(mm5, *(wsptr+2));              // save tmp12 in workspace
        psubw_r2r(mm4, mm1);                            // tmp11 = tmp0 - tmp2;

        paddw_r2r(mm4, mm7);                            // tmp10 = tmp0 + tmp2;
   movq_r2r(mm1, mm5);                          // copy tmp11

        paddw_m2r(*(wsptr+2), mm1);     // tmp1 = tmp11 + tmp12;
        movq_r2r(mm7, mm4);                             // copy tmp10           /* phase 2 */

        paddw_m2r(*(wsptr), mm7);               // tmp0 = tmp10 + tmp13;

        psubw_m2r(*(wsptr), mm4);               // tmp3 = tmp10 - tmp13;
        movq_r2r(mm7, mm0);                             //      copy tmp0

        psubw_m2r(*(wsptr+2), mm5);     // tmp2 = tmp11 - tmp12;
        paddw_r2r(mm3, mm7);                            //      wsptr[DCTSIZE*0] = (int) (tmp0 + tmp7);

        psubw_r2r(mm3, mm0);                            // wsptr[DCTSIZE*7] = (int) (tmp0 - tmp7);

        movq_r2m(mm7, *(wsptr));                //      wsptr[DCTSIZE*0]
        movq_r2r(mm1, mm3);                             //      copy tmp1

        movq_r2m(mm0, *(wsptr+14));             // wsptr[DCTSIZE*7]
        paddw_r2r(mm2, mm1);                            // wsptr[DCTSIZE*1] = (int) (tmp1 + tmp6);

        psubw_r2r(mm2, mm3);                            // wsptr[DCTSIZE*6] = (int) (tmp1 - tmp6);

        movq_r2m(mm1, *(wsptr+2));              // wsptr[DCTSIZE*1]
        movq_r2r(mm4, mm1);                             //      copy tmp3

        movq_r2m(mm3, *(wsptr+12));             // wsptr[DCTSIZE*6]

        paddw_m2r(*(wsptr+4), mm4);     // wsptr[DCTSIZE*4] = (int) (tmp3 + tmp4);

        psubw_m2r(*(wsptr+4), mm1);     // wsptr[DCTSIZE*3] = (int) (tmp3 - tmp4);

        movq_r2m(mm4, *(wsptr+8));
        movq_r2r(mm5, mm7);                             // copy tmp2

        paddw_r2r(mm6, mm5);                            // wsptr[DCTSIZE*2] = (int) (tmp2 + tmp5)

        movq_r2m(mm1, *(wsptr+6));
        psubw_r2r(mm6, mm7);                            //      wsptr[DCTSIZE*5] = (int) (tmp2 - tmp5);

        movq_r2m(mm5, *(wsptr+4));

        movq_r2m(mm7, *(wsptr+10));

        //ok


/*****************************************************************/

        idata++;
        wsptr++;

/*****************************************************************/

        movq_m2r(*(idata+10), mm1);     // load idata[DCTSIZE*5]

        movq_m2r(*(idata+6), mm0);              // load idata[DCTSIZE*3]

        movq_m2r(*(idata+2),    mm3);           // load idata[DCTSIZE*1]
        movq_r2r(mm1, mm2);                             //      copy tmp6       /* phase 6 */

        movq_m2r(*(idata+14),   mm4);           // load idata[DCTSIZE*7]
        paddw_r2r(mm0, mm1);                            //      z13 = tmp6 + tmp5;

        psubw_r2r(mm0, mm2);                            //      z10 = tmp6 - tmp5

        psllw_i2r(2, mm2);                              //      shift z10
        movq_r2r(mm2, mm0);                             //      copy z10

        pmulhw_m2r(fix_184n261, mm2);   // MULTIPLY( z12, FIX_1_847759065); /* 2*c2 */
        movq_r2r(mm3, mm5);                             //      copy tmp4

        pmulhw_m2r(fix_n184, mm0);              // MULTIPLY(z10, -FIX_1_847759065); /* 2*c2 */
        paddw_r2r(mm4, mm3);                            // z11 = tmp4 + tmp7;

        movq_r2r(mm3, mm6);                             // copy z11                     /* phase 5 */
        psubw_r2r(mm4, mm5);                            //      z12 = tmp4 - tmp7;

        psubw_r2r(mm1, mm6);                            // z11-z13
        psllw_i2r(2, mm5);                              //      shift z12

        movq_m2r(*(idata+12), mm4);     // load idata[DCTSIZE*6], even part
        movq_r2r(mm5, mm7);                             // copy z12

        pmulhw_m2r(fix_108n184, mm5);   // MULT(z12, (FIX_1_08-FIX_1_84)) //- z5; /* 2*(c2-c6) */ even part
        paddw_r2r(mm1, mm3);                            // tmp7 = z11 + z13;

        //ok

    /* Even part */
        pmulhw_m2r(fix_184, mm7);               // MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) //+ z5; /* -2*(c2+c6) */
        psllw_i2r(2, mm6);

        movq_m2r(*(idata+4), mm1);              // load idata[DCTSIZE*2]

        paddw_r2r(mm5, mm0);                            //      tmp10

        paddw_r2r(mm7, mm2);                            // tmp12

        pmulhw_m2r(fix_141, mm6);               // tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */
        psubw_r2r(mm3, mm2);                            // tmp6 = tmp12 - tmp7

        movq_r2r(mm1, mm5);                             // copy tmp1
        paddw_r2r(mm4, mm1);                            // tmp13= tmp1 + tmp3;  /* phases 5-3 */

        psubw_r2r(mm4, mm5);                            // tmp1-tmp3
        psubw_r2r(mm2, mm6);                            // tmp5 = tmp11 - tmp6;

        movq_r2m(mm1, *(wsptr));                // save tmp13 in workspace
        psllw_i2r(2, mm5);                              // shift tmp1-tmp3

        movq_m2r(*(idata), mm7);                // load idata[DCTSIZE*0]
        paddw_r2r(mm6, mm0);                            // tmp4 = tmp10 + tmp5;

        pmulhw_m2r(fix_141, mm5);               // MULTIPLY(tmp1 - tmp3, FIX_1_414213562)

        movq_m2r(*(idata+8), mm4);    // load idata[DCTSIZE*4]

        psubw_r2r(mm1, mm5);                            // tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13; /* 2*c4 */

        movq_r2m(mm0, *(wsptr+4));              // save tmp4 in workspace
        movq_r2r(mm7, mm1);                             // copy tmp0    /* phase 3 */

        movq_r2m(mm5, *(wsptr+2));              // save tmp12 in workspace
        psubw_r2r(mm4, mm1);                            // tmp11 = tmp0 - tmp2;

        paddw_r2r(mm4, mm7);                            // tmp10 = tmp0 + tmp2;
   movq_r2r(mm1, mm5);                          // copy tmp11

        paddw_m2r(*(wsptr+2), mm1);     // tmp1 = tmp11 + tmp12;
        movq_r2r(mm7, mm4);                             // copy tmp10           /* phase 2 */

        paddw_m2r(*(wsptr), mm7);               // tmp0 = tmp10 + tmp13;

        psubw_m2r(*(wsptr), mm4);               // tmp3 = tmp10 - tmp13;
        movq_r2r(mm7, mm0);                             // copy tmp0

        psubw_m2r(*(wsptr+2), mm5);     // tmp2 = tmp11 - tmp12;
        paddw_r2r(mm3, mm7);                            // wsptr[DCTSIZE*0] = (int) (tmp0 + tmp7);

        psubw_r2r(mm3, mm0);                            // wsptr[DCTSIZE*7] = (int) (tmp0 - tmp7);

        movq_r2m(mm7, *(wsptr));                // wsptr[DCTSIZE*0]
        movq_r2r(mm1, mm3);                             // copy tmp1

        movq_r2m(mm0, *(wsptr+14));             // wsptr[DCTSIZE*7]
        paddw_r2r(mm2, mm1);                            // wsptr[DCTSIZE*1] = (int) (tmp1 + tmp6);

        psubw_r2r(mm2, mm3);                            // wsptr[DCTSIZE*6] = (int) (tmp1 - tmp6);

        movq_r2m(mm1, *(wsptr+2));              // wsptr[DCTSIZE*1]
        movq_r2r(mm4, mm1);                             // copy tmp3

        movq_r2m(mm3, *(wsptr+12));             // wsptr[DCTSIZE*6]

        paddw_m2r(*(wsptr+4), mm4);     // wsptr[DCTSIZE*4] = (int) (tmp3 + tmp4);

        psubw_m2r(*(wsptr+4), mm1);     // wsptr[DCTSIZE*3] = (int) (tmp3 - tmp4);

        movq_r2m(mm4, *(wsptr+8));
        movq_r2r(mm5, mm7);                             // copy tmp2

        paddw_r2r(mm6, mm5);                            // wsptr[DCTSIZE*2] = (int) (tmp2 + tmp5)

        movq_r2m(mm1, *(wsptr+6));
        psubw_r2r(mm6, mm7);                            // wsptr[DCTSIZE*5] = (int) (tmp2 - tmp5);

        movq_r2m(mm5, *(wsptr+4));

        movq_r2m(mm7, *(wsptr+10));

/*****************************************************************/

  /* Pass 2: process rows from work array, store into output array. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

/*****************************************************************/
    /* Even part */

        wsptr--;

//    tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]);
//    tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]);
//    tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]);
//    tmp14 = ((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6]);
        movq_m2r(*(wsptr), mm0);                // wsptr[0,0],[0,1],[0,2],[0,3]

        movq_m2r(*(wsptr+1),    mm1);           // wsptr[0,4],[0,5],[0,6],[0,7]
        movq_r2r(mm0, mm2);

        movq_m2r(*(wsptr+2), mm3);              // wsptr[1,0],[1,1],[1,2],[1,3]
        paddw_r2r(mm1, mm0);                            // wsptr[0,tmp10],[xxx],[0,tmp13],[xxx]

        movq_m2r(*(wsptr+3), mm4);              // wsptr[1,4],[1,5],[1,6],[1,7]
        psubw_r2r(mm1, mm2);                            // wsptr[0,tmp11],[xxx],[0,tmp14],[xxx]

        movq_r2r(mm0, mm6);
        movq_r2r(mm3, mm5);

        paddw_r2r(mm4, mm3);                            // wsptr[1,tmp10],[xxx],[1,tmp13],[xxx]
        movq_r2r(mm2, mm1);

        psubw_r2r(mm4, mm5);                            // wsptr[1,tmp11],[xxx],[1,tmp14],[xxx]
        punpcklwd_r2r(mm3, mm0);                // wsptr[0,tmp10],[1,tmp10],[xxx],[xxx]

        movq_m2r(*(wsptr+7), mm7);              // wsptr[3,4],[3,5],[3,6],[3,7]
        punpckhwd_r2r(mm3, mm6);                // wsptr[0,tmp13],[1,tmp13],[xxx],[xxx]

        movq_m2r(*(wsptr+4), mm3);              // wsptr[2,0],[2,1],[2,2],[2,3]
        punpckldq_r2r(mm6, mm0);                // wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]

        punpcklwd_r2r(mm5, mm1);                // wsptr[0,tmp11],[1,tmp11],[xxx],[xxx]
        movq_r2r(mm3, mm4);

        movq_m2r(*(wsptr+6), mm6);              // wsptr[3,0],[3,1],[3,2],[3,3]
        punpckhwd_r2r(mm5, mm2);                // wsptr[0,tmp14],[1,tmp14],[xxx],[xxx]

        movq_m2r(*(wsptr+5), mm5);              // wsptr[2,4],[2,5],[2,6],[2,7]
        punpckldq_r2r(mm2, mm1);                // wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]


        paddw_r2r(mm5, mm3);                            // wsptr[2,tmp10],[xxx],[2,tmp13],[xxx]
        movq_r2r(mm6, mm2);

        psubw_r2r(mm5, mm4);                            // wsptr[2,tmp11],[xxx],[2,tmp14],[xxx]
        paddw_r2r(mm7, mm6);                            // wsptr[3,tmp10],[xxx],[3,tmp13],[xxx]

        movq_r2r(mm3, mm5);
        punpcklwd_r2r(mm6, mm3);                // wsptr[2,tmp10],[3,tmp10],[xxx],[xxx]

        psubw_r2r(mm7, mm2);                            // wsptr[3,tmp11],[xxx],[3,tmp14],[xxx]
        punpckhwd_r2r(mm6, mm5);                // wsptr[2,tmp13],[3,tmp13],[xxx],[xxx]

        movq_r2r(mm4, mm7);
        punpckldq_r2r(mm5, mm3);                // wsptr[2,tmp10],[3,tmp10],[2,tmp13],[3,tmp13]

        punpcklwd_r2r(mm2, mm4);                // wsptr[2,tmp11],[3,tmp11],[xxx],[xxx]

        punpckhwd_r2r(mm2, mm7);                // wsptr[2,tmp14],[3,tmp14],[xxx],[xxx]

        punpckldq_r2r(mm7, mm4);                // wsptr[2,tmp11],[3,tmp11],[2,tmp14],[3,tmp14]
        movq_r2r(mm1, mm6);

        //ok

//      mm0 =   ;wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]
//      mm1 =   ;wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]


        movq_r2r(mm0, mm2);
        punpckhdq_r2r(mm4, mm6);                // wsptr[0,tmp14],[1,tmp14],[2,tmp14],[3,tmp14]

        punpckldq_r2r(mm4, mm1);                // wsptr[0,tmp11],[1,tmp11],[2,tmp11],[3,tmp11]
        psllw_i2r(2, mm6);

        pmulhw_m2r(fix_141, mm6);
        punpckldq_r2r(mm3, mm0);                // wsptr[0,tmp10],[1,tmp10],[2,tmp10],[3,tmp10]

        punpckhdq_r2r(mm3, mm2);                // wsptr[0,tmp13],[1,tmp13],[2,tmp13],[3,tmp13]
        movq_r2r(mm0, mm7);

//    tmp0 = tmp10 + tmp13;
//    tmp3 = tmp10 - tmp13;
        paddw_r2r(mm2, mm0);                            // [0,tmp0],[1,tmp0],[2,tmp0],[3,tmp0]
        psubw_r2r(mm2, mm7);                            // [0,tmp3],[1,tmp3],[2,tmp3],[3,tmp3]

//    tmp12 = MULTIPLY(tmp14, FIX_1_414213562) - tmp13;
        psubw_r2r(mm2, mm6);                            // wsptr[0,tmp12],[1,tmp12],[2,tmp12],[3,tmp12]
//    tmp1 = tmp11 + tmp12;
//    tmp2 = tmp11 - tmp12;
        movq_r2r(mm1, mm5);

        //OK

    /* Odd part */

//    z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3];
//    z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3];
//    z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7];
//    z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7];
        movq_m2r(*(wsptr), mm3);                // wsptr[0,0],[0,1],[0,2],[0,3]
        paddw_r2r(mm6, mm1);                            // [0,tmp1],[1,tmp1],[2,tmp1],[3,tmp1]

        movq_m2r(*(wsptr+1), mm4);              // wsptr[0,4],[0,5],[0,6],[0,7]
        psubw_r2r(mm6, mm5);                            // [0,tmp2],[1,tmp2],[2,tmp2],[3,tmp2]

        movq_r2r(mm3, mm6);
        punpckldq_r2r(mm4, mm3);                // wsptr[0,0],[0,1],[0,4],[0,5]

        punpckhdq_r2r(mm6, mm4);                // wsptr[0,6],[0,7],[0,2],[0,3]
        movq_r2r(mm3, mm2);

//Save tmp0 and tmp1 in wsptr
        movq_r2m(mm0, *(wsptr));                // save tmp0
        paddw_r2r(mm4, mm2);                            // wsptr[xxx],[0,z11],[xxx],[0,z13]


//Continue with z10 --- z13
        movq_m2r(*(wsptr+2), mm6);              // wsptr[1,0],[1,1],[1,2],[1,3]
        psubw_r2r(mm4, mm3);                            // wsptr[xxx],[0,z12],[xxx],[0,z10]

        movq_m2r(*(wsptr+3), mm0);              // wsptr[1,4],[1,5],[1,6],[1,7]
        movq_r2r(mm6, mm4);

        movq_r2m(mm1, *(wsptr+1));              // save tmp1
        punpckldq_r2r(mm0, mm6);                // wsptr[1,0],[1,1],[1,4],[1,5]

        punpckhdq_r2r(mm4, mm0);                // wsptr[1,6],[1,7],[1,2],[1,3]
        movq_r2r(mm6, mm1);

//Save tmp2 and tmp3 in wsptr
        paddw_r2r(mm0, mm6);                            // wsptr[xxx],[1,z11],[xxx],[1,z13]
        movq_r2r(mm2, mm4);

//Continue with z10 --- z13
        movq_r2m(mm5, *(wsptr+2));              // save tmp2
        punpcklwd_r2r(mm6, mm2);                // wsptr[xxx],[xxx],[0,z11],[1,z11]

        psubw_r2r(mm0, mm1);                            // wsptr[xxx],[1,z12],[xxx],[1,z10]
        punpckhwd_r2r(mm6, mm4);                // wsptr[xxx],[xxx],[0,z13],[1,z13]

        movq_r2r(mm3, mm0);
        punpcklwd_r2r(mm1, mm3);                // wsptr[xxx],[xxx],[0,z12],[1,z12]

        movq_r2m(mm7, *(wsptr+3));              // save tmp3
        punpckhwd_r2r(mm1, mm0);                // wsptr[xxx],[xxx],[0,z10],[1,z10]

        movq_m2r(*(wsptr+4), mm6);              // wsptr[2,0],[2,1],[2,2],[2,3]
        punpckhdq_r2r(mm2, mm0);                // wsptr[0,z10],[1,z10],[0,z11],[1,z11]

        movq_m2r(*(wsptr+5), mm7);      // wsptr[2,4],[2,5],[2,6],[2,7]
        punpckhdq_r2r(mm4, mm3);                // wsptr[0,z12],[1,z12],[0,z13],[1,z13]

        movq_m2r(*(wsptr+6), mm1);      // wsptr[3,0],[3,1],[3,2],[3,3]
        movq_r2r(mm6, mm4);

        punpckldq_r2r(mm7, mm6);                // wsptr[2,0],[2,1],[2,4],[2,5]
        movq_r2r(mm1, mm5);

        punpckhdq_r2r(mm4, mm7);                // wsptr[2,6],[2,7],[2,2],[2,3]
        movq_r2r(mm6, mm2);

        movq_m2r(*(wsptr+7), mm4);      // wsptr[3,4],[3,5],[3,6],[3,7]
        paddw_r2r(mm7, mm6);                            // wsptr[xxx],[2,z11],[xxx],[2,z13]

        psubw_r2r(mm7, mm2);                            // wsptr[xxx],[2,z12],[xxx],[2,z10]
        punpckldq_r2r(mm4, mm1);                // wsptr[3,0],[3,1],[3,4],[3,5]

        punpckhdq_r2r(mm5, mm4);                // wsptr[3,6],[3,7],[3,2],[3,3]
        movq_r2r(mm1, mm7);

        paddw_r2r(mm4, mm1);                            // wsptr[xxx],[3,z11],[xxx],[3,z13]
        psubw_r2r(mm4, mm7);                            // wsptr[xxx],[3,z12],[xxx],[3,z10]

        movq_r2r(mm6, mm5);
        punpcklwd_r2r(mm1, mm6);                // wsptr[xxx],[xxx],[2,z11],[3,z11]

        punpckhwd_r2r(mm1, mm5);                // wsptr[xxx],[xxx],[2,z13],[3,z13]
        movq_r2r(mm2, mm4);

        punpcklwd_r2r(mm7, mm2);                // wsptr[xxx],[xxx],[2,z12],[3,z12]

        punpckhwd_r2r(mm7, mm4);                // wsptr[xxx],[xxx],[2,z10],[3,z10]

        punpckhdq_r2r(mm6, mm4);                /// wsptr[2,z10],[3,z10],[2,z11],[3,z11]

        punpckhdq_r2r(mm5, mm2);                // wsptr[2,z12],[3,z12],[2,z13],[3,z13]
        movq_r2r(mm0, mm5);

        punpckldq_r2r(mm4, mm0);                // wsptr[0,z10],[1,z10],[2,z10],[3,z10]

        punpckhdq_r2r(mm4, mm5);                // wsptr[0,z11],[1,z11],[2,z11],[3,z11]
        movq_r2r(mm3, mm4);

        punpckhdq_r2r(mm2, mm4);                // wsptr[0,z13],[1,z13],[2,z13],[3,z13]
        movq_r2r(mm5, mm1);

        punpckldq_r2r(mm2, mm3);                // wsptr[0,z12],[1,z12],[2,z12],[3,z12]
//    tmp7 = z11 + z13;         /* phase 5 */
//    tmp8 = z11 - z13;         /* phase 5 */
        psubw_r2r(mm4, mm1);                            // tmp8

        paddw_r2r(mm4, mm5);                            // tmp7
//    tmp21 = MULTIPLY(tmp8, FIX_1_414213562); /* 2*c4 */
        psllw_i2r(2, mm1);

        psllw_i2r(2, mm0);

        pmulhw_m2r(fix_141, mm1);               // tmp21
//    tmp20 = MULTIPLY(z12, (FIX_1_082392200- FIX_1_847759065))  /* 2*(c2-c6) */
//                      + MULTIPLY(z10, - FIX_1_847759065); /* 2*c2 */
        psllw_i2r(2, mm3);
        movq_r2r(mm0, mm7);

        pmulhw_m2r(fix_n184, mm7);
        movq_r2r(mm3, mm6);

        movq_m2r(*(wsptr), mm2);                // tmp0,final1

        pmulhw_m2r(fix_108n184, mm6);
//       tmp22 = MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) /* -2*(c2+c6) */
//                      + MULTIPLY(z12, FIX_1_847759065); /* 2*c2 */
        movq_r2r(mm2, mm4);                             // final1

        pmulhw_m2r(fix_184n261, mm0);
        paddw_r2r(mm5, mm2);                            // tmp0+tmp7,final1

        pmulhw_m2r(fix_184, mm3);
        psubw_r2r(mm5, mm4);                            // tmp0-tmp7,final1

//    tmp6 = tmp22 - tmp7;      /* phase 2 */
        psraw_i2r(3, mm2);                              // outptr[0,0],[1,0],[2,0],[3,0],final1

        paddw_r2r(mm6, mm7);                            // tmp20
        psraw_i2r(3, mm4);                              // outptr[0,7],[1,7],[2,7],[3,7],final1

        paddw_r2r(mm0, mm3);                            // tmp22

//    tmp5 = tmp21 - tmp6;
        psubw_r2r(mm5, mm3);                            // tmp6

//    tmp4 = tmp20 + tmp5;
        movq_m2r(*(wsptr+1), mm0);              // tmp1,final2
        psubw_r2r(mm3, mm1);                            // tmp5

        movq_r2r(mm0, mm6);                             // final2
        paddw_r2r(mm3, mm0);                            // tmp1+tmp6,final2

    /* Final output stage: scale down by a factor of 8 and range-limit */


//    outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3)
//                          & RANGE_MASK];      final1


//    outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3)
//                          & RANGE_MASK];      final2
        psubw_r2r(mm3, mm6);                            // tmp1-tmp6,final2
        psraw_i2r(3, mm0);                              // outptr[0,1],[1,1],[2,1],[3,1]

        psraw_i2r(3, mm6);                              // outptr[0,6],[1,6],[2,6],[3,6]

        packuswb_r2r(mm4, mm0);                 // out[0,1],[1,1],[2,1],[3,1],[0,7],[1,7],[2,7],[3,7]

        movq_m2r(*(wsptr+2), mm5);              // tmp2,final3
        packuswb_r2r(mm6, mm2);                 // out[0,0],[1,0],[2,0],[3,0],[0,6],[1,6],[2,6],[3,6]

//    outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3)
//                          & RANGE_MASK];      final3
        paddw_r2r(mm1, mm7);                            // tmp4
        movq_r2r(mm5, mm3);

        paddw_r2r(mm1, mm5);                            // tmp2+tmp5
        psubw_r2r(mm1, mm3);                            // tmp2-tmp5

        psraw_i2r(3, mm5);                              // outptr[0,2],[1,2],[2,2],[3,2]

        movq_m2r(*(wsptr+3), mm4);              // tmp3,final4
        psraw_i2r(3, mm3);                              // outptr[0,5],[1,5],[2,5],[3,5]



//    outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3)
//                          & RANGE_MASK];      final4
        movq_r2r(mm4, mm6);
        paddw_r2r(mm7, mm4);                            // tmp3+tmp4

        psubw_r2r(mm7, mm6);                            // tmp3-tmp4
        psraw_i2r(3, mm4);                              // outptr[0,4],[1,4],[2,4],[3,4]

        // mov                  ecx, [dataptr]

        psraw_i2r(3, mm6);                              // outptr[0,3],[1,3],[2,3],[3,3]

        packuswb_r2r(mm4, mm5);                 // out[0,2],[1,2],[2,2],[3,2],[0,4],[1,4],[2,4],[3,4]

        packuswb_r2r(mm3, mm6);                 // out[0,3],[1,3],[2,3],[3,3],[0,5],[1,5],[2,5],[3,5]
        movq_r2r(mm2, mm4);

        movq_r2r(mm5, mm7);
        punpcklbw_r2r(mm0, mm2);                // out[0,0],[0,1],[1,0],[1,1],[2,0],[2,1],[3,0],[3,1]

        punpckhbw_r2r(mm0, mm4);                // out[0,6],[0,7],[1,6],[1,7],[2,6],[2,7],[3,6],[3,7]
        movq_r2r(mm2, mm1);

        punpcklbw_r2r(mm6, mm5);                // out[0,2],[0,3],[1,2],[1,3],[2,2],[2,3],[3,2],[3,3]

        // add                  dataptr, 4

        punpckhbw_r2r(mm6, mm7);                // out[0,4],[0,5],[1,4],[1,5],[2,4],[2,5],[3,4],[3,5]

        punpcklwd_r2r(mm5, mm2);                // out[0,0],[0,1],[0,2],[0,3],[1,0],[1,1],[1,2],[1,3]

        // add                  ecx, output_col

        movq_r2r(mm7, mm6);
        punpckhwd_r2r(mm5, mm1);                // out[2,0],[2,1],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3]

        movq_r2r(mm2, mm0);
        punpcklwd_r2r(mm4, mm6);                // out[0,4],[0,5],[0,6],[0,7],[1,4],[1,5],[1,6],[1,7]

        // mov                  idata, [dataptr]

        punpckldq_r2r(mm6, mm2);                // out[0,0],[0,1],[0,2],[0,3],[0,4],[0,5],[0,6],[0,7]

        // add                  dataptr, 4

        movq_r2r(mm1, mm3);

        // add                  idata, output_col

        punpckhwd_r2r(mm4, mm7);                // out[2,4],[2,5],[2,6],[2,7],[3,4],[3,5],[3,6],[3,7]

        movq_r2m(mm2, *(dataptr));

        punpckhdq_r2r(mm6, mm0);                // out[1,0],[1,1],[1,2],[1,3],[1,4],[1,5],[1,6],[1,7]

        dataptr += rskip;
        movq_r2m(mm0, *(dataptr));

        punpckldq_r2r(mm7, mm1);                // out[2,0],[2,1],[2,2],[2,3],[2,4],[2,5],[2,6],[2,7]
        punpckhdq_r2r(mm7, mm3);                // out[3,0],[3,1],[3,2],[3,3],[3,4],[3,5],[3,6],[3,7]

        dataptr += rskip;
        movq_r2m(mm1, *(dataptr));

        dataptr += rskip;
        movq_r2m(mm3, *(dataptr));

/*******************************************************************/

        wsptr += 8;

/*******************************************************************/

//    tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]);
//    tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]);
//    tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]);
//    tmp14 = ((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6]);
        movq_m2r(*(wsptr), mm0);                // wsptr[0,0],[0,1],[0,2],[0,3]

        movq_m2r(*(wsptr+1), mm1);              // wsptr[0,4],[0,5],[0,6],[0,7]
        movq_r2r(mm0, mm2);

        movq_m2r(*(wsptr+2), mm3);              // wsptr[1,0],[1,1],[1,2],[1,3]
        paddw_r2r(mm1, mm0);                            // wsptr[0,tmp10],[xxx],[0,tmp13],[xxx]

        movq_m2r(*(wsptr+3), mm4);              // wsptr[1,4],[1,5],[1,6],[1,7]
        psubw_r2r(mm1, mm2);                            // wsptr[0,tmp11],[xxx],[0,tmp14],[xxx]

        movq_r2r(mm0, mm6);
        movq_r2r(mm3, mm5);

        paddw_r2r(mm4, mm3);                            // wsptr[1,tmp10],[xxx],[1,tmp13],[xxx]
        movq_r2r(mm2, mm1);

        psubw_r2r(mm4, mm5);                            // wsptr[1,tmp11],[xxx],[1,tmp14],[xxx]
        punpcklwd_r2r(mm3, mm0);                // wsptr[0,tmp10],[1,tmp10],[xxx],[xxx]

        movq_m2r(*(wsptr+7), mm7);      // wsptr[3,4],[3,5],[3,6],[3,7]
        punpckhwd_r2r(mm3, mm6);                // wsptr[0,tmp13],[1,tmp13],[xxx],[xxx]

        movq_m2r(*(wsptr+4),    mm3);           // wsptr[2,0],[2,1],[2,2],[2,3]
        punpckldq_r2r(mm6, mm0);                // wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]

        punpcklwd_r2r(mm5, mm1);                // wsptr[0,tmp11],[1,tmp11],[xxx],[xxx]
        movq_r2r(mm3, mm4);

        movq_m2r(*(wsptr+6), mm6);      // wsptr[3,0],[3,1],[3,2],[3,3]
        punpckhwd_r2r(mm5, mm2);                // wsptr[0,tmp14],[1,tmp14],[xxx],[xxx]

        movq_m2r(*(wsptr+5), mm5);      // wsptr[2,4],[2,5],[2,6],[2,7]
        punpckldq_r2r(mm2, mm1);                // wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]

        paddw_r2r(mm5, mm3);                            // wsptr[2,tmp10],[xxx],[2,tmp13],[xxx]
        movq_r2r(mm6, mm2);

        psubw_r2r(mm5, mm4);                            // wsptr[2,tmp11],[xxx],[2,tmp14],[xxx]
        paddw_r2r(mm7, mm6);                            // wsptr[3,tmp10],[xxx],[3,tmp13],[xxx]

        movq_r2r(mm3, mm5);
        punpcklwd_r2r(mm6, mm3);                // wsptr[2,tmp10],[3,tmp10],[xxx],[xxx]

        psubw_r2r(mm7, mm2);                            // wsptr[3,tmp11],[xxx],[3,tmp14],[xxx]
        punpckhwd_r2r(mm6, mm5);                // wsptr[2,tmp13],[3,tmp13],[xxx],[xxx]

        movq_r2r(mm4, mm7);
        punpckldq_r2r(mm5, mm3);                // wsptr[2,tmp10],[3,tmp10],[2,tmp13],[3,tmp13]

        punpcklwd_r2r(mm2, mm4);                // wsptr[2,tmp11],[3,tmp11],[xxx],[xxx]

        punpckhwd_r2r(mm2, mm7);                // wsptr[2,tmp14],[3,tmp14],[xxx],[xxx]

        punpckldq_r2r(mm7, mm4);                // wsptr[2,tmp11],[3,tmp11],[2,tmp14],[3,tmp14]
        movq_r2r(mm1, mm6);

        //OK

//      mm0 =   ;wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]
//      mm1 =   ;wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]

        movq_r2r(mm0, mm2);
        punpckhdq_r2r(mm4, mm6);                // wsptr[0,tmp14],[1,tmp14],[2,tmp14],[3,tmp14]

        punpckldq_r2r(mm4, mm1);                // wsptr[0,tmp11],[1,tmp11],[2,tmp11],[3,tmp11]
        psllw_i2r(2, mm6);

        pmulhw_m2r(fix_141, mm6);
        punpckldq_r2r(mm3, mm0);                // wsptr[0,tmp10],[1,tmp10],[2,tmp10],[3,tmp10]

        punpckhdq_r2r(mm3, mm2);                // wsptr[0,tmp13],[1,tmp13],[2,tmp13],[3,tmp13]
        movq_r2r(mm0, mm7);

//    tmp0 = tmp10 + tmp13;
//    tmp3 = tmp10 - tmp13;
        paddw_r2r(mm2, mm0);                            // [0,tmp0],[1,tmp0],[2,tmp0],[3,tmp0]
        psubw_r2r(mm2, mm7);                            // [0,tmp3],[1,tmp3],[2,tmp3],[3,tmp3]

//    tmp12 = MULTIPLY(tmp14, FIX_1_414213562) - tmp13;
        psubw_r2r(mm2, mm6);                            // wsptr[0,tmp12],[1,tmp12],[2,tmp12],[3,tmp12]
//    tmp1 = tmp11 + tmp12;
//    tmp2 = tmp11 - tmp12;
        movq_r2r(mm1, mm5);

         //OK


    /* Odd part */

//    z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3];
//    z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3];
//    z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7];
//    z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7];
        movq_m2r(*(wsptr), mm3);                // wsptr[0,0],[0,1],[0,2],[0,3]
        paddw_r2r(mm6, mm1);                            // [0,tmp1],[1,tmp1],[2,tmp1],[3,tmp1]

        movq_m2r(*(wsptr+1),    mm4);           // wsptr[0,4],[0,5],[0,6],[0,7]
        psubw_r2r(mm6, mm5);                            // [0,tmp2],[1,tmp2],[2,tmp2],[3,tmp2]

        movq_r2r(mm3, mm6);
        punpckldq_r2r(mm4, mm3);                // wsptr[0,0],[0,1],[0,4],[0,5]

        punpckhdq_r2r(mm6, mm4);                // wsptr[0,6],[0,7],[0,2],[0,3]
        movq_r2r(mm3, mm2);

//Save tmp0 and tmp1 in wsptr
        movq_r2m(mm0, *(wsptr));                // save tmp0
        paddw_r2r(mm4, mm2);                            // wsptr[xxx],[0,z11],[xxx],[0,z13]


//Continue with z10 --- z13
        movq_m2r(*(wsptr+2), mm6);              // wsptr[1,0],[1,1],[1,2],[1,3]
        psubw_r2r(mm4, mm3);                            // wsptr[xxx],[0,z12],[xxx],[0,z10]

        movq_m2r(*(wsptr+3), mm0);              // wsptr[1,4],[1,5],[1,6],[1,7]
        movq_r2r(mm6, mm4);

        movq_r2m(mm1, *(wsptr+1));              // save tmp1
        punpckldq_r2r(mm0, mm6);                // wsptr[1,0],[1,1],[1,4],[1,5]

        punpckhdq_r2r(mm4, mm0);                // wsptr[1,6],[1,7],[1,2],[1,3]
        movq_r2r(mm6, mm1);

//Save tmp2 and tmp3 in wsptr
        paddw_r2r(mm0, mm6);                            // wsptr[xxx],[1,z11],[xxx],[1,z13]
        movq_r2r(mm2, mm4);

//Continue with z10 --- z13
        movq_r2m(mm5, *(wsptr+2));              // save tmp2
        punpcklwd_r2r(mm6, mm2);                // wsptr[xxx],[xxx],[0,z11],[1,z11]

        psubw_r2r(mm0, mm1);                            // wsptr[xxx],[1,z12],[xxx],[1,z10]
        punpckhwd_r2r(mm6, mm4);                // wsptr[xxx],[xxx],[0,z13],[1,z13]

        movq_r2r(mm3, mm0);
        punpcklwd_r2r(mm1, mm3);                // wsptr[xxx],[xxx],[0,z12],[1,z12]

        movq_r2m(mm7, *(wsptr+3));              // save tmp3
        punpckhwd_r2r(mm1, mm0);                // wsptr[xxx],[xxx],[0,z10],[1,z10]

        movq_m2r(*(wsptr+4), mm6);              // wsptr[2,0],[2,1],[2,2],[2,3]
        punpckhdq_r2r(mm2, mm0);                // wsptr[0,z10],[1,z10],[0,z11],[1,z11]

        movq_m2r(*(wsptr+5), mm7);      // wsptr[2,4],[2,5],[2,6],[2,7]
        punpckhdq_r2r(mm4, mm3);                // wsptr[0,z12],[1,z12],[0,z13],[1,z13]

        movq_m2r(*(wsptr+6), mm1);      // wsptr[3,0],[3,1],[3,2],[3,3]
        movq_r2r(mm6, mm4);

        punpckldq_r2r(mm7, mm6);                // wsptr[2,0],[2,1],[2,4],[2,5]
        movq_r2r(mm1, mm5);

        punpckhdq_r2r(mm4, mm7);                // wsptr[2,6],[2,7],[2,2],[2,3]
        movq_r2r(mm6, mm2);

        movq_m2r(*(wsptr+7), mm4);      // wsptr[3,4],[3,5],[3,6],[3,7]
        paddw_r2r(mm7, mm6);                            // wsptr[xxx],[2,z11],[xxx],[2,z13]

        psubw_r2r(mm7, mm2);                            // wsptr[xxx],[2,z12],[xxx],[2,z10]
        punpckldq_r2r(mm4, mm1);                // wsptr[3,0],[3,1],[3,4],[3,5]

        punpckhdq_r2r(mm5, mm4);                // wsptr[3,6],[3,7],[3,2],[3,3]
        movq_r2r(mm1, mm7);

        paddw_r2r(mm4, mm1);                            // wsptr[xxx],[3,z11],[xxx],[3,z13]
        psubw_r2r(mm4, mm7);                            // wsptr[xxx],[3,z12],[xxx],[3,z10]

        movq_r2r(mm6, mm5);
        punpcklwd_r2r(mm1, mm6);                // wsptr[xxx],[xxx],[2,z11],[3,z11]

        punpckhwd_r2r(mm1, mm5);                // wsptr[xxx],[xxx],[2,z13],[3,z13]
        movq_r2r(mm2, mm4);

        punpcklwd_r2r(mm7, mm2);                // wsptr[xxx],[xxx],[2,z12],[3,z12]

        punpckhwd_r2r(mm7, mm4);                // wsptr[xxx],[xxx],[2,z10],[3,z10]

        punpckhdq_r2r(mm6, mm4);                // wsptr[2,z10],[3,z10],[2,z11],[3,z11]

        punpckhdq_r2r(mm5, mm2);                // wsptr[2,z12],[3,z12],[2,z13],[3,z13]
        movq_r2r(mm0, mm5);

        punpckldq_r2r(mm4, mm0);                // wsptr[0,z10],[1,z10],[2,z10],[3,z10]

        punpckhdq_r2r(mm4, mm5);                // wsptr[0,z11],[1,z11],[2,z11],[3,z11]
        movq_r2r(mm3, mm4);

        punpckhdq_r2r(mm2, mm4);                // wsptr[0,z13],[1,z13],[2,z13],[3,z13]
        movq_r2r(mm5, mm1);

        punpckldq_r2r(mm2, mm3);                // wsptr[0,z12],[1,z12],[2,z12],[3,z12]
//    tmp7 = z11 + z13;         /* phase 5 */
//    tmp8 = z11 - z13;         /* phase 5 */
        psubw_r2r(mm4, mm1);                            // tmp8

        paddw_r2r(mm4, mm5);                            // tmp7
//    tmp21 = MULTIPLY(tmp8, FIX_1_414213562); /* 2*c4 */
        psllw_i2r(2, mm1);

        psllw_i2r(2, mm0);

        pmulhw_m2r(fix_141, mm1);               // tmp21
//    tmp20 = MULTIPLY(z12, (FIX_1_082392200- FIX_1_847759065))  /* 2*(c2-c6) */
//                      + MULTIPLY(z10, - FIX_1_847759065); /* 2*c2 */
        psllw_i2r(2, mm3);
        movq_r2r(mm0, mm7);

        pmulhw_m2r(fix_n184, mm7);
        movq_r2r(mm3, mm6);

        movq_m2r(*(wsptr), mm2);                // tmp0,final1

        pmulhw_m2r(fix_108n184, mm6);
//       tmp22 = MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) /* -2*(c2+c6) */
//                      + MULTIPLY(z12, FIX_1_847759065); /* 2*c2 */
        movq_r2r(mm2, mm4);                             // final1

        pmulhw_m2r(fix_184n261, mm0);
        paddw_r2r(mm5, mm2);                            // tmp0+tmp7,final1

        pmulhw_m2r(fix_184, mm3);
        psubw_r2r(mm5, mm4);                            // tmp0-tmp7,final1

//    tmp6 = tmp22 - tmp7;      /* phase 2 */
        psraw_i2r(3, mm2);                              // outptr[0,0],[1,0],[2,0],[3,0],final1

        paddw_r2r(mm6, mm7);                            // tmp20
        psraw_i2r(3, mm4);                              // outptr[0,7],[1,7],[2,7],[3,7],final1

        paddw_r2r(mm0, mm3);                            // tmp22

//    tmp5 = tmp21 - tmp6;
        psubw_r2r(mm5, mm3);                            // tmp6

//    tmp4 = tmp20 + tmp5;
        movq_m2r(*(wsptr+1), mm0);              // tmp1,final2
        psubw_r2r(mm3, mm1);                            // tmp5

        movq_r2r(mm0, mm6);                             // final2
        paddw_r2r(mm3, mm0);                            // tmp1+tmp6,final2

    /* Final output stage: scale down by a factor of 8 and range-limit */

//    outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3)
//                          & RANGE_MASK];      final1


//    outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3)
//                          & RANGE_MASK];      final2
        psubw_r2r(mm3, mm6);                            // tmp1-tmp6,final2
        psraw_i2r(3, mm0);                              // outptr[0,1],[1,1],[2,1],[3,1]

        psraw_i2r(3, mm6);                              // outptr[0,6],[1,6],[2,6],[3,6]

        packuswb_r2r(mm4, mm0);                 // out[0,1],[1,1],[2,1],[3,1],[0,7],[1,7],[2,7],[3,7]

        movq_m2r(*(wsptr+2), mm5);              // tmp2,final3
        packuswb_r2r(mm6, mm2);                 // out[0,0],[1,0],[2,0],[3,0],[0,6],[1,6],[2,6],[3,6]

//    outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3)
//                          & RANGE_MASK];      final3
        paddw_r2r(mm1, mm7);                            // tmp4
        movq_r2r(mm5, mm3);

        paddw_r2r(mm1, mm5);                            // tmp2+tmp5
        psubw_r2r(mm1, mm3);                            // tmp2-tmp5

        psraw_i2r(3, mm5);                              // outptr[0,2],[1,2],[2,2],[3,2]

        movq_m2r(*(wsptr+3), mm4);              // tmp3,final4
        psraw_i2r(3, mm3);                              // outptr[0,5],[1,5],[2,5],[3,5]



//    outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3)
//                          & RANGE_MASK];
//    outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3)
//                          & RANGE_MASK];      final4
        movq_r2r(mm4, mm6);
        paddw_r2r(mm7, mm4);                            // tmp3+tmp4

        psubw_r2r(mm7, mm6);                            // tmp3-tmp4
        psraw_i2r(3, mm4);                              // outptr[0,4],[1,4],[2,4],[3,4]

        psraw_i2r(3, mm6);                              // outptr[0,3],[1,3],[2,3],[3,3]

        /*
   movq_r2m(mm4, *dummy);
        fprintf(stderr, "3-4 %016llx\n", dummy);
   movq_r2m(mm4, *dummy);
        fprintf(stderr, "3+4 %016llx\n", dummy);
        */


        packuswb_r2r(mm4, mm5);                 // out[0,2],[1,2],[2,2],[3,2],[0,4],[1,4],[2,4],[3,4]

        packuswb_r2r(mm3, mm6);                 // out[0,3],[1,3],[2,3],[3,3],[0,5],[1,5],[2,5],[3,5]
        movq_r2r(mm2, mm4);

        movq_r2r(mm5, mm7);
        punpcklbw_r2r(mm0, mm2);                // out[0,0],[0,1],[1,0],[1,1],[2,0],[2,1],[3,0],[3,1]

        punpckhbw_r2r(mm0, mm4);                // out[0,6],[0,7],[1,6],[1,7],[2,6],[2,7],[3,6],[3,7]
        movq_r2r(mm2, mm1);

        punpcklbw_r2r(mm6, mm5);                // out[0,2],[0,3],[1,2],[1,3],[2,2],[2,3],[3,2],[3,3]

        punpckhbw_r2r(mm6, mm7);                // out[0,4],[0,5],[1,4],[1,5],[2,4],[2,5],[3,4],[3,5]

        punpcklwd_r2r(mm5, mm2);                // out[0,0],[0,1],[0,2],[0,3],[1,0],[1,1],[1,2],[1,3]

        movq_r2r(mm7, mm6);
        punpckhwd_r2r(mm5, mm1);                // out[2,0],[2,1],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3]

        movq_r2r(mm2, mm0);
        punpcklwd_r2r(mm4, mm6);                // out[0,4],[0,5],[0,6],[0,7],[1,4],[1,5],[1,6],[1,7]

        punpckldq_r2r(mm6, mm2);                // out[0,0],[0,1],[0,2],[0,3],[0,4],[0,5],[0,6],[0,7]

        movq_r2r(mm1, mm3);

        punpckhwd_r2r(mm4, mm7);                // out[2,4],[2,5],[2,6],[2,7],[3,4],[3,5],[3,6],[3,7]

        dataptr += rskip;
        movq_r2m(mm2, *(dataptr));

        punpckhdq_r2r(mm6, mm0);                // out[1,0],[1,1],[1,2],[1,3],[1,4],[1,5],[1,6],[1,7]

        dataptr += rskip;
        movq_r2m(mm0, *(dataptr));

        punpckldq_r2r(mm7, mm1);                // out[2,0],[2,1],[2,2],[2,3],[2,4],[2,5],[2,6],[2,7]

        punpckhdq_r2r(mm7, mm3);                // out[3,0],[3,1],[3,2],[3,3],[3,4],[3,5],[3,6],[3,7]

        dataptr += rskip;
        movq_r2m(mm1, *(dataptr));

        dataptr += rskip;
        movq_r2m(mm3, *(dataptr));

#else
  int32_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int32_t tmp10, tmp11, tmp12, tmp13;
  int32_t z5, z10, z11, z12, z13;
  int16_t *inptr;
  int32_t *wsptr;
  uint8_t *outptr;
  int ctr;
  int32_t dcval;

  inptr = data;
  wsptr = ws;
  for (ctr = 8; ctr > 0; ctr--) {

    if ((inptr[8] | inptr[16] | inptr[24] |
         inptr[32] | inptr[40] | inptr[48] | inptr[56]) == 0) {
      dcval = inptr[0];
      wsptr[0] = dcval;
      wsptr[8] = dcval;
      wsptr[16] = dcval;
      wsptr[24] = dcval;
      wsptr[32] = dcval;
      wsptr[40] = dcval;
      wsptr[48] = dcval;
      wsptr[56] = dcval;

      inptr++;
      wsptr++;
      continue;
    }

    tmp0 = inptr[0];
    tmp1 = inptr[16];
    tmp2 = inptr[32];
    tmp3 = inptr[48];

    tmp10 = tmp0 + tmp2;
    tmp11 = tmp0 - tmp2;

    tmp13 = tmp1 + tmp3;
    tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13;

    tmp0 = tmp10 + tmp13;
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;

    tmp4 = inptr[8];
    tmp5 = inptr[24];
    tmp6 = inptr[40];
    tmp7 = inptr[56];

    z13 = tmp6 + tmp5;
    z10 = tmp6 - tmp5;
    z11 = tmp4 + tmp7;
    z12 = tmp4 - tmp7;

    tmp7 = z11 + z13;
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562);

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065);
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5;
    tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5;

    tmp6 = tmp12 - tmp7;
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    wsptr[0] = (int32_t) (tmp0 + tmp7);
    wsptr[56] = (int32_t) (tmp0 - tmp7);
    wsptr[8] = (int32_t) (tmp1 + tmp6);
    wsptr[48] = (int32_t) (tmp1 - tmp6);
    wsptr[16] = (int32_t) (tmp2 + tmp5);
    wsptr[40] = (int32_t) (tmp2 - tmp5);
    wsptr[32] = (int32_t) (tmp3 + tmp4);
    wsptr[24] = (int32_t) (tmp3 - tmp4);

    inptr++;
    wsptr++;
  }

  wsptr = ws;
  for (ctr = 0; ctr < 8; ctr++) {
    outptr = &(odata[ctr*rskip]);

    tmp10 = wsptr[0] + wsptr[4];
    tmp11 = wsptr[0] - wsptr[4];

    tmp13 = wsptr[2] + wsptr[6];
    tmp12 = MULTIPLY(wsptr[2] - wsptr[6], FIX_1_414213562) - tmp13;

    tmp0 = tmp10 + tmp13;
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;

    z13 = wsptr[5] + wsptr[3];
    z10 = wsptr[5] - wsptr[3];
    z11 = wsptr[1] + wsptr[7];
    z12 = wsptr[1] - wsptr[7];

    tmp7 = z11 + z13;
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562);

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065);
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5;
    tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5;

    tmp6 = tmp12 - tmp7;
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    outptr[0] = RL(DESCALE(tmp0 + tmp7));
    outptr[7] = RL(DESCALE(tmp0 - tmp7));
    outptr[1] = RL(DESCALE(tmp1 + tmp6));
    outptr[6] = RL(DESCALE(tmp1 - tmp6));
    outptr[2] = RL(DESCALE(tmp2 + tmp5));
    outptr[5] = RL(DESCALE(tmp2 - tmp5));
    outptr[4] = RL(DESCALE(tmp3 + tmp4));
    outptr[3] = RL(DESCALE(tmp3 - tmp4));

    wsptr += 8;
  }
#endif
}

inline void RTjpeg::CalcTbls(void)
{
    int i;
    uint64_t qual;

    qual = (uint64_t)Q << (32 - 7); /* 32 bit FP, 255=2, 0=0 */

    for(i = 0; i < 64; i++)
    {
        lqt[i] = (int32_t)((qual/((uint64_t)RTjpeg_lum_quant_tbl[i]<<16))>>3);
        if (lqt[i] == 0)
            lqt[i]=1;

        cqt[i] = (int32_t)((qual/((uint64_t)RTjpeg_chrom_quant_tbl[i]<<16))>>3);
        if (cqt[i] == 0)
            cqt[i]=1;

        liqt[i] = (1<<16) / (lqt[i]<<3);
        ciqt[i] = (1<<16) / (cqt[i]<<3);
        lqt[i] = ((1<<16) / liqt[i])>>3;
        cqt[i] = ((1<<16) / ciqt[i])>>3;
    }

    lb8 = 0;
    while (liqt[RTjpeg_ZZ[++lb8]] <= 8)
        ;
    lb8--;
    cb8 = 0;

    while (ciqt[RTjpeg_ZZ[++cb8]] <= 8)
        ;
    cb8--;
}

int RTjpeg::SetQuality(int *quality)
{
    if (*quality < 1)
        *quality = 1;
    if (*quality > 255)
        *quality = 255;

    Q = *quality;

    CalcTbls();
    DctInit();
    IdctInit();
    QuantInit();

    return 0;
}

int RTjpeg::SetFormat(int *fmt)
{
    f = *fmt;
    return 0;
}

int RTjpeg::SetSize(int *w, int *h)
{
    if ((*w < 0) || (*w > 65535))
        return -1;
    if ((*h < 0) || (*h > 65535))
        return -1;

    width = *w;
    height = *h;
    Ywidth = width>>3;
    Ysize = width * height;
    Cwidth = width>>4;
    Csize = (width>>1) * height;

    if (key_rate > 0)
    {
        unsigned long tmp;
        if (old)
            delete [] old_start;
        old_start = new int16_t[((4*width*height)+32)];

        tmp = (unsigned long)old_start;
        tmp += 32;
        tmp = tmp>>5;

        old = (int16_t *)(tmp<<5);
        if (!old)
        {
            fprintf(stderr, "RTjpeg: Could not allocate memory\n");
            return -1;
        }
        memset(old, 0, ((4*width*height)));
    }
    return 0;
}

int RTjpeg::SetIntra(int *key, int *lm, int *cm)
{
    unsigned long tmp;

    if (*key < 0)
        *key = 0;
    if (*key > 255)
        *key = 255;
    key_rate = *key;

    if (*lm < 0)
        *lm = 0;
    if (*lm > 16)
        *lm = 16;
    if (*cm < 0)
        *cm = 0;
    if (*cm > 16)
        *cm = 16;

#ifdef MMX
    lmask.uq = (((uint64_t)(*lm)<<48)|((uint64_t)(*lm)<<32)|((uint64_t)(*lm)<<16)|(uint64_t)(*lm));
    cmask.uq = (((uint64_t)(*cm)<<48)|((uint64_t)(*cm)<<32)|((uint64_t)(*cm)<<16)|(uint64_t)(*cm));
#else
    lmask = *lm;
    cmask = *cm;
#endif

    if (old)
        delete [] old_start;
    old_start = new int16_t[((4*width*height)+32)];
    tmp = (unsigned long)old_start;
    tmp += 32;
    tmp = tmp >> 5;
    old = (int16_t *)(tmp << 5);
    if (!old)
    {
         fprintf(stderr, "RTjpeg: Could not allocate memory\n");
         return -1;
    }
    memset(old, 0, ((4*width*height)));

    return 0;
}

RTjpeg::RTjpeg(void)
{
    for (int i = 0; i < 64; i++)
    {
        block[i] = 0;
        lqt[i] = cqt[i] = liqt[i] = ciqt[i] = 0;
        for (int j = 0; j < 4; j++)
            ws[i*j] = 0;
    }
    lb8 = cb8 = Ywidth = Cwidth = Ysize = Csize = key_count = 0;
    width = height = Q = f = key_rate = 0;

    old = old_start = NULL;

#ifdef MMX
    lmask.q = cmask.q = 0;
    RTjpeg_ones.q =(long long)0x0001000100010001LL;
    RTjpeg_half.q =(long long)0x7fff7fff7fff7fffLL;
    RTjpeg_C4.q   =(long long)0x2D412D412D412D41LL;
    RTjpeg_C6.q   =(long long)0x187E187E187E187ELL;
    RTjpeg_C2mC6.q=(long long)0x22A322A322A322A3LL;
    RTjpeg_C2pC6.q=(long long)0x539F539F539F539FLL;
    RTjpeg_zero.q =(long long)0x0000000000000000LL;
#else
    lmask = cmask = 0;
#endif
}

RTjpeg::~RTjpeg(void)
{
    if (old_start)
        delete [] old_start;
}

inline int RTjpeg::compressYUV420(int8_t *sp, uint8_t **planes)
{
 int8_t * sb;
 uint8_t * bp = planes[0];
 uint8_t * bp1 = bp + (width<<3);
 uint8_t * bp2 = planes[1];
 uint8_t * bp3 = planes[2];
 int i, j, k;

#ifdef MMX
 emms();
#endif
 sb = sp;
/* Y */
 for(i = height >> 1; i; i -= 8)
 {
  for(j = 0, k = 0; j < width; j += 16, k += 8)
  {
    DctY(bp+j, Ywidth);
    Quant(block, lqt);
    sp += b2s(block, sp, lb8);

    DctY(bp+j+8, Ywidth);
    Quant(block, lqt);
    sp += b2s(block, sp, lb8);

    DctY(bp1+j, Ywidth);
    Quant(block, lqt);
    sp += b2s(block, sp, lb8);

    DctY(bp1+j+8, Ywidth);
    Quant(block, lqt);
    sp += b2s(block, sp, lb8);

    DctY(bp2+k, Cwidth);
    Quant(block, cqt);
    sp += b2s(block, sp, cb8);

    DctY(bp3+k, Cwidth);
    Quant(block, cqt);
    sp += b2s(block, sp, cb8);
  }
  bp += width<<4;
  bp1 += width<<4;
  bp2 += width<<2;
  bp3 += width<<2;
 }
#ifdef MMX
 emms();
#endif
 return (sp - sb);
}

inline int RTjpeg::compressYUV422(int8_t *sp, uint8_t **planes)
{
 int8_t * sb;
 uint8_t * bp = planes[0];
 uint8_t * bp2 = planes[1];
 uint8_t * bp3 = planes[2];
 int i, j, k;

#ifdef MMX
 emms();
#endif
 sb=sp;
/* Y */
 for(i=height; i; i-=8)
 {
  for(j=0, k=0; j<width; j+=16, k+=8)
  {
   DctY(bp+j, Ywidth);
   Quant(block, lqt);
   sp += b2s(block, sp, lb8);

   DctY(bp+j+8, Ywidth);
   Quant(block, lqt);
   sp += b2s(block, sp, lb8);

   DctY(bp2+k, Cwidth);
   Quant(block, cqt);
   sp+=b2s(block, sp, cb8);

   DctY(bp3+k, Cwidth);
   Quant(block, cqt);
   sp+=b2s(block, sp, cb8);

  }
  bp += width << 3;
  bp2 += width << 2;
  bp3 += width << 2;

 }
#ifdef MMX
 emms();
#endif
 return (sp-sb);
}

inline int RTjpeg::compress8(int8_t *sp, uint8_t **planes)
{
 int8_t * sb;
 uint8_t * bp = planes[0];
 int i, j;

#ifdef MMX
 emms();
#endif

 sb=sp;
/* Y */
 for(i=0; i<height; i+=8)
 {
  for(j=0; j<width; j+=8)
  {
   DctY(bp+j, width);
   Quant(block, lqt);
   sp += b2s(block, sp, lb8);
  }
  bp += width;
 }

#ifdef MMX
 emms();
#endif
 return (sp-sb);
}

inline void RTjpeg::decompressYUV422(int8_t *sp, uint8_t **planes)
{
 uint8_t * bp = planes[0];
 uint8_t * bp2 = planes[1];
 uint8_t * bp3 = planes[2];
 int i, j,k;

#ifdef MMX
 emms();
#endif

/* Y */
 for(i=height; i; i-=8)
 {
  for(k=0, j=0; j<width; j+=16, k+=8) {
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, lb8, liqt);
    Idct(bp+j, block, width);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, lb8, liqt);
    Idct(bp+j+8, block, width);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, cb8, ciqt);
    Idct(bp2+k, block, width>>1);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, cb8, ciqt);
    Idct(bp3+k, block, width>>1);
   }
  }
  bp += width<<3;
  bp2 += width<<2;
  bp3 += width<<2;
 }
#ifdef MMX
 emms();
#endif
}

inline void RTjpeg::decompressYUV420(int8_t *sp, uint8_t **planes)
{
 uint8_t * bp = planes[0];
 uint8_t * bp1 = bp + (width<<3);
 uint8_t * bp2 = planes[1];
 uint8_t * bp3 = planes[2];
 int i, j,k;

#ifdef MMX
 emms();
#endif

/* Y */
 for(i=height>>1; i; i-=8)
 {
  for(k=0, j=0; j<width; j+=16, k+=8) {
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, lb8, liqt);
    Idct(bp+j, block, width);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, lb8, liqt);
    Idct(bp+j+8, block, width);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, lb8, liqt);
    Idct(bp1+j, block, width);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, lb8, liqt);
    Idct(bp1+j+8, block, width);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, cb8, ciqt);
    Idct(bp2+k, block, width>>1);
   }
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, cb8, ciqt);
    Idct(bp3+k, block, width>>1);
   }
  }
  bp += width<<4;
  bp1 += width<<4;
  bp2 += width<<2;
  bp3 += width<<2;
 }
#ifdef MMX
 emms();
#endif
}

inline void RTjpeg::decompress8(int8_t *sp, uint8_t **planes)
{
 uint8_t * bp = planes[0];
 int i, j;

#ifdef MMX
 emms();
#endif

/* Y */
 for(i=0; i<height; i+=8)
 {
  for(j=0; j<width; j+=8)
   if (*sp==-1)sp++;
   else
   {
    sp += s2b(block, sp, lb8, liqt);
    Idct(bp+j, block, width);
   }
  bp += width<<3;
 }
}

#ifdef MMX

int RTjpeg::bcomp(int16_t *rblock, int16_t *old, mmx_t *mask)
{
 int i;
 mmx_t *mold=(mmx_t *)old;
 mmx_t *mblock=(mmx_t *)rblock;
 volatile mmx_t result;
 static mmx_t neg= { (unsigned long long)0xffffffffffffffffULL };

 movq_m2r(*mask, mm7);
 movq_m2r(neg, mm6);
 pxor_r2r(mm5, mm5);

 for(i=0; i<8; i++)
 {
  movq_m2r(*(mblock++), mm0);
                        movq_m2r(*(mblock++), mm2);
  movq_m2r(*(mold++), mm1);
                        movq_m2r(*(mold++), mm3);
  psubsw_r2r(mm1, mm0);
                        psubsw_r2r(mm3, mm2);
  movq_r2r(mm0, mm1);
                        movq_r2r(mm2, mm3);
  pcmpgtw_r2r(mm7, mm0);
                        pcmpgtw_r2r(mm7, mm2);
  pxor_r2r(mm6, mm1);
                        pxor_r2r(mm6, mm3);
  pcmpgtw_r2r(mm7, mm1);
                        pcmpgtw_r2r(mm7, mm3);
  por_r2r(mm0, mm5);
                        por_r2r(mm2, mm5);
  por_r2r(mm1, mm5);
                        por_r2r(mm3, mm5);
 }
 movq_r2m(mm5, result);

 if (result.q)
 {
  for(i=0; i<16; i++)((uint64_t *)old)[i]=((uint64_t *)rblock)[i];
  return 0;
 }
 return 1;
}

#else
int RTjpeg::bcomp(int16_t *rblock, int16_t *old, uint16_t *mask)
{
 int i;

 for(i=0; i<64; i++)
  if (abs(old[i]-rblock[i])>*mask)
  {
   for(i=0; i<16; i++)((uint64_t *)old)[i]=((uint64_t *)rblock)[i];
   return 0;
  }
 return 1;
}
#endif

inline int RTjpeg::mcompressYUV420(int8_t *sp, uint8_t **planes)
{
 int8_t * sb;
 int16_t * lblock;
 uint8_t * bp = planes[0];
 uint8_t * bp1 = bp + (width<<3);
 uint8_t * bp2 = planes[1];
 uint8_t * bp3 = planes[2];
 int i, j, k;

 sb = sp;
 lblock = old;
/* Y */
 for(i = height>>1; i; i-=8)
 {
  for(j=0, k=0; j < width; j+=16, k+=8)
  {
   DctY(bp+j, Ywidth);
   Quant(block, lqt);
   if (bcomp(block, lblock, &lmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp+=b2s(block, sp, lb8);
   lblock += 64;

   DctY(bp+j+8, Ywidth);
   Quant(block, lqt);
   if (bcomp(block, lblock, &lmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp += b2s(block, sp, lb8);
   lblock += 64;

   DctY(bp1+j, Ywidth);
   Quant(block, lqt);
   if (bcomp(block, lblock, &lmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp += b2s(block, sp, lb8);
   lblock += 64;

   DctY(bp1+j+8, Ywidth);
   Quant(block, lqt);
   if (bcomp(block, lblock, &lmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp += b2s(block, sp, lb8);
   lblock += 64;

   DctY(bp2+k, Cwidth);
   Quant(block, cqt);
   if (bcomp(block, lblock, &cmask))
   {
    *((uint8_t *)sp++)=255;
   }
        else sp+=b2s(block, sp, cb8);
   lblock+=64;

   DctY(bp3+k, Cwidth);
   Quant(block, cqt);
   if (bcomp(block, lblock, &cmask))
   {
    *((uint8_t *)sp++)=255;
   }
        else sp+=b2s(block, sp, cb8);
   lblock+=64;
  }
  bp += width<<4;
  bp1 += width<<4;
  bp2 += width<<2;
  bp3 += width<<2;
 }
#ifdef MMX
 emms();
#endif
 return (sp-sb);
}


inline int RTjpeg::mcompressYUV422(int8_t *sp, uint8_t **planes)
{
 int8_t * sb;
 int16_t *lblock;
 uint8_t * bp = planes[0];
 uint8_t * bp2 = planes[1];
 uint8_t * bp3 = planes[2];
 int i, j, k;

 sb=sp;
 lblock = old;
 for(i = height; i; i-=8)
 {
  for(j=0, k=0; j<width; j+=16, k+=8)
  {
   DctY(bp+j, Ywidth);
   Quant(block, lqt);
   if (bcomp(block, lblock, &lmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp+=b2s(block, sp, lb8);
   lblock+=64;

   DctY(bp+j+8, Ywidth);
   Quant(block, lqt);
   if (bcomp(block, lblock, &lmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp+=b2s(block, sp, lb8);
   lblock+=64;

   DctY(bp2+k, Cwidth);
   Quant(block, cqt);
   if (bcomp(block, lblock, &cmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp+=b2s(block, sp, cb8);
   lblock+=64;

   DctY(bp3+k, Cwidth);
   Quant(block, cqt);
   if (bcomp(block, lblock, &cmask))
   {
    *((uint8_t *)sp++)=255;
   }
   else sp+=b2s(block, sp, cb8);
   lblock+=64;

  }
  bp += width<<3;
  bp2 += width<<2;
  bp3 += width<<2;
 }
#ifdef MMX
 emms();
#endif
 return (sp-sb);
}

inline int RTjpeg::mcompress8(int8_t *sp, uint8_t **planes)
{
 uint8_t * bp = planes[0];
 int8_t * sb;
 int16_t *lblock;
 int i, j;

 sb=sp;
 lblock = old;
 for(i=0; i<height; i+=8)
 {
  for(j=0; j<width; j+=8)
  {
   DctY(bp+j, width);
   Quant(block, lqt);
   if (bcomp(block, lblock, &lmask))
   {
    *((uint8_t *)sp++)=255;
   } else sp+=b2s(block, sp, lb8);
   lblock+=64;
  }
  bp+=width<<3;
 }
#ifdef MMX
 emms();
#endif
 return (sp-sb);
}

void RTjpeg::SetNextKey(void)
{
    key_count = 0;
}

int RTjpeg::Compress(int8_t *sp, uint8_t **planes)
{
 RTjpeg_frameheader * fh = (RTjpeg_frameheader *)sp;
 int ds = 0;

 if (key_rate == 0)
 {
  switch(f)
  {
   case RTJ_YUV420: ds = compressYUV420((int8_t*)&(fh->data), planes); break;
   case RTJ_YUV422: ds = compressYUV422((int8_t*)&(fh->data), planes); break;
   case RTJ_RGB8: ds = compress8((int8_t*)&(fh->data), planes); break;
  }
  fh->key = 0;
 } else {
  if (key_count == 0)
   memset(old, 0, ((4 * width * height)));
  switch(f)
  {
   case RTJ_YUV420: ds = mcompressYUV420((int8_t*)&(fh->data), planes); break;
   case RTJ_YUV422: ds = mcompressYUV422((int8_t*)&(fh->data), planes); break;
   case RTJ_RGB8: ds = mcompress8((int8_t*)&(fh->data), planes); break;
  }
  fh->key = key_count;
  if (++key_count > key_rate)
   key_count = 0;
 }
 ds += RTJPEG_HEADER_SIZE;
 fh->framesize = RTJPEG_SWAP_WORD(ds);
 fh->headersize = RTJPEG_HEADER_SIZE;
 fh->version = RTJPEG_FILE_VERSION;
 fh->width = RTJPEG_SWAP_HALFWORD(width);
 fh->height = RTJPEG_SWAP_HALFWORD(height);
 fh->quality = Q;
 return ds;
}

void RTjpeg::Decompress(int8_t *sp, uint8_t **planes)
{
 RTjpeg_frameheader * fh = (RTjpeg_frameheader *)sp;

 if ((RTJPEG_SWAP_HALFWORD(fh->width) != width)||
    (RTJPEG_SWAP_HALFWORD(fh->height) != height))
 {
  int w = RTJPEG_SWAP_HALFWORD(fh->width);
  int h = RTJPEG_SWAP_HALFWORD(fh->height);
  SetSize(&w, &h);
 }
 if (fh->quality != Q)
 {
  int q = fh->quality;
  SetQuality(&q);
 }
 switch(f)
 {
  case RTJ_YUV420: decompressYUV420((int8_t*)&(fh->data), planes); break;
  case RTJ_YUV422: decompressYUV422((int8_t*)&(fh->data), planes); break;
  case RTJ_RGB8: decompress8((int8_t*)&(fh->data), planes); break;
 }
}
