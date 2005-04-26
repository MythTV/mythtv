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
#include "../libavcodec/avcodec.h" // for CodecID

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
                                     CodecID av_codec_id);

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

    virtual float GetDisplayAspect(void) { return 4.0/3; }

    int GetLetterbox(void) { return letterbox; }
    void ToggleLetterbox(int letterboxMode = kLetterbox_Toggle);

    // pass in null to use the pause frame, if it exists.
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              NuppelVideoPlayer *pipPlayer) = 0;

    void ExposeEvent(void) { needrepaint = true; }

    int ChangeBrightness(bool up);
    int ChangeContrast(bool up);
    int ChangeColour(bool up);
    int ChangeHue(bool up);
    int GetCurrentBrightness(void) { return brightness; }
    int GetCurrentContrast(void) { return contrast; }
    int GetCurrentColour(void) { return colour; }
    int GetCurrentHue(void) { return hue; }

    bool AllowPreviewEPG(void) { return allowpreviewepg; }

    virtual bool hasMCAcceleration() const { return false; }
    virtual bool hasIDCTAcceleration() const { return false; }
    virtual bool hasVLDAcceleration() const { return false; }

    void SetFramesPlayed(long long fp) { framesPlayed = fp; };
    long long GetFramesPlayed(void) { return framesPlayed; };

    bool IsErrored() { return errored; }

    // Video Buffer Management
    void SetPrebuffering(bool normal) { vbuffers.SetPrebuffering(normal); }
    void ClearAfterSeek(void) { vbuffers.ClearAfterSeek(); }
    bool WaitForAvailable(uint w) { return vbuffers.WaitForAvailable(w); }

    int ValidVideoFrames(void) { return vbuffers.ValidVideoFrames(); }
    int FreeVideoFrames(void) { return vbuffers.FreeVideoFrames(); }
    bool EnoughFreeFrames(void) { return vbuffers.EnoughFreeFrames(); }
    bool EnoughDecodedFrames(void) { return vbuffers.EnoughDecodedFrames(); }
    bool EnoughPrebufferedFrames(void) { return vbuffers.EnoughPrebufferedFrames(); }
    
    virtual VideoFrame *GetNextFreeFrame(bool with_lock = false,
                                         bool allow_unsafe = false)
        { return vbuffers.GetNextFreeFrame(with_lock, allow_unsafe); }
    virtual void ReleaseFrame(VideoFrame *frame) { vbuffers.ReleaseFrame(frame); }
    virtual void StartDisplayingFrame(void) { vbuffers.StartDisplayingFrame(); }
    virtual void DoneDisplayingFrame(void) { vbuffers.DoneDisplayingFrame(); }
    virtual void DiscardFrame(VideoFrame *frame) { vbuffers.DiscardFrame(frame); }
    virtual void DiscardFrames(void) { vbuffers.DiscardFrames(); }

    VideoFrame *GetLastDecodedFrame(void) { return vbuffers.GetLastDecodedFrame(); }
    VideoFrame *GetLastShownFrame(void)  { return vbuffers.GetLastShownFrame(); }
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

    void CopyFrame(VideoFrame* to, VideoFrame* from);

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
