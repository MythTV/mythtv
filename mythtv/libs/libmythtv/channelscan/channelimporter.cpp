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

    // Print out each channel
    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_ANY))
    {
        cout << "Before processing: " << endl;
        ChannelImporterBasicStats infoA = CollectStats(transports);
        cout << FormatChannels(transports, infoA).toLatin1().constData() << endl;
        cout << endl << endl;
    }

    uint saved_scan = 0;
    if (m_do_save)
        saved_scan = SaveScan(transports);

    CleanupDuplicates(transports);

    FilterServices(transports);

    if (m_lcn_only)
        FilterChannelNumber(transports);

    // Pull in DB info
    sourceid = transports[0].m_channels[0].m_source_id;
    ScanDTVTransportList db_trans = GetDBTransports(sourceid, transports);

    // Make sure "Open Cable" channels are marked that way.
    FixUpOpenCable(transports);

    // if scan was not aborted prematurely..
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
    cout << FormatChannels(transports, info).toLatin1().constData() << endl;

    // Create summary
    QString msg = GetSummary(transports.size(), info, stats);
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

uint ChannelImporter::DeleteChannels(
    ScanDTVTransportList &transports)
{
    vector<uint> off_air_list;
    QMap<uint,bool> deleted;

    for (size_t i = 0; i < transports.size(); ++i)
    {
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transports[i].m_channels[j];
            bool was_in_db = chan.m_db_mplexid && chan.m_channel_id;
            if (!was_in_db)
                continue;

            if (!chan.m_in_pmt)
                off_air_list.push_back(i<<16|j);
        }
    }

    ScanDTVTransportList newlist;
    if (off_air_list.empty())
    {
        return 0;
    }

    // ask user whether to delete all or some of these stale channels
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
            int i = off_air_list[k] >> 16, j = off_air_list[k] & 0xFFFF;
            ChannelUtil::DeleteChannel(
                transports[i].m_channels[j].m_channel_id);
            deleted[off_air_list[k]] = true;
        }
    }
    else if (kDeleteInvisibleAll == action)
    {
        for (size_t k = 0; k < off_air_list.size(); ++k)
        {
            int i = off_air_list[k] >> 16, j = off_air_list[k] & 0xFFFF;
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

    // TODO print list of stale channels (as deleted if action approved).

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
    }
    return 0;
}

void ChannelImporter::InsertChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info)
{
    ScanDTVTransportList list = transports;
    ScanDTVTransportList filtered;

    // insert/update all channels with non-conflicting channum
    // and complete tuning information.

    uint chantype = (uint) kChannelTypeNonConflictingFirst;
    for (; chantype <= (uint) kChannelTypeNonConflictingLast; ++chantype)
    {
        ChannelType type = (ChannelType) chantype;
        uint new_chan, old_chan;
        CountChannels(list, info, type, new_chan, old_chan);

        if (kNTSCNonConflicting == type)
            continue;

        if (old_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n old %1 channel(s).", "", old_chan)
                              .arg(toString(type));

            UpdateAction action = QueryUserUpdate(msg);
            list = UpdateChannels(list, info, action, type, filtered);
        }
        if (new_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n new non-conflicting %1 channel(s).",
                              "", new_chan).arg(toString(type));

            InsertAction action = QueryUserInsert(msg);
            list = InsertChannels(list, info, action, type, filtered);
        }
    }

    if (!m_is_interactive)
        return;

    // sum uniques again
    ChannelImporterBasicStats      ninfo  = CollectStats(list);
    ChannelImporterUniquenessStats nstats = CollectUniquenessStats(list, ninfo);
    cout << endl << endl << "Printing remaining channels" << endl;
    cout << FormatChannels(list, ninfo).toLatin1().constData() << endl;
    cout << GetSummary(list.size(), ninfo, nstats).toLatin1().constData()
         << endl << endl;

    // if any of the potential uniques is high and inserting
    // with those as the channum would result in few conflicts
    // ask user if it is ok to to proceed using it as the channum

    // for remaining channels with complete tuning information
    // insert channels with contiguous list of numbers as the channums
    chantype = (uint) kChannelTypeConflictingFirst;
    for (; chantype <= (uint) kChannelTypeConflictingLast; ++chantype)
    {

        ChannelType type = (ChannelType) chantype;
        uint new_chan, old_chan;
        CountChannels(list, info, type, new_chan, old_chan);
        if (new_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n new conflicting %1 channel(s).",
                             "", new_chan).arg(toString(type));

            InsertAction action = QueryUserInsert(msg);
            list = InsertChannels(list, info, action, type, filtered);
        }
        if (old_chan)
        {
            //: %n is the number of channels, %1 is the type of channel
            QString msg = tr("Found %n conflicting old %1 channel(s).",
                             "", old_chan).arg(toString(type));

            UpdateAction action = QueryUserUpdate(msg);
            list = UpdateChannels(list, info, action, type, filtered);
        }
    }

    // print list of inserted channels
    // print list of ignored channels (by ignored reason category)
    // print list of invalid channels
}

ScanDTVTransportList ChannelImporter::InsertChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    InsertAction action, ChannelType type,
    ScanDTVTransportList &filtered)
{
    QString channelFormat = "%1_%2";

    ScanDTVTransportList next_list;

    bool ignore_rest = false;

    // insert all channels with non-conflicting channum
    // and complete tuning information.
    for (size_t i = 0; i < transports.size(); ++i)
    {
        bool created_new_transport = false;
        ScanDTVTransport new_transport;
        bool created_filter_transport = false;
        ScanDTVTransport filter_transport;

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

            if (ignore_rest)
            {
                cout<<QString("Skipping Insert: %1")
                    .arg(FormatChannel(transports[i], chan))
                    .toLatin1().constData()<<endl;
                handle = false;
            }

            if (handle && kInsertManual == action)
            {
                OkCancelType rc = QueryUserInsert(info, transports[i], chan);
                if (kOCTCancelAll == rc)
                {
                    ignore_rest = true;
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
                    OkCancelType rc =
                        QueryUserResolve(info, transports[i], chan);

                    conflicting = true;
                    if (kOCTCancelAll == rc)
                        ignore_rest = true;
                    else if (kOCTOk == rc)
                        conflicting = false;
                }

                if (conflicting)
                {
                    cout<<QString("Skipping Insert: %1")
                        .arg(FormatChannel(transports[i], chan))
                        .toLatin1().constData()<<endl;
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

                    cout<<"Insert("<<chan.m_si_standard.toLatin1().constData()
                        <<"): "<<chan.m_chan_num.toLatin1().constData()<<endl;

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
                        chan.m_default_authority);

                    if (!transports[i].m_iptv_tuning.GetDataURL().isEmpty())
                        ChannelUtil::CreateIPTVTuningData(chan.m_channel_id,
                                          transports[i].m_iptv_tuning);
                }
            }

            if (filter)
            {
                if (!created_filter_transport)
                {
                    filter_transport = transports[i];
                    filter_transport.m_channels.clear();
                    created_filter_transport = true;
                }
                filter_transport.m_channels.push_back(transports[i].m_channels[j]);
            }
            else if (!inserted)
            {
                if (!created_new_transport)
                {
                    new_transport = transports[i];
                    new_transport.m_channels.clear();
                    created_new_transport = true;
                }
                new_transport.m_channels.push_back(transports[i].m_channels[j]);
            }
        }

        if (created_filter_transport)
            filtered.push_back(filter_transport);

        if (created_new_transport)
            next_list.push_back(new_transport);
    }

    return next_list;
}

ScanDTVTransportList ChannelImporter::UpdateChannels(
    const ScanDTVTransportList &transports,
    const ChannelImporterBasicStats &info,
    UpdateAction action, ChannelType type,
    ScanDTVTransportList &filtered)
{
    QString channelFormat = "%1_%2";
    bool renameChannels = false;

    ScanDTVTransportList next_list;

    // update all channels with non-conflicting channum
    // and complete tuning information.
    for (size_t i = 0; i < transports.size(); ++i)
    {
        bool created_transport = false;
        ScanDTVTransport new_transport;
        bool created_filter_transport = false;
        ScanDTVTransport filter_transport;

        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
        {
            ChannelInsertInfo chan = transports[i].m_channels[j];

            bool filter = false, handle = false;
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
                    cout<<"Skipping Update("
                        <<chan.m_si_standard.toLatin1().constData()<<"): "
                        <<chan.m_chan_num.toLatin1().constData()<<endl;
                    handle = false;
                }
            }

            if (m_is_interactive && (kUpdateManual == action))
            {
                // TODO Ask user how to update this channel..
            }

            bool updated = false;
            if (handle)
            {
                cout<<"Update("<<chan.m_si_standard.toLatin1().constData()<<"): "
                    <<chan.m_chan_num.toLatin1().constData()<<endl;

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
                    chan.m_default_authority);
            }

            if (filter)
            {
                if (!created_filter_transport)
                {
                    filter_transport = transports[i];
                    filter_transport.m_channels.clear();
                    created_filter_transport = true;
                }
                filter_transport.m_channels.push_back(transports[i].m_channels[j]);
            }
            else if (!updated)
            {
                if (!created_transport)
                {
                    new_transport = transports[i];
                    new_transport.m_channels.clear();
                    created_transport = true;
                }
                new_transport.m_channels.push_back(transports[i].m_channels[j]);
            }
        }

        if (created_filter_transport)
            filtered.push_back(filter_transport);

        if (created_transport)
            next_list.push_back(new_transport);
    }

    return next_list;
}

void ChannelImporter::CleanupDuplicates(ScanDTVTransportList &transports) const
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

            // filter channels out only in channels.conf, i.e. not found
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

// Remove the channels that do not have a logical channel number
void ChannelImporter::FilterChannelNumber(ScanDTVTransportList &transports) const
{
    for (size_t i = 0; i < transports.size(); ++i)
    {
        ChannelInsertInfoList filtered;
        for (size_t k = 0; k < transports[i].m_channels.size(); ++k)
        {
            if (transports[i].m_channels[k].m_chan_num.isEmpty())
            {
                QString msg = FormatChannel(transports[i], transports[i].m_channels[k]);
                LOG(VB_GENERAL, LOG_DEBUG, QString("No LCN: %1").arg(msg));
                continue;
            }
            filtered.push_back(transports[i].m_channels[k]);
        }
        transports[i].m_channels = filtered;
    }
}

/** \fn ChannelImporter::GetDBTransports(uint,ScanDTVTransportList&) const
 *  \brief Adds found channel info to transports list,
 *         returns channels in DB which were not found in scan
 */
ScanDTVTransportList ChannelImporter::GetDBTransports(
    uint sourceid, ScanDTVTransportList &transports) const
{
    ScanDTVTransportList not_in_scan;

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
        uint mplexid = query.value(0).toUInt();

        ScanDTVTransport newt;
        if (!newt.FillFromDB(tuner_type, mplexid))
            continue;

        bool newt_found = false;
        QMap<uint,bool> found_chan;

        for (size_t i = 0; i < transports.size(); ++i)
        {
            if (!transports[i].IsEqual(tuner_type, newt, 500 * freq_mult, true))
                continue;

            transports[i].m_mplex = mplexid;
            newt_found = true;
            for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
            {
                ChannelInsertInfo &chan = transports[i].m_channels[j];
                for (size_t k = 0; k < newt.m_channels.size(); ++k)
                {
                    if (newt.m_channels[k].IsSameChannel(chan, true))
                    {
                        found_chan[k] = true;
                        chan.m_db_mplexid = mplexid;
                        chan.m_channel_id = newt.m_channels[k].m_channel_id;
                    }
                }
            }
            break;
        }

        if (!newt_found)
        {
            /* XXX HACK -- begin
             * disabling adding transponders not found in the scan list
             * to the db list to avoid deleting many channels as off air
             * for a single transponder scan
            not_in_scan.push_back(newt);
             * XXX HACK -- end */
        }
        else
        {
            ScanDTVTransport tmp = newt;
            tmp.m_channels.clear();

            for (size_t k = 0; k < newt.m_channels.size(); ++k)
            {
                if (!found_chan[k])
                    tmp.m_channels.push_back(newt.m_channels[k]);
            }

            if (!tmp.m_channels.empty())
                not_in_scan.push_back(tmp);
        }
    }

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
    const ScanDTVTransportList      &transports,
    const ChannelImporterBasicStats &info)
{
    QString msg;

    for (size_t i = 0; i < transports.size(); ++i)
        for (size_t j = 0; j < transports[i].m_channels.size(); ++j)
            msg += FormatChannel(transports[i], transports[i].m_channels[j],
                                 &info) + "\n";

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
            return ((chan.m_si_standard == "atsc") &&
                    (info.m_atscnum_cnt[(chan.m_atsc_major_channel << 16) |
                                        (chan.m_atsc_minor_channel)] == 1));

        case kDVBNonConflicting:
            return ((chan.m_si_standard == "dvb") &&
                    (info.m_prognum_cnt[chan.m_service_id] == 1));

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

/**
 * \fn ChannelImporter::ComputeSuggestedChannelNum
 *
 * Compute a suggested channel number based on various aspects of the
 * channel information. Check to see if this channel number conflicts
 * with an existing channel number. If so, fall back to incrementing a
 * per-source number to find an unused value.
 *
 * \param info       Unused.
 * \param transport  Unused.
 * \param chan       Info describing a channel
 * \return Returns a simple name for the channel.
 */
QString ChannelImporter::ComputeSuggestedChannelNum(
    const ChannelImporterBasicStats &/*info*/,
    const ScanDTVTransport          &/*transport*/,
    const ChannelInsertInfo         &chan)
{
    static QMutex          last_free_lock;
    static QMap<uint,uint> last_free_chan_num_map;

    QString channelFormat = "%1_%2";
    QString chan_num = channelFormat
        .arg(chan.m_atsc_major_channel)
        .arg(chan.m_atsc_minor_channel);

    if (!chan.m_atsc_minor_channel)
    {
        if (chan.m_si_standard == "dvb")
        {
            chan_num = QString("%1")
                          .arg(chan.m_service_id);
        }
        else if (chan.m_freqid.isEmpty())
        {
            chan_num = QString("%1-%2")
                          .arg(chan.m_source_id)
                          .arg(chan.m_service_id);
        }
        else
        {
            chan_num = QString("%1-%2")
                          .arg(chan.m_freqid)
                          .arg(chan.m_service_id);
        }
    }

    if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
        return chan_num;

    QMutexLocker locker(&last_free_lock);
    uint last_free_chan_num = last_free_chan_num_map[chan.m_source_id];
    for (last_free_chan_num++; ; ++last_free_chan_num)
    {
        chan_num = QString::number(last_free_chan_num);
        if (!ChannelUtil::IsConflicting(chan_num, chan.m_source_id))
            break;
    }
    // cppcheck-suppress unreadVariable
    last_free_chan_num_map[chan.m_source_id] = last_free_chan_num;

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
            MythDialogBox *deleteDialog =
                new MythDialogBox(msg, popupStack, "deletechannels");

            if (deleteDialog->Create())
            {
                deleteDialog->AddButton(tr("Delete all"));
                deleteDialog->AddButton(tr("Set all invisible"));
//                  deleteDialog->AddButton(tr("Handle manually"));
                deleteDialog->AddButton(tr("Ignore all"));
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
             << tr("1. Delete all").toLatin1().constData()
             << endl
             << tr("2. Set all invisible").toLatin1().constData()
             << endl
//        cout << "3. Handle manually" << endl;
             << tr("4. Ignore all").toLatin1().constData()
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
            MythDialogBox *insertDialog =
                new MythDialogBox(msg, popupStack, "insertchannels");

            if (insertDialog->Create())
            {
                insertDialog->AddButton(tr("Insert all"));
                insertDialog->AddButton(tr("Insert manually"));
                insertDialog->AddButton(tr("Ignore all"));
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
             << tr("1. Insert all").toLatin1().constData()
             << endl
             << tr("2. Insert manually").toLatin1().constData()
             << endl
             << tr("3. Ignore all").toLatin1().constData()
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
            MythDialogBox *updateDialog =
                new MythDialogBox(msg, popupStack, "updatechannels");

            if (updateDialog->Create())
            {
                updateDialog->AddButton(tr("Update all"));
                updateDialog->AddButton(tr("Update manually"));
                updateDialog->AddButton(tr("Ignore all"));
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
        action = (1 == ret) ? kUpdateManual    : action;
        action = (2 == ret) ? kUpdateIgnoreAll : action;
    }
    else if (m_is_interactive)
    {
        cout << msg.toLatin1().constData()
             << endl
             << tr("Do you want to:").toLatin1().constData()
             << endl
             << tr("1. Update all").toLatin1().constData()
             << endl
             << tr("2. Update manually").toLatin1().constData()
             << endl
             << tr("3. Ignore all").toLatin1().constData()
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
    MythDialogBox *popup = new MythDialogBox(title, message, popupStack,
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

    if (1 == dc)
    {
        MythTextInputDialog *textEdit =
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

    bool ok = (0 == dc);

    return (ok) ? kOCTOk :
        ((1 == dc) ? kOCTCancel : kOCTCancelAll);
}

OkCancelType ChannelImporter::QueryUserResolve(
    const ChannelImporterBasicStats &info,
    const ScanDTVTransport          &transport,
    ChannelInsertInfo               &chan)
{
    QString msg = tr("Channel %1 was found to be in conflict with other "
                     "channels.").arg(SimpleFormatChannel(transport, chan));

    OkCancelType ret = kOCTCancel;

    if (m_use_gui)
    {
        while (true)
        {
            QString msg2 = msg;
            msg2 += " ";
            msg2 += tr("Please enter a unique channel number.");

            QString val = ComputeSuggestedChannelNum(info, transport, chan);
            msg2 += " ";
            msg2 += tr("Default value is %1").arg(val);
            ret = ShowManualChannelPopup(
                GetMythMainWindow(), tr("Channel Importer"),
                msg2, val);

            if (kOCTOk != ret)
                break; // user canceled..

            bool ok = (val.length() >= 1);
            ok = ok && ((val[0] >= '0') && (val[0] <= '9'));
            ok = ok && !ChannelUtil::IsConflicting(
                val, chan.m_source_id, chan.m_channel_id);

            chan.m_chan_num = (ok) ? val : chan.m_chan_num;
            if (ok)
                break;
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

            bool ok = (val.length() >= 1);
            ok = ok && ((val[0] >= '0') && (val[0] <= '9'));
            ok = ok && !ChannelUtil::IsConflicting(
                val, chan.m_source_id, chan.m_channel_id);

            chan.m_chan_num = (ok) ? val : chan.m_chan_num;
            if (ok)
            {
                ret = kOCTOk;
                break;
            }
        }
    }

    return ret;
}

OkCancelType ChannelImporter::QueryUserInsert(
    const ChannelImporterBasicStats &info,
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

            QString val = ComputeSuggestedChannelNum(info, transport, chan);
            msg2 += " ";
            msg2 += tr("Default value is %1").arg(val);
            ret = ShowManualChannelPopup(
                GetMythMainWindow(), tr("Channel Importer"),
                msg2, val);

            if (kOCTOk != ret)
                break; // user canceled..

            bool ok = (val.length() >= 1);
            ok = ok && ((val[0] >= '0') && (val[0] <= '9'));
            ok = ok && !ChannelUtil::IsConflicting(
                val, chan.m_source_id, chan.m_channel_id);

            chan.m_chan_num = (ok) ? val : chan.m_chan_num;
            if (ok)
            {
                ret = kOCTOk;
                break;
            }
        }
    }
    else if (m_is_interactive)
    {
        cout << msg.toLatin1().constData() << endl;

        QString cancelStr    = QCoreApplication::translate("(Common)",
                                                           "Cancel").toLower();
        QString cancelAllStr = QCoreApplication::translate("(Common)",
                                   "Cancel All").toLower();

        //: %1 is the translation of "cancel", %2 of "cancel all"
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

            bool ok = (val.length() >= 1);
            ok = ok && ((val[0] >= '0') && (val[0] <= '9'));
            ok = ok && !ChannelUtil::IsConflicting(
                val, chan.m_source_id, chan.m_channel_id);

            chan.m_chan_num = (ok) ? val : chan.m_chan_num;
            if (ok)
            {
                ret = kOCTOk;
                break;
            }
        }
    }

    return ret;
}
