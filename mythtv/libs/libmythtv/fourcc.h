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

#ifndef MKTAG(a,b,c,d)
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
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
 *****************************************************************************/
/* See http://www.fourcc.org/yuv.php for more info on formats */

#define FOURCC_I420 MKTAG('I','4','2','0')
#define FOURCC_IYUV MKTAG('I','Y','U','V')
#define FOURCC_RGB2 MKTAG('R','G','B','2')
#define FOURCC_RGBX MKTAG('R','G','B','X')
#define FOURCC_RV15 MKTAG('R','V','1','5')
#define FOURCC_RV16 MKTAG('R','V','1','6')
#define FOURCC_RV24 MKTAG('R','V','2','4')
#define FOURCC_RV32 MKTAG('R','V','3','2')
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
