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
    CetonRecorder(TVRec *rec, CetonChannel *channel)
        : DTVRecorder(rec), m_channel(channel) {}

    void run(void) override; // RecorderBase

    bool Open(void);
    void Close(void);
    bool IsOpen(void) const { return m_streamHandler; }

  protected:
    void StartNewFile(void) override; // RecorderBase
    QString GetSIStandard(void) const override; // DTVRecorder
    bool PauseAndWait(std::chrono::milliseconds timeout = 100ms) override; // RecorderBase

  private:
    void ReaderPaused(int fd);

  private:
    CetonChannel       *m_channel        {nullptr};
    CetonStreamHandler *m_streamHandler  {nullptr};
};

#endif
