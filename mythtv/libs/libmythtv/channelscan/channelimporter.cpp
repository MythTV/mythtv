// -*- Mode: c++ -*-
/*
 *  Copyright (C) Daniel Kristjansson 2007
 *
 *  This file is licensed under GPL v2 or (at your option) any later version.
 *
 */

// C++ includes
#include <iostream>
#include <utility>
#include <algorithm>

// Qt includes
#include <QTextStream>
#include <QElapsedTimer>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythdialogbox.h"

#include "channelimporter.h"
#include "channelutil.h"
#include "mpeg/mpegstreamdata.h" // for kEncDecrypted

#define LOC QString("ChanImport: ")

static const QString kATSCChannelFormat = "%1_%2";

static QString map_str(QString str)
{
    if (str.isEmpty())
        return "";
    return str;
}

// Use the service ID as default channel number when there is no
// DVB logical channel number or ATSC channel number found in the scan.
//
static void channum_not_empty(ChannelInsertInfo &chan)
{
    if (chan.m_chanNum.isEmpty())
    {
        chan.m_chanNum = QString("%1").arg(chan.m_serviceId);
    }
}

static uint getLcnOffset(int sourceid)
{
    uint lcnOffset = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
            "SELECT lcnoffset "
            "FROM videosource "
            "WHERE videosource.sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("ChannelImporter", query);
    }
    else if (query.next())
    {
        lcnOffset = query.value(0).toUInt();
    }

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Logical Channel Number offset:%1")
            .arg(lcnOffset));

    return lcnOffset;
}

ChannelImporter::ChannelImporter(bool gui, bool interactive,
                    bool _delete, bool insert, bool save,
                    bool fta_only, bool lcn_only, bool complete_only,
                    bool full_channel_search,
                    bool remove_duplicates,
                    ServiceRequirements service_requirements,
                    bool success) :
        m_useGui(gui),
        m_isInteractive(interactive),
        m_doDelete(_delete),
        m_doInsert(insert),
        m_doSave(save),
        m_ftaOnly(fta_only),
        m_lcnOnly(lcn_only),
        m_completeOnly(complete_only),
        m_fullChannelSearch(full_channel_search),
        m_removeDuplicates(remove_duplicates),
        m_success(success),
        m_serviceRequirements(service_requirements)
{
    if (gCoreContext->IsBackend() && m_useGui)
    {
        m_useWeb = true;
        m_pWeb = ChannelScannerWeb::getInstance();
    }
}

void ChannelImporter::Process(const ScanDTVTransportList &_transports,
                              int sourceid)
{
    m_lcnOffset = getLcnOffset(sourceid);

    if (_transports.empty())
    {
        if (m_useGui)
        {
            int channels = ChannelUtil::GetChannelCount(sourceid);

            LOG(VB_GENERAL, LOG_INFO, LOC + (channels ?
                                             (m_success ?
                                              QString("Found %1 channels")
                                              .arg(channels) :
                                              "No new channels to process") :
                                             "No channels to process.."));

            QString msg;
            if (!channels)
                msg = tr("Failed to find any channels.");
            else if (m_success)
                msg = tr("Found %n channel(s)", "", channels);
            else
                msg = tr("Failed to find any new channels!");

            if (m_useWeb)
                m_pWeb->m_dlgMsg = msg;
            else
                ShowOkPopup(msg);

        }
        else
        {
            std::cout << (ChannelUtil::GetChannelCount() ?
                     "No new channels to process" :
                     "No channels to process..");
        }

        return;
    }


    // Temporary check, incomplete code
    //  Otherwise may crash the backend
    // if (m_useWeb)
    //     return;

    ScanDTVTransportList transports = _transports;
    QString msg;
    QTextStream ssMsg(&msg);

    // Scan parameters
    {
        bool require_av = (m_serviceRequirements & kRequireAV) == kRequireAV;
        bool require_a  = (m_serviceRequirements & kRequireAudio) != 0;
        const char *desired { "all" };
        if (require_av)
            desired = "tv";
        else if (require_a)
            desired = "tv+radio";
        ssMsg << Qt::endl << Qt::endl;
        ssMsg << "Scan parameters:" << Qt::endl;
        ssMsg << "Desired Services            : " << desired << Qt::endl;
        ssMsg << "Unencrypted Only            : " << (m_ftaOnly           ? "yes" : "no") << Qt::endl;
        ssMsg << "Logical Channel Numbers only: " << (m_lcnOnly           ? "yes" : "no") << Qt::endl;
        ssMsg << "Complete scan data required : " << (m_completeOnly      ? "yes" : "no") << Qt::endl;
        ssMsg << "Full search for old channels: " << (m_fullChannelSearch ? "yes" : "no") << Qt::endl;
        ssMsg << "Remove duplicates           : " << (m_removeDuplicates  ? "yes" : "no") << Qt::endl;
    }

    // Transports and channels before processing
    if (!transports.empty())
    {
        ssMsg << Qt::endl;
        ssMsg << "Transport list before processing (" << transports.size() << "):" << Qt::endl;
        ssMsg << FormatTransports(transports).toLatin1().constData();

        ChannelImporterBasicStats info = CollectStats(transports);
        ssMsg << Qt::endl;
        ssMsg << "Channel list before processing (";
        ssMsg << SimpleCountChannels(transports) << "):" << Qt::endl;
        ssMsg << FormatChannels(transports, &info).toLatin1().constData();
    }
    LOG(VB_GENERAL, LOG_INFO, LOC + msg);

    uint saved_scan = 0;
    if (m_doSave)
        saved_scan = SaveScan(transports);

    // Merge transports with the same frequency into one
    MergeSameFrequency(transports);

    // Remove duplicate transports with a lower signal strength.
    if (m_removeDuplicates)
    {
        ScanDTVTransportList duplicates;
        RemoveDuplicates(transports, duplicates);
        if (!duplicates.empty())
        {
            msg = "";
            ssMsg << Qt::endl;
            ssMsg << "Discarded duplicate transports (" << duplicates.size() << "):" << Qt::endl;
            ssMsg << FormatTransports(duplicates).toLatin1().constData() << Qt::endl;
            ssMsg << "Discarded duplicate channels (" << SimpleCountChannels(duplicates) << "):" << Qt::endl;
            ssMsg << FormatChannels(duplicates).toLatin1().constData() << Qt::endl;
            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);
        }
    }

    // Process Logical Channel Numbers
    if (m_doLcn)
    {
        ChannelNumbers(transports);
    }

    // Remove the channels that do not pass various criteria.
    FilterServices(transports);

    // Remove the channels that have been relocated.
    if (m_removeDuplicates)
    {
        FilterRelocatedServices(transports);
    }

    // Pull in DB info in transports
    // Channels not found in scan but only in DB are returned in db_trans
    ScanDTVTransportList db_trans = GetDBTransports(sourceid, transports);
    msg = "";
    ssMsg << Qt::endl;
    if (!db_trans.empty())
    {
        ssMsg << Qt::endl;
        ssMsg << "Transports with channels in DB but not in scan (";
        ssMsg << db_trans.size() << "):" << Qt::endl;
        ssMsg << FormatTransports(db_trans).toLatin1().constData();
    }

    // Make sure "Open Cable" channels are marked that way.
    FixUpOpenCable(transports);

    // All channels in the scan after comparing with the database
    {
        ChannelImporterBasicStats info = CollectStats(transports);
        ssMsg << Qt::endl;
        ssMsg << "Channel list after compare with database (";
        ssMsg << SimpleCountChannels(transports) << "):" << Qt::endl;
        ssMsg << FormatChannels(transports, &info).toLatin1().constData();
    }

    // Add channels from the DB to the channels from the scan
    // and possibly delete one or more of the off-air channels
    if (m_doDelete)
    {
        ScanDTVTransportList trans = transports;
        std::copy(db_trans.cbegin(), db_trans.cend(), std::back_inserter(trans));
        uint deleted_count = DeleteChannels(trans);
        if (deleted_count)
            transports = trans;
    }

    // Determine System Info standards..
    ChannelImporterBasicStats info = CollectStats(transports);

    // Determine uniqueness of various naming schemes
    ChannelImporterUniquenessStats stats =
        CollectUniquenessStats(transports, info);

    // Final channel list
    ssMsg << Qt::endl;
    ssMsg << "Channel list (" << SimpleCountChannels(transports) << "):" << Qt::endl;
    ssMsg << FormatChannels(transports).toLatin1().constData();

    // Create summary
    ssMsg << Qt::endl;
    ssMsg << GetSummary(info, stats) << Qt::endl;

    LOG(VB_GENERAL, LOG_INFO, LOC + msg);

    if (m_doInsert)
        InsertChannels(transports, info);

    if (m_doDelete && sourceid)
        DeleteUnusedTransports(sourceid);

    if (m_doDelete || m_doInsert)
        ScanInfo::MarkProcessed(saved_scan);
}

QString ChannelImporter::toString(ChannelType type)
{
    switch (type)
    {
        // non-conflicting
        case kATSCNonConflicting: return "ATSC";
        case kDVBNonConflicting:  return "DVB";
        case kSCTENonConflicting: return "SCTE";
        case kMPEGNonConflicting: return "MPEG";
        case kNTSCNonConflicting: return "NTSC";
        // conflicting
        case kATSCConflicting:    return "ATSC";
        case kDVBConflicting:     return "DVB";
        case kSCTEConflicting:    return "SCTE";
        case kMPEGConflicting:    return "MPEG";
        case kNTSCConflicting:    return "NTSC";
    }
    return "Unknown";
}

// Ask user what to do with the off-air channels
//
uint ChannelImporter::DeleteChannels(
    ScanDTVTransportList &transports)
{
    std::vector<uint> off_air_list;
    QMap<uint,bool> deleted;
    ScanDTVTransportList off_air_transports;

    for (size_t i = 0; i < transports.size(); ++i)
    {
        ScanDTVTransport transport_copy;
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transports[i].m_channels[j];
            bool was_in_db = (chan.m_dbMplexId != 0U) && (chan.m_channelId != 0U);
            if (!was_in_db)
                continue;

            if (!chan.m_inPmt)
            {
                off_air_list.push_back(i<<16|j);
                AddChanToCopy(transport_copy, transports[i], chan);
            }
        }
        if (!transport_copy.m_channels.empty())
            off_air_transports.push_back(transport_copy);
    }

    if (off_air_list.empty())
        return 0;

    // List of off-air channels (in database but not in the scan)
    std::cout << std::endl << "Off-air channels (" << SimpleCountChannels(off_air_transports) << "):" << std::endl;
    ChannelImporterBasicStats infoA = CollectStats(off_air_transports);
    std::cout << FormatChannels(off_air_transports, &infoA).toLatin1().constData() << std::endl;

    // Ask user whether to delete all or some of these stale channels
    // if some is selected ask about each individually
    //: %n is the number of channels
    QString msg = tr("Found %n off-air channel(s).", "", off_air_list.size());
    if (m_useWeb)
        m_pWeb->log(msg);
    DeleteAction action = QueryUserDelete(msg);
    if (kDeleteIgnoreAll == action)
        return 0;

    if (kDeleteAll == action)
    {
        for (uint item : off_air_list)
        {
            int i = item >> 16;
            int j = item & 0xFFFF;
            ChannelUtil::DeleteChannel(
                transports[i].m_channels[j].m_channelId);
            deleted[item] = true;
        }
    }
    else if (kDeleteInvisibleAll == action)
    {
        for (uint item : off_air_list)
        {
            int i = item >> 16;
            int j = item & 0xFFFF;
            int chanid = transports[i].m_channels[j].m_channelId;
            QString channum = ChannelUtil::GetChanNum(chanid);
            ChannelUtil::SetVisible(chanid, kChannelNotVisible);
            ChannelUtil::SetChannelValue("channum", QString("_%1").arg(channum),
                                         chanid);
        }
    }
    else
    {
        // TODO manual delete
    }

    // TODO delete encrypted channels when m_ftaOnly set

    if (deleted.empty())
        return 0;

    // Create a new transports list without the deleted channels
    ScanDTVTransportList newlist;
    for (size_t i = 0; i < transports.size(); ++i)
    {
        newlist.push_back(transports[i]);
        newlist.back().m_channels.clear();
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            if (!deleted.contains(i<<16|j))
            {
                newlist.back().m_channels.push_back(
                    transports[i].m_channels[j]);
            }
        }
    }

    transports = newlist;
    return deleted.size();
}

uint ChannelImporter::DeleteUnusedTransports(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT mplexid FROM dtv_multiplex "
        "WHERE sourceid = :SOURCEID1 AND "
        "      mplexid NOT IN "
        " (SELECT mplexid "
        "  FROM channel "
        "  WHERE sourceid = :SOURCEID2)");
    query.bindValue(":SOURCEID1", sourceid);
    query.bindValue(":SOURCEID2", sourceid);
    if (!query.exec())
    {
        MythDB::DBError("DeleteUnusedTransports() -- select", query);
        return 0;
    }

    QString msg = tr("Found %n unused transport(s).", "", query.size());
    LOG(VB_GENERAL, LOG_INFO, LOC + msg);
    if (m_useWeb)
        m_pWeb->log(msg);

    if (query.size() == 0)
        return 0;

    DeleteAction action = QueryUserDelete(msg);
    if (kDeleteIgnoreAll == action)
        return 0;

    if (kDeleteAll == action)
    {
        query.prepare(
            "DELETE FROM dtv_multiplex "
            "WHERE sourceid = :SOURCEID1 AND "
            "      mplexid NOT IN "
            " (SELECT mplexid "
            "  FROM channel "
            "  WHERE sourceid = :SOURCEID2)");
        query.bindValue(":SOURCEID1", sourceid);
        query.bindValue(":SOURCEID2", sourceid);
        if (!query.exec())
        {
            MythDB::DBError("DeleteUnusedTransports() -- delete", query);
            return 0;
        }
    }
    else
    {
        // TODO manual delete
        LOG(VB_GENERAL, LOG_INFO, LOC + "Manual delete of transport not implemented");
    }
    return 0;
}

void ChannelImporter::InsertChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info)
{
    ScanDTVTransportList list = transports;
    ScanDTVTransportList inserted;
    ScanDTVTransportList updated;
    ScanDTVTransportList skipped_inserts;
    ScanDTVTransportList skipped_updates;

    // Insert or update all channels with non-conflicting channum
    // and complete tuning information.
    uint chantype = (uint) kChannelTypeNonConflictingFirst;
    for (; chantype <= (uint) kChannelTypeNonConflictingLast; ++chantype)
    {
        auto type = (ChannelType) chantype;
        uint new_chan = 0;
        uint old_chan = 0;
        CountChannels(list, info, type, new_chan, old_chan);

        if (kNTSCNonConflicting == type)
            continue;

        if (old_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n old %1 channel(s).", "", old_chan)
                              .arg(toString(type));

            UpdateAction action = QueryUserUpdate(msg);
            list = UpdateChannels(list, info, action, type, updated, skipped_updates);
        }
        if (new_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n new %1 channel(s).", "", new_chan)
                              .arg(toString(type));

            InsertAction action = QueryUserInsert(msg);
            list = InsertChannels(list, info, action, type, inserted, skipped_inserts);
        }
    }

    if (!m_isInteractive)
        return;

    // If any of the potential uniques is high and inserting
    // with those as the channum would result in few conflicts
    // ask user if it is ok to to proceed using it as the channum

    // For remaining channels with complete tuning information
    // insert channels with contiguous list of numbers as the channums
    chantype = (uint) kChannelTypeConflictingFirst;
    for (; chantype <= (uint) kChannelTypeConflictingLast; ++chantype)
    {
        auto type = (ChannelType) chantype;
        uint new_chan = 0;
        uint old_chan = 0;
        CountChannels(list, info, type, new_chan, old_chan);

        if (old_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n conflicting old %1 channel(s).",
                             "", old_chan).arg(toString(type));

            UpdateAction action = QueryUserUpdate(msg);
            list = UpdateChannels(list, info, action, type, updated, skipped_updates);
        }
        if (new_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n new conflicting %1 channel(s).",
                             "", new_chan).arg(toString(type));

            InsertAction action = QueryUserInsert(msg);
            list = InsertChannels(list, info, action, type, inserted, skipped_inserts);
        }
    }

    QString msg;
    QTextStream ssMsg(&msg);

    if (!updated.empty())
    {
        ssMsg << Qt::endl << Qt::endl;
        ssMsg << "Updated old transports (" << updated.size() << "):" << Qt::endl;
        ssMsg << FormatTransports(updated).toLatin1().constData();

        ssMsg << Qt::endl;
        ssMsg << "Updated old channels (" << SimpleCountChannels(updated) << "):" << Qt::endl;
        ssMsg << FormatChannels(updated).toLatin1().constData();
    }
    if (!skipped_updates.empty())
    {
        ssMsg << Qt::endl;
        ssMsg << "Skipped old channels (" << SimpleCountChannels(skipped_updates) << "):" << Qt::endl;
        ssMsg << FormatChannels(skipped_updates).toLatin1().constData();
    }
    if (!inserted.empty())
    {
        ssMsg << Qt::endl;
        ssMsg << "Inserted new channels (" << SimpleCountChannels(inserted) << "):" << Qt::endl;
        ssMsg << FormatChannels(inserted).toLatin1().constData();
    }
    if (!skipped_inserts.empty())
    {
        ssMsg << Qt::endl;
        ssMsg << "Skipped new channels (" << SimpleCountChannels(skipped_inserts) << "):" << Qt::endl;
        ssMsg << FormatChannels(skipped_inserts).toLatin1().constData();
    }

    // Remaining channels and sum uniques again
    if (!list.empty())
    {
        ChannelImporterBasicStats      ninfo  = CollectStats(list);
        ChannelImporterUniquenessStats nstats = CollectUniquenessStats(list, ninfo);
        ssMsg << Qt::endl;
        ssMsg << "Remaining channels (" << SimpleCountChannels(list) << "):" << Qt::endl;
        ssMsg << FormatChannels(list).toLatin1().constData() << Qt::endl;
        ssMsg << GetSummary(ninfo, nstats).toLatin1().constData();
    }
    LOG(VB_GENERAL, LOG_INFO, LOC + msg);
}

// ChannelImporter::InsertChannels
//
// transports       List of channels to insert
// info             Channel statistics
// action           Insert all, Insert manually, Ignore all
// type             Channel type such as dvb or atsc
// inserted_list    List of inserted channels
// skipped_list     List of skipped channels
//
// return:          List of channels that have not been inserted
//
ScanDTVTransportList ChannelImporter::InsertChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    InsertAction action,
    ChannelType type,
    ScanDTVTransportList &inserted_list,
    ScanDTVTransportList &skipped_list)
{
    ScanDTVTransportList next_list;

    bool cancel_all = false;
    bool ok_all = false;

    // Insert all channels with non-conflicting channum
    // and complete tuning information.
    for (const auto & transport : transports)
    {
        ScanDTVTransport new_transport;
        ScanDTVTransport inserted_transport;
        ScanDTVTransport skipped_transport;

        for (size_t j = 0; j < transport.m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transport.m_channels[j];

            bool asked = false;
            bool filter = false;
            bool handle = false;
            if (!chan.m_channelId && (kInsertIgnoreAll == action) &&
                IsType(info, chan, type))
            {
                filter = true;
            }
            else if (!chan.m_channelId && IsType(info, chan, type))
            {
                handle = true;
            }

            if (cancel_all)
            {
                handle = false;
            }

            if (handle && kInsertManual == action)
            {
                OkCancelType rc = QueryUserInsert(transport, chan);
                if (kOCTCancelAll == rc)
                {
                    cancel_all = true;
                    handle = false;
                }
                else if (kOCTCancel == rc)
                {
                    handle = false;
                }
                else if (kOCTOk == rc)
                {
                    asked = true;
                }
            }

            if (handle)
            {
                bool conflicting = false;
                channum_not_empty(chan);
                conflicting = ChannelUtil::IsConflicting(
                    chan.m_chanNum, chan.m_sourceId);

                // Only ask if not already asked before with kInsertManual
                if (m_isInteractive && !asked &&
                    (conflicting || (kChannelTypeConflictingFirst <= type)))
                {
                    bool ok_done = false;
                    if (ok_all)
                    {
                        QString val = ComputeSuggestedChannelNum(chan);
                        bool ok = CheckChannelNumber(val, chan);
                        if (ok)
                        {
                            chan.m_chanNum = val;
                            conflicting = false;
                            ok_done = true;
                        }
                    }
                    if (!ok_done)
                    {
                        OkCancelType rc =
                            QueryUserResolve(transport, chan);

                        conflicting = true;
                        if (kOCTCancelAll == rc)
                        {
                            cancel_all = true;
                        }
                        else if (kOCTOk == rc)
                        {
                            conflicting = false;
                        }
                        else if (kOCTOkAll == rc)
                        {
                            conflicting = false;
                            ok_all = true;
                        }
                    }
                }

                if (conflicting)
                {
                    handle = false;
                }
            }

            bool inserted = false;
            if (handle)
            {
                int chanid = ChannelUtil::CreateChanID(
                    chan.m_sourceId, chan.m_chanNum);

                chan.m_channelId = (chanid > 0) ? chanid : 0;

                if (chan.m_channelId)
                {
                    uint tsid = chan.m_vctTsId;
                    tsid = (tsid) ? tsid : chan.m_sdtTsId;
                    tsid = (tsid) ? tsid : chan.m_patTsId;
                    tsid = (tsid) ? tsid : chan.m_vctChanTsId;

                    chan.m_dbMplexId = ChannelUtil::CreateMultiplex(
                        chan.m_sourceId, transport, tsid, chan.m_origNetId);
                }

                if (chan.m_channelId && chan.m_dbMplexId)
                {
                    chan.m_channelId = chanid;

                    inserted = ChannelUtil::CreateChannel(
                        chan.m_dbMplexId,
                        chan.m_sourceId,
                        chan.m_channelId,
                        chan.m_callSign,
                        chan.m_serviceName,
                        chan.m_chanNum,
                        chan.m_serviceId,
                        chan.m_atscMajorChannel,
                        chan.m_atscMinorChannel,
                        chan.m_useOnAirGuide,
                        chan.m_hidden ? kChannelNotVisible : kChannelVisible,
                        chan.m_freqId,
                        QString(),
                        chan.m_format,
                        QString(),
                        chan.m_defaultAuthority,
                        chan.m_serviceType);

                    if (!transport.m_iptvTuning.GetDataURL().isEmpty())
                        ChannelUtil::CreateIPTVTuningData(chan.m_channelId,
                                          transport.m_iptvTuning);
                }
            }

            if (inserted)
            {
                // Update list of inserted channels
                AddChanToCopy(inserted_transport, transport, chan);
            }

            if (filter)
            {
                // Update list of skipped channels
                AddChanToCopy(skipped_transport, transport, chan);
            }
            else if (!inserted)
            {
                // Update list of remaining channels
                AddChanToCopy(new_transport, transport, chan);
            }
        }

        if (!new_transport.m_channels.empty())
            next_list.push_back(new_transport);

        if (!skipped_transport.m_channels.empty())
            skipped_list.push_back(skipped_transport);

        if (!inserted_transport.m_channels.empty())
            inserted_list.push_back(inserted_transport);
    }

    return next_list;
}

// ChannelImporter::UpdateChannels
//
// transports       List of channels to update
// info             Channel statistics
// action           Update All, Ignore All
// type             Channel type such as dvb or atsc
// updated_list     List of updated channels
// skipped_list     List of skipped channels
//
// return:          List of channels that have not been updated
//
ScanDTVTransportList ChannelImporter::UpdateChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    UpdateAction action,
    ChannelType type,
    ScanDTVTransportList &updated_list,
    ScanDTVTransportList &skipped_list) const
{
    ScanDTVTransportList next_list;

    // update all channels with non-conflicting channum
    // and complete tuning information.
    for (const auto & transport : transports)
    {
        ScanDTVTransport new_transport;
        ScanDTVTransport updated_transport;
        ScanDTVTransport skipped_transport;

        for (size_t j = 0; j < transport.m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transport.m_channels[j];

            bool filter = false;
            bool handle = false;
            if (chan.m_channelId && (kUpdateIgnoreAll == action) &&
                IsType(info, chan, type))
            {
                filter = true;
            }
            else if (chan.m_channelId && IsType(info, chan, type))
            {
                handle = true;
            }

            if (handle)
            {
                bool conflicting = false;

                if (m_keepChannelNumbers)
                {
                    ChannelUtil::UpdateChannelNumberFromDB(chan);
                }
                channum_not_empty(chan);
                conflicting = ChannelUtil::IsConflicting(
                    chan.m_chanNum, chan.m_sourceId, chan.m_channelId);

                if (conflicting)
                {
                    handle = false;

                    // Update list of skipped channels
                    AddChanToCopy(skipped_transport, transport, chan);
                }
            }

            bool updated = false;
            if (handle)
            {
                ChannelUtil::UpdateInsertInfoFromDB(chan);

                // Find the matching multiplex. This updates the
                // transport and network ID's in case the transport
                // was created manually
                uint tsid = chan.m_vctTsId;
                tsid = (tsid) ? tsid : chan.m_sdtTsId;
                tsid = (tsid) ? tsid : chan.m_patTsId;
                tsid = (tsid) ? tsid : chan.m_vctChanTsId;

                chan.m_dbMplexId = ChannelUtil::CreateMultiplex(
                    chan.m_sourceId, transport, tsid, chan.m_origNetId);

                ChannelVisibleType visible { kChannelVisible };
                if (chan.m_visible == kChannelAlwaysVisible ||
                      chan.m_visible == kChannelNeverVisible)
                    visible = chan.m_visible;
                else if (chan.m_hidden)
                    visible = kChannelNotVisible;

                updated = ChannelUtil::UpdateChannel(
                    chan.m_dbMplexId,
                    chan.m_sourceId,
                    chan.m_channelId,
                    chan.m_callSign,
                    chan.m_serviceName,
                    chan.m_chanNum,
                    chan.m_serviceId,
                    chan.m_atscMajorChannel,
                    chan.m_atscMinorChannel,
                    chan.m_useOnAirGuide,
                    visible,
                    chan.m_freqId,
                    QString(),
                    chan.m_format,
                    QString(),
                    chan.m_defaultAuthority,
                    chan.m_serviceType);
            }

            if (updated)
            {
                // Update list of updated channels
                AddChanToCopy(updated_transport, transport, chan);
            }

            if (filter)
            {
                // Update list of skipped channels
                AddChanToCopy(skipped_transport, transport, chan);
            }
            else if (!updated)
            {
                // Update list of remaining channels
                AddChanToCopy(new_transport, transport, chan);
            }
        }

        if (!new_transport.m_channels.empty())
            next_list.push_back(new_transport);

        if (!skipped_transport.m_channels.empty())
            skipped_list.push_back(skipped_transport);

        if (!updated_transport.m_channels.empty())
            updated_list.push_back(updated_transport);
    }

    return next_list;
}

// ChannelImporter::AddChanToCopy
//
// Add channel to copy of transport.
// This is used to keep track of what is done with each channel
//
// transport_copy   with zero to all channels of transport
// transport        transport with channel info as scanned
// chan             one channel of transport, to be copied
//
void ChannelImporter::AddChanToCopy(
    ScanDTVTransport &transport_copy,
    const ScanDTVTransport &transport,
    const ChannelInsertInfo &chan
)
{
    if (transport_copy.m_channels.empty())
    {
        transport_copy = transport;
        transport_copy.m_channels.clear();
    }
    transport_copy.m_channels.push_back(chan);
}

// ChannelImporter::MergeSameFrequency
//
// Merge transports that are on the same frequency by
// combining all channels of both transports into one transport
//
void ChannelImporter::MergeSameFrequency(ScanDTVTransportList &transports)
{
    ScanDTVTransportList no_dups;

    DTVTunerType tuner_type(DTVTunerType::kTunerTypeATSC);
    if (!transports.empty())
        tuner_type = transports[0].m_tunerType;

    bool is_dvbs = ((DTVTunerType::kTunerTypeDVBS1 == tuner_type) ||
                    (DTVTunerType::kTunerTypeDVBS2 == tuner_type));

    uint freq_mult = (is_dvbs) ? 1 : 1000;

    std::vector<bool> ignore;
    ignore.resize(transports.size());
    for (size_t i = 0; i < transports.size(); ++i)
    {
        if (ignore[i])
            continue;

        for (size_t j = i+1; j < transports.size(); ++j)
        {
            if (!transports[i].IsEqual(
                    tuner_type, transports[j], 500 * freq_mult))
            {
                continue;
            }

            for (size_t k = 0; k < transports[j].m_channels.size(); ++k)
            {
                bool found_same = false;
                for (size_t l = 0; l < transports[i].m_channels.size(); ++l)
                {
                    if (transports[j].m_channels[k].IsSameChannel(
                            transports[i].m_channels[l]))
                    {
                        found_same = true;
                        transports[i].m_channels[l].ImportExtraInfo(
                            transports[j].m_channels[k]);
                    }
                }
                if (!found_same)
                    transports[i].m_channels.push_back(transports[j].m_channels[k]);
            }
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("Transport on same frequency:") + FormatTransport(transports[j]));
            ignore[j] = true;
        }
        no_dups.push_back(transports[i]);
    }
    transports = no_dups;
}

// ChannelImporter::RemoveDuplicates
//
// When there are two transports that have the same list of channels
// but that are received on different frequencies then remove
// the transport with the weakest signal.
//
// In DVB two transports are duplicates when the original network ID and the
// transport ID are the same. This is possibly different in ATSC.
// Here all channels of both transports are compared.
//
void ChannelImporter::RemoveDuplicates(ScanDTVTransportList &transports, ScanDTVTransportList &duplicates)
{
    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Number of transports:%1").arg(transports.size()));

    ScanDTVTransportList no_dups;
    std::vector<bool> ignore;
    ignore.resize(transports.size());
    for (size_t i = 0; i < transports.size(); ++i)
    {
        ScanDTVTransport &ta = transports[i];
        LOG(VB_CHANSCAN, LOG_INFO, LOC + "Transport " +
            FormatTransport(ta) + QString(" size(%1)").arg(ta.m_channels.size()));

        if (!ignore[i])
        {
            for (size_t j = i+1; j < transports.size(); ++j)
            {
                ScanDTVTransport &tb = transports[j];
                bool found_same = true;
                bool found_diff = true;
                if (ta.m_channels.size() == tb.m_channels.size())
                {
                    LOG(VB_CHANSCAN, LOG_DEBUG, LOC + "Comparing transport A " +
                        FormatTransport(ta) + QString(" size(%1)").arg(ta.m_channels.size()));
                    LOG(VB_CHANSCAN, LOG_DEBUG, LOC + "Comparing transport B " +
                        FormatTransport(tb) + QString(" size(%1)").arg(tb.m_channels.size()));

                    for (size_t k = 0; found_same && k < tb.m_channels.size(); ++k)
                    {
                        if (tb.m_channels[k].IsSameChannel(ta.m_channels[k], 0))
                        {
                            found_diff = false;
                        }
                        else
                        {
                            found_same = false;
                        }
                    }
                }

                // Transport with the lowest signal strength is duplicate
                if (found_same && !found_diff)
                {
                    size_t lowss = transports[i].m_signalStrength < transports[j].m_signalStrength ? i : j;
                    ignore[lowss] = true;
                    duplicates.push_back(transports[lowss]);

                    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Duplicate transports found:");
                    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Transport A " + FormatTransport(transports[i]));
                    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Transport B " + FormatTransport(transports[j]));
                    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Discarding  " + FormatTransport(transports[lowss]));
                }
            }
        }
        if (!ignore[i])
        {
            no_dups.push_back(transports[i]);
        }
    }

    transports = no_dups;
}

void ChannelImporter::FilterServices(ScanDTVTransportList &transports) const
{
    bool require_av = (m_serviceRequirements & kRequireAV) == kRequireAV;
    bool require_a  = (m_serviceRequirements & kRequireAudio) != 0;

    for (auto & transport : transports)
    {
        ChannelInsertInfoList filtered;
        for (auto & channel : transport.m_channels)
        {
            if (m_ftaOnly && channel.m_isEncrypted &&
                channel.m_decryptionStatus != kEncDecrypted)
                continue;

            if (require_a && channel.m_isDataService)
                continue;

            if (require_av && channel.m_isAudioService)
                continue;

            // Filter channels out that do not have a logical channel number
            if (m_lcnOnly && channel.m_chanNum.isEmpty())
            {
                QString msg = FormatChannel(transport, channel);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("No LCN: %1").arg(msg));
                continue;
            }

            // Filter channels out that are not present in PAT and PMT.
            if (m_completeOnly &&
                !(channel.m_inPat &&
                  channel.m_inPmt ))
            {
                QString msg = FormatChannel(transport, channel);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Not in PAT/PMT: %1").arg(msg));
                continue;
            }

            // Filter channels out that are not present in SDT and that are not ATSC
            if (m_completeOnly &&
                channel.m_atscMajorChannel == 0 &&
                channel.m_atscMinorChannel == 0 &&
                (!channel.m_inPat ||
                 !channel.m_inPmt ||
                 !channel.m_inSdt ||
                 (channel.m_patTsId !=
                  channel.m_sdtTsId)))
            {
                QString msg = FormatChannel(transport, channel);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Not in PAT/PMT/SDT: %1").arg(msg));
                continue;
            }

            // Filter channels out that do not have a name
            if (m_completeOnly && channel.m_serviceName.isEmpty())
            {
                QString msg = FormatChannel(transport, channel);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("No name: %1").arg(msg));
                continue;
            }

            // Filter channels out only in channels.conf, i.e. not found
            if (channel.m_inChannelsConf &&
                !(channel.m_inPat ||
                  channel.m_inPmt ||
                  channel.m_inVct ||
                  channel.m_inNit ||
                  channel.m_inSdt))
                continue;

            filtered.push_back(channel);
        }
        transport.m_channels = filtered;
    }
}

void ChannelImporter::FilterRelocatedServices(ScanDTVTransportList &transports)
{
    QMap<uint64_t, bool> rs;

    // Search all channels to find relocated services
    for (auto & transport : transports)
    {
        for (auto & channel : transport.m_channels)
        {
            if (channel.m_oldOrigNetId > 0)
            {
                uint64_t key = ((uint64_t)channel.m_oldOrigNetId << 32) | (channel.m_oldTsId << 16) | channel.m_oldServiceId;
                rs[key] = true;
            }
        }
    }

    // Remove all relocated services
    for (auto & transport : transports)
    {
        ChannelInsertInfoList filtered;
        for (auto & channel : transport.m_channels)
        {
            uint64_t key = ((uint64_t)channel.m_origNetId << 32) | (channel.m_sdtTsId << 16) | channel.m_serviceId;
            if (rs.value(key, false))
            {
                QString msg = FormatChannel(transport, channel);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Relocated: %1").arg(msg));
                continue;
            }
            filtered.push_back(channel);
        }
        transport.m_channels = filtered;
    }
}

// Process DVB Channel Numbers
void ChannelImporter::ChannelNumbers(ScanDTVTransportList &transports) const
{
    QMap<qlonglong, uint> map_sid_scn;     // HD Simulcast channel numbers, service ID is key
    QMap<qlonglong, uint> map_sid_lcn;     // Logical channel numbers, service ID is key
    QMap<uint, qlonglong> map_lcn_sid;     // Logical channel numbers, channel number is key

    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Process DVB Channel Numbers"));
    for (auto & transport : transports)
    {
        for (auto & channel : transport.m_channels)
        {
            LOG(VB_CHANSCAN, LOG_DEBUG, LOC + QString("Channel onid:%1 sid:%2 lcn:%3 scn:%4")
                .arg(channel.m_origNetId).arg(channel.m_serviceId).arg(channel.m_logicalChannel)
                .arg(channel.m_simulcastChannel));
            qlonglong key = ((qlonglong)channel.m_origNetId<<32) | channel.m_serviceId;
            if (channel.m_logicalChannel > 0)
            {
                map_sid_lcn[key] = channel.m_logicalChannel;
                map_lcn_sid[channel.m_logicalChannel] = key;
            }
            if (channel.m_simulcastChannel > 0)
            {
                map_sid_scn[key] = channel.m_simulcastChannel;
            }
        }
    }

    // Process the HD Simulcast Channel Numbers
    //
    // For each channel with a HD Simulcast Channel Number, do use that
    // number as the Logical Channel Number; the SD channel that now has
    // this LCN does get the original LCN of the HD Simulcast channel.
    // If this is not selected then the Logical Channel Numbers are used
    // without the override from the HD Simulcast channel numbers.
    // This usually means that channel numbers 1, 2, 3 etc are used for SD channels
    // while the corresponding HD channels do have higher channel numbers.
    // When the HD Simulcast channel numbers are enabled then channel numbers 1, 2, 3 etc are
    // used for the HD channels and the corresponding SD channels use the high channel numbers.
    if (m_doScn)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Process Simulcast Channel Numbers"));

        QMap<qlonglong, uint>::iterator it;
        for (it = map_sid_scn.begin(); it != map_sid_scn.end(); ++it)
        {
            // Exchange LCN between the SD channel and the HD simulcast channel
            qlonglong key_hd = it.key();                // Key of HD channel
            uint scn_hd = *it;                          // SCN of the HD channel
            uint lcn_sd = scn_hd;                       // Old LCN of the SD channel
            uint lcn_hd = map_sid_lcn[key_hd];          // Old LCN of the HD channel

            qlonglong key_sd = map_lcn_sid[lcn_sd];     // Key of the SD channel

            map_sid_lcn[key_sd] = lcn_hd;               // SD channel gets old LCN of HD channel
            map_sid_lcn[key_hd] = lcn_sd;               // HD channel gets old LCN of SD channel
            map_lcn_sid[lcn_hd] = key_sd;               // SD channel gets key of SD channel
            map_lcn_sid[lcn_sd] = key_hd;               // HD channel gets key of SD channel
        }
    }

    // Update channels with the resulting Logical Channel Numbers
    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Process Logical Channel Numbers"));
    for (auto & transport : transports)
    {
        for (auto & channel : transport.m_channels)
        {
            if (channel.m_chanNum.isEmpty())
            {
                qlonglong key = ((qlonglong)channel.m_origNetId<<32) | channel.m_serviceId;
                QMap<qlonglong, uint>::const_iterator it = map_sid_lcn.constFind(key);
                if (it != map_sid_lcn.cend())
                {
                    channel.m_chanNum = QString::number(*it + m_lcnOffset);
                    LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
                        QString("Final channel sid:%1 channel %2")
                            .arg(channel.m_serviceId).arg(channel.m_chanNum));
                }
                else
                {
                    LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
                        QString("Final channel sid:%1 NO channel number")
                            .arg(channel.m_serviceId));
                }
            }
            else
            {
                LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
                    QString("Final channel sid:%1 has already channel number %2")
                        .arg(channel.m_serviceId).arg(channel.m_chanNum));
            }
        }
    }
}

/** \fn ChannelImporter::GetDBTransports(uint,ScanDTVTransportList&) const
 *  \brief Adds found channel info to transports list,
 *         returns channels in DB which were not found in scan
 *         in another transport list. This can be the same transport
 *         if e.g. one channel is in the DB but not in the scan, but
 *         it can also contain transports that are not found in the scan.
 */
ScanDTVTransportList ChannelImporter::GetDBTransports(
    uint sourceid, ScanDTVTransportList &transports) const
{
    ScanDTVTransportList not_in_scan;
    int found_in_same_transport = 0;
    int found_in_other_transport = 0;
    int found_nowhere = 0;

    DTVTunerType tuner_type(DTVTunerType::kTunerTypeATSC);
    if (!transports.empty())
        tuner_type = transports[0].m_tunerType;

    bool is_dvbs =
        (DTVTunerType::kTunerTypeDVBS1 == tuner_type) ||
        (DTVTunerType::kTunerTypeDVBS2 == tuner_type);

    uint freq_mult = (is_dvbs) ? 1 : 1000;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE sourceid = :SOURCEID "
        "GROUP BY mplexid "
        "ORDER BY mplexid");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
    {
        MythDB::DBError("GetDBTransports()", query);
        return not_in_scan;
    }

    QMap<uint,bool> found_in_scan;
    while (query.next())
    {
        ScanDTVTransport db_transport;
        uint mplexid = query.value(0).toUInt();
        if (db_transport.FillFromDB(tuner_type, mplexid))
        {
            if (db_transport.m_channels.empty())
            {
                continue;
            }
        }

        bool found_transport = false;
        QMap<uint,bool> found_in_database;

        // Search for old channels in the same transport of the scan.
        for (size_t ist = 0; ist < transports.size(); ++ist)                                // All transports in scan
        {
            ScanDTVTransport &scan_transport = transports[ist];                             // Transport from the scan
            if (scan_transport.IsEqual(tuner_type, db_transport, 500 * freq_mult, true))    // Same transport?
            {
                found_transport = true;                                                     // Yes
                scan_transport.m_mplex = db_transport.m_mplex;                              // Found multiplex
                for (size_t jdc = 0; jdc < db_transport.m_channels.size(); ++jdc)           // All channels in database transport
                {
                    if (!found_in_database[jdc])                                            // Channel not found yet?
                    {
                        ChannelInsertInfo &db_chan = db_transport.m_channels[jdc];          // Channel in database transport
                        for (size_t ksc = 0; ksc < scan_transport.m_channels.size(); ++ksc) // All channels in scanned transport
                        {                                                                   // Channel in scanned transport
                            if (!found_in_scan[(ist<<16)+ksc])                              // Scanned channel not yet found?
                            {
                                ChannelInsertInfo &scan_chan = scan_transport.m_channels[ksc];
                                if (db_chan.IsSameChannel(scan_chan, 2))                    // Same transport, relaxed check
                                {
                                    found_in_same_transport++;
                                    found_in_database[jdc] = true;                          // Channel from db found in scan
                                    found_in_scan[(ist<<16)+ksc] = true;                    // Channel from scan found in db
                                    scan_chan.m_dbMplexId = db_transport.m_mplex;           // Found multiplex
                                    scan_chan.m_channelId = db_chan.m_channelId;            // This is the crucial field
                                    break;                                                  // Ready with scanned transport
                                }
                            }
                        }
                    }
                }
            }
        }

        // Search for old channels in all transports of the scan.
        // This is done for all channels that have not yet been found.
        // This can identify the channels that have moved to another transport.
        if (m_fullChannelSearch)
        {
            for (size_t ist = 0; ist < transports.size(); ++ist)                            // All transports in scan
            {
                ScanDTVTransport &scan_transport = transports[ist];                         // Scanned transport
                for (size_t jdc = 0; jdc < db_transport.m_channels.size(); ++jdc)           // All channels in database transport
                {
                    if (!found_in_database[jdc])                                            // Channel not found yet?
                    {
                        ChannelInsertInfo &db_chan = db_transport.m_channels[jdc];          // Channel in database transport
                        for (size_t ksc = 0; ksc < scan_transport.m_channels.size(); ++ksc) // All channels in scanned transport
                        {
                            if (!found_in_scan[(ist<<16)+ksc])                              // Scanned channel not yet found?
                            {
                                ChannelInsertInfo &scan_chan = scan_transport.m_channels[ksc];
                                if (db_chan.IsSameChannel(scan_chan, 1))                    // Other transport, check
                                {                                                           // network id and service id
                                    found_in_other_transport++;
                                    found_in_database[jdc] = true;                          // Channel from db found in scan
                                    found_in_scan[(ist<<16)+ksc] = true;                    // Channel from scan found in db
                                    scan_chan.m_channelId = db_chan.m_channelId;            // This is the crucial field
                                    break;                                                  // Ready with scanned transport
                                }
                            }
                        }
                    }
                }
            }
        }

        // If the transport in the database is found in the scan
        // then all channels in that transport that are not found
        // in the scan are copied to the "not_in_scan" list.
        if (found_transport)
        {
            ScanDTVTransport tmp = db_transport;
            tmp.m_channels.clear();

            for (size_t idc = 0; idc < db_transport.m_channels.size(); ++idc)
            {
                if (!found_in_database[idc])
                {
                    tmp.m_channels.push_back(db_transport.m_channels[idc]);
                    found_nowhere++;
                }
            }

            if (!tmp.m_channels.empty())
                not_in_scan.push_back(tmp);
        }
    }
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Old channels found in same transport: %1")
            .arg(found_in_same_transport));
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Old channels found in other transport: %1")
            .arg(found_in_other_transport));
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Old channels not found (off-air): %1")
            .arg(found_nowhere));

    return not_in_scan;
}

void ChannelImporter::FixUpOpenCable(ScanDTVTransportList &transports)
{
    ChannelImporterBasicStats info;
    for (auto & transport : transports)
    {
        for (auto & chan : transport.m_channels)
        {
            if (((chan.m_couldBeOpencable && (chan.m_siStandard == "mpeg")) ||
                 chan.m_isOpencable) && !chan.m_inVct)
            {
                chan.m_siStandard = "opencable";
            }
        }
    }
}

ChannelImporterBasicStats ChannelImporter::CollectStats(
    const ScanDTVTransportList &transports)
{
    ChannelImporterBasicStats info;
    for (const auto & transport : transports)
    {
        for (const auto & chan : transport.m_channels)
        {
            int enc {0};
            if (chan.m_isEncrypted)
                enc = (chan.m_decryptionStatus == kEncDecrypted) ? 2 : 1;
            if (chan.m_siStandard == "atsc")      info.m_atscChannels[enc] += 1;
            if (chan.m_siStandard == "dvb")       info.m_dvbChannels[enc]  += 1;
            if (chan.m_siStandard == "mpeg")      info.m_mpegChannels[enc] += 1;
            if (chan.m_siStandard == "opencable") info.m_scteChannels[enc] += 1;
            if (chan.m_siStandard == "ntsc")      info.m_ntscChannels[enc] += 1;
            if (chan.m_siStandard != "ntsc")
            {
                ++info.m_progNumCnt[chan.m_serviceId];
                ++info.m_chanNumCnt[map_str(chan.m_chanNum)];
            }
            if (chan.m_siStandard == "atsc")
            {
                ++info.m_atscNumCnt[(chan.m_atscMajorChannel << 16) |
                                     (chan.m_atscMinorChannel)];
                ++info.m_atscMinCnt[chan.m_atscMinorChannel];
                ++info.m_atscMajCnt[chan.m_atscMajorChannel];
            }
            if (chan.m_siStandard == "ntsc")
            {
                ++info.m_atscNumCnt[(chan.m_atscMajorChannel << 16) |
                                     (chan.m_atscMinorChannel)];
            }
        }
    }

    return info;
}

ChannelImporterUniquenessStats ChannelImporter::CollectUniquenessStats(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info)
{
    ChannelImporterUniquenessStats stats;

    for (const auto & transport : transports)
    {
        for (const auto & chan : transport.m_channels)
        {
            stats.m_uniqueProgNum +=
                (info.m_progNumCnt[chan.m_serviceId] == 1) ? 1 : 0;
            stats.m_uniqueChanNum +=
                (info.m_chanNumCnt[map_str(chan.m_chanNum)] == 1) ? 1 : 0;

            if (chan.m_siStandard == "atsc")
            {
                stats.m_uniqueAtscNum +=
                    (info.m_atscNumCnt[(chan.m_atscMajorChannel << 16) |
                                        (chan.m_atscMinorChannel)] == 1) ? 1 : 0;
                stats.m_uniqueAtscMin +=
                    (info.m_atscMinCnt[(chan.m_atscMinorChannel)] == 1) ? 1 : 0;
                stats.m_maxAtscMajCnt = std::max(
                    stats.m_maxAtscMajCnt,
                    info.m_atscMajCnt[chan.m_atscMajorChannel]);
            }
        }
    }

    stats.m_uniqueTotal = (stats.m_uniqueProgNum + stats.m_uniqueAtscNum +
                          stats.m_uniqueAtscMin + stats.m_uniqueChanNum);

    return stats;
}


QString ChannelImporter::FormatChannel(
    const ScanDTVTransport          &transport,
    const ChannelInsertInfo         &chan,
    const ChannelImporterBasicStats *info)
{
    QString msg;
    QTextStream ssMsg(&msg);

    ssMsg << transport.m_frequency << ":";

    QString si_standard = (chan.m_siStandard=="opencable") ?
        QString("scte") : chan.m_siStandard;

    if (si_standard == "atsc" || si_standard == "scte")
    {
        ssMsg << (QString("%1:%2:%3-%4:%5:%6=%7=%8:%9")
                  .arg(chan.m_callSign, chan.m_chanNum)
                  .arg(chan.m_atscMajorChannel)
                  .arg(chan.m_atscMinorChannel)
                  .arg(chan.m_serviceId)
                  .arg(chan.m_vctTsId)
                  .arg(chan.m_vctChanTsId)
                  .arg(chan.m_patTsId)
                  .arg(si_standard)).toLatin1().constData();
    }
    else if (si_standard == "dvb")
    {
        ssMsg << (QString("%1:%2:%3:%4:%5:%6=%7:%8")
                  .arg(chan.m_serviceName, chan.m_chanNum)
                  .arg(chan.m_netId).arg(chan.m_origNetId)
                  .arg(chan.m_serviceId)
                  .arg(chan.m_sdtTsId)
                  .arg(chan.m_patTsId)
                  .arg(si_standard)).toLatin1().constData();
    }
    else
    {
        ssMsg << (QString("%1:%2:%3:%4:%5")
                  .arg(chan.m_callSign, chan.m_chanNum)
                  .arg(chan.m_serviceId)
                  .arg(chan.m_patTsId)
                  .arg(si_standard)).toLatin1().constData();
    }

    if (info)
    {
        ssMsg << ' ';
        msg = msg.leftJustified(72);

        ssMsg << chan.m_channelId;

        ssMsg << ":"
              << (QString("cnt(pnum:%1,channum:%2)")
                  .arg(info->m_progNumCnt[chan.m_serviceId])
                  .arg(info->m_chanNumCnt[map_str(chan.m_chanNum)])
                  ).toLatin1().constData();

        if (chan.m_siStandard == "atsc")
        {
            ssMsg <<
                (QString(":atsc_cnt(tot:%1,minor:%2)")
                 .arg(info->m_atscNumCnt[
                          (chan.m_atscMajorChannel << 16) |
                          (chan.m_atscMinorChannel)])
                 .arg(info->m_atscMinCnt[chan.m_atscMinorChannel])
                    ).toLatin1().constData();
        }
    }

    return msg;
}

/**
 * \fn ChannelImporter::SimpleFormatChannel
 *
 * Format channel information into a simple string. The format of this
 * string will depend on the type of standard used for the channels
 * (atsc/scte/opencable/dvb).
 *
 * \param transport  Unused.
 * \param chan       Info describing a channel
 * \return Returns a simple name for the channel.
 */
QString ChannelImporter::SimpleFormatChannel(
    const ScanDTVTransport          &/*transport*/,
    const ChannelInsertInfo         &chan)
{
    QString msg;
    QTextStream ssMsg(&msg);

    QString si_standard = (chan.m_siStandard=="opencable") ?
        QString("scte") : chan.m_siStandard;

    if (si_standard == "atsc" || si_standard == "scte")
    {
        if (si_standard == "atsc")
        {
            ssMsg << (kATSCChannelFormat
                  .arg(chan.m_atscMajorChannel)
                  .arg(chan.m_atscMinorChannel)).toLatin1().constData();
        }
        else if (chan.m_freqId.isEmpty())
        {
            ssMsg << (QString("%1-%2")
                  .arg(chan.m_sourceId)
                  .arg(chan.m_serviceId)).toLatin1().constData();
        }
        else
        {
            ssMsg << (QString("%1-%2")
                  .arg(chan.m_freqId)
                  .arg(chan.m_serviceId)).toLatin1().constData();
        }

        if (!chan.m_callSign.isEmpty())
            ssMsg << (QString(" (%1)")
                  .arg(chan.m_callSign)).toLatin1().constData();
    }
    else if (si_standard == "dvb")
    {
        ssMsg << (QString("%1 (%2 %3)")
                  .arg(chan.m_serviceName).arg(chan.m_serviceId)
                  .arg(chan.m_netId)).toLatin1().constData();
    }
    else if (chan.m_freqId.isEmpty())
    {
        ssMsg << (QString("%1-%2")
                  .arg(chan.m_sourceId).arg(chan.m_serviceId))
                  .toLatin1().constData();
    }
    else
    {
        ssMsg << (QString("%1-%2")
                  .arg(chan.m_freqId).arg(chan.m_serviceId))
                  .toLatin1().constData();
    }

    return msg;
}

QString ChannelImporter::FormatChannels(
    const ScanDTVTransportList      &transports_in,
    const ChannelImporterBasicStats *info)
{
    // Sort transports in order of increasing frequency
    struct less_than_key
    {
        bool operator() (const ScanDTVTransport &t1, const ScanDTVTransport &t2)
        {
            return t1.m_frequency < t2.m_frequency;
        }
    };
    ScanDTVTransportList transports(transports_in);
    std::sort(transports.begin(), transports.end(), less_than_key());

    QString msg;

    for (auto & transport : transports)
    {
        auto fmt_chan = [transport, info](const QString & m, const auto & chan)
            { return m + FormatChannel(transport, chan, info) + "\n"; };
        msg = std::accumulate(transport.m_channels.cbegin(), transport.m_channels.cend(),
                              msg, fmt_chan);
    }

    return msg;
}

QString ChannelImporter::FormatTransport(
    const ScanDTVTransport &transport)
{
    QString msg;
    QTextStream ssMsg(&msg);
    ssMsg << transport.toString();
    ssMsg << QString(" onid:%1").arg(transport.m_networkID);
    ssMsg << QString(" tsid:%1").arg(transport.m_transportID);
    ssMsg << QString(" ss:%1").arg(transport.m_signalStrength);
    return msg;
}

QString ChannelImporter::FormatTransports(
    const ScanDTVTransportList      &transports_in)
{
    // Sort transports in order of increasing frequency
    struct less_than_key
    {
        bool operator() (const ScanDTVTransport &t1, const ScanDTVTransport &t2)
        {
            return t1.m_frequency < t2.m_frequency;
        }
    };
    ScanDTVTransportList transports(transports_in);
    std::sort(transports.begin(), transports.end(), less_than_key());

    auto fmt_trans = [](const QString& msg, const auto & transport)
        { return msg + FormatTransport(transport) + "\n"; };
    return std::accumulate(transports.cbegin(), transports.cend(),
                           QString(), fmt_trans);
}

QString ChannelImporter::GetSummary(
    const ChannelImporterBasicStats      &info,
    const ChannelImporterUniquenessStats &stats)
{
    QString msg = tr("Channels: FTA Enc Dec\n") +
        QString("ATSC      %1 %2 %3\n")
            .arg(info.m_atscChannels[0],3)
            .arg(info.m_atscChannels[1],3)
            .arg(info.m_atscChannels[2],3) +
        QString("DVB       %1 %2 %3\n")
            .arg(info.m_dvbChannels [0],3)
            .arg(info.m_dvbChannels [1],3)
            .arg(info.m_dvbChannels [2],3) +
        QString("SCTE      %1 %2 %3\n")
            .arg(info.m_scteChannels[0],3)
            .arg(info.m_scteChannels[1],3)
            .arg(info.m_scteChannels[2],3) +
        QString("MPEG      %1 %2 %3\n")
            .arg(info.m_mpegChannels[0],3)
            .arg(info.m_mpegChannels[1],3)
            .arg(info.m_mpegChannels[2],3) +
        QString("NTSC      %1\n")
            .arg(info.m_ntscChannels[0],3) +
        tr("Unique: prog %1 atsc %2 atsc minor %3 channum %4\n")
            .arg(stats.m_uniqueProgNum).arg(stats.m_uniqueAtscNum)
            .arg(stats.m_uniqueAtscMin).arg(stats.m_uniqueChanNum) +
        tr("Max atsc major count: %1")
            .arg(stats.m_maxAtscMajCnt);

    return msg;
}

bool ChannelImporter::IsType(
    const ChannelImporterBasicStats &info,
    const ChannelInsertInfo &chan, ChannelType type)
{
    switch (type)
    {
        case kATSCNonConflicting:
            return ((chan.m_siStandard == "atsc") /* &&
                    (info.m_atscNumCnt[(chan.m_atscMajorChannel << 16) |
                                        (chan.m_atscMinorChannel)] == 1) */);

        case kDVBNonConflicting:
            return ((chan.m_siStandard == "dvb") /* &&
                    (info.m_progNumCnt[chan.m_serviceId] == 1) */);

        case kMPEGNonConflicting:
            return ((chan.m_siStandard == "mpeg") &&
                    (info.m_chanNumCnt[map_str(chan.m_chanNum)] == 1));

        case kSCTENonConflicting:
            return (((chan.m_siStandard == "scte") ||
                    (chan.m_siStandard == "opencable")) &&
                    (info.m_chanNumCnt[map_str(chan.m_chanNum)] == 1));

        case kNTSCNonConflicting:
            return ((chan.m_siStandard == "ntsc") &&
                    (info.m_atscNumCnt[(chan.m_atscMajorChannel << 16) |
                                        (chan.m_atscMinorChannel)] == 1));

        case kATSCConflicting:
            return ((chan.m_siStandard == "atsc") &&
                    (info.m_atscNumCnt[(chan.m_atscMajorChannel << 16) |
                                        (chan.m_atscMinorChannel)] != 1));

        case kDVBConflicting:
            return ((chan.m_siStandard == "dvb") &&
                    (info.m_progNumCnt[chan.m_serviceId] != 1));

        case kMPEGConflicting:
            return ((chan.m_siStandard == "mpeg") &&
                    (info.m_chanNumCnt[map_str(chan.m_chanNum)] != 1));

        case kSCTEConflicting:
            return (((chan.m_siStandard == "scte") ||
                    (chan.m_siStandard == "opencable")) &&
                    (info.m_chanNumCnt[map_str(chan.m_chanNum)] != 1));

        case kNTSCConflicting:
            return ((chan.m_siStandard == "ntsc") &&
                    (info.m_atscNumCnt[(chan.m_atscMajorChannel << 16) |
                                        (chan.m_atscMinorChannel)] != 1));
    }
    return false;
}

void ChannelImporter::CountChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    ChannelType type, uint &new_chan, uint &old_chan)
{
    new_chan = old_chan = 0;
    for (const auto & transport : transports)
    {
        for (const auto& chan : transport.m_channels)
        {
            if (IsType(info, chan, type))
            {
                if (chan.m_channelId)
                    ++old_chan;
                else
                    ++new_chan;
            }
        }
    }
}

int ChannelImporter::SimpleCountChannels(
    const ScanDTVTransportList &transports)
{
    auto add_count = [](int count, const auto & transport)
        { return count + transport.m_channels.size(); };
    return std::accumulate(transports.cbegin(), transports.cend(),
                           0, add_count);
}

/**
 * \fn ChannelImporter::ComputeSuggestedChannelNum
 *
 * Compute a suggested channel number that is unique in the video source.
 * Check first to see if the existing channel number conflicts
 * with an existing channel number. If so, try adding a suffix
 * starting with 'A' to make the number unique while still being
 * recognizable. For instance, the second "7-2" channel will be called "7-2A".
 * If this fails then fall back to incrementing a
 * per-source number to find an unused value.
 *
 * \param chan       Info describing a channel
 * \return Returns a simple name for the channel.
 */
QString ChannelImporter::ComputeSuggestedChannelNum(
    const ChannelInsertInfo         &chan)
{
    static QMutex          s_lastFreeLock;
    static QMap<uint,uint> s_lastFreeChanNumMap;
    QString chanNum;

    // Suggest existing channel number if non-conflicting
    if (!ChannelUtil::IsConflicting(chan.m_chanNum, chan.m_sourceId))
        return chan.m_chanNum;

    // Add a suffix to make it unique
    for (char suffix = 'A'; suffix <= 'Z'; ++suffix)
    {
        chanNum = chan.m_chanNum + suffix;
        if (!ChannelUtil::IsConflicting(chanNum, chan.m_sourceId))
            return chanNum;
    }

    // Find unused channel number
    QMutexLocker locker(&s_lastFreeLock);
    uint last_free_chan_num = s_lastFreeChanNumMap[chan.m_sourceId];
    for (last_free_chan_num++; ; ++last_free_chan_num)
    {
        chanNum = QString::number(last_free_chan_num);
        if (!ChannelUtil::IsConflicting(chanNum, chan.m_sourceId))
            break;
    }
    s_lastFreeChanNumMap[chan.m_sourceId] = last_free_chan_num;

    return chanNum;
}

ChannelImporter::DeleteAction
ChannelImporter::QueryUserDelete(const QString &msg)
{
    DeleteAction action = kDeleteAll;
    if (m_useGui)
    {
        m_functorRetval = -1;
        while (m_functorRetval < 0)
        {
            if (m_useWeb) {
                m_pWeb->m_mutex.lock();
                m_pWeb->m_dlgMsg = msg;
                m_pWeb->m_dlgButtons.append(tr("Delete All"));
                m_pWeb->m_dlgButtons.append(tr("Set all invisible"));
                m_pWeb->m_dlgButtons.append(tr("Ignore All"));
                m_pWeb->m_waitCondition.wait(&m_pWeb->m_mutex);
                m_functorRetval = m_pWeb->m_dlgButton;
                m_pWeb->m_mutex.unlock();
                continue;
            }

            MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
            auto *deleteDialog =
                new MythDialogBox(msg, popupStack, "deletechannels");

            if (deleteDialog->Create())
            {
                deleteDialog->AddButton(tr("Delete All"));
                deleteDialog->AddButton(tr("Set all invisible"));
//                  deleteDialog->AddButton(tr("Handle manually"));
                deleteDialog->AddButton(tr("Ignore All"));
                QObject::connect(deleteDialog, &MythDialogBox::Closed, this,
                                 [this](const QString & /*resultId*/, int result)
                                 {
                                     m_functorRetval = result;
                                     m_eventLoop.quit();
                                 });
                popupStack->AddScreen(deleteDialog);

                m_eventLoop.exec();
            }
        }

        switch (m_functorRetval)
        {
          case 0: action = kDeleteAll;          break;
          case 1: action = kDeleteInvisibleAll; break;
          case 2: action = kDeleteIgnoreAll;    break;
        }
    }
    else if (m_isInteractive)
    {
        std::cout << msg.toLatin1().constData()
             << std::endl
             << tr("Do you want to:").toLatin1().constData()
             << std::endl
             << tr("1. Delete All").toLatin1().constData()
             << std::endl
             << tr("2. Set all invisible").toLatin1().constData()
             << std::endl
//        cout << "3. Handle manually" << endl;
             << tr("4. Ignore All").toLatin1().constData()
             << std::endl;
        while (true)
        {
            std::string ret;
            std::cin >> ret;
            bool ok = false;
            uint val = QString(ret.c_str()).toUInt(&ok);
            if (ok && (val == 1 || val == 2 || val == 4))
            {
                action = (1 == val) ? kDeleteAll       : action;
                action = (2 == val) ? kDeleteInvisibleAll : action;
                //action = (3 == val) ? kDeleteManual    : action;
                action = (4 == val) ? kDeleteIgnoreAll : action;
                break;
            }

            //cout << "Please enter either 1, 2, 3 or 4:" << endl;
            std::cout << tr("Please enter either 1, 2 or 4:")
                .toLatin1().constData() << std::endl;
        }
    }

    return action;
}

ChannelImporter::InsertAction
ChannelImporter::QueryUserInsert(const QString &msg)
{
    InsertAction action = kInsertAll;
    if (m_useGui)
    {
        m_functorRetval = -1;
        while (m_functorRetval < 0)
        {
            if (m_useWeb) {
                m_pWeb->m_mutex.lock();
                m_pWeb->m_dlgMsg = msg;
                m_pWeb->m_dlgButtons.append(tr("Insert All"));
                m_pWeb->m_dlgButtons.append(tr("Insert Manually"));
                m_pWeb->m_dlgButtons.append(tr("Ignore All"));
                m_pWeb->m_waitCondition.wait(&m_pWeb->m_mutex);
                m_functorRetval = m_pWeb->m_dlgButton;
                m_pWeb->m_mutex.unlock();
                continue;
            }

            MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
            auto *insertDialog =
                new MythDialogBox(msg, popupStack, "insertchannels");

            if (insertDialog->Create())
            {
                insertDialog->AddButton(tr("Insert All"));
                insertDialog->AddButton(tr("Insert Manually"));
                insertDialog->AddButton(tr("Ignore All"));
                QObject::connect(insertDialog, &MythDialogBox::Closed, this,
                                 [this](const QString & /*resultId*/, int result)
                                 {
                                     m_functorRetval = result;
                                     m_eventLoop.quit();
                                 });

                popupStack->AddScreen(insertDialog);
                m_eventLoop.exec();
            }
        }

        switch (m_functorRetval)
        {
          case 0: action = kInsertAll;       break;
          case 1: action = kInsertManual;    break;
          case 2: action = kInsertIgnoreAll; break;
        }
    }
    else if (m_isInteractive)
    {
        std::cout << msg.toLatin1().constData()
             << std::endl
             << tr("Do you want to:").toLatin1().constData()
             << std::endl
             << tr("1. Insert All").toLatin1().constData()
             << std::endl
             << tr("2. Insert Manually").toLatin1().constData()
             << std::endl
             << tr("3. Ignore All").toLatin1().constData()
             << std::endl;
        while (true)
        {
            std::string ret;
            std::cin >> ret;
            bool ok = false;
            uint val = QString(ret.c_str()).toUInt(&ok);
            if (ok && (1 <= val) && (val <= 3))
            {
                action = (1 == val) ? kInsertAll       : action;
                action = (2 == val) ? kInsertManual    : action;
                action = (3 == val) ? kInsertIgnoreAll : action;
                break;
            }

            std::cout << tr("Please enter either 1, 2, or 3:")
                .toLatin1().constData() << std::endl;
        }
    }

    m_functorRetval = 0;    // Reset default menu choice to first item for next menu
    return action;
}

ChannelImporter::UpdateAction
ChannelImporter::QueryUserUpdate(const QString &msg)
{
    UpdateAction action = kUpdateAll;

    if (m_useGui)
    {
        m_functorRetval = -1;
        while (m_functorRetval < 0)
        {
            if (m_useWeb) {
                m_pWeb->m_mutex.lock();
                m_pWeb->m_dlgMsg = msg;
                m_pWeb->m_dlgButtons.append(tr("Update All"));
                m_pWeb->m_dlgButtons.append(tr("Ignore All"));
                m_pWeb->m_waitCondition.wait(&m_pWeb->m_mutex);
                m_functorRetval = m_pWeb->m_dlgButton;
                m_pWeb->m_mutex.unlock();
                continue;
            }

            MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
            auto *updateDialog =
                new MythDialogBox(msg, popupStack, "updatechannels");

            if (updateDialog->Create())
            {
                updateDialog->AddButton(tr("Update All"));
                updateDialog->AddButton(tr("Ignore All"));
                QObject::connect(updateDialog, &MythDialogBox::Closed, this,
                                 [this](const QString& /*resultId*/, int result)
                                 {
                                     m_functorRetval = result;
                                     m_eventLoop.quit();
                                 });

                popupStack->AddScreen(updateDialog);
                m_eventLoop.exec();
            }
        }

        switch (m_functorRetval)
        {
          case 0: action = kUpdateAll;       break;
          case 1: action = kUpdateIgnoreAll; break;
        }
    }
    else if (m_isInteractive)
    {
        std::cout << msg.toLatin1().constData()
             << std::endl
             << tr("Do you want to:").toLatin1().constData()
             << std::endl
             << tr("1. Update All").toLatin1().constData()
             << std::endl
             << tr("2. Update Manually").toLatin1().constData()
             << std::endl
             << tr("3. Ignore All").toLatin1().constData()
             << std::endl;
        while (true)
        {
            std::string ret;
            std::cin >> ret;
            bool ok = false;
            uint val = QString(ret.c_str()).toUInt(&ok);
            if (ok && (1 <= val) && (val <= 3))
            {
                action = (1 == val) ? kUpdateAll       : action;
                action = (2 == val) ? kUpdateManual    : action;
                action = (3 == val) ? kUpdateIgnoreAll : action;
                break;
            }

            std::cout << tr("Please enter either 1, 2, or 3:")
                .toLatin1().constData() << std::endl;
        }
    }
    m_functorRetval = 0;    // Reset default menu choice to first item for next menu
    return action;
}

OkCancelType ChannelImporter::ShowManualChannelPopup(
    const QString& title,
    const QString& message, QString &text)
{
    int dmc = m_functorRetval;      // Default menu choice
    m_functorRetval = -1;

    MythScreenStack *popupStack = nullptr;
    if (m_useWeb) {
        m_pWeb->m_mutex.lock();
        m_pWeb->m_dlgMsg = message;
        m_pWeb->m_dlgButtons.append(tr("OK"));
        m_pWeb->m_dlgButtons.append(tr("Edit"));
        m_pWeb->m_dlgButtons.append(tr("Cancel"));
        m_pWeb->m_dlgButtons.append(tr("Cancel All"));
        m_pWeb->m_waitCondition.wait(&m_pWeb->m_mutex);
        m_functorRetval = m_pWeb->m_dlgButton;
        m_pWeb->m_mutex.unlock();
    }
    else
    {
        MythMainWindow *parent = GetMythMainWindow();
        popupStack = parent->GetStack("popup stack");
        auto *popup = new MythDialogBox(title, message, popupStack,
                                        "manualchannelpopup");

        if (popup->Create())
        {
            popup->AddButtonD(QCoreApplication::translate("(Common)", "OK"), 0 == dmc);
            popup->AddButtonD(tr("Edit"), 1 == dmc);
            popup->AddButtonD(QCoreApplication::translate("(Common)", "Cancel"), 2 == dmc);
            popup->AddButtonD(QCoreApplication::translate("(Common)", "Cancel All"), 3 == dmc);
            QObject::connect(popup, &MythDialogBox::Closed, this,
                            [this](const QString & /*resultId*/, int result)
                            {
                                m_functorRetval = result;
                                m_eventLoop.quit();
                            });
            popupStack->AddScreen(popup);
            m_eventLoop.exec();
        }
        else
        {
            delete popup;
            popup = nullptr;
        }
    }
    // Choice "Edit"
    if (1 == m_functorRetval)
    {

        if (m_useWeb) {
            m_pWeb->m_mutex.lock();
            m_pWeb->m_dlgMsg = tr("Please enter a unique channel number.");
            m_pWeb->m_dlgInputReq = true;
            m_pWeb->m_waitCondition.wait(&m_pWeb->m_mutex);
            m_functorRetval = m_pWeb->m_dlgButton;
            text = m_pWeb->m_dlgString;
            m_pWeb->m_mutex.unlock();
        }
        else
        {
            auto *textEdit =
                new MythTextInputDialog(popupStack,
                                        tr("Please enter a unique channel number."),
                                        FilterNone, false, text);
            if (textEdit->Create())
            {
                QObject::connect(textEdit, &MythTextInputDialog::haveResult, this,
                                [this,&text](QString result)
                                {
                                    m_functorRetval = 0;
                                    text = std::move(result);
                                });
                QObject::connect(textEdit, &MythTextInputDialog::Exiting, this,
                                [this]()
                                {
                                    m_eventLoop.quit();
                                });

                popupStack->AddScreen(textEdit);
                m_eventLoop.exec();
            }
            else
            {
                delete textEdit;
            }
        }
    }
    OkCancelType rval = kOCTCancel;
    switch (m_functorRetval) {
        case 0: rval = kOCTOk;        break;
        // NOLINTNEXTLINE(bugprone-branch-clone)
        case 1: rval = kOCTCancel;    break;    // "Edit" is done already
        case 2: rval = kOCTCancel;    break;
        case 3: rval = kOCTCancelAll; break;
    }
    return rval;
}

OkCancelType ChannelImporter::ShowResolveChannelPopup(
    const QString& title,
    const QString& message, QString &text)
{
    int dmc = m_functorRetval;      // Default menu choice
    m_functorRetval = -1;

    MythScreenStack *popupStack = nullptr;
    if (m_useWeb) {
        m_pWeb->m_mutex.lock();
        m_pWeb->m_dlgMsg = message;
        m_pWeb->m_dlgButtons.append(tr("OK"));
        m_pWeb->m_dlgButtons.append(tr("OK All"));
        m_pWeb->m_dlgButtons.append(tr("Edit"));
        m_pWeb->m_dlgButtons.append(tr("Cancel"));
        m_pWeb->m_dlgButtons.append(tr("Cancel All"));
        m_pWeb->m_waitCondition.wait(&m_pWeb->m_mutex);
        m_functorRetval = m_pWeb->m_dlgButton;
        m_pWeb->m_mutex.unlock();
    }
    else
    {
        MythMainWindow *parent = GetMythMainWindow();
        popupStack = parent->GetStack("popup stack");
        auto *popup = new MythDialogBox(title, message, popupStack,
                                        "resolvechannelpopup");

        if (popup->Create())
        {
            popup->AddButtonD(QCoreApplication::translate("(Common)", "OK"), 0 == dmc);
            popup->AddButtonD(QCoreApplication::translate("(Common)", "OK All"), 1 == dmc);
            popup->AddButtonD(tr("Edit"), 2 == dmc);
            popup->AddButtonD(QCoreApplication::translate("(Common)", "Cancel"), 3 == dmc);
            popup->AddButtonD(QCoreApplication::translate("(Common)", "Cancel All"), 4 == dmc);
            QObject::connect(popup, &MythDialogBox::Closed, this,
                            [this](const QString & /*resultId*/, int result)
                            {
                                m_functorRetval = result;
                                m_eventLoop.quit();
                            });
            popupStack->AddScreen(popup);
            m_eventLoop.exec();
        }
        else
        {
            delete popup;
            popup = nullptr;
        }
    }
    // Choice "Edit"
    if (2 == m_functorRetval)
    {
        if (m_useWeb) {
            m_pWeb->m_mutex.lock();
            m_pWeb->m_dlgMsg = tr("Please enter a unique channel number.");
            m_pWeb->m_dlgInputReq = true;
            m_pWeb->m_waitCondition.wait(&m_pWeb->m_mutex);
            m_functorRetval = m_pWeb->m_dlgButton;
            text = m_pWeb->m_dlgString;
            m_pWeb->m_mutex.unlock();
        }
        else
        {
            auto *textEdit =
                new MythTextInputDialog(popupStack,
                                        tr("Please enter a unique channel number."),
                                        FilterNone, false, text);
            if (textEdit->Create())
            {
                QObject::connect(textEdit, &MythTextInputDialog::haveResult, this,
                                [this,&text](QString result)
                                {
                                    m_functorRetval = 0;
                                    text = std::move(result);
                                });
                QObject::connect(textEdit, &MythTextInputDialog::Exiting, this,
                                [this]()
                                {
                                    m_eventLoop.quit();
                                });

                popupStack->AddScreen(textEdit);
                m_eventLoop.exec();
            }
            else
            {
                delete textEdit;
            }
        }
    }

    OkCancelType rval = kOCTCancel;
    switch (m_functorRetval) {
        case 0: rval = kOCTOk;        break;
        case 1: rval = kOCTOkAll;     break;
        // NOLINTNEXTLINE(bugprone-branch-clone)
        case 2: rval = kOCTCancel;    break;    // "Edit" is done already
        case 3: rval = kOCTCancel;    break;
        case 4: rval = kOCTCancelAll; break;
    }
    return rval;
}

OkCancelType ChannelImporter::QueryUserResolve(
    const ScanDTVTransport          &transport,
    ChannelInsertInfo               &chan)
{
    QString msg = tr("Channel %1 has channel number %2 but that is already in use.")
                    .arg(SimpleFormatChannel(transport, chan),
                         chan.m_chanNum);

    OkCancelType ret = kOCTCancel;

    if (m_useGui)
    {
        while (true)
        {
            QString msg2 = msg;
            msg2 += "\n";
            msg2 += tr("Please enter a unique channel number.");

            QString val = ComputeSuggestedChannelNum(chan);
            msg2 += "\n";
            msg2 += tr("Default value is %1.").arg(val);
            ret = ShowResolveChannelPopup(
                    tr("Channel Importer"),
                msg2, val);

            if (kOCTOk != ret && kOCTOkAll != ret)
                break; // user canceled..

            bool ok = CheckChannelNumber(val, chan);
            if (ok)
            {
                chan.m_chanNum = val;
                break;
            }
        }
    }
    else if (m_isInteractive)
    {
        std::cout << msg.toLatin1().constData() << std::endl;

        QString cancelStr = QCoreApplication::translate("(Common)",
                                                        "Cancel").toLower();
        QString cancelAllStr = QCoreApplication::translate("(Common)",
                                   "Cancel All").toLower();
        QString msg2 = tr("Please enter a non-conflicting channel number "
                          "(or type '%1' to skip, '%2' to skip all):")
            .arg(cancelStr, cancelAllStr);

        while (true)
        {
            std::cout << msg2.toLatin1().constData() << std::endl;
            std::string sret;
            std::cin >> sret;
            QString val = QString(sret.c_str());
            if (val.toLower() == cancelStr)
            {
                ret = kOCTCancel;
                break; // user canceled..
            }
            if (val.toLower() == cancelAllStr)
            {
                ret = kOCTCancelAll;
                break; // user canceled..
            }

            bool ok = CheckChannelNumber(val, chan);
            if (ok)
            {
                chan.m_chanNum = val;
                ret = kOCTOk;
                break;
            }
        }
    }

    return ret;
}

OkCancelType ChannelImporter::QueryUserInsert(
    const ScanDTVTransport          &transport,
    ChannelInsertInfo               &chan)
{
    QString msg = tr("You chose to manually insert channel %1.")
        .arg(SimpleFormatChannel(transport, chan));

    OkCancelType ret = kOCTCancel;

    if (m_useGui)
    {
        while (true)
        {
            QString msg2 = msg;
            msg2 += " ";
            msg2 += tr("Please enter a unique channel number.");

            QString val = ComputeSuggestedChannelNum(chan);
            msg2 += " ";
            msg2 += tr("Default value is %1").arg(val);
            ret = ShowManualChannelPopup(
                 tr("Channel Importer"),
                msg2, val);

            if (kOCTOk != ret)
                break; // user canceled..

            bool ok = CheckChannelNumber(val, chan);
            if (ok)
            {
                chan.m_chanNum = val;
                ret = kOCTOk;
                break;
            }
        }
    }
    else if (m_isInteractive)
    {
        std::cout << msg.toLatin1().constData() << std::endl;

        QString cancelStr    = QCoreApplication::translate("(Common)", "Cancel").toLower();
        QString cancelAllStr = QCoreApplication::translate("(Common)", "Cancel All").toLower();

        //: %1 is the translation of "Cancel", %2 of "Cancel All"
        QString msg2 = tr("Please enter a non-conflicting channel number "
                          "(or type '%1' to skip, '%2' to skip all): ")
                          .arg(cancelStr, cancelAllStr);

        while (true)
        {
            std::cout << msg2.toLatin1().constData() << std::endl;
            std::string sret;
            std::cin >> sret;
            QString val = QString(sret.c_str());
            if (val.toLower() == cancelStr)
            {
                ret = kOCTCancel;
                break; // user canceled..
            }
            if (val.toLower() == cancelAllStr)
            {
                ret = kOCTCancelAll;
                break; // user canceled..
            }

            bool ok = CheckChannelNumber(val, chan);
            if (ok)
            {
                chan.m_chanNum = val;
                ret = kOCTOk;
                break;
            }
        }
    }

    return ret;
}

// ChannelImporter::CheckChannelNumber
//
// Check validity of a new channel number.
// The channel number is not a number but it is a string that starts with a digit.
// The channel number should not yet exist in this video source.
//
bool ChannelImporter::CheckChannelNumber(
    const QString           &num,
    const ChannelInsertInfo &chan)
{
    bool ok = (num.length() >= 1);
    ok = ok && ((num[0] >= '0') && (num[0] <= '9'));
    ok = ok && !ChannelUtil::IsConflicting(
        num, chan.m_sourceId, chan.m_channelId);
    return ok;
}
