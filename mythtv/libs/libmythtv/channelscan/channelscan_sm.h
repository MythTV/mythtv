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
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QPair>
#include <QRunnable>
#include <QSet>
#include <QString>

// MythTV includes
#include "dtvconfparserhelpers.h" // for DTVTunerType
#include "frequencytables.h"
#include "iptvchannelfetcher.h"
#include "mpeg/streamlisteners.h"
#include "scanmonitor.h"
#include "signalmonitorlistener.h"

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

using pmt_vec_t = std::vector<const ProgramMapTable*>;
using pmt_map_t = QMap<uint, pmt_vec_t>;
class ScannedChannelInfo;
using ChannelListItem = QPair<transport_scan_items_it_t, ScannedChannelInfo*>;
using ChannelList = QList<ChannelListItem>;

class ChannelScanSM;
class AnalogSignalHandler : public SignalMonitorListener
{
  public:
    explicit AnalogSignalHandler(ChannelScanSM *_siscan) : m_siScan(_siscan) { }

  public:
    inline void AllGood(void) override; // SignalMonitorListener
    void StatusSignalLock(const SignalMonitorValue &/*val*/) override { } // SignalMonitorListener
    void StatusChannelTuned(const SignalMonitorValue &/*val*/) override { } // SignalMonitorListener
    void StatusSignalStrength(const SignalMonitorValue &/*val*/) override { } // SignalMonitorListener

  private:
    ChannelScanSM *m_siScan;
};

class ChannelScanSM : public MPEGStreamListener,
                      public ATSCMainStreamListener,
                      public DVBMainStreamListener,
                      public DVBOtherStreamListener,
                      public QRunnable
{
    friend class AnalogSignalHandler;

  public:
    ChannelScanSM(ScanMonitor *scan_monitor,
                  const QString &cardtype, ChannelBase* channel, int sourceID,
                  std::chrono::milliseconds signal_timeout, std::chrono::milliseconds channel_timeout,
                  QString inputname, bool test_decryption);
    ~ChannelScanSM() override;

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(
        int SourceID, const QString &std, const QString &mod, const QString &country,
        const QString &table_start = QString(),
        const QString &table_end   = QString());
    bool ScanTransportsStartingOn(
        int sourceid, const QMap<QString,QString> &startChan);
    bool ScanTransport(uint mplexid, bool follow_nit);
    bool ScanCurrentTransport(const QString &sistandard);
    bool ScanForChannels(
        uint sourceid, const QString &std, const QString &cardtype,
        const DTVChannelList &channels);
    bool ScanIPTVChannels(uint sourceid, const fbox_chan_map_t &iptv_channels);

    bool ScanExistingTransports(uint sourceid, bool follow_nit);

    void SetAnalog(bool is_analog);
    void SetSourceID(int SourceID)     { m_sourceID = SourceID; }
    void SetSignalTimeout(std::chrono::milliseconds val)    { m_signalTimeout = val; }
    void SetChannelTimeout(std::chrono::milliseconds val)   { m_channelTimeout = val; }
    void SetScanDTVTunerType(DTVTunerType t) { m_scanDTVTunerType = t; }
    void SetScanDTVTunerType(int t) { m_scanDTVTunerType = DTVTunerType(t); }

    std::chrono::milliseconds GetSignalTimeout(void)  const { return m_signalTimeout; }
    std::chrono::milliseconds GetChannelTimeout(void) const { return m_channelTimeout; }

    SignalMonitor    *GetSignalMonitor(void) { return m_signalMonitor; }
    DTVSignalMonitor *GetDTVSignalMonitor(void);
    DVBSignalMonitor *GetDVBSignalMonitor(void);

    using chan_info_map_t = QMap<uint,ChannelInsertInfo>;
    chan_info_map_t GetChannelList(transport_scan_items_it_t trans_info,
                                   ScannedChannelInfo *scan_info) const;
    uint GetCurrentTransportInfo(QString &chan, QString &chan_tr) const;
    ScanDTVTransportList GetChannelList(bool addFullTS) const;

    // MPEG
    void HandlePAT(const ProgramAssociationTable *pat) override; // MPEGStreamListener
    void HandleCAT(const ConditionalAccessTable *cat) override; // MPEGStreamListener
    void HandlePMT(uint program_num, const ProgramMapTable *pmt) override; // MPEGStreamListener
    void HandleEncryptionStatus(uint pnum, bool encrypted) override; // MPEGStreamListener

    // ATSC Main
    void HandleSTT(const SystemTimeTable */*stt*/) override {} // ATSCMainStreamListener
    void HandleMGT(const MasterGuideTable *mgt) override; // ATSCMainStreamListener
    void HandleVCT(uint tsid, const VirtualChannelTable *vct) override; // ATSCMainStreamListener

    // DVB Main
    void HandleNIT(const NetworkInformationTable *nit) override; // DVBMainStreamListener
    void HandleSDT(uint tsid, const ServiceDescriptionTable *sdt) override; // DVBMainStreamListener
    void HandleTDT(const TimeDateTable */*tdt*/) override {} // DVBMainStreamListener

    // DVB Other
    void HandleNITo(const NetworkInformationTable */*nit*/) override {} // DVBOtherStreamListener
    void HandleSDTo(uint tsid, const ServiceDescriptionTable *sdt) override; // DVBOtherStreamListener
    void HandleBAT(const BouquetAssociationTable *bat) override; // DVBOtherStreamListener

  private:
    // Gets
    DTVChannel       *GetDTVChannel(void);
    const DTVChannel *GetDTVChannel(void) const;
    V4LChannel       *GetV4LChannel(void);
    HDHRChannel      *GetHDHRChannel(void);
    DVBChannel       *GetDVBChannel(void);
    const DVBChannel *GetDVBChannel(void) const;

    void run(void) override; // QRunnable

    bool HasTimedOut(void);
    void HandleActiveScan(void);
    bool Tune(transport_scan_items_it_t transport);
    void ScanTransport(transport_scan_items_it_t transport);
    DTVTunerType GuessDTVTunerType(DTVTunerType type) const;
    static void LogLines(const QString& string);

    // Updates Transport Scan progress bar
    inline void UpdateScanPercentCompleted(void);

    bool CheckImportedList(const DTVChannelInfoList &channels,
                           uint mpeg_program_num,
                           QString &service_name,
                           QString &callsign,
                           QString &common_status_info);

    void IgnoreDataOnlyMsg( const QString &name, int aux_num);
    void IgnoreEmptyChanMsg(const QString &name, int aux_num);
    void IgnoreAudioOnlyMsg(const QString &name, int aux_num);
    void IgnoreEncryptedMsg(const QString &name, int aux_num);

    bool TestNextProgramEncryption(void);
    void UpdateScanTransports(uint frequency, const NetworkInformationTable *nit);
    bool UpdateChannelInfo(bool wait_until_complete);

    void HandleAllGood(void); // used for analog scanner

    bool AddToList(uint mplexid);

    static QString loc(const ChannelScanSM *siscan);

    static const std::chrono::milliseconds kDVBTableTimeout;
    static const std::chrono::milliseconds kATSCTableTimeout;
    static const std::chrono::milliseconds kMPEGTableTimeout;

  private:
    // Set in constructor
    ScanMonitor      *m_scanMonitor;
    ChannelBase      *m_channel;
    SignalMonitor    *m_signalMonitor;
    int               m_sourceID;
    std::chrono::milliseconds m_signalTimeout;
    std::chrono::milliseconds m_channelTimeout;
    std::chrono::milliseconds m_otherTableTimeout {0ms};
    std::chrono::milliseconds m_otherTableTime    {0ms};
    bool              m_setOtherTables    {false};
    QString           m_inputName;
    bool              m_testDecryption;
    bool              m_extendScanList    {false};

    // Scanning parameters
    uint              m_frequency         {0};
    uint              m_bouquetId         {0};
    uint              m_regionId          {0};
    uint              m_nitId             {0};
    uint              m_lcnOffset         {0};

    // Optional info
    DTVTunerType      m_scanDTVTunerType  {DTVTunerType::kTunerTypeUnknown};

    /// The big lock
    mutable QMutex    m_lock;

    // State
    bool              m_scanning          {false};
    volatile bool     m_threadExit        {false};
    bool              m_waitingForTables  {false};
    QElapsedTimer     m_timer;

    // Transports List
    int                         m_transportsScanned {0};
    QSet<uint32_t>              m_tsScanned;
    QMap<uint32_t,DTVMultiplex> m_extendTransports;
    transport_scan_items_t      m_scanTransports;
    transport_scan_items_it_t   m_current;
    transport_scan_items_it_t   m_nextIt;
    bool                        m_currentTestingDecryption {false};
    QMap<uint, uint>            m_currentEncryptionStatus;
    QMap<uint, bool>            m_currentEncryptionStatusChecked;
    QMap<uint64_t, QString>     m_defAuthorities;
    bool                        m_dvbt2Tried {false};

    /// Found Channel Info
    ChannelList          m_channelList;
    uint                 m_channelsFound       {0};
    ScannedChannelInfo  *m_currentInfo         {nullptr};

    // Analog Info
    AnalogSignalHandler *m_analogSignalHandler {nullptr};

    // Scanner thread, runs ChannelScanSM::run()
    MThread             *m_scannerThread       {nullptr};

    // Protect UpdateChannelInfo
    QMutex               m_mutex;
};

inline void ChannelScanSM::UpdateScanPercentCompleted(void)
{
    int tmp = (m_transportsScanned * 100) /
              (m_scanTransports.size() + m_extendTransports.size());
    m_scanMonitor->ScanPercentComplete(tmp);
}

void AnalogSignalHandler::AllGood(void)
{
    m_siScan->HandleAllGood();
}

#endif // SISCAN_H
