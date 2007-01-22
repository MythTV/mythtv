// -*- Mode: c++ -*-
/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbchannel.cpp of the MythTV project.
 */

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <qobject.h>
#include <qstring.h>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "dtvchannel.h"
#include "streamlisteners.h"
#include "diseqc.h"

class TVRec;
class DVBCam;
class DVBRecorder;

class DVBChannel : public DTVChannel
{
  public:
    DVBChannel(int cardnum, TVRec *parent = NULL);
    ~DVBChannel();

    bool Open(void);
    void Close(void);

    // Sets
    void SetPMT(const ProgramMapTable*);
    void SetTimeOffset(double offset);
    void SetSlowTuning(uint how_slow_in_ms)
        { tuning_delay = how_slow_in_ms; }

    // Gets
    bool IsOpen(void)                   const { return GetFd() >= 0; }
    int  GetFd(void)                    const { return fd_frontend; }
    bool IsTuningParamsProbeSupported(void) const;

    QString GetDevice(void) const { return QString::number(GetCardNum()); }
    /// Returns DVB device number, used to construct filenames for DVB devices
    int     GetCardNum(void)            const { return cardnum; };
    /// Returns frontend name as reported by driver
    QString GetFrontendName(void)       const;
    DTVTunerType GetCardType(void)      const { return card_type; }
    /// Returns true iff we have a faulty DVB driver that munges PMT
    bool HasCRCBug(void)                const { return has_crc_bug; }
    uint GetMinSignalMonitorDelay(void) const { return sigmon_delay; }
    /// Returns rotor object if it exists, NULL otherwise.
    const DiSEqCDevRotor *GetRotor(void) const;

    // Commands
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting);
    bool SetChannelByString(const QString &chan);
    bool Tune(const DTVMultiplex &tuning, QString inputname)
        { return Tune(tuning, inputname, false, false); }
    bool Tune(const DTVMultiplex &tuning, QString inputname,
              bool force_reset, bool same_input);
    bool TuneMultiplex(uint mplexid, QString inputname);
    bool Retune(void);

    bool ProbeTuningParams(DTVMultiplex &tuning) const;

    // PID caching
    void SaveCachedPids(const pid_cache_t&) const;
    void GetCachedPids(pid_cache_t&) const;

  private:
    int  GetChanID(void) const;

    void CheckOptions(DTVMultiplex &t) const;
    bool CheckModulation(DTVModulation modulation) const;
    bool CheckCodeRate(DTVCodeRate rate) const;

  private:
    // Data
    DiSEqCDev         diseqc_dev;
    DiSEqCDevSettings diseqc_settings;
    DiSEqCDevTree    *diseqc_tree;
    DVBCam           *dvbcam; ///< Used to decrypt encrypted streams

    // Device info
    QString           frontend_name;
    DTVTunerType      card_type;
    uint64_t          capabilities;
    uint64_t          ext_modulations;
    uint64_t          frequency_minimum;
    uint64_t          frequency_maximum;
    uint              symbol_rate_minimum;
    uint              symbol_rate_maximum;

    // Tuning State
    mutable QMutex    tune_lock;
    /// Last tuning options Tune() attempted to send to hardware
    DTVMultiplex      desired_tuning;
    /// Last tuning options Tune() succesfully sent to hardware
    DTVMultiplex      prev_tuning;

    uint              tuning_delay;///< Extra delay to add for broken drivers
    uint              sigmon_delay;///< Minimum delay between FE_LOCK checks
    bool              first_tune;  ///< Used to force hardware reset

    // Other State
    int               fd_frontend; ///< File descriptor for tuning hardware
    int               cardnum;     ///< DVB Card number
    bool              has_crc_bug; ///< true iff our driver munges PMT
    int               nextInputID; ///< Signal an input change
};

#endif
