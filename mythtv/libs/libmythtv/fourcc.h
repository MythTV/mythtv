#ifndef _FOURCC_H
#define _FOURCC_H
/******************************************************************************
 * fourcc.h - Four Character Constants
 *
 * Ever since the early days of the Macintosh clipboard and AmigaDOS,
 * little 32 bit strings have been used to identify the contents of a file.
 * TIFF, AIFF, JPEG, MPEG - they have spread everywhere.
 * Including the MythTV source code.
 *
 * Time to define them in one file.
 * 
 * $Id$
 *****************************************************************************/

#include "mythconfig.h"

#ifdef __cplusplus
extern "C" {
#include "libavutil/common.h" // for MKTAG
}
#endif

/* Probably not thread safe */
static inline char * fourcc_str(int i)
{
    static char str[5];

    str[0] = ((char) (i & 0xFF)),
    str[1] = ((char) ((i >> 8) & 0xFF)),
    str[2] = ((char) ((i >> 16) & 0xFF)),
    str[3] = ((char) ((i >> 24) & 0xFF)),
    str[4] = '\0';

    return str;
}


/******************************************************************************
 * Bitmap formats:
 *
 * Note: V4L2 also defines several of these FourCCs in videodev2.h
 *            (e.g. V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_MPEG).
 *       Use those only where data is being read from a tuner card
 *****************************************************************************/
/* See http://www.fourcc.org/yuv.php for more info on formats */

#define FOURCC_422P MKTAG('4','2','2','P') /**< YVU422 planar */
#define FOURCC_AI44 MKTAG('A','I','4','4')
#define FOURCC_I420 MKTAG('I','4','2','0')
#define FOURCC_IA44 MKTAG('I','A','4','4')
#define FOURCC_IYUV MKTAG('I','Y','U','V')
#define FOURCC_RGB2 MKTAG('R','G','B','2')
#define FOURCC_RGBX MKTAG('R','G','B','X')
#define FOURCC_RV15 MKTAG('R','V','1','5')
#define FOURCC_RV16 MKTAG('R','V','1','6')
#define FOURCC_RV24 MKTAG('R','V','2','4')
#define FOURCC_RV32 MKTAG('R','V','3','2')
#define FOURCC_YU12 MKTAG('Y','U','1','2')
#define FOURCC_YUNV MKTAG('Y','U','N','V')
#define FOURCC_YUY2 MKTAG('Y','U','Y','2')
#define FOURCC_YUYV MKTAG('Y','U','Y','V')
#define FOURCC_YV12 MKTAG('Y','V','1','2')


/* These should probably all be defined by */
/*  MAKEFOURCC(), MKTAG() or v4l2_fourcc() */

#define GUID_I420_PLANAR 0x30323449
#define GUID_IYUV_PLANAR 0x56555949 /**< bit equivalent to I420 */
#define GUID_YV12_PLANAR 0x32315659
#define GUID_IA44_PACKED 0x34344941
#define GUID_AI44_PACKED 0x34344149
#define GUID_YUY2_PACKED 0x32595559 /* same as YUYV */
#define GUID_UYVY_PACKED 0x59565955


/******************************************************************************
 * Common audio codec IDs:
 *****************************************************************************/

#define FOURCC_LAME MKTAG('L','A','M','E')
#define FOURCC_RAWA MKTAG('R','A','W','A')
#define FOURCC_AC3  MKTAG('A','C','3',' ')


/******************************************************************************
 * Common video codec IDs:
 *****************************************************************************/

#define FOURCC_DIV3 MKTAG('D','I','V','3') /* MPEG4 v3 */
#define FOURCC_DIVX MKTAG('D','I','V','X') /* MPEG4 */
#define FOURCC_dvsd MKTAG('d','v','s','d') /* DV cameras (FireWire/IEEE1394) */
#define FOURCC_H263 MKTAG('H','2','6','3')
#define FOURCC_H264 MKTAG('H','2','6','4')
#define FOURCC_HFYU MKTAG('H','F','Y','U')
#define FOURCC_I263 MKTAG('I','2','6','3')
#define FOURCC_MJPG MKTAG('M','J','P','G')
#define FOURCC_MP42 MKTAG('M','P','4','2') /* MPEG4 v2 */
#define FOURCC_MPEG MKTAG('M','P','E','G') /* MPEG1 */
#define FOURCC_MPG2 MKTAG('M','P','G','2') /* MPEG2 */
#define FOURCC_MPG4 MKTAG('M','P','G','4') /* MPEG4 v1 */
#define FOURCC_RJPG MKTAG('R','J','P','G')
#define FOURCC_WMV1 MKTAG('W','M','V','1')


/*****************************************************************************/
#endif  // ifndef _FOURCC_H
