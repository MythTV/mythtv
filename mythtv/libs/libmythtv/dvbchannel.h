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

#include <map>
using namespace std;

#include "mythcontext.h"
#include "mythdbcon.h"
#include "channelbase.h"

#include "dvbtypes.h"
#include "dvbdiseqc.h"
#include "dvbsiparser.h"

class TVRec;
class DVBCam;
class DVBSignalMonitor;

class DVBChannel : public QObject, public ChannelBase
{
    Q_OBJECT
public:
    DVBChannel(int cardnum, TVRec *parent = NULL);
    ~DVBChannel();

    bool Open();
    void Close() {};
    void CloseDVB();

    fe_type_t GetCardType() { return info.type; };
    bool GetTuningParams(DVBTuning &tuning);

    bool SetTransportByInt(int mplexid);
    bool SetChannelByString(const QString &chan);
    void StopTuning();
    void SetFreqTable(const QString &name);
    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
                      { (void)newcapchannel; (void)setstarting; }

    void GetCurrentChannel(dvb_channel_t *& chan)
        { chan = &chan_opts; };

    bool Tune(dvb_channel_t& channel, bool all=false);
    bool TuneTransport(dvb_channel_t& channel, bool all=false, int timeout=30000);
    int  GetCurrentTransportDBID() { return currentTID; };
    void SetCurrentTransportDBID(int _id) { currentTID = _id; };
    int  GetCardNum() { return cardnum; };
    void RecorderStarted();

    void SetCAPMT(PMTObject pmt);

    void StartMonitor();
    void StopMonitor();

    DVBSIParser*        siparser;
    DVBSignalMonitor*   monitor;

public slots:
    void SetPMT(PMTObject NewPMT);

signals:
    void ChannelChanged(dvb_channel_t& chan);
    void Tuning(dvb_channel_t& channel);

private:
    bool GetTransportOptions(int mplexid);

    bool GetChannelOptions(QString channum);

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

    DVBDiSEqC*          diseqc;
    DVBCam*             dvbcam;

    pthread_t           siparser_thread;

    dvb_frontend_info   info;
    dvb_channel_t       chan_opts;
    DVBTuning        prev_tuning;

    int     cardnum;
    volatile int fd_frontend;
    int     currentTID;


    bool    stopTuning;
    bool    force_channel_change;
    bool    first_tune;
};

#endif
