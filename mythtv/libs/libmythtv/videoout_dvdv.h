// DVDV is based on Accellent by John Dagliesh:
//   http://www.defyne.org/dvb/accellent.html

#ifndef VIDEOOUT_DVDV_H_
#define VIDEOOUT_DVDV_H_

#ifndef HAVE_DVDV
// This eliminates several #ifdefs in avformatdecoder.cpp

class DVDV {
  public:
    DVDV();
    ~DVDV();

    void Reset() {};
    void SetVideoSize(int, int) {};
    bool PreProcessFrame(AVCodecContext *) { return false; };
    void PostProcessFrame(AVCodecContext *, VideoFrame *, int, bool) {};
};

#else // HAVE_DVDV

#include "dvdv.h"
#include "frame.h"

struct DVDV_Private;
struct AVCodecContext;

// This class manages the support for Apple's counterpart
// to XVMC. It's used by AvFormatDecoder and VideoOutputQuartz,
// and also interacts with libavcodec (see dvdv.h).

class DVDV
{
  public:
    DVDV();
    ~DVDV();
    
    // Initialize the Accel params with the size of video to be decoded.
    bool SetVideoSize(int width, int height);

    // Tear down most of Accel without deleting the instance.
    // (We need to be torn down in the same thread that called
    //  SetVideoSize.)
    void Teardown();
    
    // Reset the Accel state.
    void Reset();
    
    // Prepare the Accel code for a call to avcodec_decode_video.
    bool PreProcessFrame(AVCodecContext *context);

    // Process the macroblocks collected by avcodec_decode_video.
    void PostProcessFrame(AVCodecContext *context,
                          VideoFrame *pic, int pict_type, bool gotpicture);

    // Resize and reposition the video subwindow.
    void MoveResize(int imgx, int imgy, int imgw, int imgh,
                    int dispxoff, int dispyoff, int dispwoff, int disphoff);
    
    // Update the OSD display.
    void DrawOSD(unsigned char *y, unsigned char *u, unsigned char *v,
                 unsigned char *alpha);

   // Decode a buffered video frame.
    void DecodeFrame(VideoFrame *pic);
    
    // Draw the most recently decoded video frame to the screen.
    void ShowFrame();
    
    // This is a hack and a perversion of singleton semantics, but
    // otherwise we'd have to pass the instance from:
    //      AvFormatDecoder (instance creator) ->
    //        DecoderBase ->
    //          NuppelVideoPlayer ->
    //            VideoOutput ->
    //              VideoOutputQuartz (caller of ShowFrame and MoveResize)
    static inline class DVDV *singleton() { return m_singleton; };
    
  protected:
    static class DVDV   *m_singleton;
    struct DVDV_Private *d;
};

#endif // HAVE_DVDV
#endif // VIDEOOUT_DVDV_H_
