/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBRECORDER_H
#define DVBRECORDER_H

#include <vector>
#include <map>
using namespace std;

#include "recorderbase.h"
#include "transform.h"

#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbsections.h"
#include "dvbcam.h"

class DVBRecorder: public QObject, public RecorderBase
{
    Q_OBJECT
public:
    DVBRecorder(DVBChannel* dvbchannel);
   ~DVBRecorder();

    void SetOption(const QString &name, int value);
    void SetOption(const QString &name, const QString &value)
                                      { RecorderBase::SetOption(name, value); }
    void SetVideoFilters(QString &filters);

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);

    void Initialize(void);
    void StartRecording(void);
    void StopRecording(void);
    void Reset(void);

    void Pause(bool clear = true);
    void Unpause(void);
    bool GetPause(void);
    void WaitForPause(void);

    bool IsRecording(void);
    bool IsErrored(void) { return false; }

    long long GetFramesWritten(void);

    int GetVideoFd(void);

    long long GetKeyframePosition(long long desired);
    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);

public slots:
    void ChannelChanged(dvb_channel_t& chan);

signals:
    void Paused();
    void Unpaused();
    void Started();
    void Stopped();
    void Receiving();

private:
    bool Open();
    void Close();

    void FinishRecording();

    void ReadFromDMX();
    static void ProcessData(unsigned char *buffer, int len, void *priv);
    void LocalProcessData(unsigned char *buffer, int len);

    void CloseFilters();
    void OpenFilters(dvb_pid_t& pid, dmx_pes_type_t type);
    void SetDemuxFilters(dvb_pids_t& pids);
    void CorrectStreamNumber(ipack* ip, int pid);

    static void* QualityMonitorHelper(void*);
    int GetIDForCardNumber(int cardnum);
    void* QualityMonitorThread();
    void QualityMonitorSample(int cardnum, dvb_stats_t& sample);
    void ExpireQualityData();

    bool recording;
    bool encoding;

    bool paused;
    bool was_paused;
    bool mainpaused;
    bool cleartimeonpause;

    long long framesWritten;
    unsigned char prvpkt[3];

    bool wait_for_seqstart;
    bool wait_for_seqstart_enabled;

    bool signal_monitor_quit;

    QMap<long long, long long> positionMap;
    QMap<long long, long long> positionMapDelta;

    bool    dvb_on_demand;
    bool    isopen;
    int     cardnum;
    bool    swfilter;
    bool    swfilter_open;
    bool    recordts;
    bool    channel_changed;
    bool    receiving;
    
    int     dmx_buf_size;
    int     pkt_buf_size;
    unsigned char* pktbuffer;

    unsigned int cont_errors;
    unsigned int stream_overflows;
    unsigned int bad_packets;

    int     signal_monitor_interval;
    int     expire_data_days;

    int fd_dvr;
    vector<int> fd_demux;
    pid_ipack_t pid_ipack;
    map<uint16_t,uint8_t> contcounter;
    QString pid_string;

    dvb_channel_t   chan_opts;

    DVBChannel*     dvbchannel;
};

#endif
