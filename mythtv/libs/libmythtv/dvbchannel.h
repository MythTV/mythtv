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
#include "channelbase.h"

#include "dvbtypes.h"
#include "dvbdiseqc.h"

class TVRec;
class DVBCam;
class DVBSections;

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

    bool SetChannelByString(const QString &chan);
    bool Tune(dvb_channel_t& channel, bool all=false);

    void SetFreqTable(const QString &name);
    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
                      { (void)newcapchannel; (void)setstarting; }

    void GetCurrentChannel(dvb_channel_t *& chan)
        { chan = &chan_opts; };

    bool ParseQPSK(const QString& frequency, const QString& inversion,
                   const QString& symbol_rate, const QString& fec_inner,
                   const QString& pol, 
                   const QString& diseqc_type, const QString& diseqc_port,
                   const QString& lnb_lof_switch, const QString& lnb_lof_hi,
                   const QString& lnb_lof_lo, dvb_tuning_t& t);

    bool ParseQAM(const QString& frequency, const QString& inversion,
                  const QString& symbol_rate, const QString& fec_inner,
                  const QString& modulation, dvb_tuning_t& t);

    bool ParseOFDM(const QString& frequency, const QString& inversion,
                   const QString& bandwidth, const QString& coderate_hp,
                   const QString& coderate_lp, const QString& constellation,
                   const QString& trans_mode, const QString& guard_interval,
                   const QString& hierarchy, dvb_tuning_t& p);

    void RecorderStarted();
    bool FillFrontendStats(dvb_stats_t &stats);

signals:
    void ChannelChanged(dvb_channel_t& chan);

    void Status(dvb_stats_t &stats);
    void Status(QString val);

    void StatusSignalToNoise(int);
    void StatusSignalStrength(int);
    void StatusBitErrorRate(int);
    void StatusUncorrectedBlocks(int);

protected:
    void connectNotify(const char* signal);
    void disconnectNotify(const char* signal);

private:
    static void* StatusMonitorHelper(void*);
    void StatusMonitorLoop();

    bool GetChannelOptions(QString channum);
    bool GetChannelPids(QSqlDatabase*& db_conn, pthread_mutex_t*& db_lock,
                        int chanid);
    void PrintChannelOptions();

    void CheckOptions();
    bool CheckModulation(fe_modulation_t& modulation);
    bool CheckCodeRate(fe_code_rate_t& rate);

    bool TuneQPSK(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool TuneQAM(dvb_tuning_t& tuning, bool reset, bool& havetuned);
    bool TuneOFDM(dvb_tuning_t& tuning, bool reset, bool& havetuned);

    bool ParseQuery(QSqlQuery& query);

private:
    int cardnum;

    struct dvb_frontend_info info;

    bool force_channel_change;
    bool first_tune;
    dvb_channel_t   chan_opts;
    dvb_tuning_t    prev_tuning;

    int fd_frontend;
    bool isOpen;

    pthread_t statusMonitorThread;
    bool monitorRunning;
    int monitorClients;

    DVBDiSEqC* diseqc;

    DVBCam*      dvbcam;
    DVBSections* dvbsct;
};

#endif
