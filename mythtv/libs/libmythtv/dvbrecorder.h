// -*- Mode: c++ -*-
/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBRECORDER_H
#define DVBRECORDER_H

// C++ includes
#include <vector>
using namespace std;

// Qt includes
#include <qmutex.h>

#include "dtvrecorder.h"
#include "tspacket.h"
#include "transform.h"
#include "DeviceReadBuffer.h"

#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbsiparser.h"

class ProgramAssociationTable;
class ProgramMapTable;
class TSPacket;

class PIDInfo
{
  public:
    PIDInfo() :
        filter_fd(-1),  continuityCount(0xFF), ip(NULL),
        isVideo(false), isEncrypted(false),    payloadStartSeen(false) {;}

    int    filter_fd;         ///< Input filter file descriptor
    uint   continuityCount;   ///< last Continuity Count (sentinel 0xFF)
    ipack *ip;                ///< TS->PES converter
    bool   isVideo;
    bool   isEncrypted;       ///< true if PID is marked as encrypted
    bool   payloadStartSeen;  ///< true if payload start packet seen on PID

    inline void Close(void);
    inline bool CheckCC(uint cc);
};
typedef QMap<uint,PIDInfo*> PIDInfoMap;

/** \class DVBRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from DVB drivers.
 *
 *  \sa DTVRecorder, HDTVRecorder
 */
class DVBRecorder: public DTVRecorder, private ReaderPausedCB
{
    Q_OBJECT
  public:
    DVBRecorder(TVRec *rec, DVBChannel* dvbchannel);
   ~DVBRecorder();

    void SetOption(const QString &name, int value);

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);

    void StartRecording(void);
    void Reset(void);
    void StopRecording(void);

    bool Open(void);
    bool IsOpen(void) const { return _stream_fd >= 0; }
    void Close(void);

    bool RecordsTransportStream(void) const
        { return _record_transport_stream_option; }

    bool RecordsProgramStream(void) const
        { return !_record_transport_stream_option; }

  public slots:
    void SetPMTObject(const PMTObject*);
    void deleteLater(void);

  private:
    void TeardownAll(void);

    bool Poll(void) const;

    uint ProcessDataTS(unsigned char *buffer, uint len);
    bool ProcessTSPacket(const TSPacket& tspacket);

    void AutoPID(void);
    bool OpenFilters(void);
    void CloseFilters(void);
    void OpenFilter(uint pid, ES_Type type, dmx_pes_type_t pes_type,
                    uint mpeg_stream_type);

    void SetPAT(ProgramAssociationTable*);
    void SetPMT(ProgramMapTable*);

    void CreatePAT(void);
    void CreatePMT(void);
    void WritePATPMT(void);

    void DebugTSHeader(unsigned char* buffer, int len);

    void ReaderPaused(int fd);
    bool PauseAndWait(int timeout = 100);

    // TS->PS Transform
    ipack *CreateIPack(ES_Type type);
    void ProcessDataPS(unsigned char *buffer, uint len);
    static void process_data_ps_cb(unsigned char*,int,void*);

    DeviceReadBuffer *_drb;

  private:
    // Options set in SetOption()
    int             _card_number_option;
    bool            _record_transport_stream_option;
    bool            _hw_decoder_option;

    // DVB stuff
    DVBChannel*     dvbchannel;

    // general recorder stuff
    /// Set when we want to generate a new filter set
    bool            _reset_pid_filters;
    QMutex          _pid_lock;
    PIDInfoMap      _pid_infos;

    // PS recorder stuff
    int             _ps_rec_audio_id;
    int             _ps_rec_video_id;
    unsigned char   _ps_rec_buf[3];

    // TS recorder stuff
    ProgramAssociationTable *_pat;
    ProgramMapTable         *_pmt;
    uint            _next_pmt_version;
    int             _ts_packets_until_psip_sync;

    // Input Misc
    /// PMT on input side
    PMTObject       _input_pmt;

    // Statistics
    mutable uint        _continuity_error_count;
    mutable uint        _stream_overflow_count;
    mutable uint        _bad_packet_count;
    mutable MythTimer   _poll_timer;

    // Constants
    static const int PMT_PID;
    static const int TSPACKETS_BETWEEN_PSIP_SYNC;
    static const int POLL_INTERVAL;
    static const int POLL_WARNING_TIMEOUT;
};

inline void PIDInfo::Close(void)
{
    if (filter_fd >= 0)
        close(filter_fd);

    if (ip)
    {
        free_ipack(ip);
        free(ip);
    }
}

inline bool PIDInfo::CheckCC(uint new_cnt)
{
    if (continuityCount == 0xFF)
    {
        continuityCount = new_cnt;
        return true;
    }

    continuityCount = (continuityCount+1) & 0xf;
    if (continuityCount == new_cnt)
        return true;
    
    continuityCount = new_cnt;
    return false;
}

#endif
