/* 
   RTjpeg (C) Justin Schoeman 1998 (justin@suntiger.ee.up.ac.za)
   
   With modifications by:
   (c) 1998, 1999 by Joerg Walter <trouble@moes.pmnet.uni-oldenburg.de>
   and
   (c) 1999 by Wim Taymans <wim.taymans@tvd.be>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public  
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,  
but WITHOUT ANY WARRANTY; without even the implied warranty of   
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public   
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#ifndef __RTJPEG_H__
#define __RTJPEG_H__

#include "mythconfig.h"
#include "mythtvexp.h"
#include <cstdint>

/*
 * Macros and definitions used internally to RTjpeg
 */

#define RTJPEG_FILE_VERSION 0
#define RTJPEG_HEADER_SIZE 12

#if HAVE_BIGENDIAN
#define RTJPEG_SWAP_WORD(a) ( ((a) << 24) | \
			(((a) << 8) & 0x00ff0000) | \
			(((a) >> 8) & 0x0000ff00) | \
			((unsigned long)(a) >>24) )
#define RTJPEG_SWAP_HALFWORD(a) ( (((a) << 8) & 0xff00) | \
			(((a) >> 8) & 0x00ff) )
#else
#define RTJPEG_SWAP_WORD(a) (a)
#define RTJPEG_SWAP_HALFWORD(a) (a)
#endif

#if HAVE_STDINT_H
#include <cstdint>
#endif

#ifdef MMX
#include "ffmpeg-mmx.h"
#endif

/* Format definitions */

#define RTJ_YUV420 0
#define RTJ_YUV422 1
#define RTJ_RGB8   2

class RTjpeg
{
  public:
    RTjpeg();
   ~RTjpeg();
   
    int SetQuality(int *quality);
    int SetFormat(const int *fmt);
    int SetSize(const int *w, const int *h);
    int SetIntra(int *key, int *lm, int *cm);

    int Compress(int8_t *sp, uint8_t **planes);
    void Decompress(int8_t *sp, uint8_t **planes);

    void SetNextKey(void);

private:
    static int b2s(const int16_t *data, int8_t *strm, uint8_t bt8);
    static int s2b(int16_t *data, const int8_t *strm, uint8_t bt8, int32_t *qtbla);

    void QuantInit(void);
    static void Quant(int16_t *block, int32_t *qtbl);
   
    void DctInit(void);
    void DctY(uint8_t *idata, int rskip);

    void IdctInit(void);
    void Idct(uint8_t *odata, int16_t *data, int rskip);

    void CalcTbls(void);

    inline int compressYUV420(int8_t *sp, uint8_t **planes);
    inline int compressYUV422(int8_t *sp, uint8_t **planes);
    inline int compress8(int8_t *sp, uint8_t **planes);

    int mcompressYUV420(int8_t *sp, uint8_t **planes);
    int mcompressYUV422(int8_t *sp, uint8_t **planes);
    int mcompress8(int8_t *sp, uint8_t **planes);
    
    void decompressYUV422(int8_t *sp, uint8_t **planes);
    void decompressYUV420(int8_t *sp, uint8_t **planes);
    void decompress8(int8_t *sp, uint8_t **planes);

#ifdef MMX
    static int bcomp(int16_t *rblock, int16_t *old, mmx_t *mask);
#else
    static int bcomp(int16_t *rblock, int16_t *old, uint16_t *mask);
#endif
    
    int16_t   m_block[64] MALIGN32 {0};
    int32_t   m_ws[64*4]  MALIGN32 {0};
    int32_t   m_lqt[64]   MALIGN32 {0};
    int32_t   m_cqt[64]   MALIGN32 {0};
    int32_t   m_liqt[64]  MALIGN32 {0};
    int32_t   m_ciqt[64]  MALIGN32 {0};
    int32_t   m_lB8                {0};
    int32_t   m_cB8                {0};
    int32_t   m_yWidth             {0};
    int32_t   m_cWidth             {0};
    int32_t   m_ySize              {0};
    int32_t   m_cSize              {0};
    int16_t  *m_old                {nullptr};
    int16_t  *m_oldStart           {nullptr};
    int       m_keyCount           {0};

    int       m_width              {0};
    int       m_height             {0};
    int       m_q                  {0};
    int       m_f                  {0};
#ifdef MMX
    mmx_t     m_lMask              {};
    mmx_t     m_cMask              {};
#else
    uint16_t  m_lMask              {0};
    uint16_t  m_cMask              {0};
#endif
    int       m_keyRate            {0};
};

struct RTjpeg_frameheader {
	uint32_t framesize;
	uint8_t headersize;
	uint8_t version;
	uint16_t width;
	uint16_t height;
	uint8_t quality;
	uint8_t key;
	uint8_t data;
};

#endif
