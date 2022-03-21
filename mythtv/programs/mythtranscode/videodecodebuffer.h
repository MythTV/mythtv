#ifndef VIDEODECODEBUFFER_H
#define VIDEODECODEBUFFER_H

// Qt
#include <QList>
#include <QWaitCondition>
#include <QMutex>
#include <QRunnable>

// MythTV
#include "libmythtv/mythvideoout.h"

class MythTranscodePlayer;
class MythVideoOutput;

class VideoDecodeBuffer : public QRunnable
{
  public:
    VideoDecodeBuffer(MythTranscodePlayer* Player, MythVideoOutput* Videoout,
                      bool Cutlist, int Size = 5);
    ~VideoDecodeBuffer() override;

    void       stop     ();
    void       run      () override;
    MythVideoFrame *GetFrame(int &DidFF, bool &Key);

  private:
    struct DecodedFrameInfo
    {
        MythVideoFrame *frame;
        int         didFF;
        bool        isKey;
    };

    MythTranscodePlayer* const m_player   { nullptr };
    MythVideoOutput* const  m_videoOutput  { nullptr };
    bool const              m_honorCutlist;
    int const               m_maxFrames;
    bool volatile           m_runThread   { true  };
    bool volatile           m_isRunning   { false };
    QMutex mutable          m_queueLock; // Guards the following...
    bool                    m_eof         { false };
    QList<DecodedFrameInfo> m_frameList;
    QWaitCondition          m_frameWaitCond;
};

#endif

