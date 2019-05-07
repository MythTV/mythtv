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
        bool cutlist, int size = 5)
        : m_player(player),        m_videoOutput(videoout),
          m_honorCutlist(cutlist), m_maxFrames(size) {}
    virtual ~VideoDecodeBuffer();

    void          stop(void);
    void run() override; // QRunnable
    VideoFrame *GetFrame(int &didFF, bool &isKey);

  private:
    typedef struct decodedFrameInfo
    {
        VideoFrame *frame;
        int         didFF;
        bool        isKey;
    } DecodedFrameInfo;

    MythPlayer * const      m_player      {nullptr};
    VideoOutput * const     m_videoOutput {nullptr};
    bool const              m_honorCutlist;
    int const               m_maxFrames;
    bool volatile           m_runThread   {true};
    bool volatile           m_isRunning   {false};
    QMutex mutable          m_queueLock; // Guards the following...
    bool                    m_eof         {false};
    QList<DecodedFrameInfo> m_frameList;
    QWaitCondition          m_frameWaitCond;
};

#endif
/* vim: set expandtab tabstop=4 shiftwidth=4: */

