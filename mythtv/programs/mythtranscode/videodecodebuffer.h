#ifndef VIDEODECODEBUFFER_H
#define VIDEODECODEBUFFER_H

#include <QList>
#include <QWaitCondition>
#include <QMutex>
#include <QRunnable>

#include "videooutbase.h"

#include "mythplayer.h"

class VideoDecodeBuffer : public QRunnable
{
  public:
    VideoDecodeBuffer(MythPlayer *player, VideoOutput *videoout,
        bool cutlist, int size = 5);
    ~VideoDecodeBuffer();

    void        stop(void);
    void        run();
    VideoFrame *GetFrame(int &didFF, bool &isKey);

  private:
    typedef struct decodedFrameInfo
    {
        VideoFrame *frame;
        int         didFF;
        bool        isKey;
    } DecodedFrameInfo;

    MythPlayer             *m_player;
    VideoOutput            *m_videoOutput;
    bool                    m_honorCutlist;
    bool                    m_eof;
    int                     m_maxFrames;
    bool                    m_runThread;
    bool                    m_isRunning;
    QMutex                  m_queueLock;
    QList<DecodedFrameInfo> m_frameList;
    QWaitCondition          m_frameWaitCond;
    QMutex                  m_frameWaitLock;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

