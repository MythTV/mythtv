// -*- Mode: c++ -*-
/*
 *  Copyright (C) John Poet 2013
 *
 *  Copyright notice is in v4l2recorder.cpp of the MythTV project.
 */

#ifndef _V4L2encRecorder_H_
#define _V4L2encRecorder_H_

// MythTV includes
#include "v4l2encstreamhandler.h"
#include "v4lrecorder.h"
#include "dtvrecorder.h"

class RecordingProfile;
class V4LChannel;
class QString;
class TVRec;

/** \class V4L2encRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from V4L2 recorders.
 *
 * Note: make sure the v4l2 program is executable
 *
 *  \sa DTVRecorder
 */
class V4L2encRecorder : public V4LRecorder
{
  public:
    V4L2encRecorder(TVRec *rec, V4LChannel *channel);

    void run(void);

    bool Open(void);
    bool IsOpen(void) const { return m_stream_handler; }
    void Close(void);
    void StartNewFile(void);

    bool PauseAndWait(int timeout = 500);

  protected:
    bool StartEncoding(void);
    bool StopEncoding(void);

    void SetIntOption(RecordingProfile *profile, const QString &name);
    void SetStrOption(RecordingProfile *profile, const QString &name);
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

  private:
    V4LChannel        *m_channel;
    V4L2encStreamHandler *m_stream_handler;
};

#endif // _V4L2enc_RECORDER_H_
