// -*- Mode: c++ -*-
/*
 *  Copyright (C) John Poet 2013
 *
 *  Copyright notice is in ExternalRecorder.cpp of the MythTV project.
 */

#ifndef EXTERNAL_RECORDER_H
#define EXTERNAL_RECORDER_H

// MythTV includes
#include "libmythbase/mythchrono.h"

#include "dtvrecorder.h"

class ExternalStreamHandler;
class RecordingProfile;
class ExternalChannel;
class QString;
class TVRec;

/** \class ExternalRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from External 'blackbox' recorders.
 *
 * Note: make sure the external program is executable
 *
 *  \sa DTVRecorder
 */
class ExternalRecorder : public DTVRecorder
{
  public:
    ExternalRecorder(TVRec *rec, ExternalChannel *channel)
        : DTVRecorder(rec), m_channel(channel) {}

    void run(void) override; // RecorderBase

    bool Open(void);
    bool IsOpen(void) const { return m_streamHandler; }
    void Close(void);
    void StartNewFile(void) override; // RecorderBase

    bool PauseAndWait(std::chrono::milliseconds timeout = 100ms) override; // RecorderBase

  protected:
    bool StartStreaming(void);
    bool StopStreaming(void);

  private:
    ExternalChannel       *m_channel        {nullptr};
    ExternalStreamHandler *m_streamHandler  {nullptr};
};

#endif // EXTERNAL_RECORDER_H
