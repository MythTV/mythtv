// -*- Mode: c++ -*-
/*
 *  Copyright (C) Kenneth Aafloy 2003
 *
 *  Copyright notice is in dvbchannel.cpp of the MythTV project.
 */

#ifndef DVBCHANNEL_H
#define DVBCHANNEL_H

#include <QObject>
#include <QString>
#include <QMap>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "dtvchannel.h"
#include "dtvconfparserhelpers.h" // for DTVTunerType
#include "streamlisteners.h"
#include "diseqc.h"

class TVRec;
class DVBCam;
class DVBRecorder;
class DVBChannel;

typedef QMap<const DVBChannel*,bool> IsOpenMap;

class DVBChannel : public DTVChannel
{
  public:
    DVBChannel(const QString &device, TVRec *parent = NULL);
    ~DVBChannel();

    bool Open(void) { return Open(this); }
    void Close(void) { Close(this); }

    bool Init(QString &inputname, QString &startchannel, bool setchan);

    // Sets
    void SetPMT(const ProgramMapTable*);
    void SetTimeOffset(double offset);
    void SetSlowTuning(uint how_slow_in_ms)
        { tuning_delay = how_slow_in_ms; }

    // Gets
    bool IsOpen(void) const;
    int  GetFd(void)                    const { return fd_frontend; }
    bool IsTuningParamsProbeSupported(void) const;

    QString GetDevice(void)             const { return device; }
    /// Returns DVB device number, used to construct filenames for DVB devices
    QString GetCardNum(void)            const { return device; };
    /// Returns frontend name as reported by driver
    QString GetFrontendName(void)       const;
    bool IsMaster(void)                 const;
    /// Returns true iff we have a faulty DVB driver that munges PMT
    bool HasCRCBug(void)                const { return has_crc_bug; }
    uint GetMinSignalMonitorDelay(void) const { return sigmon_delay; }
    /// Returns rotor object if it exists, NULL otherwise.
    const DiSEqCDevRotor *GetRotor(void) const;

    /// Returns true iff we have a signal carrier lock.
    bool HasLock(bool *ok = NULL) const;
    /// Returns signal strength in the range [0.0..1.0] (non-calibrated).
    double GetSignalStrength(bool *ok = NULL) const;
    /// \brief Returns signal/noise in the range [0..1.0].
    /// Some drivers report the actual ratio, while others report
    /// the dB, but in this case some weak signals may report a
    /// very high S/N since negative dB are not supported by
    /// MythTV or the 4.0 version of the DVB API due to the
    /// large number of drivers that ignored the fact that this
    /// was a signed number in the 3.0 API.
    double GetSNR(bool *ok = NULL) const;
    /// Returns # of corrected bits since last call. First call undefined.
    double GetBitErrorRate(bool *ok = NULL) const;
    /// Returns # of uncorrected blocks since last call. First call undefined.
    double GetUncorrectedBlockCount(bool *ok = NULL) const;

    // Commands
    using DTVChannel::SwitchToInput;
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting);
    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex &tuning, QString inputname);
    bool Tune(const DTVMultiplex &tuning, uint inputid,
              bool force_reset = false, bool same_input = false);
    bool Retune(void);

    bool ProbeTuningParams(DTVMultiplex &tuning) const;

  private:
    bool Open(DVBChannel*);
    void Close(DVBChannel*);

    int  GetChanID(void) const;

    void CheckOptions(DTVMultiplex &t) const;
    void CheckFrequency(uint64_t frequency) const;
    bool CheckModulation(DTVModulation modulation) const;
    bool CheckCodeRate(DTVCodeRate rate) const;

    typedef DVBChannel* DVBChannelP;
    DVBChannel *GetMasterLock(void);
    static void ReturnMasterLock(DVBChannelP &dvbm);

    typedef const DVBChannel* DVBChannelCP;
    const DVBChannel *GetMasterLock(void) const;
    static void ReturnMasterLock(DVBChannelCP &dvbm);

  private:
    IsOpenMap         is_open;

    // Data
    DiSEqCDev         diseqc_dev;
    DiSEqCDevSettings diseqc_settings;
    DiSEqCDevTree    *diseqc_tree;
    DVBCam           *dvbcam; ///< Used to decrypt encrypted streams

    // Device info
    QString           frontend_name;
    uint64_t          capabilities;
    uint64_t          ext_modulations;
    uint64_t          frequency_minimum;
    uint64_t          frequency_maximum;
    uint              symbol_rate_minimum;
    uint              symbol_rate_maximum;

    // Tuning State
    mutable QMutex    tune_lock;
    mutable QMutex    hw_lock;
    /// Last tuning options Tune() attempted to send to hardware
    DTVMultiplex      desired_tuning;
    /// Last tuning options Tune() succesfully sent to hardware
    DTVMultiplex      prev_tuning;

    uint              last_lnb_dev_id;

    uint              tuning_delay;///< Extra delay to add for broken drivers
    uint              sigmon_delay;///< Minimum delay between FE_LOCK checks
    bool              first_tune;  ///< Used to force hardware reset

    // Other State
    int               fd_frontend; ///< File descriptor for tuning hardware
    QString           device;      ///< DVB Device
    bool              has_crc_bug; ///< true iff our driver munges PMT
};

#endif
