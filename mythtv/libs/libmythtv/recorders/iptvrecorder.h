/** -*- Mode: c++ -*-
 *  IPTVRecorder
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef IPTV_RECORDER_H
#define IPTV_RECORDER_H

// MythTV includes
#include "dtvrecorder.h"
#include "mpeg/streamlisteners.h"

class IPTVChannel;

class IPTVRecorder : public DTVRecorder
{
  public:
    IPTVRecorder(TVRec *rec, IPTVChannel *channel);
    ~IPTVRecorder() override;

    bool Open(void);
    void Close(void);
    bool IsOpen(void) const;
    void StartNewFile(void) override; // RecorderBase

    void SetStreamData(MPEGStreamData *data) override; // DTVRecorder
    bool PauseAndWait(std::chrono::milliseconds timeout = 100ms) override; // RecorderBase

    void run(void) override; // RecorderBase

  private:
    IPTVChannel *m_channel {nullptr};
};

#endif // IPTV_RECORDER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
