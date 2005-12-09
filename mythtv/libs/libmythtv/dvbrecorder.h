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

#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbsiparser.h"

class ProgramAssociationTable;
class ProgramMapTable;

/** \class DVBRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from DVB drivers.
 *
 *  \sa DTVRecorder, HDTVRecorder
 */
class DVBRecorder: public DTVRecorder
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
    void ReadFromDMX(void);
    static void ProcessDataPS(unsigned char *buffer, int len, void *priv);
    void LocalProcessDataPS(unsigned char *buffer, int len);

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

    // PS recorder stuff
    int             _ps_rec_audio_id;
    int             _ps_rec_video_id;
    unsigned char   _ps_rec_buf[3];
    pid_ipack_t     _ps_rec_pid_ipack;

    // TS recorder stuff
    ProgramAssociationTable *_pat;
    ProgramMapTable         *_pmt;
    uint            _next_pmt_version;
    uint            _ts_packets_until_psip_sync;
    QMap<uint,bool> _payload_start_seen;
    QMap<uint,bool> _videoPID;

    // Input Misc
    /// PMT on input side
    PMTObject       _input_pmt;
    /// Input filter file descriptors
    vector<int>     _pid_filters;
    /// Input polling structure for _stream_fd
    struct pollfd   _polls;
    /// Encrypted PID, so we can drop these
    QMap<uint,bool> _encrypted_pid;

    // Statistics
    mutable uint        _continuity_error_count;
    mutable uint        _stream_overflow_count;
    mutable uint        _bad_packet_count;
    mutable QMap<uint,int> _continuity_count;

    // For debugging
    bool data_found; ///< debugging variable used by transform.c
    bool keyframe_found;

    // Constants
    static const int PMT_PID;
    static const int TSPACKETS_BETWEEN_PSIP_SYNC;
    static const int POLL_INTERVAL;
    static const int POLL_WARNING_TIMEOUT;
};

#endif
