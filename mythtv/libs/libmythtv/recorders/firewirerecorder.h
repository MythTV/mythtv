/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _FIREWIRERECORDER_H_
#define _FIREWIRERECORDER_H_

// MythTV headers
#include "dtvrecorder.h"
#include "tspacket.h"
#include "streamlisteners.h"

class TVRec;
class FirewireChannel;

/** \class FirewireRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle DVB and ATSC streams from a firewire input.
 *
 *  \sa DTVRecorder
 */
class FirewireRecorder :
    public DTVRecorder,
    public TSDataListener
{
    friend class MPEGStreamData;
    friend class TSPacketProcessor;

  public:
    FirewireRecorder(TVRec *rec, FirewireChannel *chan);
    virtual ~FirewireRecorder();

    // Commands
    bool Open(void);
    void Close(void);

    void StartStreaming(void);
    void StopStreaming(void);

    void run(void);
    bool PauseAndWait(int timeout = 100);

    // Implements TSDataListener
    void AddData(const unsigned char *data, uint dataSize);

    bool ProcessTSPacket(const TSPacket &tspacket);

    // Sets
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

  protected:
    virtual void InitStreamData(void);
    FirewireRecorder(TVRec *rec);

  private:
    FirewireChannel       *channel;
    bool                   isopen;
    vector<unsigned char>  buffer;
};

#endif //  _FIREWIRERECORDER_H_
