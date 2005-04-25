/*
	h263.cpp

	(c) 2004 Paul Volkaerts
	
  Container class for the H.263 Video Codec, which just interfaces to the libavcodec routines

  TODO:-
    These are non reentrant so need a mutex implemented, if they are to be used elsewhere in the Myth frontend
*/
#include <qapplication.h>
#include <qimage.h>

#include <iostream>
using namespace std;

#ifndef WIN32
#include <linux/videodev.h>
#include "config.h"
#endif

#include "h263.h"


H263Container::H263Container()
{
    lastCompressedSize = 0;
    PostEncodeFrame = 0;
    h263Encoder = 0;
    h263EncContext = 0;

    h263Decoder = 0;
    h263DecContext = 0;
    pictureIn = 0;

    avcodec_init();
    avcodec_register_all();
}

H263Container::~H263Container()
{
}

bool H263Container::H263StartEncoder(int w, int h, int fps)
{
    h263Encoder = avcodec_find_encoder(CODEC_ID_H263);
    if (!h263Encoder) 
    {
        cerr << "Could not find H.263 Encoder\n";
        return false;
    }

    h263EncContext = avcodec_alloc_context();
    
    /* put sample parameters */
    h263EncContext->bit_rate = 2000000;
    /* resolution must be a multiple of two */
    h263EncContext->width = w;  
    h263EncContext->height = h;
    /* frames per second */
    h263EncContext->frame_rate = fps;  
    h263EncContext->frame_rate_base= 1;
    h263EncContext->gop_size = 600; // Max allowed by library. Was fps*5; /* emit one intra frame every five secs */
    h263EncContext->max_b_frames=0;

    /* open it */
    if (avcodec_open(h263EncContext, h263Encoder) < 0) 
    {
        cerr << "Could not open H.263 Encoder\n";
        return false;
    }
    
    /* alloc image and output buffer */
    MaxPostEncodeSize = 100000;
    PostEncodeFrame = (uchar *)malloc(MaxPostEncodeSize);
    
    pictureOut.linesize[0] = h263EncContext->width;
    pictureOut.linesize[1] = h263EncContext->width / 2;
    pictureOut.linesize[2] = h263EncContext->width / 2;

    return true;
}

bool H263Container::H263StartDecoder(int w, int h)
{
    h263Decoder = avcodec_find_decoder(CODEC_ID_H263);
    if (!h263Decoder)
    {
        cerr << "Could not find H.263 decoder\n";
        return false;
    }

    h263DecContext = avcodec_alloc_context();
    pictureIn = avcodec_alloc_frame();

    h263DecContext->codec_id = CODEC_ID_H263;
    h263DecContext->width = w;
    h263DecContext->height = h;

    /* open it */
    if (avcodec_open(h263DecContext, h263Decoder) < 0) 
    {
        cerr << "Could not open H.263 Decoder\n";
        return false;
    }
    
    return true;
}

uchar *H263Container::H263EncodeFrame(const uchar *yuvFrame, int *len)
{
    int size = h263EncContext->width * h263EncContext->height;
    pictureOut.data[0] = (uchar *)yuvFrame;
    pictureOut.data[1] = pictureOut.data[0] + size;
    pictureOut.data[2] = pictureOut.data[1] + size / 4;

    *len = lastCompressedSize = avcodec_encode_video(h263EncContext, PostEncodeFrame, MaxPostEncodeSize, &pictureOut);

    return PostEncodeFrame;
}

uchar *H263Container::H263DecodeFrame(const uchar *h263Frame, int h263FrameLen, uchar *rgbBuffer, int rgbBufferSize)
{
    int got_picture;

    int len = avcodec_decode_video(h263DecContext, pictureIn, &got_picture, (uchar *) h263Frame, h263FrameLen);
    if (len != h263FrameLen)
    {
        cerr << "Error decoding frame; " << len << endl;
        return 0;
    }

    if (got_picture)
    {
        YUV420PtoRGB32(pictureIn->data[0], pictureIn->data[1], pictureIn->data[2], h263DecContext->width, h263DecContext->height, pictureIn->linesize[0], rgbBuffer, rgbBufferSize);
        return rgbBuffer;
    }
    return 0;
}

void H263Container::H263ForceIFrame()
{
    // libavcodec does not support forcing an I-frame, so close & reopen encoder for now
    while (lastCompressedSize != 0)
        lastCompressedSize = avcodec_encode_video(h263EncContext, PostEncodeFrame, MaxPostEncodeSize, NULL);
    avcodec_close(h263EncContext);
    avcodec_open(h263EncContext, h263Encoder);
}

void H263Container::H263StopEncoder()
{
    while (lastCompressedSize != 0)
    {
        lastCompressedSize = avcodec_encode_video(h263EncContext, PostEncodeFrame, MaxPostEncodeSize, NULL);
    }

    if (PostEncodeFrame)
    {
        free(PostEncodeFrame);
        PostEncodeFrame = 0;
    }

    if (h263EncContext)
    {
        avcodec_close(h263EncContext);
        av_free(h263EncContext);
        h263EncContext = 0;
    }
}

void H263Container::H263StopDecoder()
{
    int got_picture;

    // See if there is a last frame
    avcodec_decode_video(h263DecContext, pictureIn, &got_picture, NULL, 0);

    if (h263DecContext)
    {
        avcodec_close(h263DecContext);
        av_free(h263DecContext);
        h263DecContext = 0;
    }

    if (pictureIn)
        av_free(pictureIn);
    pictureIn = 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//  YUV and RGB handling routines.  These are probably in a library somewhere so are only here
//  until I find a more efficient way of doing this
//////////////////////////////////////////////////////////////////////////////////////////////


void RGB24toRGB32(const unsigned char *rgb24, unsigned char *rgb32, int len)
{
  for (int i=0; i<len; i++)
  {
    QRgb *rgb = (QRgb *)rgb32;
    *rgb = qRgb(*(rgb24+2), *(rgb24+1), *(rgb24));
    rgb24 += 3;
    rgb32 += 4;
  }
}


// Thanks to ...
// Nemosoft Unv.      nemosoft@smcc.demon.nl.
//
// YUV420P is encoded as 8bits per pixel for Y and 8bits per 2x2 square of pixels for U and V.
//
// Each YUV420P format frame has width * height * 8bits per pixel for Y followed by 
// width/2 * height/2 * 8 bits per pixels for U then
// width/2 * height/2 * 8 bits per pixels for V
//
// YUV -> RGB calculations are:-
//
// R = V/0.877 + Y
// B = U/0.433 + Y
// G = (Y+128 - (0.299*R) - (0.144*B)) / 0.587
//

void YUV420PtoRGB32(int width, int height, int stride, const unsigned char *src, unsigned char *dst, int dstSize)
{
	int h, w;
	const unsigned char *py, *pu, *pv;
	py = src;
	pu = py + (stride * height);
	pv = pu + (stride * height) / 4;
  int yStrideDelta = stride-width;
  int uvStride = stride>>1;

  if (dstSize < (width*height*4))
  {
    cout << "YUVtoRGB buffer (" << dstSize << ") too small for " << width << "x" << height << " pixels" << endl;
    return;
  }

	for (h=0; h<height; h++) 
	{
		for (w=0; w<width; w++)
		{
			signed int _r,_g,_b; 
			signed int r, g, b;
			signed int y, u, v;

			y = *py++ - 16;
			u = pu[w>>1] - 128;
			v = pv[w>>1] - 128;

			_r = _R(y,u,v);
			_g = _G(y,u,v);
			_b = _B(y,u,v);

			r = _S(_r);
			g = _S(_g);
			b = _S(_b);

      *dst++ = r;
      *dst++ = g;
      *dst++ = b;
      *dst++ = 0;
		}

    py += yStrideDelta;
		if (h%2) {
			pu += uvStride;
			pv += uvStride;
		}
	}
}


void YUV420PtoRGB32(const uchar *py, const uchar *pu, const uchar *pv, int width, int height, int stride, unsigned char *dst, int dstSize)
{
	int h, w;
  int yStrideDelta = stride-width;
  int uvStride = stride>>1;

  if (dstSize < (width*height*4))
  {
    cout << "YUVtoRGB buffer (" << dstSize << ") too small for " << width << "x" << height << " pixels" << endl;
    return;
  }

	for (h=0; h<height; h++) 
	{
		for (w=0; w<width; w++)
		{
			signed int _r,_g,_b; 
			signed int r, g, b;
			signed int y, u, v;

			y = *py++ - 16;
			u = pu[w>>1] - 128;
			v = pv[w>>1] - 128;

			_r = _R(y,u,v);
			_g = _G(y,u,v);
			_b = _B(y,u,v);

			r = _S(_r);
			g = _S(_g);
			b = _S(_b);

      *dst++ = r;
      *dst++ = g;
      *dst++ = b;
      *dst++ = 0;
		}

    py += yStrideDelta;
		if (h%2) {
			pu += uvStride;
			pv += uvStride;
		}
	}
}

void YUV422PtoRGB32(int width, int height, const unsigned char *src, unsigned char *dst, int dstSize)
{
	int h, w;
	const unsigned char *py, *pu, *pv;
	py = src;
	pu = py + (width * height);
	pv = pu + (width * height) / 4;

  if (dstSize < (width*height*4))
  {
    cout << "YUVtoRGB buffer (" << dstSize << ") too small for " << width << "x" << height << " pixels" << endl;
    return;
  }

	for (h=0; h<height; h++) 
	{
		for (w=0; w<width; w++)
		{
			signed int _r,_g,_b; 
			signed int r, g, b;
			signed int y, u, v;

			y = *py++ - 16;
			u = pu[w>>1] - 128;
			v = pv[w>>1] - 128;

			_r = _R(y,u,v);
			_g = _G(y,u,v);
			_b = _B(y,u,v);

			r = _S(_r);
			g = _S(_g);
			b = _S(_b);

      *dst++ = r;
      *dst++ = g;
      *dst++ = b;
      *dst++ = 0;
		}

		pu += (width>>1);
		pv += (width>>1);
	}
}

void YUV422PtoYUV420P(int width, int height, unsigned char *image)
{
  uchar *srcU = image + (width*height);
  uchar *dstU = image + (width*height);
  uchar *srcV = srcU + (width*height/2);
  uchar *dstV = srcU + (width*height/4);

	for (int h=0; h<height; h+=2) 
	{
    memcpy(dstU, srcU, width/2);
    memcpy(dstV, srcV, width/2);

    dstU += width/2;
    dstV += width/2;
    srcU += width;
    srcV += width;
	}
}


void cropYuvImage(const uchar *yuvBuffer, int ow, int oh, int cx, int cy, int cw, int ch, uchar *dst)
{
    // Only handle even number of cropped lines so we don't have to worry about breaking up 2x2 colour blocks
    if ((cw%2) || (ch%2) || (cx%2) || (cy%2))
    {
        cout << "YUV crop fn does not handle odd sizes; x,y=" << cx << "," << cy << "  w,h=" << cw << "," << ch << endl;
        return;
    }

    int h;
    const unsigned char *srcy, *srcu, *srcv;
    unsigned char *dsty, *dstu, *dstv;
    srcy = yuvBuffer + (ow*cy) + cx; // Skip all cropped lines at top & cropped line to left
    srcu = yuvBuffer + (ow*oh) + (ow*cy/4) + (cx/2);
    srcv = srcu + (ow*oh/4);
    dsty = dst;
    dstu = dsty + (cw*ch);
    dstv = dstu + (cw*ch) / 4;

    for (h=0; h<ch; h++) 
    {
        memcpy(dsty, srcy, cw);
        dsty += cw;
        srcy += ow;
    }

    for (h=0; h<ch/2; h++) 
    {
        memcpy(dstu, srcu, cw/2);
        dstu += (cw/2);
        srcu += (ow/2);
        memcpy(dstv, srcv, cw/2);
        dstv += (cw/2);
        srcv += (ow/2);
    }
}



void scaleYuvImage(const uchar *yuvBuffer, int ow, int oh, int dw, int dh, uchar *dst)
{
  int h;
  uchar *dstu = dst + (dw*dh);
  uchar *dstv = dstu + (dw*dh/4);
  QImage yImage((uchar *)yuvBuffer, ow, oh, 8, (QRgb *)0, 0, QImage::LittleEndian);
  QImage uImage((uchar *)yuvBuffer+(ow*oh), ow/2, oh/2, 8, (QRgb *)0, 0, QImage::LittleEndian);
  QImage vImage((uchar *)yuvBuffer+(ow*oh*5/4), ow/2, oh/2, 8, (QRgb *)0, 0, QImage::LittleEndian);

  // The QImage scaling fn maintains aspect ratio but may end up with a picture thats too big
  QImage ScaledYImage = yImage.scale(dw, dh, QImage::ScaleMax);
  QImage ScaledUImage = uImage.scale(dw/2, dh/2, QImage::ScaleMax);
  QImage ScaledVImage = vImage.scale(dw/2, dh/2, QImage::ScaleMax);

  // This should ensure the scaled image is cropped to size too, for the cases where the aspect ratios did not match
  for (h=0; h<dh; h++)
  {
    memcpy(dst, ScaledYImage.scanLine(h), dw);
    dst += dw;
  }
  for (h=0; h<dh/2; h++)
  {
    memcpy(dstu, ScaledUImage.scanLine(h), dw/2);
    memcpy(dstv, ScaledVImage.scanLine(h), dw/2);
    dstu += (dw/2);
    dstv += (dw/2);
  }
}


void flipYuv420pImage(const uchar *yuvBuffer, int w, int h, uchar *dst)
{
    int h1;

    // Copy Y plane
    const unsigned char *srcy = yuvBuffer + (w*(h-1)); 
    for (h1=0; h1<h; h1++) 
    {
        memcpy(dst, srcy, w);
        dst += w;
        srcy -= w;
    }

    // Copy U&V planes
    const unsigned char *srcu = yuvBuffer + (w*h) + (w*(h-2)/4); 
    const unsigned char *srcv = yuvBuffer + (w*h) + (w*h/4) + (w*(h-2)/4); 
    uchar *dstu = dst;
    uchar *dstv = dst+(w*h/4);
    w /= 2;
    h /= 2;
    for (h1=0; h1<h; h1++) 
    {
        memcpy(dstu, srcu, w);
        dstu += w;
        srcu -= w;
        memcpy(dstv, srcv, w);
        dstv += w;
        srcv -= w;
    }
}


void flipYuv422pImage(const uchar *yuvBuffer, int w, int h, uchar *dst)
{
    int h1;

    // Copy Y plane
    const unsigned char *srcy = yuvBuffer + (w*(h-1)); 
    for (h1=0; h1<h; h1++) 
    {
        memcpy(dst, srcy, w);
        dst += w;
        srcy -= w;
    }

    // Copy U&V planes
    const unsigned char *srcu = yuvBuffer + (w*h) + (w*(h-1)/2); 
    const unsigned char *srcv = yuvBuffer + (w*h) + (w*h/2) + (w*(h-1)/2); 
    uchar *dstu = dst;
    uchar *dstv = dst+(w*h/2);
    w /= 2;
    for (h1=0; h1<h; h1++) 
    {
        memcpy(dstu, srcu, w);
        dstu += w;
        srcu -= w;
        memcpy(dstv, srcv, w);
        dstv += w;
        srcv -= w;
    }
}


void flipRgb32Image(const uchar *rgbBuffer, int w, int h, uchar *dst)
{
    w *= 4;
    const unsigned char *src = rgbBuffer + (w*(h-1)); 
    for (int h1=0; h1<h; h1++) 
    {
        memcpy(dst, src, w);
        dst += w;
        src -= w;
    }
}


void flipRgb24Image(const uchar *rgbBuffer, int w, int h, uchar *dst)
{
    w *= 3;
    const unsigned char *src = rgbBuffer + (w*(h-1)); 
    for (int h1=0; h1<h; h1++) 
    {
        memcpy(dst, src, w);
        dst += w;
        src -= w;
    }
}


