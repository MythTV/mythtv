// -*- Mode: c++ -*-
/*
 *  Copyright (C) Daniel Kristjansson 2007
 *
 *  This file is licensed under GPL v2 or (at your option) any later version.
 *
 */

#ifndef CHANNEL_IMPORTER_H
#define CHANNEL_IMPORTER_H

// C++ headers
#include <cstring>

// Qt headers
#include <QMap>
#include <QObject>
#include <QString>
#include <QCoreApplication>

// MythTV headers
#include "libmythtv/mythtvexp.h"
#include "libmythui/mythmainwindow.h"

#include "scaninfo.h"
#include "channelscantypes.h"
#include "channelscanner_web.h"

enum OkCancelType : std::int8_t {
    kOCTCancelAll = -1,
    kOCTCancel    = +0,
    kOCTOk        = +1,
    kOCTOkAll     = +2,
};

using ChanStats = std::array<uint,3>;

class ChannelImporterBasicStats
{
  public:
    ChannelImporterBasicStats() = default;

    // totals
    ChanStats m_atscChannels {};
    ChanStats m_dvbChannels  {};
    ChanStats m_scteChannels {};
    ChanStats m_mpegChannels {};
    ChanStats m_ntscChannels {};

    // per channel counts
    QMap<uint,uint>    m_progNumCnt;
    QMap<uint,uint>    m_atscNumCnt;
    QMap<uint,uint>    m_atscMinCnt;
    QMap<uint,uint>    m_atscMajCnt;
    QMap<QString,uint> m_chanNumCnt;
};

class ChannelImporterUniquenessStats
{
  public:
    ChannelImporterUniquenessStats() = default;

    uint m_uniqueProgNum {0};
    uint m_uniqueAtscNum {0};
    uint m_uniqueAtscMin {0};
    uint m_uniqueChanNum {0};
    uint m_uniqueTotal   {0};
    uint m_maxAtscMajCnt {0};
};

class MTV_PUBLIC ChannelImporter : public QObject
{
    Q_OBJECT

  public:
    ChannelImporter(bool gui, bool interactive,
                    bool _delete, bool insert, bool save,
                    bool fta_only, bool lcn_only, bool complete_only,
                    bool full_channel_search,
                    bool remove_duplicates,
                    ServiceRequirements service_requirements,
                    bool success = false);

    void Process(const ScanDTVTransportList &_transports, int sourceid = -1);

  protected:
    enum DeleteAction : std::uint8_t
    {
        kDeleteAll,
        kDeleteManual,
        kDeleteIgnoreAll,
        kDeleteInvisibleAll,
    };
    enum InsertAction : std::uint8_t
    {
        kInsertAll,
        kInsertManual,
        kInsertIgnoreAll,
    };
    enum UpdateAction : std::uint8_t
    {
        kUpdateAll,
        kUpdateManual,
        kUpdateIgnoreAll,
    };

    enum ChannelType : std::uint8_t
    {
        kChannelTypeFirst = 0,

        kChannelTypeNonConflictingFirst = kChannelTypeFirst,
        kATSCNonConflicting = kChannelTypeFirst,
        kDVBNonConflicting  = 1,
        kSCTENonConflicting = 2,
        kMPEGNonConflicting = 3,
        kNTSCNonConflicting = 4,
        kChannelTypeNonConflictingLast = kNTSCNonConflicting,

        kChannelTypeConflictingFirst = 5,
        kATSCConflicting = kChannelTypeConflictingFirst,
        kDVBConflicting  = 6,
        kSCTEConflicting = 7,
        kMPEGConflicting = 8,
        kNTSCConflicting = 9,
        kChannelTypeConflictingLast = kNTSCConflicting,
        kChannelTypeLast = kChannelTypeConflictingLast,
    };

    static QString toString(ChannelType type);

    static void MergeSameFrequency(ScanDTVTransportList &transports);
    static void RemoveDuplicates(ScanDTVTransportList &transports, ScanDTVTransportList &duplicates);
    void FilterServices(ScanDTVTransportList &transports) const;
    static void FilterRelocatedServices(ScanDTVTransportList &transports);
    void ChannelNumbers(ScanDTVTransportList &transports) const;

    ScanDTVTransportList GetDBTransports(
        uint sourceid, ScanDTVTransportList &transports) const;

    uint DeleteChannels(ScanDTVTransportList &transports);
    uint DeleteUnusedTransports(uint sourceid);

    void InsertChannels(const ScanDTVTransportList &transports,
                        const ChannelImporterBasicStats &info);

    ScanDTVTransportList InsertChannels(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info,
        InsertAction action,
        ChannelType type,
        ScanDTVTransportList &inserted,
        ScanDTVTransportList &skipped);

    ScanDTVTransportList UpdateChannels(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info,
        UpdateAction action,
        ChannelType type,
        ScanDTVTransportList &updated,
        ScanDTVTransportList &skipped) const;

    /// For multiple channels
    DeleteAction QueryUserDelete(const QString &msg);

    /// For multiple channels
    InsertAction QueryUserInsert(const QString &msg);

    /// For multiple channels
    UpdateAction QueryUserUpdate(const QString &msg);

    /// For a single channel
    OkCancelType QueryUserResolve(
        const ScanDTVTransport          &transport,
        ChannelInsertInfo               &chan);

    /// For a single channel
    OkCancelType QueryUserInsert(
        const ScanDTVTransport          &transport,
        ChannelInsertInfo               &chan);

    static QString ComputeSuggestedChannelNum(
        const ChannelInsertInfo         &chan);

    OkCancelType ShowManualChannelPopup(
        const QString& title,
        const QString& message, QString &text);

    OkCancelType ShowResolveChannelPopup(
        const QString& title,
        const QString& message, QString &text);

    static void FixUpOpenCable(ScanDTVTransportList &transports);

    static ChannelImporterBasicStats CollectStats(
        const ScanDTVTransportList &transports);

    static ChannelImporterUniquenessStats CollectUniquenessStats(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info);

    static QString FormatChannels(
        const ScanDTVTransportList      &transports,
        const ChannelImporterBasicStats *info = nullptr);

    static QString FormatChannel(
        const ScanDTVTransport          &transport,
        const ChannelInsertInfo         &chan,
        const ChannelImporterBasicStats *info = nullptr);

    static QString SimpleFormatChannel(
        const ScanDTVTransport          &transport,
        const ChannelInsertInfo         &chan);

    static QString FormatTransport(
        const ScanDTVTransport          &transport);

    static QString FormatTransports(
        const ScanDTVTransportList      &transports_in);

    static QString GetSummary(
        const ChannelImporterBasicStats      &info,
        const ChannelImporterUniquenessStats &stats);

    static bool IsType(
        const ChannelImporterBasicStats &info,
        const ChannelInsertInfo &chan, ChannelType type);

    static void CountChannels(
        const ScanDTVTransportList &transports,
        const ChannelImporterBasicStats &info,
        ChannelType type, uint &new_chan, uint &old_chan);

    static int SimpleCountChannels(
        const ScanDTVTransportList &transports);

    static bool CheckChannelNumber(
        const QString           &num,
        const ChannelInsertInfo &chan);

    static void AddChanToCopy(
        ScanDTVTransport &transport_copy,
        const ScanDTVTransport &transport,
        const ChannelInsertInfo &chan);

  private:
    bool m_useGui;
    bool m_isInteractive;
    bool m_useWeb                       {false};
    bool m_doDelete;
    bool m_doInsert;
    bool m_doSave;
    bool m_doLcn                        {true};     // Use Logical Channel Numbers
    bool m_doScn                        {true};     // Use HD Simulcast logical channel numbers
    bool m_ftaOnly                      {true};     // Only FreeToAir (non-encrypted) channels desired post scan?
    bool m_lcnOnly                      {false};    // Only services with logical channel numbers desired post scan?
    bool m_completeOnly                 {true};     // Only services with complete scandata desired post scan?
    bool m_keepChannelNumbers           {true};     // Keep existing channel numbers on channel update
    bool m_fullChannelSearch            {false};    // Full search for old channels across transports in database
    bool m_removeDuplicates             {false};    // Remove duplicate transports and channels in scan
    bool m_success                      {false};    // To pass information IPTV channel scan succeeded
    int  m_lcnOffset                    {0};        // Read from database table videosource
    int  m_functorRetval                {0};
    ChannelScannerWeb * m_pWeb          {nullptr};

    ServiceRequirements m_serviceRequirements;  // Services desired post scan
    QEventLoop          m_eventLoop;
};

#endif // CHANNEL_IMPORTER_H
