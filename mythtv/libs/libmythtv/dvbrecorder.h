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

    void run(void);

    bool Open(void);
    bool IsOpen(void) const;
    void Close(void);
    void StartNewFile(void);

  private:
    bool PauseAndWait(int timeout = 100);

    QString GetSIStandard(void) const;
    void SetCAMPMT(const ProgramMapTable*);
    void UpdateCAMTimeOffset(void);

  private:
    DVBChannel       *_channel;
    DVBStreamHandler *_stream_handler;
};

#endif // _DVB_RECORDER_H_
