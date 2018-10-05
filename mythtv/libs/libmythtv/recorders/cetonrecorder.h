/** -*- Mode: c++ -*-
 *  CetonRecorder
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONRECORDER_H_
#define CETONRECORDER_H_

// Qt includes
#include <QString>

// MythTV includes
#include "dtvrecorder.h"

class CetonChannel;
class CetonStreamHandler;

class CetonRecorder : public DTVRecorder
{
  public:
    CetonRecorder(TVRec *rec, CetonChannel *channel);

    void run(void) override; // RecorderBase

    bool Open(void);
    void Close(void);
    void StartNewFile(void) override; // RecorderBase

    bool IsOpen(void) const { return _stream_handler; }

    QString GetSIStandard(void) const override; // DTVRecorder

  private:
    void ReaderPaused(int fd);
    bool PauseAndWait(int timeout = 100) override; // RecorderBase

  private:
    CetonChannel       *_channel;
    CetonStreamHandler *_stream_handler;
};

#endif
