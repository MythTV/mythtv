/*
	h263.h

	(c) 2004 Paul Volkaerts
	
    header for the H263 Container class
*/

#ifndef H263_CONTAINER_H_
#define H263_CONTAINER_H_


#ifndef WIN32
#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>
#include <mythtv/volumecontrol.h>

#include "directory.h"
#endif

#include "webcam.h"
#include "sipfsm.h"
#include "rtp.h"

extern "C" {
#ifdef WIN32
#include "libavcodec/avcodec.h"
#else
#include "mythtv/ffmpeg/avcodec.h"
#endif
}


#define MAX_RGB_704_576     (704*576*4)
#define MAX_YUV_704_576     (800*576*3/2) // Add a little onto the width in case the stride is bigger than the width


// Declare static YUV and RGB handling fns.
void YUV422PtoYUV420P(int width, int height, unsigned char *image);
void RGB24toRGB32(const unsigned char *rgb24, unsigned char *rgb32, int len);
void YUV422PtoRGB32(int width, int height, const unsigned char *src, unsigned char *dst, int dstSize);
void YUV420PtoRGB32(const uchar *py, const uchar *pu, const uchar *pv, int width, int height, int stride, unsigned char *dst, int dstSize);
void YUV420PtoRGB32(int width, int height, int stride, const unsigned char *src, unsigned char *dst, int dstSize);
void scaleYuvImage(const uchar *yuvBuffer, int ow, int oh, int dw, int dh, uchar *dst);
void cropYuvImage(const uchar *yuvBuffer, int ow, int oh, int cx, int cy, int cw, int ch, uchar *dst);
void flipRgb32Image(const uchar *rgbBuffer, int w, int h, uchar *dst);
void flipYuv420pImage(const uchar *yuvBuffer, int w, int h, uchar *dst);
void flipYuv422pImage(const uchar *yuvBuffer, int w, int h, uchar *dst);
void flipRgb24Image(const uchar *rgbBuffer, int w, int h, uchar *dst);



class H263Container
{
  public:
    H263Container(void);
    virtual ~H263Container(void);

    bool H263StartEncoder(int w, int h, int fps);
    bool H263StartDecoder(int w, int h);
    uchar *H263EncodeFrame(const uchar *yuvFrame, int *len);
    uchar *H263DecodeFrame(const uchar *h263Frame, int h263FrameLen, uchar *rgbBuffer, int rgbBufferSize);
    void H263StopEncoder();
    void H263StopDecoder();

  private:
    AVFrame pictureOut, *pictureIn;
    AVCodec *h263Encoder, *h263Decoder;
    AVCodecContext *h263EncContext, *h263DecContext;
    int MaxPostEncodeSize, lastCompressedSize;
    unsigned char *PostEncodeFrame, *PreEncodeFrame;

};

#endif
