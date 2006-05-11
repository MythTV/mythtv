// -*- Mode: c++ -*-

#ifndef _OSD_CHROMAKEY_H_
#define _OSD_CHROMAKEY_H_

#include "videoout_xv.h"

class ChromaKeyOSD
{
  public:
    ChromaKeyOSD(VideoOutputXv *vo) :
        videoOutput(vo), current(-1), revision(-1)
    {
        bzero(vf,        2 * sizeof(VideoFrame));
        bzero(img,       2 * sizeof(XImage*));
        bzero(shm_infos, 2 * sizeof(XShmSegmentInfo));
    }

    bool ProcessOSD(OSD *osd);
    void AllocImage(int i);
    void FreeImage(int i);
    void Clear(int i);
    void Reset(void) { current = -1; revision = -1; }

    XImage *GetImage() { return (current < 0) ? NULL : img[current]; }

  private:
    void Reinit(int i);

    VideoOutputXv   *videoOutput;
    int              current;
    int              revision;
    VideoFrame       vf[2];
    XImage          *img[2];
    XShmSegmentInfo  shm_infos[2];
};

#endif // _OSD_CHROMAKEY_H_
