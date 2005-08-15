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

class TVRec;
class DVBCam;

class DVBChannel : public QObject, public ChannelBase
{
    Q_OBJECT
  public:
    DVBChannel(int cardnum, TVRec *parent = NULL);
    ~DVBChannel();

    bool Open(void);
    void Close(void);

    // Sets
    bool SetChannelByString(const QString &chan);
    void SetFreqTable(const QString &name);
    void SetCAPMT(const PMTObject *pmt);

    // Gets
    bool IsOpen(void)                   const { return GetFd() >= 0; }
    int  GetFd(void)                    const { return fd_frontend; }

    QString GetDevice(void) const { return QString::number(GetCardNum()); }
    int  GetCardNum(void)               const { return cardnum; };
    const dvb_channel_t& GetCurrentChannel() const
        { return chan_opts; };
    bool GetTuningParams(DVBTuning &tuning) const;
    fe_type_t   GetCardType(void)       const { return info.type; };
    /// Returns table standard ("dvb" or "atsc")
    QString     GetSIStandard(void)     const { return chan_opts.sistandard; }
    /// Returns the dvb service id if the table standard is "dvb"
    uint        GetServiceID()          const { return chan_opts.serviceID; }
    /// Returns true iff SetPMT has been called with a non-NULL value.
    bool        IsPMTSet()              const { return chan_opts.IsPMTSet(); }

    // Commands
    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; }
    bool Tune(const dvb_channel_t& channel, bool force_reset=false);

    // Set/Get/Command just for SIScan/ScanWizardScanner
    void SetMultiplexID(int mplexid)          { currentTID = mplexid; };
    int  GetMultiplexID(void)           const { return currentTID; };
    bool TuneMultiplex(uint mplexid);

    // PID caching
    void SaveCachedPids(const pid_cache_t&) const;
    void GetCachedPids(pid_cache_t&) const;

    // Messages to DVBChannel
  public slots:
    void RecorderStarted();
    void SetPMT(const PMTObject *pmt);

    // Messages from DVBChannel
  signals:
    void ChannelChanged(dvb_channel_t& chan);

  private:
    int  GetCardID() const;
    int  GetChanID() const;
    bool GetTransportOptions(int mplexid);
    bool GetChannelOptions(const QString &channum);

    void CheckOptions();
    bool CheckModulation(fe_modulation_t modulation) const;
    bool CheckCodeRate(fe_code_rate_t rate) const;

    bool TuneTransport(dvb_channel_t&, bool, int);
    bool TuneQPSK(DVBTuning& tuning, bool reset, bool& havetuned);
    bool TuneATSC(DVBTuning& tuning, bool reset, bool& havetuned);
    bool TuneQAM( DVBTuning& tuning, bool reset, bool& havetuned);
    bool TuneOFDM(DVBTuning& tuning, bool reset, bool& havetuned);

  private:
    // Data
    DVBDiSEqC*        diseqc; ///< Used to send commands to external devices
    DVBCam*           dvbcam; ///< Used to decrypt encrypted streams

    dvb_frontend_info info;   ///< Contains info on tuning hardware
    dvb_channel_t     chan_opts;   ///< Tuning options sent to tuning hardware
    DVBTuning         prev_tuning; ///< Last tuning options sent to hardware

    volatile int      fd_frontend; ///< File descriptor for tuning hardware
    int               cardnum;     ///< DVB Card number
    int               currentTID;  ///< Stores mplexid from database

    bool              first_tune;  ///< Used to force hardware reset
};

#endif
