/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef SISCAN_H
#define SISCAN_H

// Qt includes
#include <QRunnable>
#include <QString>
#include <QList>
#include <QPair>
#include <QMap>
#include <QSet>
#include <QMutex>

// MythTV includes
#include "frequencytables.h"
#include "iptvchannelfetcher.h"
#include "streamlisteners.h"
#include "scanmonitor.h"
#include "signalmonitorlistener.h"
#include "dtvconfparserhelpers.h" // for DTVTunerType

class MThread;
class MSqlQuery;

class ChannelBase;
class DTVChannel;
class V4LChannel;
class DVBChannel;
class HDHRChannel;

class SignalMonitor;
class DTVSignalMonitor;
class DVBSignalMonitor;

typedef vector<const ProgramMapTable*>  pmt_vec_t;
typedef QMap<uint, pmt_vec_t>           pmt_map_t;
class ScannedChannelInfo;
typedef QPair<transport_scan_items_it_t, ScannedChannelInfo*> ChannelListItem;
typedef QList<ChannelListItem> ChannelList;

class ChannelScanSM;
class AnalogSignalHandler : public SignalMonitorListener
{
  public:
    AnalogSignalHandler(ChannelScanSM *_siscan) : siscan(_siscan) { }

  public:
    virtual inline void AllGood(void);
    virtual void StatusSignalLock(const SignalMonitorValue&) { }
    virtual void StatusChannelTuned(const SignalMonitorValue&) { }
    virtual void StatusSignalStrength(const SignalMonitorValue&) { }

  private:
    ChannelScanSM *siscan;
};

class ChannelScanSM : public MPEGStreamListener,
                      public ATSCMainStreamListener,
                      public DVBMainStreamListener,
                      public DVBOtherStreamListener,
                      public QRunnable
{
    friend class AnalogSignalHandler;

  public:
    ChannelScanSM(ScanMonitor *_scan_monitor,
                  const QString &_cardtype, ChannelBase* _channel, int _sourceID,
                  uint signal_timeout, uint channel_timeout,
                  const QString &_inputname, bool test_decryption);
    ~ChannelScanSM();

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(
        int src, const QString &std, const QString &mod, const QString &table,
        const QString &table_start = QString::null,
        const QString &table_end   = QString::null);
    bool ScanTransportsStartingOn(
        int sourceid, const QMap<QString,QString> &valueMap);
    bool ScanTransport(uint mplexid, bool follow_nit);
    bool ScanCurrentTransport(const QString &sistandard);
    bool ScanForChannels(
        uint sourceid, const QString &std, const QString &cardtype,
        const DTVChannelList&);
    bool ScanIPTVChannels(uint sourceid, const fbox_chan_map_t &iptv_channels);

    bool ScanExistingTransports(uint sourceid, bool follow_nit);

    void SetAnalog(bool is_analog);
    void SetSourceID(int _SourceID)   { m_sourceID                = _SourceID; }
    void SetSignalTimeout(uint val)    { m_signalTimeout = val; }
    void SetChannelTimeout(uint val)   { m_channelTimeout = val; }
    void SetScanDTVTunerType(DTVTunerType t) { m_scanDTVTunerType = t; }

    uint GetSignalTimeout(void)  const { return m_signalTimeout; }
    uint GetChannelTimeout(void) const { return m_channelTimeout; }

    SignalMonitor    *GetSignalMonitor(void) { return m_signalMonitor; }
    DTVSignalMonitor *GetDTVSignalMonitor(void);
    DVBSignalMonitor *GetDVBSignalMonitor(void);

    typedef QMap<uint,ChannelInsertInfo> chan_info_map_t;
    chan_info_map_t GetChannelList(transport_scan_items_it_t trans_info,
                                   ScannedChannelInfo *scan_info) const;
    uint GetCurrentTransportInfo(QString &chan, QString &chan_tr) const;
    ScanDTVTransportList GetChannelList(void) const;

    // MPEG
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) { }
    void HandlePMT(uint, const ProgramMapTable*);
    void HandleEncryptionStatus(uint pnum, bool encrypted);

    // ATSC Main
    void HandleSTT(const SystemTimeTable*) {}
    void HandleMGT(const MasterGuideTable*);
    void HandleVCT(uint tsid, const VirtualChannelTable*);

    // DVB Main
    void HandleNIT(const NetworkInformationTable*);
    void HandleSDT(uint tsid, const ServiceDescriptionTable*);
    void HandleTDT(const TimeDateTable*) {}

    // DVB Other
    void HandleNITo(const NetworkInformationTable*) {}
    void HandleSDTo(uint tsid, const ServiceDescriptionTable*);
    void HandleBAT(const BouquetAssociationTable*);

  private:
    // some useful gets
    DTVChannel       *GetDTVChannel(void);
    const DTVChannel *GetDTVChannel(void) const;
    V4LChannel       *GetV4LChannel(void);
    HDHRChannel      *GetHDHRChannel(void);
    DVBChannel       *GetDVBChannel(void);
    const DVBChannel *GetDVBChannel(void) const;

    void run(void); // QRunnable

    bool HasTimedOut(void);
    void HandleActiveScan(void);
    bool Tune(const transport_scan_items_it_t &transport);
    void ScanTransport(const transport_scan_items_it_t &transport);
    DTVTunerType GuessDTVTunerType(DTVTunerType) const;

    /// \brief Updates Transport Scan progress bar
    inline void UpdateScanPercentCompleted(void);

    bool CheckImportedList(const DTVChannelInfoList&,
                           uint mpeg_program_num,
                           QString &service_name,
                           QString &callsign,
                           QString &common_status_info);

    void IgnoreDataOnlyMsg( const QString &name, int aux_num);
    void IgnoreEmptyChanMsg(const QString &name, int aux_num);
    void IgnoreAudioOnlyMsg(const QString &name, int aux_num);
    void IgnoreEncryptedMsg(const QString &name, int aux_num);

    bool TestNextProgramEncryption(void);
    void UpdateScanTransports(const NetworkInformationTable *nit);
    bool UpdateChannelInfo(bool wait_until_complete);

    void HandleAllGood(void); // used for analog scanner

    bool AddToList(uint mplexid);

    static QString loc(const ChannelScanSM*);

    static const uint kDVBTableTimeout;
    static const uint kATSCTableTimeout;
    static const uint kMPEGTableTimeout;

  private:
    // Set in constructor
    ScanMonitor      *m_scanMonitor;
    ChannelBase      *m_channel;
    SignalMonitor    *m_signalMonitor;
    int               m_sourceID;
    uint              m_signalTimeout;
    uint              m_channelTimeout;
    uint              m_otherTableTimeout;
    uint              m_otherTableTime;
    bool              m_setOtherTables;
    QString           m_inputName;
    bool              m_testDecryption;
    bool              m_extendScanList;

    // Optional info
    DTVTunerType      m_scanDTVTunerType;

    /// The big lock
    mutable QMutex    m_lock;

    // State
    bool              m_scanning;
    volatile bool     m_threadExit;
    bool              m_waitingForTables;
    QTime             m_timer;

    // Transports List
    int                         m_transportsScanned;
    QSet<uint32_t>              m_tsScanned;
    QMap<uint32_t,DTVMultiplex> m_extendTransports;
    transport_scan_items_t      m_scanTransports;
    transport_scan_items_it_t   m_current;
    transport_scan_items_it_t   m_nextIt;
    bool                        m_currentTestingDecryption;
    QMap<uint, uint>            m_currentEncryptionStatus;
    QMap<uint, bool>            m_currentEncryptionStatusChecked;
    QMap<uint64_t, QString>     m_defAuthorities;

    /// Found Channel Info
    ChannelList       m_channelList;
    uint              m_channelsFound;
    ScannedChannelInfo *m_currentInfo;

    // Analog Info
    AnalogSignalHandler *m_analogSignalHandler;

    /// Scanner thread, runs ChannelScanSM::run()
    MThread          *m_scannerThread;
    QMutex           m_mutex;
};

inline void ChannelScanSM::UpdateScanPercentCompleted(void)
{
    int tmp = (m_transportsScanned * 100) /
              (m_scanTransports.size() + m_extendTransports.size());
    m_scanMonitor->ScanPercentComplete(tmp);
}

void AnalogSignalHandler::AllGood(void)
{
    siscan->HandleAllGood();
}

#endif // SISCAN_H
