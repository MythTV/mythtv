// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#ifndef SIGNALMONITOR_H
#define SIGNALMONITOR_H

// C headers
#include <pthread.h>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <qobject.h>
#include <qmutex.h>

// MythTV headers
#include "signalmonitorvalue.h"
#include "channelbase.h"
#include "cardutil.h"

#define DBG_SM(FUNC, MSG) VERBOSE(VB_CHANNEL, \
    "SM("<<channel->GetDevice()<<")::"<<FUNC<<": "<<MSG);

enum {
    kDTVSigMon_PATSeen    = 0x00000001, ///< maps program numbers to PMT pids
    kDTVSigMon_PMTSeen    = 0x00000002, ///< maps to audio, video and other streams pids
    kDTVSigMon_MGTSeen    = 0x00000004, ///< like NIT, but there is only one.
    kDTVSigMon_VCTSeen    = 0x00000008, ///< like SDT
    kDTVSigMon_TVCTSeen   = 0x00000010, ///< terrestrial version of VCT
    kDTVSigMon_CVCTSeen   = 0x00000020, ///< cable version of VCT
    kDTVSigMon_NITSeen    = 0x00000040, ///< like MGT, but there can be multiple tables.
    kDTVSigMon_SDTSeen    = 0x00000080, ///< like VCT

    kDTVSigMon_PATMatch   = 0x00000100,
    kDTVSigMon_PMTMatch   = 0x00000200,
    kDTVSigMon_MGTMatch   = 0x00000400,
    kDTVSigMon_VCTMatch   = 0x00000800,
    kDTVSigMon_TVCTMatch  = 0x00001000,
    kDTVSigMon_CVCTMatch  = 0x00002000,
    kDTVSigMon_NITMatch   = 0x00004000,
    kDTVSigMon_SDTMatch   = 0x00008000,

    kDTVSigMon_WaitForPAT = 0x00010000,
    kDTVSigMon_WaitForPMT = 0x00020000,
    kDTVSigMon_WaitForMGT = 0x00040000,
    kDTVSigMon_WaitForVCT = 0x00080000,
    kDTVSigMon_WaitForNIT = 0x00100000,
    kDTVSigMon_WaitForSDT = 0x00200000,
    kDTVSigMon_WaitForSig = 0x00400000,

    kDTVSigMon_WaitForAll = 0x00FF0000,

    kDVBSigMon_WaitForSNR = 0x01000000,
    kDVBSigMon_WaitForBER = 0x02000000,
    kDVBSigMon_WaitForUB  = 0x04000000,
};

inline QString sm_flags_to_string(uint);

class SignalMonitor: virtual public QObject
{
    Q_OBJECT
  public:
    /// Returns true iff the card type supports signal monitoring.
    static inline bool IsSupported(QString cardtype);
    static SignalMonitor *Init(QString cardtype, int db_cardnum,
                               ChannelBase *channel);
    virtual ~SignalMonitor();

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Control  // // // // // // // // // // // // // // // // // // // //

    virtual void Start();
    virtual void Stop();
    virtual void Kick();
    virtual bool WaitForLock(int timeout = -1);

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Flags // // // // // // // // // // // // // // // // // // // // //

    virtual void AddFlags(uint _flags)
    {
        DBG_SM("AddFlags", sm_flags_to_string(_flags));
        flags |= _flags;
    }
    virtual void RemoveFlags(uint _flags)
    {
        DBG_SM("RemoveFlags", sm_flags_to_string(_flags));
        flags &= ~_flags;
    }
    bool HasFlags(uint _flags) const   { return (flags & _flags) == _flags; }
    bool HasAnyFlag(uint _flags) const { return (flags & _flags); }
    uint GetFlags(void) const          { return flags; }

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Gets  // // // // // // // // // // // // // // // // // // // // //

    /// \brief Returns whether or not a SIGNAL MythEvent is being sent
    ///        regularly to the frontend.
    bool GetNotifyFrontend() { return notify_frontend; }
    /// \brief Returns milliseconds between signal monitoring events.
    int GetUpdateRate() { return update_rate; }
    virtual QStringList GetStatusList(bool kick = true);

    /// \brief Returns true iff signalLock.IsGood() returns true
    bool HasSignalLock(void) const
    {
        QMutexLocker locker(&statusLock);
        return signalLock.IsGood();        
    }

    virtual bool IsAllGood(void) const { return HasSignalLock(); }

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Sets  // // // // // // // // // // // // // // // // // // // // //

    /** \brief Enables or disables frontend notification of the current
     *         signal value.
     *  \param notify if true SIGNAL MythEvents are sent to the frontend,
     *         otherwise they are not.
     */
    void SetNotifyFrontend(bool notify) { notify_frontend = notify; }

    /** \brief Sets the number of milliseconds between signal monitoring
     *         attempts in the signal monitoring thread.
     *
     *   Defaults to 25 milliseconds.
     *  \param msec Milliseconds between signal monitoring events.
     */
    void SetUpdateRate(int msec)
        { update_rate = max(msec, (int)minimum_update_rate); }

  public slots:
    virtual void deleteLater(void);

  signals:
    /** \brief Signal to be sent as true when it is safe to begin
     *   or continue recording, and false if it may not be safe.
     *
     *   Note: Signals are only sent once the monitoring thread has been started.
     */
    void StatusSignalLock(const SignalMonitorValue&);

    /** \brief Signal to be sent with an actual signal value.
     *
     *   Note: Signals are only sent once the monitoring thread has been started.
     */
    void StatusSignalStrength(const SignalMonitorValue&);

    /** \brief Signal to be sent when you have a lock on all values.
     *
     *   Note: Signals are only sent once the monitoring thread has been started.
     */
    void AllGood(void);
  protected:
    SignalMonitor(int db_cardnum, ChannelBase *_channel,
                  uint wait_for_mask, const char *name = "SignalMonitor");
    
    static void* SpawnMonitorLoop(void*);
    virtual void MonitorLoop();

    /// \brief This should be overriden to actually do signal monitoring.
    virtual void UpdateValues() { ; }

    pthread_t    monitor_thread;
    ChannelBase *channel;
    int          capturecardnum;
    uint         flags;
    int          update_rate;
    uint         minimum_update_rate;
    bool         running;
    bool         exit;
    bool         update_done;
    bool         notify_frontend;

    SignalMonitorValue signalLock;
    SignalMonitorValue signalStrength;

    QMutex             startStopLock;
    mutable QMutex     statusLock;
};

inline QString sm_flags_to_string(uint flags)
{
    QString str("Seen(");
    if (kDTVSigMon_PATSeen    & flags)
        str += "PAT,";
    if (kDTVSigMon_PMTSeen    & flags)
        str += "PMT,";
    if (kDTVSigMon_MGTSeen    & flags)
        str += "MGT,";
    if (kDTVSigMon_VCTSeen    & flags)
        str += "VCT,";
    if (kDTVSigMon_TVCTSeen   & flags)
        str += "TVCT,";
    if (kDTVSigMon_CVCTSeen   & flags)
        str += "CVCT,";
    if (kDTVSigMon_NITSeen    & flags)
        str += "NIT,";
    if (kDTVSigMon_SDTSeen    & flags)
        str += "SDT,";

    str += ") Match(";
    if (kDTVSigMon_PATMatch   & flags)
        str += "PAT,";
    if (kDTVSigMon_PMTMatch   & flags)
        str += "PMT,";
    if (kDTVSigMon_MGTMatch   & flags)
        str += "MGT,";
    if (kDTVSigMon_VCTMatch   & flags)
        str += "VCT,";
    if (kDTVSigMon_TVCTMatch  & flags)
        str += "TVCT,";
    if (kDTVSigMon_CVCTMatch  & flags)
        str += "CVCT,";
    if (kDTVSigMon_NITMatch   & flags)
        str += "NIT,";
    if (kDTVSigMon_SDTMatch   & flags)
        str += "SDT,";

    str += ") Wait(";
    if (kDTVSigMon_WaitForPAT & flags)
        str += "PAT,";
    if (kDTVSigMon_WaitForPMT & flags)
        str += "PMT,";
    if (kDTVSigMon_WaitForMGT & flags)
        str += "MGT,";
    if (kDTVSigMon_WaitForVCT & flags)
        str += "VCT,";
    if (kDTVSigMon_WaitForNIT & flags)
        str += "NIT,";
    if (kDTVSigMon_WaitForSDT & flags)
        str += "SDT,";
    if (kDTVSigMon_WaitForSig & flags)
        str += "Sig,";

    if (kDVBSigMon_WaitForSNR & flags)
        str += "SNR,";
    if (kDVBSigMon_WaitForBER & flags)
        str += "BER,";
    if (kDVBSigMon_WaitForUB  & flags)
        str += "UB,";
    str += ")";
    return str;
}

inline bool SignalMonitor::IsSupported(QString cardtype)
{
    if (CardUtil::IsDVBCardType(cardtype))
        return true;
    if (cardtype.upper() == "HDTV")
        return true;

    return false;
}


#endif // SIGNALMONITOR_H
