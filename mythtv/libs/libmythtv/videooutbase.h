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

enum VideoOutputType
{
    kVideoOutput_Default = 0,
    kVideoOutput_XvMC,
    kVideoOutput_VIA
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

    bool GetLetterbox(void) { return letterbox; }
    void ToggleLetterbox(void);

    int ValidVideoFrames(void);
    int FreeVideoFrames(void);

    bool EnoughFreeFrames(void);
    bool EnoughDecodedFrames(void);

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

  protected:
    void ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer);
    void DisplayOSD(VideoFrame *frame, OSD *osd);

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

    bool letterbox;

    VideoFrame *vbuffers;

    QMutex video_buflock;

    QMap<VideoFrame *, int> vbufferMap;
    QPtrQueue<VideoFrame> availableVideoBuffers;
    QPtrQueue<VideoFrame> usedVideoBuffers;

    int rpos;
    int vpos;

    int needfreeframes;
    int needprebufferframes;
};

#endif
