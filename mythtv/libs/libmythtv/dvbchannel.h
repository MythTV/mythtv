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
    friend class ScanWizardScanner; // needs access to siparser
    friend class SIScan;            // needs access to siparser
    Q_OBJECT
  public:
    DVBChannel(int cardnum, TVRec *parent = NULL);
    ~DVBChannel();

    bool Open();
    void Close() {};
    void CloseDVB();

    // Sets
    bool SetChannelByString(const QString &chan);
    void SetFreqTable(const QString &name);
    void SetCAPMT(const PMTObject *pmt);

    // Gets
    int  GetFd()                   const { return fd_frontend; }

    QString GetDevice(void) const { return QString::number(GetCardNum()); }
    int  GetCardNum()              const { return cardnum; };
    const dvb_channel_t& GetCurrentChannel() const
        { return chan_opts; };
    bool GetTuningParams(DVBTuning &tuning) const;
    fe_type_t   GetCardType()      const { return info.type; };
    /// Returns table standard ("dvb" or "atsc")
    QString     GetSIStandard()    const { return chan_opts.sistandard; }

    // Commands
    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; }
    bool Tune(dvb_channel_t& channel, bool all=false);
    bool TuneTransport(dvb_channel_t& channel, bool all=false, int timeout=30000);
    void StopTuning();

    // Set/Get/Command just for SIScan/ScanWizardScanner
    void SetMultiplexID(int mplexid) { currentTID = mplexid; };
    int  GetMultiplexID() const { return currentTID; };
    bool TuneMultiplex(int mplexid);

    // PID caching
    void SaveCachedPids(const pid_cache_t&) const;
    void GetCachedPids(pid_cache_t&) const;

    // Old stuff
    /// @deprecated Use SetMultiplexID(int)
    void SetCurrentTransportDBID(int _id) { SetMultiplexID(_id); }
    /// @deprecated Use TuneMultiplex(int)
    bool SetTransportByInt(int mplexid)   { return TuneMultiplex(mplexid); }
    /// @deprecated Use GetMultiplexID()
    int  GetCurrentTransportDBID()  const { return GetMultiplexID(); }
    /// @deprecated Use GetProgramNumber()
    uint GetServiceID()             const { return GetProgramNumber(); }

    // Messages to DVBChannel
  public slots:
    void RecorderStarted();
    void SetPMT(const PMTObject *pmt);

    // Messages from DVBChannel
  signals:
    void ChannelChanged(dvb_channel_t& chan);

  private:
    // Helper methods
    void SetCachedATSCInfo(const QString &chan);

    int  GetChanID() const;
    bool GetTransportOptions(int mplexid);
    bool GetChannelOptions(const QString &channum);

    void CheckOptions();
    bool CheckModulation(fe_modulation_t modulation) const;
    bool CheckCodeRate(fe_code_rate_t rate) const;

    static void *SpawnSectionReader(void *param);

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

    bool              force_channel_change; ///< Used to force hardware reset
    bool              stopTuning;
    pthread_t         siparser_thread;
    DVBSIParser*      siparser;
};

#endif
