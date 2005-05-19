#ifndef VIDEOOUTBASE_H_
#define VIDEOOUTBASE_H_

extern "C" {
#include "frame.h"
#include "filter.h"
}

#include <qframe.h>
#include <qptrqueue.h>
#include <qptrlist.h>
#include "videobuffers.h"

using namespace std;

class NuppelVideoPlayer;
class OSD;
class OSDSurface;
class FilterChain;
class FilterManager;

extern "C" {
struct ImgReSampleContext;
}

enum VideoOutputType
{
    kVideoOutput_Default = 0,
    kVideoOutput_VIA,
    kVideoOutput_IVTV,
    kVideoOutput_Directfb
};

enum MythCodecID
{
// if you add anything to this list please update
// myth2av_codecid in videoout_xv.cpp
    kCodec_NONE = 0,

    kCodec_MPEG1,
    kCodec_MPEG2,
    kCodec_H263,
    kCodec_MPEG4,
    
    kCodec_NORMAL_END,

    kCodec_MPEG1_XVMC,
    kCodec_MPEG2_XVMC,
    kCodec_H263_XVMC,
    kCodec_MPEG4_XVMC,

    kCodec_MPEG1_IDCT,
    kCodec_MPEG2_IDCT,
    kCodec_H263_IDCT,
    kCodec_MPEG4_IDCT,

    kCodec_STD_XVMC_END,

    kCodec_MPEG1_VLD,
    kCodec_MPEG2_VLD,
    kCodec_H263_VLD,
    kCodec_MPEG4_VLD,

    kCodec_SPECIAL_END,
};

enum PictureAttribute
{
    kPictureAttribute_None = 0,
    kPictureAttribute_MIN = 1,
    kPictureAttribute_Brightness = 1,
    kPictureAttribute_Contrast,
    kPictureAttribute_Colour,
    kPictureAttribute_Hue,
    kPictureAttribute_Volume,
    kPictureAttribute_MAX
};

enum PIPLocations
{
    kPIPTopLeft = 0,
    kPIPBottomLeft,
    kPIPTopRight,
    kPIPBottomRight
};

enum ZoomDirections
{
    kZoomHome = 0,
    kZoomIn,
    kZoomOut,
    kZoomUp,
    kZoomDown,
    kZoomLeft,
    kZoomRight
};

enum letterboxModes
{
    kLetterbox_Toggle = -1,
    kLetterbox_Off = 0,
    kLetterbox_4_3,
    kLetterbox_16_9,
    kLetterbox_4_3_Zoom,
    kLetterbox_16_9_Zoom,
    kLetterbox_16_9_Stretch,
    kLetterbox_Fill,
    kLetterbox_END
};

enum FrameScanType {
    kScan_Ignore      = -1,
    kScan_Progressive =  0,
    kScan_Interlaced  =  1,
    kScan_Intr2ndField=  2,
    kScan_Detect      =  3
};

class VideoOutput
{
  public:
    static VideoOutput *InitVideoOut(VideoOutputType type,
                                     MythCodecID av_codec_id);

    VideoOutput();
    virtual ~VideoOutput();

    virtual bool Init(int width, int height, float aspect,
                      WId winid, int winx, int winy, int winw, 
                      int winh, WId embedid = 0);

    virtual bool SetupDeinterlace(bool i);
    virtual bool NeedsDoubleFramerate(void) const;
    virtual bool ApproveDeintFilter(const QString& filtername) const;

    virtual void PrepareFrame(VideoFrame *buffer, FrameScanType) = 0;
    virtual void Show(FrameScanType) = 0;

    virtual void InputChanged(int width, int height, float aspect);
    virtual void AspectChanged(float aspect);

    virtual void EmbedInWidget(WId wid, int x, int y, int w, int h);
    virtual void StopEmbedding(void);

    virtual void MoveResize(void);
    virtual void Zoom(int direction);
 
    virtual void GetDrawSize(int &xoff, int &yoff, int &width, int &height);
    virtual void GetVisibleSize(int &xoff, int &yoff, int &width, int &height);

    virtual int GetRefreshRate(void) = 0;

    virtual void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    virtual void DrawUnusedRects(bool sync = true) = 0;

    /// \brief Returns current display aspect ratio.
    virtual float GetDisplayAspect(void) { return 4.0/3; }

    /// \brief Returns current letterboxing mode
    /// \sa ToggleLetterbox(int)
    int GetLetterbox(void) { return letterbox; }
    void ToggleLetterbox(int letterboxMode = kLetterbox_Toggle);

    // pass in null to use the pause frame, if it exists.
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              NuppelVideoPlayer *pipPlayer) = 0;

    /// \brief Tells video output that a full repaint is needed.
    void ExposeEvent(void) { needrepaint = true; }

    int ChangeBrightness(bool up);
    int ChangeContrast(bool up);
    int ChangeColour(bool up);
    int ChangeHue(bool up);

    /// \brief Returns current playback brightness
    int GetCurrentBrightness(void) { return brightness; }
    /// \brief Returns current playback contrast
    int GetCurrentContrast(void) { return contrast; }
    /// \brief Returns current playback colour
    int GetCurrentColour(void) { return colour; }
    /// \brief Returns current playback hue
    int GetCurrentHue(void) { return hue; }

    bool AllowPreviewEPG(void) { return allowpreviewepg; }

    /// \brief Returns true iff Motion Compensation acceleration is available.
    virtual bool hasMCAcceleration() const { return false; }
    /// \brief Returns true iff Inverse Discrete Cosine Transform acceleration
    ///        is available.
    virtual bool hasIDCTAcceleration() const { return false; }
    /// \brief Returns true iff VLD acceleration is available.
    virtual bool hasVLDAcceleration() const { return false; }

    /// \brief Sets the number of frames played
    void SetFramesPlayed(long long fp) { framesPlayed = fp; };
    /// \brief Returns the number of frames played
    long long GetFramesPlayed(void) { return framesPlayed; };

    /// \brief Returns true if a fatal error has been encountered.
    bool IsErrored() { return errored; }

    // Video Buffer Management
    /// \brief Sets whether to use a normal number of buffers or fewer buffers.
    void SetPrebuffering(bool normal) { vbuffers.SetPrebuffering(normal); }
    /// \brief Tells video output to toss decoded buffers due to a seek
    void ClearAfterSeek(void) { vbuffers.ClearAfterSeek(); }
    /// \brief Blocks until a frame is available for decoding onto.
    bool WaitForAvailable(uint w) { return vbuffers.WaitForAvailable(w); }

    /// \brief Returns number of frames that are fully decoded.
    int ValidVideoFrames(void) { return vbuffers.ValidVideoFrames(); }
    /// \brief Returns number of frames available for decoding onto.
    int FreeVideoFrames(void) { return vbuffers.FreeVideoFrames(); }
    /// \brief Returns true iff enough frames are available to decode onto.
    bool EnoughFreeFrames(void) { return vbuffers.EnoughFreeFrames(); }
    /// \brief Returns true iff there are plenty of decoded frames ready
    ///        for display.
    bool EnoughDecodedFrames(void) { return vbuffers.EnoughDecodedFrames(); }
    /// \brief Returns true iff we have at least the minimum number of
    ///        decoded frames ready for display.
    bool EnoughPrebufferedFrames(void) { return vbuffers.EnoughPrebufferedFrames(); }
    
    /**
     * \brief Blocks until it is possible to return a frame for decoding onto.
     * \param with_lock if true frames are properly locked, but this means you
     *        must unlock them when you are done, so this is disabled by default.
     * \param allow_unsafe if true then that are queued for display can be
     *       returned as frames to decode onto, this defaults to false.
     */
    virtual VideoFrame *GetNextFreeFrame(bool with_lock = false,
                                         bool allow_unsafe = false)
        { return vbuffers.GetNextFreeFrame(with_lock, allow_unsafe); }
    /// \brief Releases a frame from the ready for decoding queue onto the
    ///        queue of frames ready for display.
    virtual void ReleaseFrame(VideoFrame *frame) { vbuffers.ReleaseFrame(frame); }
    /// \brief Tell GetLastShownFrame() to return the next frame from the head
    ///        of the queue of frames to display.
    virtual void StartDisplayingFrame(void) { vbuffers.StartDisplayingFrame(); }
    /// \brief Releases frame returned from GetLastShownFrame() onto the 
    ///        queue of frames ready for decoding onto.
    virtual void DoneDisplayingFrame(void) { vbuffers.DoneDisplayingFrame(); }
    /// \brief Releases frame from any queue onto the 
    ///        queue of frames ready for decoding onto.
    virtual void DiscardFrame(VideoFrame *frame) { vbuffers.DiscardFrame(frame); }
    /// \brief Releases all frames not being actively displayed from any queue
    ///        onto the queue of frames ready for decoding onto.
    virtual void DiscardFrames(void) { vbuffers.DiscardFrames(); }

    /// \bug not implemented correctly. vpos is not updated.
    VideoFrame *GetLastDecodedFrame(void) { return vbuffers.GetLastDecodedFrame(); }

    /// \brief Returns frame from the head of the ready to be displayed queue,
    ///        if StartDisplayingFrame has been called.
    VideoFrame *GetLastShownFrame(void)  { return vbuffers.GetLastShownFrame(); }
    /// \brief Updates frame displayed when video is paused.
    virtual void UpdatePauseFrame(void) = 0;

  protected:
    void InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                     int needprebuffer_normal, int needprebuffer_small,
                     int keepprebuffer);

    virtual void ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer);
    int DisplayOSD(VideoFrame *frame, OSD *osd, int stride = -1, int revision = -1);

    void BlendSurfaceToYV12(OSDSurface *surface, unsigned char *yuvptr,
                            int stride = -1);
    void BlendSurfaceToI44(OSDSurface *surface, unsigned char *yuvptr,
                           bool alphafirst, int stride = -1);   
    void BlendSurfaceToARGB(OSDSurface *surface, unsigned char *argbptr,
                            int stride = -1);
 
    virtual int ChangePictureAttribute(int attributeType, int newValue);

    static void CopyFrame(VideoFrame* to, const VideoFrame* from);

    void DoPipResize(int pipwidth, int pipheight);
    void ShutdownPipResize(void);

    void AspectSet(float aspect);

    int XJ_width, XJ_height;
    float XJ_aspect;
    float videoAspect;

    int w_mm;
    int h_mm;

    int myth_dsw;
    int myth_dsh;

    int img_xoff, img_yoff;
    float img_hscanf, img_vscanf;

    int imgx, imgy, imgw, imgh;
    int dispxoff, dispyoff, dispwoff, disphoff;

    int dispx, dispy, dispw, disph;
    int olddispx, olddispy, olddispw, olddisph;
    bool embedding;

    int brightness, contrast, colour, hue;

    int letterbox;

    int PIPLocation;

    int ZoomedIn;
    int ZoomedUp;
    int ZoomedRight;

    VideoBuffers vbuffers;

    bool needrepaint;

    int desired_piph;
    int desired_pipw;

    int piph_in, piph_out;
    int pipw_in, pipw_out;
    unsigned char *piptmpbuf;

    ImgReSampleContext *pipscontext;

    bool allowpreviewepg;

    bool m_deinterlacing;
    QString m_deintfiltername;
    FilterManager *m_deintFiltMan;
    FilterChain *m_deintFilter;

    bool m_deinterlaceBeforeOSD;;

    long long framesPlayed;

    bool errored;
};

#endif
