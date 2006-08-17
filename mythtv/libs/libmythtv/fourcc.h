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

/* See http://www.fourcc.org/yuv.php for more info on formats */

/* These should probably all be defined by */
/*  MAKEFOURCC(), MKTAG() or v4l2_fourcc() */

#define GUID_I420_PLANAR 0x30323449
#define GUID_IYUV_PLANAR 0x56555949 /**< bit equivalent to I420 */
#define GUID_YV12_PLANAR 0x32315659
