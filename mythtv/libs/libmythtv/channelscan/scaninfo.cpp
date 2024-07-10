// C++ headers
#include <cstdint>
#include <utility>

// Qt headers
#include <QString>

// MythTV headers
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "scaninfo.h"

ScanInfo::ScanInfo(uint scanid, uint cardid, uint sourceid,
                   bool processed, QDateTime scandate) :
    m_scanid(scanid), m_cardid(cardid), m_sourceid(sourceid),
    m_processed(processed), m_scandate(std::move(scandate))
{
}

uint SaveScan(const ScanDTVTransportList &scan)
{
    LOG(VB_CHANSCAN, LOG_INFO, QString("SaveScan() scan.size(): %1")
            .arg(scan.size()));

    uint scanid = 0;
    if (scan.empty() || scan[0].m_channels.empty())
        return scanid;

    uint sourceid = scan[0].m_channels[0].m_sourceId;
    uint cardid   = scan[0].m_cardid;

    // Delete saved scans when there are too many or when they are too old
    const std::vector<ScanInfo> list = LoadScanList(sourceid);
    for (uint i = 0; i < list.size(); i++)
    {
        if (((i + 10) < (list.size())) ||
            (list[i].m_scandate < MythDate::current().addMonths(-6)))
        {
            LOG(VB_CHANSCAN, LOG_INFO, "SaveScan() " +
                QString("Delete saved scan id:%1 date:%2")
                .arg(list[i].m_scanid).arg(list[i].m_scandate.toString()));
            ScanInfo::DeleteScan(list[i].m_scanid);
        }
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "INSERT INTO channelscan ( cardid,  sourceid,  scandate) "
        "VALUES                  (:CARDID, :SOURCEID, :SCANDATE) ");
    query.bindValue(":CARDID",   cardid);
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":SCANDATE", MythDate::current());

    if (!query.exec())
    {
        MythDB::DBError("SaveScan 1", query);
        return scanid;
    }

    query.prepare("SELECT MAX(scanid) FROM channelscan");
    if (!query.exec())
        MythDB::DBError("SaveScan 2", query);
    else if (query.next())
        scanid = query.value(0).toUInt();

    if (!scanid)
        return scanid;

    for (const auto & si : scan)
        si.SaveScan(scanid);

    return scanid;
}

ScanDTVTransportList LoadScan(uint scanid)
{
    ScanDTVTransportList list;
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());
    query.prepare(
        "SELECT frequency,         inversion,      symbolrate, "            // 0, 1, 2
        "       fec,               polarity, "                              // 3, 4
        "       hp_code_rate,      lp_code_rate,   modulation, "            // 5, 6, 7
        "       transmission_mode, guard_interval, hierarchy, "             // 8, 9, 10
        "       modulation,        bandwidth,      sistandard, "            // 11, 12, 13
        "       tuner_type,        transportid,    mod_sys, "               // 14, 15, 16
        "       rolloff,           signal_strength "                        // 17, 18
        "FROM channelscan_dtv_multiplex "
        "WHERE scanid = :SCANID");
    query.bindValue(":SCANID", scanid);
    if (!query.exec())
    {
        MythDB::DBError("LoadScan 1", query);
        return list;
    }

    while (query.next())
    {
        ScanDTVTransport mux;
        mux.ParseTuningParams(
            (DTVTunerType) query.value(14).toUInt(),
            query.value(0).toString(),  query.value(1).toString(),          // frequency            inversion
            query.value(2).toString(),  query.value(3).toString(),          // symbolrate           fec
            query.value(4).toString(),  query.value(5).toString(),          // polarity             hp_code_rate
            query.value(6).toString(),  query.value(7).toString(),          // lp_code_rate         modulation
            query.value(8).toString(),  query.value(9).toString(),          // transmission_mode    guard_interval
            query.value(10).toString(), query.value(11).toString(),         // hierarchy            modulation
            query.value(12).toString(), query.value(16).toString(),         // bandwidth            mod_sys
            query.value(17).toString(), query.value(18).toString());        // roloff               signal_strength

        mux.m_sistandard = query.value(13).toString();                      // sistandard

        query2.prepare(
            "SELECT "
            "    mplex_id,           source_id,          channel_id,         "      // 0, 1, 2
            "    callsign,           service_name,       chan_num,           "      // 3, 4, 5
            "    service_id,         atsc_major_channel, atsc_minor_channel, "      // 6, 7, 8
            "    use_on_air_guide,   hidden,             hidden_in_guide,    "      // 9, 10, 11
            "    freqid,             icon,               tvformat,           "      // 12, 13, 14
            "    xmltvid,            pat_tsid,           vct_tsid,           "      // 15, 16, 17
            "    vct_chan_tsid,      sdt_tsid,           orig_netid,         "      // 18, 19, 20
            "    netid,              si_standard,        in_channels_conf,   "      // 21, 22, 23
            "    in_pat,             in_pmt,             in_vct,             "      // 24, 25, 26
            "    in_nit,             in_sdt,             is_encrypted,       "      // 27, 28, 29
            "    is_data_service,    is_audio_service,   is_opencable,       "      // 30, 31, 32
            "    could_be_opencable, decryption_status,  default_authority,  "      // 33, 34, 35
            "    service_type,       logical_channel,    simulcast_channel   "      // 36, 37, 38
            "FROM channelscan_channel "
            "WHERE transportid = :TRANSPORTID");
        query2.bindValue(":TRANSPORTID", query.value(15).toUInt());

        if (!query2.exec())
        {
            MythDB::DBError("LoadScan 2", query2);
            continue;
        }

        while (query2.next())
        {
            QString si_standard = query2.value(22).toString();
            si_standard = (si_standard.isEmpty()) ?
                query.value(13).toString() : si_standard;

            ChannelInsertInfo chan(
                query2.value(0).toUInt(),           // mplex_id
                query2.value(1).toUInt(),           // source_id
                query2.value(2).toUInt(),           // channel_id
                query2.value(3).toString(),         // callsign
                query2.value(4).toString(),         // service_name
                query2.value(5).toString(),         // chan_num
                query2.value(6).toUInt(),           // service_id

                query2.value(7).toUInt(),           // atsc_major_channel
                query2.value(8).toUInt(),           // atsc_minor_channel
                query2.value(9).toBool(),           // use_on_air_guide
                query2.value(10).toBool(),          // hidden
                query2.value(11).toBool(),          // hidden_in_guide

                query2.value(12).toString(),        // freqid
                query2.value(13).toString(),        // icon
                query2.value(14).toString(),        // tvformat
                query2.value(15).toString(),        // xmltvid

                query2.value(16).toUInt(),          // pat_tsid
                query2.value(17).toUInt(),          // vct_tsid
                query2.value(18).toUInt(),          // vct_chan_tsid
                query2.value(19).toUInt(),          // sdt_tsid

                query2.value(20).toUInt(),          // orig_netid
                query2.value(21).toUInt(),          // netid

                si_standard,

                query2.value(23).toBool(),          // in_channels_conf
                query2.value(24).toBool(),          // in_pat
                query2.value(25).toBool(),          // in_pmt
                query2.value(26).toBool(),          // in_vct
                query2.value(27).toBool(),          // in_nit
                query2.value(28).toBool(),          // in_sdt

                query2.value(29).toBool(),          // is_encrypted
                query2.value(30).toBool(),          // is_data_service
                query2.value(31).toBool(),          // is_audio_service
                query2.value(32).toBool(),          // is_opencable
                query2.value(33).toBool(),          // could_be_opencable
                query2.value(34).toInt(),           // decryption_status
                query2.value(35).toString(),        // default_authority
                query2.value(36).toUInt(),          // service_type
                query2.value(37).toUInt(),          // logical_channel
                query2.value(38).toUInt());         // simulcast_channel
            mux.m_channels.push_back(chan);
        }

        list.push_back(mux);
    }

    return list;
}

bool ScanInfo::MarkProcessed(uint scanid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE channelscan "
        "SET processed = 1 "
        "WHERE scanid = :SCANID");
    query.bindValue(":SCANID", scanid);

    if (!query.exec())
    {
        MythDB::DBError("MarkProcessed", query);
        return false;
    }

    return true;
}

bool ScanInfo::DeleteScan(uint scanid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM channelscan_channel "
        "WHERE scanid = :SCANID");
    query.bindValue(":SCANID", scanid);

    if (!query.exec())
    {
        MythDB::DBError("DeleteScan", query);
        return false;
    }

    query.prepare(
        "DELETE FROM channelscan_dtv_multiplex "
        "WHERE scanid = :SCANID");
    query.bindValue(":SCANID", scanid);

    if (!query.exec())
    {
        MythDB::DBError("DeleteScan", query);
        return false;
    }

    query.prepare(
        "DELETE FROM channelscan "
        "WHERE scanid = :SCANID");
    query.bindValue(":SCANID", scanid);

    if (!query.exec())
    {
        MythDB::DBError("DeleteScan", query);
        return false;
    }

    return true;
}

void ScanInfo::DeleteScansFromSource(uint sourceid)
{
    std::vector<ScanInfo> scans = LoadScanList(sourceid);
    for (auto &scan : scans)
    {
        DeleteScan(scan.m_scanid);
    }
}

std::vector<ScanInfo> LoadScanList(void)
{
    std::vector<ScanInfo> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT scanid, cardid, sourceid, processed, scandate "
        "FROM channelscan "
        "ORDER BY scanid, sourceid, cardid, scandate");

    if (!query.exec())
    {
        MythDB::DBError("LoadScanList", query);
        return list;
    }

    while (query.next())
    {
        list.emplace_back(query.value(0).toUInt(),
                          query.value(1).toUInt(),
                          query.value(2).toUInt(),
                          (bool) query.value(3).toUInt(),
                          MythDate::as_utc(query.value(4).toDateTime()));
    }

    return list;
}

std::vector<ScanInfo> LoadScanList(uint sourceid)
{
    std::vector<ScanInfo> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT scanid, cardid, sourceid, processed, scandate "
        "FROM channelscan "
        "WHERE sourceid = :SOURCEID "
        "ORDER BY scanid, sourceid, cardid, scandate");
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec())
    {
        MythDB::DBError("LoadScanList", query);
        return list;
    }

    while (query.next())
    {
        list.emplace_back(query.value(0).toUInt(),
                          query.value(1).toUInt(),
                          query.value(2).toUInt(),
                          (bool) query.value(3).toUInt(),
                          MythDate::as_utc(query.value(4).toDateTime()));
    }

    return list;
}
