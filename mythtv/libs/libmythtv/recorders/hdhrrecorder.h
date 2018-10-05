/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDHOMERUNRECORDER_H_
#define HDHOMERUNRECORDER_H_

// Qt includes
#include <QString>

// MythTV includes
#include "dtvrecorder.h"

class HDHRChannel;
class HDHRStreamHandler;

class HDHRRecorder : public DTVRecorder
{
  public:
    HDHRRecorder(TVRec *rec, HDHRChannel *channel);

    void run(void) override; // RecorderBase

    bool Open(void);
    bool IsOpen(void) const { return _stream_handler; }
    void Close(void);
    void StartNewFile(void) override; // RecorderBase

    QString GetSIStandard(void) const override; // DTVRecorder

  private:
    void ReaderPaused(int fd);
    bool PauseAndWait(int timeout = 100) override; // RecorderBase

  private:
    HDHRChannel       *_channel;
    HDHRStreamHandler *_stream_handler;
};

#endif
