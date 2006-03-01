/**
 *  FirewireRecorderBase
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FIREWIRERECORDERBASE_H_
#define FIREWIRERECORDERBASE_H_

#include "dtvrecorder.h"
#include "mpeg/tspacket.h"
#include <time.h>

#define FIREWIRE_TIMEOUT 15

/** \class FirewireRecorderBase
 *  \brief This is a specialization of DTVRecorder used to
 *         handle DVB and ATSC streams from a firewire input.
 *
 *  \sa DTVRecorder
 */
class FirewireRecorderBase : public DTVRecorder
{
  public:
    FirewireRecorderBase(TVRec *rec, char const* name) :
        DTVRecorder(rec, name),
        lastpacket(0) 
    {;}
        
    void StartRecording(void);
    void ProcessTSPacket(const TSPacket &tspacket);
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    bool PauseAndWait(int timeout = 100);

  public slots:
    void deleteLater(void);

 protected:
    static int read_tspacket (
        unsigned char *tspacket, int len, uint dropped, void *callback_data);

 private:
    virtual void Close() = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool grab_frames() = 0;
    virtual void no_data() = 0;

  private:
    time_t lastpacket;
};

#endif
