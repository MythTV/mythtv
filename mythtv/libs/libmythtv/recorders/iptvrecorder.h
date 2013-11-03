/** -*- Mode: c++ -*-
 *  IPTVRecorder
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_RECORDER_H_
#define _IPTV_RECORDER_H_

// MythTV includes
#include "dtvrecorder.h"
#include "streamlisteners.h"

class IPTVChannel;

class IPTVRecorder : public DTVRecorder
{
  public:
    IPTVRecorder(TVRec*, IPTVChannel*);
    ~IPTVRecorder();

    virtual bool Open(void); // RecorderBase
    virtual void Close(void); // RecorderBase
    bool IsOpen(void) const;
    void StartNewFile(void);

    virtual void SetStreamData(MPEGStreamData*); // DTVRecorder
    virtual bool PauseAndWait(int timeout = 100); // RecorderBase

    virtual void run(void); // QRunnable

  private:
    IPTVChannel *m_channel;
};

#endif // _IPTV_RECORDER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
