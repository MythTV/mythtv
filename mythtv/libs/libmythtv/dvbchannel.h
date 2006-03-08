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
class DVBRecorder;

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

    // Gets
    bool IsOpen(void)                   const { return GetFd() >= 0; }
    int  GetFd(void)                    const { return fd_frontend; }

    QString GetDevice(void) const { return QString::number(GetCardNum()); }
    /// Returns DVB device number, used to construct filenames for DVB devices
    int     GetCardNum(void)            const { return cardnum; };
    /// Returns frontend name as reported by driver
    QString GetFrontendName(void)       const { return info.name; }
    const dvb_channel_t& GetCurrentChannel() const
        { return chan_opts; };
    bool GetTuningParams(DVBTuning &tuning) const;
    fe_type_t   GetCardType(void)       const { return info.type; };
    /// Returns table standard ("dvb" or "atsc")
    QString     GetSIStandard(void)     const { return chan_opts.sistandard; }
    /// Returns the dvb service id if the table standard is "dvb"
    uint        GetServiceID(void)      const { return chan_opts.serviceID; }
    /// Returns true iff SetPMT has been called with a non-NULL value.
    bool        IsPMTSet(void)          const { return chan_opts.IsPMTSet(); }
    /// Returns true iff we have a faulty DVB driver that munges PMT
    bool HasCRCBug(void)                const { return has_crc_bug; }

    // Commands
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting);
    bool Tune(const dvb_channel_t& channel, bool force_reset=false);

    // Set/Get/Command just for SIScan/ScanWizardScanner
    void SetMultiplexID(int mplexid)          { currentTID = mplexid; };
    int  GetMultiplexID(void)           const { return currentTID; };
    bool TuneMultiplex(uint mplexid);

    // PID caching
    void SaveCachedPids(const pid_cache_t&) const;
    void GetCachedPids(pid_cache_t&) const;

    void SetRecorder(DVBRecorder*);

    // Messages to DVBChannel
  public slots:
    void SetPMT(uint pid, const ProgramMapTable *pmt);
    void deleteLater(void);

  private:
    int  GetChanID(void) const;
    bool GetTransportOptions(int mplexid);
    bool GetChannelOptions(const QString &channum);

    void CheckOptions();
    bool CheckModulation(fe_modulation_t modulation) const;
    bool CheckCodeRate(fe_code_rate_t rate) const;

  private:
    // Data
    DVBRecorder      *dvb_recorder; ///< Used to send PMT to recorder
    DVBDiSEqC*        diseqc; ///< Used to send commands to external devices
    DVBCam*           dvbcam; ///< Used to decrypt encrypted streams

    dvb_frontend_info info;        ///< Contains info on tuning hardware
    dvb_channel_t     chan_opts;   ///< Tuning options sent to tuning hardware
    DVBTuning         prev_tuning; ///< Last tuning options sent to hardware

    volatile int      fd_frontend; ///< File descriptor for tuning hardware
    int               cardnum;     ///< DVB Card number
    bool              has_crc_bug; ///< true iff our driver munges PMT
    int               currentTID;  ///< Stores mplexid from database

    bool              first_tune;  ///< Used to force hardware reset

    int               nextcapchannel; ///< Signal an input change
};

#endif
