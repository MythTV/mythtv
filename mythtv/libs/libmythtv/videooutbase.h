#ifndef VIDEOOUTBASE_H_
#define VIDEOOUTBASE_H_

#include "frame.h"
extern "C" {
#include "filter.h"
}

#include <qmutex.h>
#include <qmap.h>
#include <qptrqueue.h>

#include <vector>
using namespace std;

class NuppelVideoPlayer;
class OSD;
class OSDSurface;

enum VideoOutputType
{
    kVideoOutput_Default = 0,
    kVideoOutput_XvMC,
    kVideoOutput_VIA,
    kVideoOutput_IVTV
};

class VideoOutput
{
  public:
    static VideoOutput *InitVideoOut(VideoOutputType type);

    VideoOutput();
    virtual ~VideoOutput();

    void InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                     int needprebuffer);

    virtual bool Init(int width, int height, float aspect,
                      unsigned int winid, int winx, int winy, int winw, 
                      int winh, unsigned int embedid = 0);

    virtual void PrepareFrame(VideoFrame *buffer) = 0;
    virtual void Show(void) = 0;

    virtual void InputChanged(int width, int height, float aspect);
    virtual void AspectChanged(float aspect);

    virtual void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    virtual void StopEmbedding(void);

    virtual void MoveResize(void);
 
    virtual void GetDrawSize(int &xoff, int &yoff, int &width, int &height);

    virtual int GetRefreshRate(void) = 0;

    virtual void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    virtual void DrawUnusedRects(void) = 0;

    int GetLetterbox(void) { return letterbox; }
    void ToggleLetterbox(void);

    int ValidVideoFrames(void);
    int FreeVideoFrames(void);

    bool EnoughFreeFrames(void);
    bool EnoughDecodedFrames(void);
    bool EnoughPrebufferedFrames(void);
    
    VideoFrame *GetNextFreeFrame(void);
    void ReleaseFrame(VideoFrame *frame);
    void DiscardFrame(VideoFrame *frame);

    VideoFrame *GetLastDecodedFrame(void);
    VideoFrame *GetLastShownFrame(void);

    void StartDisplayingFrame(void);
    void DoneDisplayingFrame(void);

    virtual void UpdatePauseFrame(void) = 0;
    // pass in null to use the pause frame, if it exists.
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              vector<VideoFilter *> &filterList,
                              NuppelVideoPlayer *pipPlayer) = 0;

    void ClearAfterSeek(void);

    void ExposeEvent(void) { needrepaint = true; }

  protected:
    void InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                     int needprebuffer, int keepprebuffer);
	
    void ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer);
    int DisplayOSD(VideoFrame *frame, OSD *osd, int stride = -1);

    void BlendSurfaceToYV12(OSDSurface *surface, unsigned char *yuvptr,
                            int stride = -1);
    void BlendSurfaceToI44(OSDSurface *surface, unsigned char *yuvptr,
                           bool alphafirst, int stride = -1);   
    void BlendSurfaceToARGB(OSDSurface *surface, unsigned char *argbptr,
                            int stride = -1);
 
    int XJ_width, XJ_height;
    float XJ_aspect;

    int img_xoff, img_yoff;
    float img_hscanf, img_vscanf;

    int imgx, imgy, imgw, imgh;
    int dispxoff, dispyoff, dispwoff, disphoff;

    int dispx, dispy, dispw, disph;
    int olddispx, olddispy, olddispw, olddisph;
    bool embedding;

    int numbuffers;

    int letterbox;

    VideoFrame *vbuffers;

    QMutex video_buflock;

    QMap<VideoFrame *, int> vbufferMap;
    QPtrQueue<VideoFrame> availableVideoBuffers;
    QPtrQueue<VideoFrame> usedVideoBuffers;

    int rpos;
    int vpos;

    int needfreeframes;
    int needprebufferframes;
    int keepprebufferframes;

    bool needrepaint;
};

#endif
