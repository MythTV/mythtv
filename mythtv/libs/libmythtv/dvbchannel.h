/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbchannel.cpp of the MythTV project.
 */

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <qobject.h>
#include <qstring.h>
#include <qsqldatabase.h>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "channelbase.h"

#include "dvbtypes.h"
#include "dvbdiseqc.h"
#include "dvbsiparser.h"

class TVRec;
class DVBCam;

class DVBChannel : public QObject, public ChannelBase
{
    Q_OBJECT
  public:
    DVBChannel(int cardnum, TVRec *parent = NULL);
    ~DVBChannel();

    bool Open();
    void Close() {};
    void CloseDVB();

    // Sets
    void SetCurrentTransportDBID(int _id) { currentTID = _id; };
    bool SetTransportByInt(int mplexid);
    bool SetChannelByString(const QString &chan);
    void SetFreqTable(const QString &name);
    void SetCAPMT(const PMTObject *pmt);

    // Gets
    int  GetFd() const { return fd_frontend; }
    int  GetCardNum() const { return cardnum; };
    int  GetCurrentTransportDBID() const { return currentTID; };
    const dvb_channel_t& GetCurrentChannel() const
        { return chan_opts; };
    bool GetTuningParams(DVBTuning &tuning) const;
    fe_type_t   GetCardType()   const { return info.type; };
    QString     GetSIStandard() const { return chan_opts.sistandard; }
    uint        GetServiceID()  const { return chan_opts.serviceID; }
    inline void SetPMTSet(bool);
    inline bool GetPMTSet();

    // Commands
    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; }
    bool Tune(dvb_channel_t& channel, bool all=false);
    bool TuneTransport(dvb_channel_t& channel, bool all=false, int timeout=30000);
    void StopTuning();

    pthread_t           siparser_thread;
    DVBSIParser*        siparser;

    // Messages to DVBChannel
  public slots:
    void RecorderStarted();
    void SetPMT(const PMTObject *pmt);

    // Messages from DVBChannel
  signals:
    void ChannelChanged(dvb_channel_t& chan);

  private:
    // Helper methods
    bool GetTransportOptions(int mplexid);
    bool GetChannelOptions(const QString &channum);

    void PrintChannelOptions();

    void CheckOptions();
    bool CheckModulation(fe_modulation_t& modulation);
    bool CheckCodeRate(fe_code_rate_t& rate);

    bool ParseTransportQuery(MSqlQuery& query);

    bool TuneQPSK(DVBTuning& tuning, bool reset, bool& havetuned);
    bool TuneQAM(DVBTuning& tuning, bool reset, bool& havetuned);
    bool TuneOFDM(DVBTuning& tuning, bool reset, bool& havetuned);
    bool TuneATSC(DVBTuning& tuning, bool reset, bool& havetuned);

    static void *SpawnSectionReader(void *param);
    static void *SpawnSIParser(void *param);

  private:
    // Data
    DVBDiSEqC*        diseqc;
    DVBCam*           dvbcam;

    dvb_frontend_info info;
    dvb_channel_t     chan_opts;
    DVBTuning         prev_tuning;

    volatile int      fd_frontend;
    int               cardnum;
    int               currentTID;

    bool              stopTuning;
    bool              force_channel_change;
    bool              first_tune;
};

inline void DVBChannel::SetPMTSet(bool pmt_set)
{
    pthread_mutex_lock(&chan_opts.lock);
    chan_opts.PMTSet = pmt_set;
    pthread_mutex_unlock(&chan_opts.lock);
}

inline bool DVBChannel::GetPMTSet()
{
    pthread_mutex_lock(&chan_opts.lock);
    bool pmt_set = chan_opts.PMTSet;
    pthread_mutex_unlock(&chan_opts.lock);
    return pmt_set;
}

#endif
