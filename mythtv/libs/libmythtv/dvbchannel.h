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
#include <qsqldatabase.h>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "channelbase.h"
#include "streamlisteners.h"
#include "diseqc.h"

#ifdef USING_DVB
#include "dvbtypes.h"
#else // if !USING_DVB
typedef int fe_type_t;
typedef int fe_modulation_t;
typedef int fe_code_rate_t;
typedef int DVBTuning;
typedef struct { QString name; fe_type_t type; } dvb_frontend_info;
#endif //!USING_DVB

class TVRec;
class DVBCam;
class DVBRecorder;

class DVBChannel : public ChannelBase
{
  public:
    DVBChannel(int cardnum, TVRec *parent = NULL);
    ~DVBChannel();

    bool Open(void);
    void Close(void);

    // Sets
    void SetPMT(const ProgramMapTable*);
    void SetSlowTuning(uint how_slow_in_ms)
        { tuning_delay = how_slow_in_ms; }

    // Gets
    bool IsOpen(void)                   const { return GetFd() >= 0; }
    int  GetFd(void)                    const { return fd_frontend; }

    QString GetDevice(void) const { return QString::number(GetCardNum()); }
    /// Returns DVB device number, used to construct filenames for DVB devices
    int     GetCardNum(void)            const { return cardnum; };
    /// Returns frontend name as reported by driver
    QString GetFrontendName(void)       const { return info.name; }
    fe_type_t   GetCardType(void)       const { return info.type; };
    /// Returns true iff we have a faulty DVB driver that munges PMT
    bool HasCRCBug(void)                const { return has_crc_bug; }
    uint GetMinSignalMonitorDelay(void) const { return sigmon_delay; }
    /// Returns rotor object if it exists, NULL otherwise.
    const DiSEqCDevRotor *GetRotor(void) const;

    // Commands
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting);
    bool SetChannelByString(const QString &chan);
    bool Tune(const DVBTuning &tuning,
              bool force_reset = false,
              uint sourceid    = 0,
              bool same_input  = false);
    bool TuneMultiplex(uint mplexid, uint sourceid = 0);
    bool Retune(void);

    bool GetTuningParams(DVBTuning &tuning) const;

    // PID caching
    void SaveCachedPids(const pid_cache_t&) const;
    void GetCachedPids(pid_cache_t&) const;

  private:
    int  GetChanID(void) const;
    bool InitChannelParams(DVBTuning &t,
                           uint sourceid, const QString &channum);

    void CheckOptions(DVBTuning &t) const;
    bool CheckModulation(fe_modulation_t modulation) const;
    bool CheckCodeRate(fe_code_rate_t rate) const;

  private:
    // Data
    DiSEqCDev         diseqc_dev;
    DiSEqCDevSettings diseqc_settings;
    DiSEqCDevTree    *diseqc_tree;
    DVBCam           *dvbcam; ///< Used to decrypt encrypted streams

    // Tuning State
    QMutex            tune_lock;
    dvb_frontend_info info;        ///< Contains info on tuning hardware
#ifdef FE_GET_EXTENDED_INFO
    dvb_fe_caps_extended extinfo;  ///< Additional info on tuning hardware
#endif

    /// Last tuning options Tune() attempted to send to hardware
    DVBTuning         desired_tuning;
    /// Last tuning options Tune() succesfully sent to hardware
    DVBTuning         prev_tuning;

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
