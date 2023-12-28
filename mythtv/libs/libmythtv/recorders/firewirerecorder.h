/**
 *  FirewireRecorder
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FIREWIRERECORDER_H
#define FIREWIRERECORDER_H

// MythTV headers
#include "dtvrecorder.h"
#include "mpeg/streamlisteners.h"
#include "mpeg/tspacket.h"

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
    ~FirewireRecorder() override;

    // Commands
    bool Open(void);
    void Close(void);

    void StartStreaming(void);
    void StopStreaming(void);

    void run(void) override; // RecorderBase
    bool PauseAndWait(std::chrono::milliseconds timeout = 100ms) override; // RecorderBase

    // Implements TSDataListener
    void AddData(const unsigned char *data, uint len) override; // TSDataListener

    bool ProcessTSPacket(const TSPacket &tspacket) override; // DTVRecorder

    // Sets
    void SetOptionsFromProfile([[maybe_unused]] RecordingProfile *profile,
                               [[maybe_unused]] const QString &videodev,
                               [[maybe_unused]] const QString &audiodev,
                               [[maybe_unused]] const QString &vbidev) override {}; // DTVRecorder

  protected:
    void InitStreamData(void) override; // DTVRecorder
    explicit FirewireRecorder(TVRec *rec);

  private:
    FirewireChannel       *m_channel {nullptr};
    bool                   m_isopen  {false};
    std::vector<unsigned char>  m_buffer;
};

#endif // FIREWIRERECORDER_H
