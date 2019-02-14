// -*- Mode: c++ -*-
#ifndef _DVB_RECORDER_H_
#define _DVB_RECORDER_H_

// Qt includes
#include <QString>

// MythTV includes
#include "dtvrecorder.h"

class DVBStreamHandler;
class ProgramMapTable;
class DVBChannel;

/** \class DVBRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from DVB drivers.
 *
 *  \sa DTVRecorder
 */
class DVBRecorder : public DTVRecorder
{
  public:
    DVBRecorder(TVRec*, DVBChannel*);

    void run(void) override; // RecorderBase

    bool Open(void);
    bool IsOpen(void) const;
    void Close(void);
    void StartNewFile(void) override; // RecorderBase

  private:
    bool PauseAndWait(int timeout = 100) override; // RecorderBase

    QString GetSIStandard(void) const override; // DTVRecorder
    void SetCAMPMT(const ProgramMapTable*) override; // DTVRecorder
    void UpdateCAMTimeOffset(void) override; // DTVRecorder

  private:
    DVBChannel       *m_channel        {nullptr};
    DVBStreamHandler *m_stream_handler {nullptr};
};

#endif // _DVB_RECORDER_H_
