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

using IsOpenMap = QMap<const DVBChannel*,bool>;

class DVBChannel : public DTVChannel
{
  public:
    explicit DVBChannel(QString device, TVRec *parent = nullptr);
    ~DVBChannel() override;

    bool Open(void) override // ChannelBase
        { return Open(this); }
    void Close(void) override // ChannelBase
        { Close(this); }

    bool Init(QString &startchannel, bool setchan) override; // ChannelBase

    // Sets
    void SetPMT(const ProgramMapTable *pmt);
    void SetTimeOffset(double offset);
    void SetSlowTuning(uint how_slow_in_ms)
        { m_tuningDelay = how_slow_in_ms; }

    // Gets
    bool IsOpen(void) const override; // ChannelBase
    int  GetFd(void) const  override // ChannelBase
        { return m_fdFrontend; }
    bool IsTuningParamsProbeSupported(void) const;

    QString GetDevice(void) const override // ChannelBase
        { return m_device; }
    /// Returns DVB device number, used to construct filenames for DVB devices
    QString GetCardNum(void)            const { return m_device; };
    /// Returns frontend name as reported by driver
    QString GetFrontendName(void)       const { return m_frontendName; }
    bool IsMaster(void)                 const override; // DTVChannel
    /// Returns true iff we have a faulty DVB driver that munges PMT
    bool HasCRCBug(void)                const { return m_hasCrcBug; }
    uint GetMinSignalMonitorDelay(void) const { return m_sigMonDelay; }
    /// Returns rotor object if it exists, nullptr otherwise.
    const DiSEqCDevRotor *GetRotor(void) const;

    /// Returns true iff we have a signal carrier lock.
    bool HasLock(bool *ok = nullptr) const;
    /// Returns signal strength in the range [0.0..1.0] (non-calibrated).
    double GetSignalStrength(bool *ok = nullptr) const;
    /// \brief Returns signal/noise in the range [0..1.0].
    /// Some drivers report the actual ratio, while others report
    /// the dB, but in this case some weak signals may report a
    /// very high S/N since negative dB are not supported by
    /// MythTV or the 4.0 version of the DVB API due to the
    /// large number of drivers that ignored the fact that this
    /// was a signed number in the 3.0 API.
    double GetSNR(bool *ok = nullptr) const;
    /// Returns # of corrected bits since last call. First call undefined.
    double GetBitErrorRate(bool *ok = nullptr) const;
    /// Returns # of uncorrected blocks since last call. First call undefined.
    double GetUncorrectedBlockCount(bool *ok = nullptr) const;

    // Commands
    bool SwitchToInput(int newcapchannel, bool setstarting);
    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex &tuning) override; // DTVChannel
    bool Tune(const DTVMultiplex &tuning,
              bool force_reset = false, bool same_input = false);
    bool Retune(void) override; // ChannelBase

    bool ProbeTuningParams(DTVMultiplex &tuning) const;

  private:
    bool Open(DVBChannel *who);
    void Close(DVBChannel *who);

    int  GetChanID(void) const override; // ChannelBase

    void CheckOptions(DTVMultiplex &t) const override; // DTVChannel
    void CheckFrequency(uint64_t frequency) const;
    bool CheckModulation(DTVModulation modulation) const;
    bool CheckCodeRate(DTVCodeRate rate) const;

    DVBChannel *GetMasterLock(void) const;
    static void ReturnMasterLock(DVBChannel* &dvbm);

    /// \brief Get Signal strength from the DVBv5 interface [0-1.0]
    /// It is transformed to a linear relative scale if provided in dB
    /// \param ok   set to true if provided and value exists
    /// \return linear signal strength in [0.0-1.0]
    double GetSignalStrengthDVBv5(bool *ok) const;
    /// \brief Get SNR from the DVBv5 interface [0-1.0]
    /// It is transformed to a linear relative scale if provided in dB
    /// \param ok   set to true if provided and value exists
    /// \return linear SNR in [0.0-1.0]
    double GetSNRDVBv5(bool *ok) const;
    /// \brief Get Bit Error Rate from the DVBv5 interface
    /// \param ok   set to true if provided and value exists
    /// \return bit error rate
    double GetBitErrorRateDVBv5(bool *ok) const;
    /// \brief Get Uncorrected Block Count from the DVBv5 interface
    /// \param ok   set to true if provided and value exists
    /// \return     block count
    double GetUncorrectedBlockCountDVBv5(bool *ok) const;

  private:
    IsOpenMap         m_isOpen;

    // Data
    DiSEqCDevSettings m_diseqcSettings;
    DiSEqCDevTree    *m_diseqcTree          {nullptr};
                      /// Used to decrypt encrypted streams
    DVBCam           *m_dvbCam              {nullptr};

    // Device info
    QString           m_frontendName;
    uint64_t          m_capabilities        {0};
    uint64_t          m_extModulations      {0};
    uint64_t          m_frequencyMinimum    {0};
    uint64_t          m_frequencyMaximum    {0};
    uint              m_symbolRateMinimum   {0};
    uint              m_symbolRateMaximum   {0};

    // Tuning State
    mutable QMutex    m_tuneLock;
    mutable QMutex    m_hwLock              {QMutex::Recursive};
    /// Last tuning options Tune() attempted to send to hardware
    DTVMultiplex      m_desiredTuning;
    /// Last tuning options Tune() succesfully sent to hardware
    DTVMultiplex      m_prevTuning;

    uint              m_lastLnbDevId        {(uint)~0x0};

                      /// Extra delay to add for broken drivers
    uint              m_tuningDelay         {0};
                      /// Minimum delay between FE_LOCK checks
    uint              m_sigMonDelay         {25};
                      /// Used to force hardware reset
    bool              m_firstTune           {true};

    // Other State
                      /// File descriptor for tuning hardware
    int               m_fdFrontend          {-1};
    QString           m_device;                 // DVB Device
    QString           m_key;                    // master lock key
                      /// true iff our driver munges PMT
    bool              m_hasCrcBug           {false};

    static QDateTime  s_lastTuning;
    QMutex            m_tuneDelayLock;
};

#endif
