// -*- Mode: c++ -*-

#ifndef _OSD_CHROMAKEY_H_
#define _OSD_CHROMAKEY_H_

#include <QImage>
#include "mythpainter_qimage.h"
#include "mythcontext.h"
#include "videoout_xv.h"

class ChromaKeyOSD
{
  public:
    ChromaKeyOSD(VideoOutputXv *vo) :
        current_size(QSize()), current_rect(QRect()),
        videoOutput(vo), img(NULL), image(NULL), painter(NULL), visible(false)
    {
        bzero(&shm_infos, sizeof(XShmSegmentInfo));
    }
    ~ChromaKeyOSD(void);

    bool    ProcessOSD(OSD *osd);
    XImage *GetImage() { return visible ? img : NULL; }
    MythPainter* GetPainter(void) { return (MythPainter*)painter; }

  private:
    bool Init(QSize new_size);
    void TearDown(void);
    bool CreateShmImage(QSize area);
    void DestroyShmImage(void);
    void BlendOrCopy(uint32_t colour, const QRect &rect);

    QSize              current_size;
    QRect              current_rect;
    VideoOutputXv     *videoOutput;
    XImage            *img;
    XShmSegmentInfo    shm_infos;
    QImage            *image;
    MythQImagePainter *painter;
    bool               visible;
};

#endif // _OSD_CHROMAKEY_H_
