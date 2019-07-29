// -*- Mode: c++ -*-
/*
 *  Copyright (C) Daniel Kristjansson 2007
 *
 *  This file is licensed under GPL v2 or (at your option) any later version.
 *
 */

#ifndef _CHANNEL_IMPORTER_H_
#define _CHANNEL_IMPORTER_H_

// C++ headers
#include <cstring>

// Qt headers
#include <QMap>
#include <QString>
#include <QCoreApplication>

// MythTV headers
#include "mythtvexp.h"
#include "scaninfo.h"
#include "channelscantypes.h"
#include "mythmainwindow.h"

typedef enum {
    kOCTCancelAll = -1,
    kOCTCancel    = +0,
    kOCTOk        = +1,
    kOCTOkAll     = +2,
} OkCancelType;

class ChannelImporterBasicStats
{
  public:
    ChannelImporterBasicStats()
    {
        memset(m_atsc_channels, 0, sizeof(m_atsc_channels));
        memset(m_dvb_channels,  0, sizeof(m_dvb_channels));
        memset(m_scte_channels, 0, sizeof(m_scte_channels));
        memset(m_mpeg_channels, 0, sizeof(m_mpeg_channels));
        memset(m_ntsc_channels, 0, sizeof(m_ntsc_channels));
    }

    // totals
    uint m_atsc_channels[3];
    uint m_dvb_channels [3];
    uint m_scte_channels[3];
    uint m_mpeg_channels[3];
    uint m_ntsc_channels[3];

    // per channel counts
    QMap<uint,uint>    m_prognum_cnt;
    QMap<uint,uint>    m_atscnum_cnt;
    QMap<uint,uint>    m_atscmin_cnt;
    QMap<uint,uint>    m_atscmaj_cnt;
    QMap<QString,uint> m_channum_cnt;
};

class ChannelImporterUniquenessStats
{
  public:
    ChannelImporterUniquenessStats() = default;

    uint m_unique_prognum {0};
    uint m_unique_atscnum {0};
    uint m_unique_atscmin {0};
    uint m_unique_channum {0};
    uint m_unique_total   {0};
    uint m_max_atscmajcnt {0};
};

class MTV_PUBLIC ChannelImporter
{
    Q_DECLARE_TR_FUNCTIONS(ChannelImporter)

  public:
    ChannelImporter(bool gui, bool interactive,
                    bool _delete, bool insert, bool save,
                    bool fta_only, bool lcn_only, bool complete_only,
                    ServiceRequirements service_requirements,
                    bool success = false) :
        m_use_gui(gui),
        m_is_interactive(interactive),
        m_do_delete(_delete),
        m_do_insert(insert),
        m_do_save(save),
        m_fta_only(fta_only),
        m_lcn_only(lcn_only),
        m_complete_only(complete_only),
        m_success(success),
        m_service_requirements(service_requirements) { }

    void Process(const ScanDTVTransportList&, int sourceid = -1);

  protected:
    typedef enum
    {
        kDeleteAll,
        kDeleteManual,
        kDeleteIgnoreAll,
        kDeleteInvisibleAll,
    } DeleteAction;
    typedef enum
    {
        kInsertAll,
        kInsertManual,
        kInsertIgnoreAll,
    } InsertAction;
    typedef enum
    {
        kUpdateAll,
        kUpdateManual,
        kUpdateIgnoreAll,
    } UpdateAction;

    typedef enum
    {
        kChannelTypeFirst = 0,

        kChannelTypeNonConflictingFirst = kChannelTypeFirst,
        kATSCNonConflicting = kChannelTypeFirst,
        kDVBNonConflicting,
        kSCTENonConflicting,
        kMPEGNonConflicting,
        kNTSCNonConflicting,
        kChannelTypeNonConflictingLast = kNTSCNonConflicting,

        kChannelTypeConflictingFirst,
        kATSCConflicting = kChannelTypeConflictingFirst,
        kDVBConflicting,
        kSCTEConflicting,
        kMPEGConflicting,
        kNTSCConflicting,
        kChannelTypeConflictingLast = kNTSCConflicting,
        kChannelTypeLast = kChannelTypeConflictingLast,
    } ChannelType;

    QString toString(ChannelType type);

    void CleanupDuplicates(ScanDTVTransportList &transports) const;
    void FilterServices(ScanDTVTransportList &transports) const;
    ScanDTVTransportList GetDBTransports(
        uint sourceid, ScanDTVTransportList&) const;

    uint DeleteChannels(ScanDTVTransportList&);
    uint DeleteUnusedTransports(uint sourceid);

    void InsertChannels(const ScanDTVTransportList&,
                        const ChannelImporterBasicStats&);

    ScanDTVTransportList InsertChannels(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info,
        InsertAction action, ChannelType type,
        ScanDTVTransportList &filtered);

    ScanDTVTransportList UpdateChannels(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info,
        UpdateAction action, ChannelType type,
        ScanDTVTransportList &filtered);

    /// For multiple channels
    DeleteAction QueryUserDelete(const QString &msg);

    /// For multiple channels
    InsertAction QueryUserInsert(const QString &msg);

    /// For multiple channels
    UpdateAction QueryUserUpdate(const QString &msg);

    /// For a single channel
    OkCancelType QueryUserResolve(
        const ChannelImporterBasicStats &info,
        const ScanDTVTransport          &transport,
        ChannelInsertInfo               &chan);

    /// For a single channel
    OkCancelType QueryUserInsert(
        const ChannelImporterBasicStats &info,
        const ScanDTVTransport          &transport,
        ChannelInsertInfo               &chan);

    static QString ComputeSuggestedChannelNum(
        const ChannelInsertInfo         &chan);

    OkCancelType ShowManualChannelPopup(
        MythMainWindow *parent, const QString& title,
        const QString& message, QString &text);

    OkCancelType ShowResolveChannelPopup(
        MythMainWindow *parent, const QString& title,
        const QString& message, QString &text);

    static void FixUpOpenCable(ScanDTVTransportList &transports);

    static ChannelImporterBasicStats CollectStats(
        const ScanDTVTransportList &transports);

    static ChannelImporterUniquenessStats CollectUniquenessStats(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info);

    static QString FormatChannels(
        const ScanDTVTransportList      &transports,
        const ChannelImporterBasicStats &info);

    static QString FormatChannel(
        const ScanDTVTransport          &transport,
        const ChannelInsertInfo         &chan,
        const ChannelImporterBasicStats *info = nullptr);

    static QString SimpleFormatChannel(
        const ScanDTVTransport          &transport,
        const ChannelInsertInfo         &chan);

    static QString GetSummary(
        uint                                  transport_count,
        const ChannelImporterBasicStats      &info,
        const ChannelImporterUniquenessStats &stats);

    static bool IsType(
        const ChannelImporterBasicStats &info,
        const ChannelInsertInfo &chan, ChannelType type);

    static void CountChannels(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info,
        ChannelType type, uint &new_chan, uint &old_chan);

    static bool CheckChannelNumber(
        const QString           &num,
        const ChannelInsertInfo &chan);

  private:
    bool                m_use_gui;
    bool                m_is_interactive;
    bool                m_do_delete;
    bool                m_do_insert;
    bool                m_do_save;
    /// Only FreeToAir (non-encrypted) channels desired post scan?
    bool                m_fta_only;
    /// Only services with logical channel numbers desired post scan?
    bool                m_lcn_only;
    /// Only services with complete scandata desired post scan?
    bool                m_complete_only;
    /// Keep existing channel numbers on channel update
    bool                m_keep_channel_numbers      {true};
    /// To pass information IPTV channel scan succeeded
    bool                m_success {false};
    /// Services desired post scan
    ServiceRequirements m_service_requirements;

    QEventLoop          m_eventLoop;
};

#endif // _CHANNEL_IMPORTER_H_
