/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBRECORDER_H
#define DVBRECORDER_H

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
    DVBRecorder(DVBChannel* dvbchannel);
   ~DVBRecorder();

    void SetOption(const QString &name, int value);

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);

    void StartRecording(void);
    void Reset(void);

    bool Open(void);
    void Close(void);

    bool RecordsTransportStream(void) const
        { return _record_transport_stream_option; }

    bool RecordsProgramStream(void) const
        { return !_record_transport_stream_option; }

  public slots:
    void SetPMTObject(const PMTObject*);

  private:
    void ReadFromDMX(void);
    static void ProcessDataPS(unsigned char *buffer, int len, void *priv);
    void LocalProcessDataPS(unsigned char *buffer, int len);
    void LocalProcessDataTS(unsigned char *buffer, int len);

    void CloseFilters(void);
    void OpenFilters(uint16_t pid, ES_Type type, dmx_pes_type_t pes_type);
    bool SetDemuxFilters(void);
    void AutoPID(void);

    void SetPAT(ProgramAssociationTable*);
    void SetPMT(ProgramMapTable*);

    void CreatePAT(void);
    void CreatePMT(void);

    void DebugTSHeader(unsigned char* buffer, int len);

    // Options set in SetOption()
    int             _card_number_option;
    bool            _record_transport_stream_option;
    bool            _hw_decoder_option;

    // DVB stuff
    DVBChannel*     dvbchannel;

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
    /// Set when we want to generate a new filter set
    bool            _reset_pid_filters;
    /// Encrypted PID, so we can drop these
    QMap<uint,bool> _encrypted_pid;

    // locking
    QMutex          _pid_read_lock;
    QMutex          _pid_change_lock;

    // Statistics
    uint            _continuity_error_count;
    uint            _stream_overflow_count;
    uint            _bad_packet_count;
    QMap<uint,int>  _continuity_count;

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
