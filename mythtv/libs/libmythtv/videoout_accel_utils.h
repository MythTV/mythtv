// AccelUtils is based on Accellent by John Dagliesh:
//   http://www.defyne.org/dvb/accellent.html

#ifndef ACCEL_UTILS_H_
#define ACCEL_UTILS_H_

#include "frame.h"

struct AccelUtilsData;

// This class manages the support for Apple's counterpart
// to XVMC. It's used by AvFormatDecoder and VideoOutputQuartz,
// and also interacts with libavcodec (see dvdv.h).

class AccelUtils
{
  public:
    AccelUtils();
    ~AccelUtils();
    
    // Initialize the Accel params with the size of video to be decoded.
    void SetVideoSize(int width, int height);

    // Tear down most of Accel without deleting the instance.
    // (We need to be torn down in the same thread that called
    //  SetVideoSize.)
    void Teardown();
    
    // Reset the Accel state.
    void Reset();
    
    // Prepare the Accel code for a call to avcodec_decode_video.
    void PreProcessFrame();

    // Process the macroblocks collected by avcodec_decode_video.
    void PostProcessFrame(VideoFrame *pic, int pict_type, bool gotpicture);

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
    static inline class AccelUtils *singleton() { return m_singleton; };
    
  protected:
    static class AccelUtils *m_singleton;
    struct AccelUtilsData *d;
};

#endif // ACCEL_UTILS_H_

