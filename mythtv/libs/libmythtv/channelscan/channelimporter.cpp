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

using namespace std;

// MythTV headers
#include "channelimporter.h"
#include "mythdialogbox.h"
#include "mythdb.h"
#include "mpegstreamdata.h" // for kEncDecrypted
#include "channelutil.h"

#define LOC QString("ChanImport: ")

static QString map_str(QString str)
{
    if (str.isEmpty())
        return "";
    return str;
}

void ChannelImporter::Process(const ScanDTVTransportList &_transports,
                              int sourceid)
{
    if (_transports.empty())
    {
        if (m_use_gui)
        {
            int channels = ChannelUtil::GetChannelCount(sourceid);

            LOG(VB_GENERAL, LOG_INFO, LOC + (channels ?
                                             (m_success ?
                                              QString("Found %1 channels")
                                              .arg(channels) :
                                              "No new channels to process") :
                                             "No channels to process.."));

            ShowOkPopup(
                channels ?
                (m_success ? tr("Found %n channel(s)", "", channels) :
                             tr("Failed to find any new channels!"))
                           : tr("Failed to find any channels."));
        }
        else
        {
            cout << (ChannelUtil::GetChannelCount() ?
                     "No new channels to process" :
                     "No channels to process..");
        }

        return;
    }

    ScanDTVTransportList transports = _transports;

    // Print some scan parameters
    {
        cout << endl << "Scan parameters:" << endl;
        bool require_av = (m_service_requirements & kRequireAV) == kRequireAV;
        bool require_a  = (m_service_requirements & kRequireAudio) != 0;
        cout << "Desired Services            : " << (require_av ? "tv" : require_a ? "tv+radio" : "all") << endl;
        cout << "Unencrypted Only            : " << (m_fta_only            ? "yes" : "no") << endl;
        cout << "Logical Channel Numbers only: " << (m_lcn_only            ? "yes" : "no") << endl;
        cout << "Complete scan data required : " << (m_complete_only       ? "yes" : "no") << endl;
        cout << "Full search for old channels: " << (m_full_channel_search ? "yes" : "no") << endl;
    }

    // Print out each channel
    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_ANY))
    {
        cout << endl << "Channel list before processing (";
        cout << SimpleCountChannels(transports) << "):" << endl;
        ChannelImporterBasicStats infoA = CollectStats(transports);
        cout << FormatChannels(transports, &infoA).toLatin1().constData() << endl;
        cout << endl;
    }

    uint saved_scan = 0;
    if (m_do_save)
        saved_scan = SaveScan(transports);

    CleanupDuplicates(transports);

    FilterServices(transports);

    // Print out each transport
    uint transports_scanned_size = transports.size();
    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_ANY))
    {
        cout << endl;
        cout << "Transport list (" << transports_scanned_size << "):" << endl;
        cout << FormatTransports(transports).toLatin1().constData() << endl;
    }

    // Pull in DB info in transports
    // Channels not found in scan but only in DB are returned in db_trans
    sourceid = transports[0].m_channels[0].m_source_id;
    ScanDTVTransportList db_trans = GetDBTransports(sourceid, transports);

    // Make sure "Open Cable" channels are marked that way.
    FixUpOpenCable(transports);

    // All channels in the scan after comparing with the database
    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_ANY))
    {
        cout << endl << "Channel list after compare with database (";
        cout << SimpleCountChannels(transports) << "):" << endl;
        ChannelImporterBasicStats infoA = CollectStats(transports);
        cout << FormatChannels(transports, &infoA).toLatin1().constData() << endl;
        cout << endl;
    }

    // Add channels from the DB to the channels from the scan
    // and possibly delete one or more of the off-air channels
    if (m_do_delete)
    {
        ScanDTVTransportList trans = transports;
        for (size_t i = 0; i < db_trans.size(); ++i)
            trans.push_back(db_trans[i]);
        uint deleted_count = DeleteChannels(trans);
        if (deleted_count)
            transports = trans;
    }

    // Determine System Info standards..
    ChannelImporterBasicStats info = CollectStats(transports);

    // Determine uniqueness of various naming schemes
    ChannelImporterUniquenessStats stats =
        CollectUniquenessStats(transports, info);

    // Print out each channel
    cout << endl;
    cout << "Channel list (" << SimpleCountChannels(transports) << "):" << endl;
    cout << FormatChannels(transports, &info).toLatin1().constData() << endl;

    // Create summary
    QString msg = GetSummary(transports_scanned_size, info, stats);
    cout << msg.toLatin1().constData() << endl << endl;

    if (m_do_insert)
        InsertChannels(transports, info);

    if (m_do_delete && sourceid)
        DeleteUnusedTransports(sourceid);

    if (m_do_delete || m_do_insert)
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
    vector<uint> off_air_list;
    QMap<uint,bool> deleted;
    ScanDTVTransportList off_air_transports;

    for (size_t i = 0; i < transports.size(); ++i)
    {
        ScanDTVTransport transport_copy;
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transports[i].m_channels[j];
            bool was_in_db = chan.m_db_mplexid && chan.m_channel_id;
            if (!was_in_db)
                continue;

            if (!chan.m_in_pmt)
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
    cout << endl << "Off-air channels (" << SimpleCountChannels(off_air_transports) << "):" << endl;
    ChannelImporterBasicStats infoA = CollectStats(off_air_transports);
    cout << FormatChannels(off_air_transports, &infoA).toLatin1().constData() << endl;

    // Ask user whether to delete all or some of these stale channels
    // if some is selected ask about each individually
    //: %n is the number of channels
    QString msg = tr("Found %n off-air channel(s).", "", off_air_list.size());
    DeleteAction action = QueryUserDelete(msg);
    if (kDeleteIgnoreAll == action)
        return 0;

    if (kDeleteAll == action)
    {
        for (size_t k = 0; k < off_air_list.size(); ++k)
        {
            int i = off_air_list[k] >> 16;
            int j = off_air_list[k] & 0xFFFF;
            ChannelUtil::DeleteChannel(
                transports[i].m_channels[j].m_channel_id);
            deleted[off_air_list[k]] = true;
        }
    }
    else if (kDeleteInvisibleAll == action)
    {
        for (size_t k = 0; k < off_air_list.size(); ++k)
        {
            int i = off_air_list[k] >> 16;
            int j = off_air_list[k] & 0xFFFF;
            int chanid = transports[i].m_channels[j].m_channel_id;
            QString channum = ChannelUtil::GetChanNum(chanid);
            ChannelUtil::SetVisible(chanid, false);
            ChannelUtil::SetChannelValue("channum", QString("_%1").arg(channum),
                                         chanid);
        }
    }
    else
    {
        // TODO manual delete
    }

    // TODO delete encrypted channels when m_fta_only set

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

    if (!m_is_interactive)
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

    // List what has been done with each channel
    if (!updated.empty())
    {
        cout << endl << "Updated old channels (" << SimpleCountChannels(updated) << "):" << endl;
        cout << FormatChannels(updated).toLatin1().constData() << endl;
    }
    if (!skipped_updates.empty())
    {
        cout << endl << "Skipped old channels (" << SimpleCountChannels(skipped_updates) << "):" << endl;
        cout << FormatChannels(skipped_updates).toLatin1().constData() << endl;
    }
    if (!inserted.empty())
    {
        cout << endl << "Inserted new channels (" << SimpleCountChannels(inserted) << "):" << endl;
        cout << FormatChannels(inserted).toLatin1().constData() << endl;
    }
    if (!skipped_inserts.empty())
    {
        cout << endl << "Skipped new channels (" << SimpleCountChannels(skipped_inserts) << "):" << endl;
        cout << FormatChannels(skipped_inserts).toLatin1().constData() << endl;
    }

    // Remaining channels and sum uniques again
    if (!list.empty())
    {
        ChannelImporterBasicStats      ninfo  = CollectStats(list);
        ChannelImporterUniquenessStats nstats = CollectUniquenessStats(list, ninfo);
        cout << "Remaining channels (" << SimpleCountChannels(list) << "):" << endl;
        cout << FormatChannels(list).toLatin1().constData() << endl;
        cout << endl;
        cout << GetSummary(list.size(), ninfo, nstats).toLatin1().constData();
        cout << endl;
    }
}

// ChannelImporter::InsertChannels
//
// transports       List of channels to update
// info             Channel statistics
// action           Insert all, Insert manually, Ignore all
// type             Channel type such as dvb or atsc
// inserted_list    List of inserted channels
// skipped_list     List of skipped channels
//
// return:          List of transports/channels that have not been inserted
//
ScanDTVTransportList ChannelImporter::InsertChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    InsertAction action,
    ChannelType type,
    ScanDTVTransportList &inserted_list,
    ScanDTVTransportList &skipped_list)
{
    QString channelFormat = "%1_%2";

    ScanDTVTransportList next_list;

    bool cancel_all = false;
    bool ok_all = false;

    // Insert all channels with non-conflicting channum
    // and complete tuning information.
    for (size_t i = 0; i < transports.size(); ++i)
    {
        ScanDTVTransport new_transport;
        ScanDTVTransport inserted_transport;
        ScanDTVTransport skipped_transport;

        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transports[i].m_channels[j];

            bool asked = false;
            bool filter = false;
            bool handle = false;
            if (!chan.m_channel_id && (kInsertIgnoreAll == action) &&
                IsType(info, chan, type))
            {
                filter = true;
            }
            else if (!chan.m_channel_id && IsType(info, chan, type))
            {
                handle = true;
            }

            if (cancel_all)
            {
                handle = false;
            }

            if (handle && kInsertManual == action)
            {
                OkCancelType rc = QueryUserInsert(transports[i], chan);
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

                if (chan.m_chan_num.isEmpty() ||
                    ChannelUtil::IsConflicting(chan.m_chan_num, chan.m_source_id))
                {
                    if ((kATSCNonConflicting == type) ||
                        (kATSCConflicting == type))
                    {
                        chan.m_chan_num = channelFormat
                            .arg(chan.m_atsc_major_channel)
                            .arg(chan.m_atsc_minor_channel);
                    }
                    else if (chan.m_si_standard == "dvb")
                    {
                        chan.m_chan_num = QString("%1")
                                            .arg(chan.m_service_id);
                    }
                    else if (chan.m_freqid.isEmpty())
                    {
                        chan.m_chan_num = QString("%1-%2")
                                            .arg(chan.m_source_id)
                                            .arg(chan.m_service_id);
                    }
                    else
                    {
                        chan.m_chan_num = QString("%1-%2")
                                            .arg(chan.m_freqid)
                                            .arg(chan.m_service_id);
                    }

                    conflicting = ChannelUtil::IsConflicting(
                        chan.m_chan_num, chan.m_source_id);
                }

                // Only ask if not already asked before with kInsertManual
                if (m_is_interactive && !asked &&
                    (conflicting || (kChannelTypeConflictingFirst <= type)))
                {
                    bool ok_done = false;
                    if (ok_all)
                    {
                        QString val = ComputeSuggestedChannelNum(chan);
                        bool ok = CheckChannelNumber(val, chan);
                        if (ok)
                        {
                            chan.m_chan_num = val;
                            conflicting = false;
                            ok_done = true;
                        }
                    }
                    if (!ok_done)
                    {
                        OkCancelType rc =
                            QueryUserResolve(transports[i], chan);

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
                    chan.m_source_id, chan.m_chan_num);

                chan.m_channel_id = (chanid > 0) ? chanid : chan.m_channel_id;

                if (chan.m_channel_id)
                {
                    uint tsid = chan.m_vct_tsid;
                    tsid = (tsid) ? tsid : chan.m_sdt_tsid;
                    tsid = (tsid) ? tsid : chan.m_pat_tsid;
                    tsid = (tsid) ? tsid : chan.m_vct_chan_tsid;

                    if (!chan.m_db_mplexid)
                    {
                        chan.m_db_mplexid = ChannelUtil::CreateMultiplex(
                            chan.m_source_id, transports[i], tsid, chan.m_orig_netid);
                    }
                    else
                    {
                        // Find the matching multiplex. This updates the
                        // transport and network ID's in case the transport
                        // was created manually
                        int id = ChannelUtil::GetBetterMplexID(chan.m_db_mplexid,
                                    tsid, chan.m_orig_netid);
                        if (id >= 0)
                            chan.m_db_mplexid = id;
                    }
                }

                if (chan.m_channel_id && chan.m_db_mplexid)
                {
                    chan.m_channel_id = chanid;

                    inserted = ChannelUtil::CreateChannel(
                        chan.m_db_mplexid,
                        chan.m_source_id,
                        chan.m_channel_id,
                        chan.m_callsign,
                        chan.m_service_name,
                        chan.m_chan_num,
                        chan.m_service_id,
                        chan.m_atsc_major_channel,
                        chan.m_atsc_minor_channel,
                        chan.m_use_on_air_guide,
                        chan.m_hidden, chan.m_hidden_in_guide,
                        chan.m_freqid,
                        QString(),
                        chan.m_format,
                        QString(),
                        chan.m_default_authority,
                        chan.m_service_type);

                    if (!transports[i].m_iptvTuning.GetDataURL().isEmpty())
                        ChannelUtil::CreateIPTVTuningData(chan.m_channel_id,
                                          transports[i].m_iptvTuning);
                }
            }

            if (inserted)
            {
                // Update list of inserted channels
                AddChanToCopy(inserted_transport, transports[i], chan);
            }

            if (filter)
            {
                // Update list of skipped channels
                AddChanToCopy(skipped_transport, transports[i], chan);
            }
            else if (!inserted)
            {
                // Update list of remaining channels
                AddChanToCopy(new_transport, transports[i], chan);
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
// transports   list of transports/channels to update
// info         Channel statistics
// action       Update All, Ignore All
// type         Channel type such as dvb or atsc
// inserted     List of inserted channels
// skipped      List of skipped channels
//
// return:      List of transports/channels that have not been updated
//
ScanDTVTransportList ChannelImporter::UpdateChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    UpdateAction action,
    ChannelType type,
    ScanDTVTransportList &updated_list,
    ScanDTVTransportList &skipped_list)
{
    QString channelFormat = "%1_%2";
    bool renameChannels = false;

    ScanDTVTransportList next_list;

    // update all channels with non-conflicting channum
    // and complete tuning information.
    for (size_t i = 0; i < transports.size(); ++i)
    {
        ScanDTVTransport new_transport;
        ScanDTVTransport updated_transport;
        ScanDTVTransport skipped_transport;

        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transports[i].m_channels[j];

            bool filter = false;
            bool handle = false;
            if (chan.m_channel_id && (kUpdateIgnoreAll == action) &&
                IsType(info, chan, type))
            {
                filter = true;
            }
            else if (chan.m_channel_id && IsType(info, chan, type))
            {
                handle = true;
            }

            if (handle)
            {
                bool conflicting = false;

                if (m_keep_channel_numbers)
                {
                    ChannelUtil::UpdateChannelNumberFromDB(chan);
                }
                if (chan.m_chan_num.isEmpty() || renameChannels ||
                    ChannelUtil::IsConflicting(
                        chan.m_chan_num, chan.m_source_id, chan.m_channel_id))
                {
                    if (kATSCNonConflicting == type)
                    {
                        chan.m_chan_num = channelFormat
                            .arg(chan.m_atsc_major_channel)
                            .arg(chan.m_atsc_minor_channel);
                    }
                    else if (chan.m_si_standard == "dvb")
                    {
                        chan.m_chan_num = QString("%1")
                                            .arg(chan.m_service_id);
                    }
                    else if (chan.m_freqid.isEmpty())
                    {
                        chan.m_chan_num = QString("%1-%2")
                                            .arg(chan.m_source_id)
                                            .arg(chan.m_service_id);
                    }
                    else
                    {
                        chan.m_chan_num = QString("%1-%2")
                                            .arg(chan.m_freqid)
                                            .arg(chan.m_service_id);
                    }

                    conflicting = ChannelUtil::IsConflicting(
                        chan.m_chan_num, chan.m_source_id, chan.m_channel_id);
                }

                if (conflicting)
                {
                    handle = false;

                    // Update list of skipped channels
                    AddChanToCopy(skipped_transport, transports[i], chan);
                }
            }

            bool updated = false;
            if (handle)
            {
                ChannelUtil::UpdateInsertInfoFromDB(chan);

                // Find the matching multiplex. This updates the
                // transport and network ID's in case the transport
                // was created manually
                uint tsid = chan.m_vct_tsid;
                tsid = (tsid) ? tsid : chan.m_sdt_tsid;
                tsid = (tsid) ? tsid : chan.m_pat_tsid;
                tsid = (tsid) ? tsid : chan.m_vct_chan_tsid;
                int id = ChannelUtil::GetBetterMplexID(chan.m_db_mplexid,
                            tsid, chan.m_orig_netid);
                if (id >= 0)
                    chan.m_db_mplexid = id;

                updated = ChannelUtil::UpdateChannel(
                    chan.m_db_mplexid,
                    chan.m_source_id,
                    chan.m_channel_id,
                    chan.m_callsign,
                    chan.m_service_name,
                    chan.m_chan_num,
                    chan.m_service_id,
                    chan.m_atsc_major_channel,
                    chan.m_atsc_minor_channel,
                    chan.m_use_on_air_guide,
                    chan.m_hidden, chan.m_hidden_in_guide,
                    chan.m_freqid,
                    QString(),
                    chan.m_format,
                    QString(),
                    chan.m_default_authority,
                    chan.m_service_type);
            }

            if (updated)
            {
                // Update list of updated channels
                AddChanToCopy(updated_transport, transports[i], chan);
            }

            if (filter)
            {
                // Update list of skipped channels
                AddChanToCopy(skipped_transport, transports[i], chan);
            }
            else if (!updated)
            {
                // Update list of remaining channels
                AddChanToCopy(new_transport, transports[i], chan);
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

void ChannelImporter::CleanupDuplicates(ScanDTVTransportList &transports)
{
    ScanDTVTransportList no_dups;

    DTVTunerType tuner_type(DTVTunerType::kTunerTypeATSC);
    if (!transports.empty())
        tuner_type = transports[0].m_tuner_type;

    bool is_dvbs = ((DTVTunerType::kTunerTypeDVBS1 == tuner_type) ||
                    (DTVTunerType::kTunerTypeDVBS2 == tuner_type));

    uint freq_mult = (is_dvbs) ? 1 : 1000;

    vector<bool> ignore;
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
            ignore[j] = true;
        }
        no_dups.push_back(transports[i]);
    }

    transports = no_dups;
}

void ChannelImporter::FilterServices(ScanDTVTransportList &transports) const
{
    bool require_av = (m_service_requirements & kRequireAV) == kRequireAV;
    bool require_a  = (m_service_requirements & kRequireAudio) != 0;

    for (size_t i = 0; i < transports.size(); ++i)
    {
        ChannelInsertInfoList filtered;
        for (size_t k = 0; k < transports[i].m_channels.size(); ++k)
        {
            if (m_fta_only && transports[i].m_channels[k].m_is_encrypted &&
                transports[i].m_channels[k].m_decryption_status != kEncDecrypted)
                continue;

            if (require_a && transports[i].m_channels[k].m_is_data_service)
                continue;

            if (require_av && transports[i].m_channels[k].m_is_audio_service)
                continue;

            // Filter channels out that do not have a logical channel number
            if (m_lcn_only && transports[i].m_channels[k].m_chan_num.isEmpty())
            {
                QString msg = FormatChannel(transports[i], transports[i].m_channels[k]);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("No LCN: %1").arg(msg));
                continue;
            }

            // Filter channels out that are not present in PAT and PMT.
            if (m_complete_only &&
                !(transports[i].m_channels[k].m_in_pat &&
                  transports[i].m_channels[k].m_in_pmt ))
            {
                QString msg = FormatChannel(transports[i], transports[i].m_channels[k]);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Not in PAT/PMT: %1").arg(msg));
                continue;
            }

            // Filter channels out that are not present in SDT and that are not ATSC
            if (m_complete_only &&
                transports[i].m_channels[k].m_atsc_major_channel == 0 &&
                transports[i].m_channels[k].m_atsc_minor_channel == 0 &&
                !(transports[i].m_channels[k].m_in_pat &&
                  transports[i].m_channels[k].m_in_pmt &&
                  transports[i].m_channels[k].m_in_sdt &&
                 (transports[i].m_channels[k].m_pat_tsid ==
                  transports[i].m_channels[k].m_sdt_tsid)))
            {
                QString msg = FormatChannel(transports[i], transports[i].m_channels[k]);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Not in PAT/PMT/SDT: %1").arg(msg));
                continue;
            }

            // Filter channels out that do not have a name
            if (m_complete_only && transports[i].m_channels[k].m_service_name.isEmpty())
            {
                QString msg = FormatChannel(transports[i], transports[i].m_channels[k]);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("No name: %1").arg(msg));
                continue;
            }

            // Filter channels out only in channels.conf, i.e. not found
            if (transports[i].m_channels[k].m_in_channels_conf &&
                !(transports[i].m_channels[k].m_in_pat ||
                  transports[i].m_channels[k].m_in_pmt ||
                  transports[i].m_channels[k].m_in_vct ||
                  transports[i].m_channels[k].m_in_nit ||
                  transports[i].m_channels[k].m_in_sdt))
                continue;

            filtered.push_back(transports[i].m_channels[k]);
        }
        transports[i].m_channels = filtered;
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
        tuner_type = transports[0].m_tuner_type;

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
        QMap<uint,bool> found_chan;

        // Search for old channels in the same transport of the scan.
        for (size_t ist = 0; ist < transports.size(); ++ist)                                // All transports in scan
        {
            ScanDTVTransport &transport = transports[ist];                                  // Scanned transport

            if (transport.IsEqual(tuner_type, db_transport, 500 * freq_mult, true))         // Same transport?
            {
                found_transport = true;
                transport.m_mplex = db_transport.m_mplex;                                   // Found multiplex

                for (size_t jdc = 0; jdc < db_transport.m_channels.size(); ++jdc)           // All channels in database transport
                {
                    if (!found_chan[jdc])                                                   // Channel not found yet?
                    {
                        ChannelInsertInfo &db_chan = db_transport.m_channels[jdc];          // Channel in database transport

                        for (size_t ktc = 0; ktc < transport.m_channels.size(); ++ktc)      // All channels in scanned transport
                        {
                            ChannelInsertInfo &chan = transport.m_channels[ktc];            // Channel in scanned transport
                            if (db_chan.IsSameChannel(chan, 2))                             // Same transport, relaxed check
                            {
                                found_in_same_transport++;
                                found_chan[jdc] = true;                                     // Found channel from database in scan
                                chan.m_db_mplexid = mplexid;                                // Found multiplex
                                chan.m_channel_id = db_chan.m_channel_id;                   // This is the crucial field
                                break;                                                      // Ready with scanned transport
                            }
                        }
                    }
                }
            }
        }

        // Search for old channels in all transports of the scan.
        // This is done for all channels that have not yet been found.
        // This can identify the channels that have moved to another transport.
        if (m_full_channel_search)
        {
            for (size_t idc = 0; idc < db_transport.m_channels.size(); ++idc)               // All channels in database transport
            {
                ChannelInsertInfo &db_chan = db_transport.m_channels[idc];                  // Channel in database transport

                for (size_t jst = 0; jst < transports.size() && !found_chan[idc]; ++jst)    // All transports in scan until found
                {
                    ScanDTVTransport &transport = transports[jst];                          // Scanned transport
                    for (size_t ksc = 0; ksc < transport.m_channels.size(); ++ksc)          // All channels in scanned transport
                    {
                        ChannelInsertInfo &chan = transport.m_channels[ksc];                // Channel in scanned transport
                        if (db_chan.IsSameChannel(chan, 1))                                 // Different transport, check
                        {                                                                   // network id and service id
                            found_in_other_transport++;
                            found_chan[idc] = true;                                         // Found channel from database in scan
                            chan.m_channel_id = db_chan.m_channel_id;                       // This is the crucial field
                            break;                                                          // Ready with scanned transport
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
                if (!found_chan[idc])
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
    for (size_t i = 0; i < transports.size(); ++i)
    {
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo &chan = transports[i].m_channels[j];
            if (((chan.m_could_be_opencable && (chan.m_si_standard == "mpeg")) ||
                 chan.m_is_opencable) && !chan.m_in_vct)
            {
                chan.m_si_standard = "opencable";
            }
        }
    }
}

ChannelImporterBasicStats ChannelImporter::CollectStats(
    const ScanDTVTransportList &transports)
{
    ChannelImporterBasicStats info;
    for (size_t i = 0; i < transports.size(); ++i)
    {
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            const ChannelInsertInfo &chan = transports[i].m_channels[j];
            int enc = (chan.m_is_encrypted) ?
                ((chan.m_decryption_status == kEncDecrypted) ? 2 : 1) : 0;
            info.m_atsc_channels[enc] += (chan.m_si_standard == "atsc");
            info.m_dvb_channels [enc] += (chan.m_si_standard == "dvb");
            info.m_mpeg_channels[enc] += (chan.m_si_standard == "mpeg");
            info.m_scte_channels[enc] += (chan.m_si_standard == "opencable");
            info.m_ntsc_channels[enc] += (chan.m_si_standard == "ntsc");
            if (chan.m_si_standard != "ntsc")
            {
                ++info.m_prognum_cnt[chan.m_service_id];
                ++info.m_channum_cnt[map_str(chan.m_chan_num)];
            }
            if (chan.m_si_standard == "atsc")
            {
                ++info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                     (chan.m_atsc_minor_channel)];
                ++info.m_atscmin_cnt[chan.m_atsc_minor_channel];
                ++info.m_atscmaj_cnt[chan.m_atsc_major_channel];
            }
            if (chan.m_si_standard == "ntsc")
            {
                ++info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                     (chan.m_atsc_minor_channel)];
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

    for (size_t i = 0; i < transports.size(); ++i)
    {
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            const ChannelInsertInfo &chan = transports[i].m_channels[j];
            stats.m_unique_prognum +=
                (info.m_prognum_cnt[chan.m_service_id] == 1) ? 1 : 0;
            stats.m_unique_channum +=
                (info.m_channum_cnt[map_str(chan.m_chan_num)] == 1) ? 1 : 0;

            if (chan.m_si_standard == "atsc")
            {
                stats.m_unique_atscnum +=
                    (info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                        (chan.m_atsc_minor_channel)] == 1) ? 1 : 0;
                stats.m_unique_atscmin +=
                    (info.m_atscmin_cnt[(chan.m_atsc_minor_channel)] == 1) ? 1 : 0;
                stats.m_max_atscmajcnt = max(
                    stats.m_max_atscmajcnt,
                    info.m_atscmaj_cnt[chan.m_atsc_major_channel]);
            }
        }
    }

    stats.m_unique_total = (stats.m_unique_prognum + stats.m_unique_atscnum +
                          stats.m_unique_atscmin + stats.m_unique_channum);

    return stats;
}


QString ChannelImporter::FormatChannel(
    const ScanDTVTransport          &transport,
    const ChannelInsertInfo         &chan,
    const ChannelImporterBasicStats *info)
{
    QString msg;
    QTextStream ssMsg(&msg);

    ssMsg << transport.m_modulation.toString().toLatin1().constData()
          << ":";
    ssMsg << transport.m_frequency << ":";

    QString si_standard = (chan.m_si_standard=="opencable") ?
        QString("scte") : chan.m_si_standard;

    if (si_standard == "atsc" || si_standard == "scte")
        ssMsg << (QString("%1:%2:%3-%4:%5:%6=%7=%8:%9")
                  .arg(chan.m_callsign).arg(chan.m_chan_num)
                  .arg(chan.m_atsc_major_channel)
                  .arg(chan.m_atsc_minor_channel)
                  .arg(chan.m_service_id)
                  .arg(chan.m_vct_tsid)
                  .arg(chan.m_vct_chan_tsid)
                  .arg(chan.m_pat_tsid)
                  .arg(si_standard)).toLatin1().constData();
    else if (si_standard == "dvb")
        ssMsg << (QString("%1:%2:%3:%4:%5:%6=%7:%8")
                  .arg(chan.m_service_name).arg(chan.m_chan_num)
                  .arg(chan.m_netid).arg(chan.m_orig_netid)
                  .arg(chan.m_service_id)
                  .arg(chan.m_sdt_tsid)
                  .arg(chan.m_pat_tsid)
                  .arg(si_standard)).toLatin1().constData();
    else
        ssMsg << (QString("%1:%2:%3:%4:%5")
                  .arg(chan.m_callsign).arg(chan.m_chan_num)
                  .arg(chan.m_service_id)
                  .arg(chan.m_pat_tsid)
                  .arg(si_standard)).toLatin1().constData();

    if (info)
    {
        ssMsg <<"\t"
              << chan.m_channel_id;
    }

    if (info)
    {
        ssMsg << ":"
              << (QString("cnt(pnum:%1,channum:%2)")
                  .arg(info->m_prognum_cnt[chan.m_service_id])
                  .arg(info->m_channum_cnt[map_str(chan.m_chan_num)])
                  ).toLatin1().constData();
        if (chan.m_si_standard == "atsc")
        {
            ssMsg <<
                (QString(":atsc_cnt(tot:%1,minor:%2)")
                 .arg(info->m_atscnum_cnt[
                          (chan.m_atsc_major_channel << 16) |
                          (chan.m_atsc_minor_channel)])
                 .arg(info->m_atscmin_cnt[
                          chan.m_atsc_minor_channel])
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

    QString si_standard = (chan.m_si_standard=="opencable") ?
        QString("scte") : chan.m_si_standard;

    if (si_standard == "atsc" || si_standard == "scte")
    {

        if (si_standard == "atsc")
            ssMsg << (QString("%1-%2")
                  .arg(chan.m_atsc_major_channel)
                  .arg(chan.m_atsc_minor_channel)).toLatin1().constData();
        else if (chan.m_freqid.isEmpty())
            ssMsg << (QString("%1-%2")
                  .arg(chan.m_source_id)
                  .arg(chan.m_service_id)).toLatin1().constData();
        else
            ssMsg << (QString("%1-%2")
                  .arg(chan.m_freqid)
                  .arg(chan.m_service_id)).toLatin1().constData();

        if (!chan.m_callsign.isEmpty())
            ssMsg << (QString(" (%1)")
                  .arg(chan.m_callsign)).toLatin1().constData();
    }
    else if (si_standard == "dvb")
        ssMsg << (QString("%1 (%2 %3)")
                  .arg(chan.m_service_name).arg(chan.m_service_id)
                  .arg(chan.m_netid)).toLatin1().constData();
    else if (chan.m_freqid.isEmpty())
        ssMsg << (QString("%1-%2")
                  .arg(chan.m_source_id).arg(chan.m_service_id))
                  .toLatin1().constData();
    else
        ssMsg << (QString("%1-%2")
                  .arg(chan.m_freqid).arg(chan.m_service_id))
                  .toLatin1().constData();

    return msg;
}

QString ChannelImporter::FormatChannels(
    const ScanDTVTransportList      &transports_in,
    const ChannelImporterBasicStats *info)
{
    // Sort transports in order of increasing frequency
    struct less_than_key
    {
        inline bool operator() (const ScanDTVTransport &t1, const ScanDTVTransport &t2)
        {
            return t1.m_frequency < t2.m_frequency;
        }
    };
    ScanDTVTransportList transports(transports_in);
    std::sort(transports.begin(), transports.end(), less_than_key());

    QString msg;

    for (size_t i = 0; i < transports.size(); ++i)
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
            msg += FormatChannel(transports[i], transports[i].m_channels[j],
                                 info) + "\n";

    return msg;
}

QString ChannelImporter::FormatTransport(
    const ScanDTVTransport &transport)
{
    QString msg;
    QTextStream ssMsg(&msg);
    ssMsg << transport.toString();
    return msg;
}

QString ChannelImporter::FormatTransports(
    const ScanDTVTransportList      &transports_in)
{
    // Sort transports in order of increasing frequency
    struct less_than_key
    {
        inline bool operator() (const ScanDTVTransport &t1, const ScanDTVTransport &t2)
        {
            return t1.m_frequency < t2.m_frequency;
        }
    };
    ScanDTVTransportList transports(transports_in);
    std::sort(transports.begin(), transports.end(), less_than_key());

    QString msg;

    for (size_t i = 0; i < transports.size(); ++i)
        msg += FormatTransport(transports[i]) + "\n";

    return msg;
}

QString ChannelImporter::GetSummary(
    uint                                  transport_count,
    const ChannelImporterBasicStats      &info,
    const ChannelImporterUniquenessStats &stats)
{
    //: %n is the number of transports
    QString msg = tr("Found %n transport(s):\n", "", transport_count);
    msg += tr("Channels: FTA Enc Dec\n") +
        QString("ATSC      %1 %2 %3\n")
        .arg(info.m_atsc_channels[0],3).arg(info.m_atsc_channels[1],3)
        .arg(info.m_atsc_channels[2],3) +
        QString("DVB       %1 %2 %3\n")
        .arg(info.m_dvb_channels [0],3).arg(info.m_dvb_channels [1],3)
        .arg(info.m_dvb_channels [2],3) +
        QString("SCTE      %1 %2 %3\n")
        .arg(info.m_scte_channels[0],3).arg(info.m_scte_channels[1],3)
        .arg(info.m_scte_channels[2],3) +
        QString("MPEG      %1 %2 %3\n")
        .arg(info.m_mpeg_channels[0],3).arg(info.m_mpeg_channels[1],3)
        .arg(info.m_mpeg_channels[2],3) +
        QString("NTSC      %1\n").arg(info.m_ntsc_channels[0],3) +
        tr("Unique: prog %1 atsc %2 atsc minor %3 channum %4\n")
        .arg(stats.m_unique_prognum).arg(stats.m_unique_atscnum)
        .arg(stats.m_unique_atscmin).arg(stats.m_unique_channum) +
        tr("Max atsc major count: %1")
        .arg(stats.m_max_atscmajcnt);

    return msg;
}

bool ChannelImporter::IsType(
    const ChannelImporterBasicStats &info,
    const ChannelInsertInfo &chan, ChannelType type)
{
    switch (type)
    {
        case kATSCNonConflicting:
            return ((chan.m_si_standard == "atsc") /* &&
                    (info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                        (chan.m_atsc_minor_channel)] == 1) */);

        case kDVBNonConflicting:
            return ((chan.m_si_standard == "dvb") /* &&
                    (info.m_prognum_cnt[chan.m_service_id] == 1) */);

        case kMPEGNonConflicting:
            return ((chan.m_si_standard == "mpeg") &&
                    (info.m_channum_cnt[map_str(chan.m_chan_num)] == 1));

        case kSCTENonConflicting:
            return (((chan.m_si_standard == "scte") ||
                    (chan.m_si_standard == "opencable")) &&
                    (info.m_channum_cnt[map_str(chan.m_chan_num)] == 1));

        case kNTSCNonConflicting:
            return ((chan.m_si_standard == "ntsc") &&
                    (info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                        (chan.m_atsc_minor_channel)] == 1));

        case kATSCConflicting:
            return ((chan.m_si_standard == "atsc") &&
                    (info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                        (chan.m_atsc_minor_channel)] != 1));

        case kDVBConflicting:
            return ((chan.m_si_standard == "dvb") &&
                    (info.m_prognum_cnt[chan.m_service_id] != 1));

        case kMPEGConflicting:
            return ((chan.m_si_standard == "mpeg") &&
                    (info.m_channum_cnt[map_str(chan.m_chan_num)] != 1));

        case kSCTEConflicting:
            return (((chan.m_si_standard == "scte") ||
                    (chan.m_si_standard == "opencable")) &&
                    (info.m_channum_cnt[map_str(chan.m_chan_num)] != 1));

        case kNTSCConflicting:
            return ((chan.m_si_standard == "ntsc") &&
                    (info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                        (chan.m_atsc_minor_channel)] != 1));
    }
    return false;
}

void ChannelImporter::CountChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    ChannelType type, uint &new_chan, uint &old_chan)
{
    new_chan = old_chan = 0;
    for (size_t i = 0; i < transports.size(); ++i)
    {
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transports[i].m_channels[j];
            if (IsType(info, chan, type))
            {
                if (chan.m_channel_id)
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
    int count = 0;
    for (size_t i = 0; i < transports.size(); ++i)
    {
        count += transports[i].m_channels.size();
    }
    return count;
}

/**
 * \fn ChannelImporter::ComputeSuggestedChannelNum
 *
 * Compute a suggested channel number based on various aspects of the
 * channel information. Check to see if this channel number conflicts
 * with an existing channel number. If so, fall back to incrementing a
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

    // Suggest existing channel number if non-conflicting
    if (!ChannelUtil::IsConflicting(chan.m_chan_num, chan.m_source_id))
        return chan.m_chan_num;

    // ATSC major-minor channel number
    QString channelFormat = "%1_%2";
    QString chan_num = channelFormat
        .arg(chan.m_atsc_major_channel)
        .arg(chan.m_atsc_minor_channel);
    if (chan.m_atsc_minor_channel)
    {
        if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
            return chan_num;
    }

    // DVB
    if (chan.m_si_standard == "dvb")
    {
        // Service ID
        chan_num = QString("%1").arg(chan.m_service_id);
        if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
            return chan_num;

        // Frequency ID (channel) - Service ID
        if (!chan.m_freqid.isEmpty())
        {
            chan_num = QString("%1-%2")
                          .arg(chan.m_freqid)
                          .arg(chan.m_service_id);
            if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
                return chan_num;
        }

        // Service ID - Network ID
        chan_num = QString("%1-%2")
                        .arg(chan.m_service_id)
                        .arg(chan.m_netid);
        if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
            return chan_num;

        // Service ID - Transport ID
        chan_num = QString("%1-%2")
                        .arg(chan.m_service_id)
                        .arg(chan.m_pat_tsid);
        if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
            return chan_num;
    }

    // Find unused channel number
    QMutexLocker locker(&s_lastFreeLock);
    uint last_free_chan_num = s_lastFreeChanNumMap[chan.m_source_id];
    for (last_free_chan_num++; ; ++last_free_chan_num)
    {
        chan_num = QString::number(last_free_chan_num);
        if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
            break;
    }
    // cppcheck-suppress unreadVariable
    s_lastFreeChanNumMap[chan.m_source_id] = last_free_chan_num;

    return chan_num;
}

ChannelImporter::DeleteAction
ChannelImporter::QueryUserDelete(const QString &msg)
{
    DeleteAction action = kDeleteAll;
    if (m_use_gui)
    {
        int ret = -1;
        do
        {
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
                QObject::connect(deleteDialog, &MythDialogBox::Closed,
                                 [&](const QString & /*resultId*/, int result)
                                 {
                                     ret = result;
                                     m_eventLoop.quit();
                                 });
                popupStack->AddScreen(deleteDialog);

                m_eventLoop.exec();
            }
        } while (ret < 0);

        action = (0 == ret) ? kDeleteAll       : action;
        action = (1 == ret) ? kDeleteInvisibleAll : action;
        action = (2 == ret) ? kDeleteIgnoreAll   : action;
//        action = (2 == m_deleteChannelResult) ? kDeleteManual    : action;
//        action = (3 == m_deleteChannelResult) ? kDeleteIgnoreAll : action;
    }
    else if (m_is_interactive)
    {
        cout << msg.toLatin1().constData()
             << endl
             << tr("Do you want to:").toLatin1().constData()
             << endl
             << tr("1. Delete All").toLatin1().constData()
             << endl
             << tr("2. Set all invisible").toLatin1().constData()
             << endl
//        cout << "3. Handle manually" << endl;
             << tr("4. Ignore All").toLatin1().constData()
             << endl;
        while (true)
        {
            string ret;
            cin >> ret;
            bool ok;
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
            cout << tr("Please enter either 1, 2 or 4:")
                .toLatin1().constData() << endl;//
        }
    }

    return action;
}

ChannelImporter::InsertAction
ChannelImporter::QueryUserInsert(const QString &msg)
{
    InsertAction action = kInsertAll;
    if (m_use_gui)
    {
        int ret = -1;
        do
        {
            MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
            auto *insertDialog =
                new MythDialogBox(msg, popupStack, "insertchannels");

            if (insertDialog->Create())
            {
                insertDialog->AddButton(tr("Insert All"));
                insertDialog->AddButton(tr("Insert Manually"));
                insertDialog->AddButton(tr("Ignore All"));
                QObject::connect(insertDialog, &MythDialogBox::Closed,
                                 [&](const QString & /*resultId*/, int result)
                                 {
                                     ret = result;
                                     m_eventLoop.quit();
                                 });

                popupStack->AddScreen(insertDialog);
                m_eventLoop.exec();
            }
        } while (ret < 0);

        action = (0 == ret) ? kInsertAll       : action;
        action = (1 == ret) ? kInsertManual    : action;
        action = (2 == ret) ? kInsertIgnoreAll : action;
    }
    else if (m_is_interactive)
    {
        cout << msg.toLatin1().constData()
             << endl
             << tr("Do you want to:").toLatin1().constData()
             << endl
             << tr("1. Insert All").toLatin1().constData()
             << endl
             << tr("2. Insert Manually").toLatin1().constData()
             << endl
             << tr("3. Ignore All").toLatin1().constData()
             << endl;
        while (true)
        {
            string ret;
            cin >> ret;
            bool ok;
            uint val = QString(ret.c_str()).toUInt(&ok);
            if (ok && (1 <= val) && (val <= 3))
            {
                action = (1 == val) ? kInsertAll       : action;
                action = (2 == val) ? kInsertManual    : action;
                action = (3 == val) ? kInsertIgnoreAll : action;
                break;
            }

            cout << tr("Please enter either 1, 2, or 3:")
                .toLatin1().constData() << endl;
        }
    }

    return action;
}

ChannelImporter::UpdateAction
ChannelImporter::QueryUserUpdate(const QString &msg)
{
    UpdateAction action = kUpdateAll;

    if (m_use_gui)
    {
        int ret = -1;
        do
        {
            MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
            auto *updateDialog =
                new MythDialogBox(msg, popupStack, "updatechannels");

            if (updateDialog->Create())
            {
                updateDialog->AddButton(tr("Update All"));
                updateDialog->AddButton(tr("Ignore All"));
                QObject::connect(updateDialog, &MythDialogBox::Closed,
                                 [&](const QString& /*resultId*/, int result)
                                 {
                                     ret = result;
                                     m_eventLoop.quit();
                                 });

                popupStack->AddScreen(updateDialog);
                m_eventLoop.exec();
            }
        } while (ret < 0);

        action = (0 == ret) ? kUpdateAll       : action;
        action = (1 == ret) ? kUpdateIgnoreAll : action;
    }
    else if (m_is_interactive)
    {
        cout << msg.toLatin1().constData()
             << endl
             << tr("Do you want to:").toLatin1().constData()
             << endl
             << tr("1. Update All").toLatin1().constData()
             << endl
             << tr("2. Update Manually").toLatin1().constData()
             << endl
             << tr("3. Ignore All").toLatin1().constData()
             << endl;
        while (true)
        {
            string ret;
            cin >> ret;
            bool ok;
            uint val = QString(ret.c_str()).toUInt(&ok);
            if (ok && (1 <= val) && (val <= 3))
            {
                action = (1 == val) ? kUpdateAll       : action;
                action = (2 == val) ? kUpdateManual    : action;
                action = (3 == val) ? kUpdateIgnoreAll : action;
                break;
            }

            cout << tr("Please enter either 1, 2, or 3:")
                .toLatin1().constData() << endl;
        }
    }

    return action;
}

OkCancelType ChannelImporter::ShowManualChannelPopup(
    MythMainWindow *parent, const QString& title,
    const QString& message, QString &text)
{
    int dc = -1;
    MythScreenStack *popupStack = parent->GetStack("popup stack");
    auto *popup = new MythDialogBox(title, message, popupStack,
                                    "manualchannelpopup");

    if (popup->Create())
    {
        popup->AddButton(QCoreApplication::translate("(Common)", "OK"));
        popup->AddButton(tr("Edit"));
        popup->AddButton(QCoreApplication::translate("(Common)", "Cancel"));
        popup->AddButton(QCoreApplication::translate("(Common)", "Cancel All"));
        QObject::connect(popup, &MythDialogBox::Closed,
                         [&](const QString & /*resultId*/, int result)
                         {
                             dc = result;
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

    // Choice "Edit"
    if (1 == dc)
    {
        auto *textEdit =
            new MythTextInputDialog(popupStack,
                                    tr("Please enter a unique channel number."),
                                    FilterNone, false, text);
        if (textEdit->Create())
        {
            QObject::connect(textEdit, &MythTextInputDialog::haveResult,
                             [&](QString result)
                             {
                                 dc = 0;
                                 text = std::move(result);
                             });
            QObject::connect(textEdit, &MythTextInputDialog::Exiting,
                             [&]()
                             {
                                 m_eventLoop.quit();
                             });

            popupStack->AddScreen(textEdit);
            m_eventLoop.exec();
        }
        else
            delete textEdit;
    }

    OkCancelType rval = kOCTCancel;
    switch (dc) {
        case 0: rval = kOCTOk;        break;
        // NOLINTNEXTLINE(bugprone-branch-clone)
        case 1: rval = kOCTCancel;    break;    // "Edit" is done already
        case 2: rval = kOCTCancel;    break;
        case 3: rval = kOCTCancelAll; break;
    }
    return rval;
}

OkCancelType ChannelImporter::ShowResolveChannelPopup(
    MythMainWindow *parent, const QString& title,
    const QString& message, QString &text)
{
    int dc = -1;
    MythScreenStack *popupStack = parent->GetStack("popup stack");
    auto *popup = new MythDialogBox(title, message, popupStack,
                                    "resolvechannelpopup");

    if (popup->Create())
    {
        popup->AddButton(QCoreApplication::translate("(Common)", "OK"));
        popup->AddButton(QCoreApplication::translate("(Common)", "OK All"));
        popup->AddButton(tr("Edit"));
        popup->AddButton(QCoreApplication::translate("(Common)", "Cancel"));
        popup->AddButton(QCoreApplication::translate("(Common)", "Cancel All"));
        QObject::connect(popup, &MythDialogBox::Closed,
                         [&](const QString & /*resultId*/, int result)
                         {
                             dc = result;
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

    // Choice "Edit"
    if (2 == dc)
    {
        auto *textEdit =
            new MythTextInputDialog(popupStack,
                                    tr("Please enter a unique channel number."),
                                    FilterNone, false, text);
        if (textEdit->Create())
        {
            QObject::connect(textEdit, &MythTextInputDialog::haveResult,
                             [&](QString result)
                             {
                                 dc = 0;
                                 text = std::move(result);
                             });
            QObject::connect(textEdit, &MythTextInputDialog::Exiting,
                             [&]()
                             {
                                 m_eventLoop.quit();
                             });

            popupStack->AddScreen(textEdit);
            m_eventLoop.exec();
        }
        else
            delete textEdit;
    }

    OkCancelType rval = kOCTCancel;
    switch (dc) {
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
                    .arg(SimpleFormatChannel(transport, chan))
                    .arg(chan.m_chan_num);

    OkCancelType ret = kOCTCancel;

    if (m_use_gui)
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
                GetMythMainWindow(), tr("Channel Importer"),
                msg2, val);

            if (kOCTOk != ret && kOCTOkAll != ret)
                break; // user canceled..

            bool ok = CheckChannelNumber(val, chan);
            if (ok)
            {
                chan.m_chan_num = val;
                break;
            }
        }
    }
    else if (m_is_interactive)
    {
        cout << msg.toLatin1().constData() << endl;

        QString cancelStr = QCoreApplication::translate("(Common)",
                                                        "Cancel").toLower();
        QString cancelAllStr = QCoreApplication::translate("(Common)",
                                   "Cancel All").toLower();
        QString msg2 = tr("Please enter a non-conflicting channel number "
                          "(or type '%1' to skip, '%2' to skip all):")
            .arg(cancelStr).arg(cancelAllStr);

        while (true)
        {
            cout << msg2.toLatin1().constData() << endl;
            string sret;
            cin >> sret;
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
                chan.m_chan_num = val;
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

    if (m_use_gui)
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
                GetMythMainWindow(), tr("Channel Importer"),
                msg2, val);

            if (kOCTOk != ret)
                break; // user canceled..

            bool ok = CheckChannelNumber(val, chan);
            if (ok)
            {
                chan.m_chan_num = val;
                ret = kOCTOk;
                break;
            }
        }
    }
    else if (m_is_interactive)
    {
        cout << msg.toLatin1().constData() << endl;

        QString cancelStr    = QCoreApplication::translate("(Common)", "Cancel").toLower();
        QString cancelAllStr = QCoreApplication::translate("(Common)", "Cancel All").toLower();

        //: %1 is the translation of "Cancel", %2 of "Cancel All"
        QString msg2 = tr("Please enter a non-conflicting channel number "
                          "(or type '%1' to skip, '%2' to skip all): ")
                          .arg(cancelStr).arg(cancelAllStr);

        while (true)
        {
            cout << msg2.toLatin1().constData() << endl;
            string sret;
            cin >> sret;
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
                chan.m_chan_num = val;
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
        num, chan.m_source_id, chan.m_channel_id);
    return ok;
}
