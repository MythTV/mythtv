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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef SISCAN_H
#define SISCAN_H

// Qt includes
#include <qobject.h>
#include <qstring.h>
#include <qmap.h>
#include <qmutex.h>
#include <QList>
#include <qdatetime.h>

// MythTV includes
#include "frequencytables.h"
#include "streamlisteners.h"
#include "scanmonitor.h"
#include "signalmonitorlistener.h"

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
typedef QMap<transport_scan_items_it_t,ScannedChannelInfo*> ChannelMap;

class ChannelScanSM;
class AnalogSignalHandler : public SignalMonitorListener
{
  public:
    AnalogSignalHandler(ChannelScanSM *_siscan) : siscan(_siscan) { }

  public slots:
    virtual inline void AllGood(void);
    virtual void StatusSignalLock(const SignalMonitorValue&) { }
    virtual void StatusSignalStrength(const SignalMonitorValue&) { }

  private:
    ChannelScanSM *siscan;
};

class ChannelScanSM : public MPEGStreamListener,
                      public ATSCMainStreamListener,
                      public DVBMainStreamListener
{
    friend class AnalogSignalHandler;

  public:
    ChannelScanSM(
        ScanMonitor *_scan_monitor,
        const QString &_cardtype, ChannelBase* _channel, int _sourceID,
        uint signal_timeout, uint channel_timeout,
        const QString &_inputname);
    ~ChannelScanSM();

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(
        int src, const QString std, const QString mod, const QString country);
    bool ScanTransportsStartingOn(
        int sourceid, const QMap<QString,QString> &valueMap);
    bool ScanTransport(int mplexid);
    bool ScanForChannels(
        uint sourceid, const QString &std, const QString &cardtype,
        const DTVChannelList&);

    bool ScanServicesSourceID(int SourceID);

    void SetAnalog(bool is_analog);
    void SetSourceID(int _SourceID)   { sourceID                = _SourceID; }
    void SetSignalTimeout(uint val)    { signalTimeout = val; }
    void SetChannelTimeout(uint val)   { channelTimeout = val; }

    uint GetSignalTimeout(void)  const { return signalTimeout; }
    uint GetChannelTimeout(void) const { return channelTimeout; }

    SignalMonitor    *GetSignalMonitor(void) { return signalMonitor; }
    DTVSignalMonitor *GetDTVSignalMonitor(void);
    DVBSignalMonitor *GetDVBSignalMonitor(void);

    typedef QMap<uint,ChannelInsertInfo> chan_info_map_t;
    chan_info_map_t GetChannelList(ChannelMap::const_iterator it) const;
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

  private:
    // some useful gets
    DTVChannel       *GetDTVChannel(void);
    V4LChannel       *GetV4LChannel(void);
    DVBChannel       *GetDVBChannel(void);
    const DVBChannel *GetDVBChannel(void) const;

    /// \brief Called by SpawnScanner to run scanning thread
    void RunScanner(void);
    /// \brief Thunk to call RunScanner from pthread
    static void *SpawnScanner(void *param);

    bool HasTimedOut(void);
    void HandleActiveScan(void);
    bool Tune(const transport_scan_items_it_t transport);
    uint InsertMultiplex(const transport_scan_items_it_t transport);
    void ScanTransport(const transport_scan_items_it_t transport);

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
    bool UpdateChannelInfo(bool wait_until_complete);
    void ScanNITs(void);

    void HandleAllGood(void); // used for analog scanner


    static QString loc(const ChannelScanSM*);

  private:
    // Set in constructor
    ScanMonitor      *scan_monitor;
    ChannelBase      *channel;
    SignalMonitor    *signalMonitor;
    int               sourceID;
    uint              signalTimeout;
    uint              channelTimeout;
    QString           inputname;

    // State
    bool              scanning;
    bool              threadExit;
    bool              waitingForTables;
    QTime             timer;

    // Transports List
    int                         transportsScanned;
    transport_scan_items_t      scanTransports;
    transport_scan_items_it_t   current;
    transport_scan_items_it_t   nextIt;
    bool                        currentTestingDecryption;
    QMap<uint, uint>            currentEncryptionStatus;
    QMap<uint, bool>            currentEncryptionStatusChecked;
    QMap<uint, uint>            dvbChanNums; // pnum->channel_num

    /// Found Channel Info
    ChannelMap        channelMap;
    uint              channelsFound;

    // Analog Info
    AnalogSignalHandler *analogSignalHandler;

    /// Scanner thread, runs ChannelScanSM::StartScanner()
    pthread_t        scanner_thread;
    bool             scanner_thread_running;
};

inline void ChannelScanSM::UpdateScanPercentCompleted(void)
{
    int tmp = (transportsScanned * 100) / scanTransports.size();
    scan_monitor->ScanPercentComplete(tmp);
}

void AnalogSignalHandler::AllGood(void)
{
    siscan->HandleAllGood();
}

#endif // SISCAN_H
