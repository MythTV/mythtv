#ifndef VIDEOOUTBASE_H_
#define VIDEOOUTBASE_H_

#include "frame.h"

class VideoOutput
{
  public:
    VideoOutput();
    virtual ~VideoOutput();

    virtual bool Init(int width, int height, float aspect, int num_buffers,
                      VideoFrame *out_buffers, unsigned int winid,
                      int winx, int winy, int winw, int winh,
                      unsigned int embedid = 0);

    virtual void PrepareFrame(VideoFrame *buffer) = 0;
    virtual void Show(void) = 0;

    virtual void InputChanged(int width, int height, float aspect);

    virtual void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    virtual void StopEmbedding(void);

    virtual void MoveResize(void);
 
    virtual void GetDrawSize(int &xoff, int &yoff, int &width, int &height);

    virtual int GetRefreshRate(void) = 0;

  protected:
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
    VideoFrame *videoframes;
};

#endif
