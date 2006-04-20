/**
 *  DBOX2Recorder
 *  Copyright (c) 2005 by Levent Gündogdu
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DBOX2RECORDER_H_
#define DBOX2RECORDER_H_

// POSIX headers
#include <time.h>

// MythTV headers
#include "dtvrecorder.h"

class DBox2Channel;
class QHttp;

typedef struct stream_meta_
{
    int            socket;
    int            bufferIndex;
    unsigned char *buffer;
} stream_meta;

class DBox2Recorder;
class DBox2Relay : public QObject
{
    Q_OBJECT

  public:
    DBox2Relay(DBox2Recorder *rec) : m_rec(rec) {}
    void SetRecorder(DBox2Recorder*);

  public slots:
    void httpRequestFinished(int id, bool error);

  private:
    DBox2Recorder *m_rec;
    QMutex         m_lock;
};

class DBox2Recorder : public DTVRecorder
{
    friend class DBox2Relay;

  public:
    DBox2Recorder(TVRec *rec, DBox2Channel *channel);
    ~DBox2Recorder() { TeardownAll(); }

    // Sets
    void SetOption(const QString &name, const QString &value);
    void SetOption(const QString &name, int value);
    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    // Commands
    void StartRecording(void);
    bool Open(void); 
    void ChannelChanged(void);
    void ChannelChanging(void);

  private:
    // Methods
    void httpRequestFinished(int id, bool error);
    void TeardownAll(void);
    void CreatePAT(unsigned char *ts_packet);
    int  getPMTSectionID(unsigned char *buffer, int pmtPID);
    void updatePMTSectionID(unsigned char *buffer, int pmtPID);
    int  processStream(stream_meta *stream);
    void initStream(stream_meta *meta);
    int  OpenStream(void);
    bool RequestStream(void);
    bool RequestInfo(void);
    int  findTSHeader(unsigned char *buffer, int len);
    void Close(void);
    void ProcessTSPacket(unsigned char *tspacket, int len);

  private:
    // Members for creating/handling PAT and PMT
    int             m_cardid;
    unsigned char  *m_patPacket;
    int             pat_cc;
    int             pkts_until_pat;
    int             m_pidPAT;
    vector<int>     m_pids;
    int             m_pmtPID;
    int             m_ac3PID;
    int             m_sectionID;
    DBox2Channel   *m_channel;

    // Connection relevant members
    int             port;
    int             httpPort;
    QString         ip;
    bool            isOpen;
    QHttp          *http;
    DBox2Relay     *m_relay;
    int             m_lastPIDRequestID;
    int             m_lastInfoRequestID;
    time_t          lastpacket;
    int             bufferSize;
    stream_meta     transportStream;
    int             m_videoWidth;
    int             m_videoHeight;
    QString         m_videoFormat;
    bool            _request_abort;
};

#endif
