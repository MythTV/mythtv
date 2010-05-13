// POSIX headers
#include <stdint.h>

// Qt headers
#include <QString>

// MythTV headers
#include "mythdb.h"
#include "scaninfo.h"
#include "mythdbcon.h"
#include "mythverbose.h"

ScanInfo::ScanInfo() : scanid(0), cardid(0), sourceid(0), processed(false) { }

ScanInfo::ScanInfo(uint _scanid, uint _cardid, uint _sourceid,
                   bool _processed, const QDateTime &_scandate) :
    scanid(_scanid), cardid(_cardid), sourceid(_sourceid),
    processed(_processed), scandate(_scandate)
{
}

uint SaveScan(const ScanDTVTransportList &scan)
{
    VERBOSE(VB_CHANSCAN, QString("SaveScan() scan.size(): %1")
            .arg(scan.size()));

    uint scanid = 0;
    if (scan.empty() || scan[0].channels.empty())
        return scanid;

    uint sourceid = scan[0].channels[0].source_id;
    uint cardid   = scan[0].cardid;

    // Delete very old scans
    const vector<ScanInfo> list = LoadScanList();
    for (uint i = 0; i < list.size(); i++)
    {
        if (list[i].scandate > QDateTime::currentDateTime().addDays(-14))
            continue;
        if ((list[i].cardid == cardid) && (list[i].sourceid == sourceid))
            ScanInfo::DeleteScan(list[i].scanid);
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "INSERT INTO channelscan ( cardid,  sourceid,  scandate) "
        "VALUES                  (:CARDID, :SOURCEID, :SCANDATE) ");
    query.bindValue(":CARDID",   cardid);
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":SCANDATE", QDateTime::currentDateTime());

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

    for (uint i = 0; i < scan.size(); i++)
        scan[i].SaveScan(scanid);

    return scanid;
}

ScanDTVTransportList LoadScan(uint scanid)
{
    ScanDTVTransportList list;
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());
    query.prepare(
        "SELECT frequency,         inversion,      symbolrate, "
        "       fec,               polarity, "
        "       hp_code_rate,      lp_code_rate,   modulation, "
        "       transmission_mode, guard_interval, hierarchy, "
        "       modulation,        bandwidth,      sistandard, "
        "       tuner_type,        transportid,    mod_sys, "
        "       rolloff "
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
            query.value(0).toString(),  query.value(1).toString(),
            query.value(2).toString(),  query.value(3).toString(),
            query.value(4).toString(),  query.value(5).toString(),
            query.value(6).toString(),  query.value(7).toString(),
            query.value(8).toString(),  query.value(9).toString(),
            query.value(10).toString(), query.value(11).toString(),
            query.value(12).toString(), query.value(13).toString(),
            query.value(14).toString());

        query2.prepare(
            "SELECT "
            "    mplex_id,           source_id,          channel_id,         "
            "    callsign,           service_name,       chan_num,           "
            "    service_id,         atsc_major_channel, atsc_minor_channel, "
            "    use_on_air_guide,   hidden,             hidden_in_guide,    "
            "    freqid,             icon,               tvformat,           "
            "    xmltvid,            pat_tsid,           vct_tsid,           "
            "    vct_chan_tsid,      sdt_tsid,           orig_netid,         "
            "    netid,              si_standard,        in_channels_conf,   "
            "    in_pat,             in_pmt,             in_vct,             "
            "    in_nit,             in_sdt,             is_encrypted,       "
            "    is_data_service,    is_audio_service,   is_opencable,       "
            "    could_be_opencable, decryption_status,  default_authority   "
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
                query2.value(0).toUInt()/*mplex_id*/,
                query2.value(1).toUInt()/*source_id*/,
                query2.value(2).toUInt()/*channel_id*/,
                query2.value(3).toString()/*callsign*/,
                query2.value(4).toString()/*service_name*/,
                query2.value(5).toString()/*chan_num*/,
                query2.value(6).toUInt()/*service_id*/,

                query2.value(7).toUInt()/*atsc_major_channel*/,
                query2.value(8).toUInt()/*atsc_minor_channel*/,
                query2.value(9).toBool()/*use_on_air_guide*/,
                query2.value(10).toBool()/*hidden*/,
                query2.value(11).toBool()/*hidden_in_guide*/,

                query2.value(12).toString()/*freqid*/,
                query2.value(13).toString()/*icon*/,
                query2.value(14).toString()/*tvformat*/,
                query2.value(15).toString()/*xmltvid*/,

                query2.value(16).toUInt()/*pat_tsid*/,
                query2.value(17).toUInt()/*vct_tsid*/,
                query2.value(18).toUInt()/*vct_chan_tsid*/,
                query2.value(19).toUInt()/*sdt_tsid*/,

                query2.value(20).toUInt()/*orig_netid*/,
                query2.value(21).toUInt()/*netid*/,

                si_standard,

                query2.value(23).toBool()/*in_channels_conf*/,
                query2.value(24).toBool()/*in_pat*/,
                query2.value(25).toBool()/*in_pmt*/,
                query2.value(26).toBool()/*in_vct*/,
                query2.value(27).toBool()/*in_nit*/,
                query2.value(28).toBool()/*in_sdt*/,

                query2.value(29).toBool()/*is_encrypted*/,
                query2.value(30).toBool()/*is_data_service*/,
                query2.value(31).toBool()/*is_audio_service*/,
                query2.value(32).toBool()/*is_opencable*/,
                query2.value(33).toBool()/*could_be_opencable*/,
                query2.value(34).toInt()/*decryption_status*/,
                query2.value(35).toString()/*default_authority*/);

            mux.channels.push_back(chan);
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

vector<ScanInfo> LoadScanList(void)
{
    vector<ScanInfo> list;

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
        list.push_back(
            ScanInfo(query.value(0).toUInt(),
                     query.value(1).toUInt(),
                     query.value(2).toUInt(),
                     (bool) query.value(3).toUInt(),
                     query.value(4).toDateTime()));
    }

    return list;
}
