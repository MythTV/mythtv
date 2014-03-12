#ifndef VIDEODECODEBUFFER_H
#define VIDEODECODEBUFFER_H

#include <QList>
#include <QWaitCondition>
#include <QMutex>
#include <QRunnable>

#include "videooutbase.h"

class MythPlayer;
class VideoOutput;

class VideoDecodeBuffer : public QRunnable
{
  public:
    VideoDecodeBuffer(MythPlayer *player, VideoOutput *videoout,
        bool cutlist, int size = 5);
    virtual ~VideoDecodeBuffer();

    void          stop(void);
    virtual void run();
    VideoFrame *GetFrame(int &didFF, bool &isKey);

  private:
    typedef struct decodedFrameInfo
    {
        VideoFrame *frame;
        int         didFF;
        bool        isKey;
    } DecodedFrameInfo;

    MythPlayer * const      m_player;
    VideoOutput * const     m_videoOutput;
    bool const              m_honorCutlist;
    int const               m_maxFrames;
    bool volatile           m_runThread;
    bool volatile           m_isRunning;
    QMutex mutable          m_queueLock; // Guards the following...
    bool                    m_eof;
    QList<DecodedFrameInfo> m_frameList;
    QWaitCondition          m_frameWaitCond;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

