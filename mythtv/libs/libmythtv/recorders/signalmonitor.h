// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#ifndef SIGNALMONITOR_H
#define SIGNALMONITOR_H

// C++ headers
#include <vector>
#include <algorithm>

// Qt headers
#include <QWaitCondition>
#include <QMutex>
#include <QCoreApplication>
#include <QRecursiveMutex>

// MythTV headers
#include "libmythbase/mthread.h"
#include "libmythbase/mythtimer.h"

#include "cardutil.h"
#include "channelbase.h"
#include "signalmonitorlistener.h"
#include "signalmonitorvalue.h"

inline QString sm_flags_to_string(uint64_t flags);

class TVRec;

class SignalMonitor : protected MThread
{
    Q_DECLARE_TR_FUNCTIONS(SignalMonitor);

  public:
    /// Returns true iff the card type supports signal monitoring.
    static inline bool IsRequired(const QString &cardtype);
    static inline bool IsSupported(const QString &cardtype);
    static SignalMonitor *Init(const QString& cardtype, int db_cardnum,
                               ChannelBase *channel,
                               bool release_stream);
    ~SignalMonitor() override;

    // Prevent implicit conversion of wrongly ordered arguments
    SignalMonitor(int, ChannelBase *, uint64_t, bool) = delete;

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Control  // // // // // // // // // // // // // // // // // // // //

    virtual void Start();
    virtual void Stop();

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Flags // // // // // // // // // // // // // // // // // // // // //

    virtual void AddFlags(uint64_t _flags);
    virtual void RemoveFlags(uint64_t _flags);
    bool HasFlags(uint64_t _flags)   const;
    bool HasAnyFlag(uint64_t _flags) const;
    uint64_t GetFlags(void)          const { return m_flags; }
    virtual bool HasExtraSlowTuning(void) const { return false; }

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Gets  // // // // // // // // // // // // // // // // // // // // //

    /// \brief Returns whether or not a SIGNAL MythEvent is being sent
    ///        regularly to the frontend.
    bool GetNotifyFrontend() const { return m_notifyFrontend; }
    /// \brief Returns milliseconds between signal monitoring events.
    std::chrono::milliseconds GetUpdateRate() const { return m_updateRate; }
    virtual QStringList GetStatusList(void) const;
    int GetSignalStrength(void) { return m_signalStrength.GetNormalizedValue(0,100); }

    /// \brief Returns true iff scriptStatus.IsGood() and signalLock.IsGood()
    ///        return true
    bool HasSignalLock(void) const
    {
        QMutexLocker locker(&m_statusLock);
        return m_scriptStatus.IsGood() && m_signalLock.IsGood();
    }

    virtual bool IsAllGood(void) const { return HasSignalLock(); }
    bool         IsErrored(void) const { return !m_error.isEmpty(); }

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Sets  // // // // // // // // // // // // // // // // // // // // //

    /** \brief Enables or disables frontend notification of the current
     *         signal value.
     *  \param notify if true SIGNAL MythEvents are sent to the frontend,
     *         otherwise they are not.
     */
    void SetNotifyFrontend(bool notify) { m_notifyFrontend = notify; }

    /** \brief Indicate if table monitoring is needed
     *  \param parent The TVRec* that this signal monitor is attached to.
     *  \param EITscan Unused
     *  \param monitor if true parent->SetupDTVSignalMonitor is called
     *         after the channel is tuned.
     */
    void SetMonitoring(TVRec * parent, bool EITscan, bool monitor)
        { m_pParent = parent; m_eitScan = EITscan, m_tablemon = monitor; }

    /** \brief Sets the number of milliseconds between signal monitoring
     *         attempts in the signal monitoring thread.
     *
     *   Defaults to 25 milliseconds.
     *  \param msec Milliseconds between signal monitoring events.
     */
    void SetUpdateRate(std::chrono::milliseconds msec)
        { m_updateRate = std::max(msec, m_minimumUpdateRate); }

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Listeners   // // // // // // // // // // // // // // // // // // //
    void AddListener(SignalMonitorListener *listener);
    void RemoveListener(SignalMonitorListener *listener);
    void SendMessage(SignalMonitorMessageType type,
                     const SignalMonitorValue &val);
    void SendMessageAllGood(void);
    virtual void EmitStatus(void);

  protected:
    SignalMonitor(int _inputid, ChannelBase *_channel,
                  bool _release_stream, uint64_t wait_for_mask);

    void run(void) override; // MThread

    /// \brief This should be overridden to actually do signal monitoring.
    virtual void UpdateValues(void);

  public:
    /// We've seen a PAT,
    /// which maps MPEG program numbers to pids where we find PMTs
    static const uint64_t kDTVSigMon_PATSeen    = 0x0000000001ULL;
    /// We've seen a PMT,
    /// which maps program to audio, video and other stream PIDs
    static const uint64_t kDTVSigMon_PMTSeen    = 0x0000000002ULL;
    /// We've seen a MGT,
    /// which tells us on which PIDs to find VCT and other ATSC tables
    static const uint64_t kDTVSigMon_MGTSeen    = 0x0000000004ULL;
    /// We've seen a VCT, which maps ATSC Channels to
    /// MPEG program numbers, and provides additional data
    static const uint64_t kDTVSigMon_VCTSeen    = 0x0000000008ULL;
    /// We've seen a TVCT, the terrestrial version of the VCT
    static const uint64_t kDTVSigMon_TVCTSeen   = 0x0000000010ULL;
    /// We've seen a CVCT, the cable version of the VCT
    static const uint64_t kDTVSigMon_CVCTSeen   = 0x0000000020ULL;
    /// We've seen a NIT,
    /// which tells us where to find SDT and other DVB tables
    static const uint64_t kDTVSigMon_NITSeen    = 0x0000000040ULL;
    /// We've seen a SDT, which maps DVB Channels to MPEG
    /// program numbers, and provides additional data
    static const uint64_t kDTVSigMon_SDTSeen    = 0x0000000080ULL;
    /// We've seen the FireWire STB power state
    static const uint64_t kFWSigMon_PowerSeen   = 0x0000000100ULL;
    /// We've seen something indicating whether the data stream is encrypted
    static const uint64_t kDTVSigMon_CryptSeen  = 0x0000000200ULL;

    /// We've seen a PAT matching our requirements
    static const uint64_t kDTVSigMon_PATMatch   = 0x0000001000ULL;
    /// We've seen a PMT matching our requirements
    static const uint64_t kDTVSigMon_PMTMatch   = 0x0000002000ULL;
    /// We've seen an MGT matching our requirements
    static const uint64_t kDTVSigMon_MGTMatch   = 0x0000004000ULL;
    /// We've seen a VCT matching our requirements
    static const uint64_t kDTVSigMon_VCTMatch   = 0x0000008000ULL;
    /// We've seen a TVCT matching our requirements
    static const uint64_t kDTVSigMon_TVCTMatch  = 0x0000010000ULL;
    /// We've seen a CVCT matching our requirements
    static const uint64_t kDTVSigMon_CVCTMatch  = 0x0000020000ULL;
    /// We've seen an NIT matching our requirements
    static const uint64_t kDTVSigMon_NITMatch   = 0x0000040000ULL;
    /// We've seen an SDT matching our requirements
    static const uint64_t kDTVSigMon_SDTMatch   = 0x0000080000ULL;
    /// We've seen a FireWire STB power state matching our requirements
    static const uint64_t kFWSigMon_PowerMatch  = 0x0000100000ULL;
    /// We've seen unencrypted data in data stream
    static const uint64_t kDTVSigMon_CryptMatch = 0x0000200000ULL;

    static const uint64_t kDTVSigMon_WaitForPAT = 0x0001000000ULL;
    static const uint64_t kDTVSigMon_WaitForPMT = 0x0002000000ULL;
    static const uint64_t kDTVSigMon_WaitForMGT = 0x0004000000ULL;
    static const uint64_t kDTVSigMon_WaitForVCT = 0x0008000000ULL;
    static const uint64_t kDTVSigMon_WaitForNIT = 0x0010000000ULL;
    static const uint64_t kDTVSigMon_WaitForSDT = 0x0020000000ULL;
    static const uint64_t kSigMon_WaitForSig    = 0x0040000000ULL;
    static const uint64_t kFWSigMon_WaitForPower= 0x0080000000ULL;
    static const uint64_t kDTVSigMon_WaitForCrypt=0x0100000000ULL;

    static const uint64_t kDTVSigMon_WaitForAll = 0x01FF000000ULL;

    /// Wait for the Signal to Noise Ratio to rise above a threshold
    static const uint64_t kDVBSigMon_WaitForSNR = 0x1000000000ULL;
    /// Wait for the Bit Error Rate to fall below a threshold
    static const uint64_t kDVBSigMon_WaitForBER = 0x2000000000ULL;
    /// Wait for uncorrected FEC blocks to fall below a threshold
    static const uint64_t kDVBSigMon_WaitForUB  = 0x4000000000ULL;
    /// Wait for rotor to complete turning the antenna
    static const uint64_t kDVBSigMon_WaitForPos = 0x8000000000ULL;

  protected:
    ChannelBase       *m_channel             {nullptr};
    TVRec             *m_pParent             {nullptr};
    int                m_inputid;
    volatile uint64_t  m_flags;
    bool               m_releaseStream;
    std::chrono::milliseconds m_updateRate          {25ms};
    std::chrono::milliseconds m_minimumUpdateRate   {5ms};
    bool               m_updateDone          {false};
    bool               m_notifyFrontend      {true};
    bool               m_tablemon            {false};
    bool               m_eitScan             {false};

    // not to be confused with StreamHandler::m_bError.
    QString            m_error;

    SignalMonitorValue m_signalLock;
    SignalMonitorValue m_signalStrength;
    SignalMonitorValue m_scriptStatus;

    std::vector<SignalMonitorListener*> m_listeners;

    QMutex             m_startStopLock;
    QWaitCondition     m_startStopWait;       // protected by startStopLock
    volatile bool      m_running      {false}; // protected by startStopLock
    volatile bool      m_exit         {false}; // protected by startStopLock

    mutable QRecursiveMutex m_statusLock;
    mutable QMutex     m_listenerLock;
};

inline QString sm_flags_to_string(uint64_t flags)
{
    QString str("Seen(");
    if (SignalMonitor::kDTVSigMon_PATSeen    & flags)
        str += "PAT,";
    if (SignalMonitor::kDTVSigMon_PMTSeen    & flags)
        str += "PMT,";
    if (SignalMonitor::kDTVSigMon_MGTSeen    & flags)
        str += "MGT,";
    if (SignalMonitor::kDTVSigMon_VCTSeen    & flags)
        str += "VCT,";
    if (SignalMonitor::kDTVSigMon_TVCTSeen   & flags)
        str += "TVCT,";
    if (SignalMonitor::kDTVSigMon_CVCTSeen   & flags)
        str += "CVCT,";
    if (SignalMonitor::kDTVSigMon_NITSeen    & flags)
        str += "NIT,";
    if (SignalMonitor::kDTVSigMon_SDTSeen    & flags)
        str += "SDT,";
    if (SignalMonitor::kFWSigMon_PowerSeen   & flags)
        str += "STB,";
    if (SignalMonitor::kDTVSigMon_CryptSeen  & flags)
        str += "Crypt,";

    str += ") Match(";
    if (SignalMonitor::kDTVSigMon_PATMatch   & flags)
        str += "PAT,";
    if (SignalMonitor::kDTVSigMon_PMTMatch   & flags)
        str += "PMT,";
    if (SignalMonitor::kDTVSigMon_MGTMatch   & flags)
        str += "MGT,";
    if (SignalMonitor::kDTVSigMon_VCTMatch   & flags)
        str += "VCT,";
    if (SignalMonitor::kDTVSigMon_TVCTMatch  & flags)
        str += "TVCT,";
    if (SignalMonitor::kDTVSigMon_CVCTMatch  & flags)
        str += "CVCT,";
    if (SignalMonitor::kDTVSigMon_NITMatch   & flags)
        str += "NIT,";
    if (SignalMonitor::kDTVSigMon_SDTMatch   & flags)
        str += "SDT,";
    if (SignalMonitor::kFWSigMon_PowerMatch  & flags)
        str += "STB,";
    if (SignalMonitor::kDTVSigMon_CryptMatch & flags)
        str += "Crypt,";

    str += ") Wait(";
    if (SignalMonitor::kDTVSigMon_WaitForPAT & flags)
        str += "PAT,";
    if (SignalMonitor::kDTVSigMon_WaitForPMT & flags)
        str += "PMT,";
    if (SignalMonitor::kDTVSigMon_WaitForMGT & flags)
        str += "MGT,";
    if (SignalMonitor::kDTVSigMon_WaitForVCT & flags)
        str += "VCT,";
    if (SignalMonitor::kDTVSigMon_WaitForNIT & flags)
        str += "NIT,";
    if (SignalMonitor::kDTVSigMon_WaitForSDT & flags)
        str += "SDT,";
    if (SignalMonitor::kSigMon_WaitForSig & flags)
        str += "Sig,";
    if (SignalMonitor::kFWSigMon_WaitForPower& flags)
        str += "STB,";
    if (SignalMonitor::kDTVSigMon_WaitForCrypt & flags)
        str += "Crypt,";

    if (SignalMonitor::kDVBSigMon_WaitForSNR & flags)
        str += "SNR,";
    if (SignalMonitor::kDVBSigMon_WaitForBER & flags)
        str += "BER,";
    if (SignalMonitor::kDVBSigMon_WaitForUB  & flags)
        str += "UB,";
    if (SignalMonitor::kDVBSigMon_WaitForPos & flags)
        str += "Pos,";

    str += ")";
    return str;
}

inline bool SignalMonitor::IsRequired(const QString &cardtype)
{
    return (cardtype != "IMPORT" && cardtype != "DEMO");
}

inline bool SignalMonitor::IsSupported(const QString &cardtype)
{
    return IsRequired(cardtype);
}


#endif // SIGNALMONITOR_H
