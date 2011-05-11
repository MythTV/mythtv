// -*- Mode: c++ -*-
/**
 *  IPTVRecorder
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_RECORDER_H_
#define _IPTV_RECORDER_H_

#include <QWaitCondition>

#include "dtvrecorder.h"
#include "streamlisteners.h"

class QString;
class IPTVChannel;

/** \brief Processes data from a IPTVFeeder and writes it to disk.
 */
class IPTVRecorder : public DTVRecorder, public TSDataListener,
                     public MPEGSingleProgramStreamListener
{
    friend class IPTVMediaSink;

  public:
    IPTVRecorder(TVRec *rec, IPTVChannel *channel);
    ~IPTVRecorder();

    bool Open(void);
    void Close(void);

    virtual void StartRecording(void);
    virtual void StopRecording(void);

    virtual void SetOptionsFromProfile(RecordingProfile*, const QString&,
                                       const QString&, const QString&) {}

    virtual void SetStreamData(void);

  private:
    void ProcessTSPacket(const TSPacket& tspacket);
    virtual bool PauseAndWait(int timeout = 100);

    // implements TSDataListener
    void AddData(const unsigned char *data, unsigned int dataSize);

    // implements MPEGSingleProgramStreamListener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

  private:
    IPTVChannel *_channel;

  private:
    IPTVRecorder &operator=(const IPTVRecorder&); //< avoid default impl
    IPTVRecorder(const IPTVRecorder&);            //< avoid default impl
    IPTVRecorder();                                  //< avoid default impl
};

#endif // _IPTV_RECORDER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
