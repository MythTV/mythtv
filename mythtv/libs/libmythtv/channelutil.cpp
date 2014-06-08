// -*- Mode: c++ -*-

#include <stdint.h>

#include <algorithm>
#include <set>
using namespace std;

#include <QRegExp>
#include <QImage>
#include <QFile>
#include <QReadWriteLock>
#include <QHash>

#include "channelutil.h"
#include "mythdb.h"
#include "dvbtables.h"
#include "mythmiscutil.h"

#define LOC QString("ChanUtil: ")

const QString ChannelUtil::kATSCSeparators = "(_|-|#|\\.)";

static uint get_dtv_multiplex(uint     db_source_id,  QString sistandard,
                              uint64_t frequency,
                              // DVB specific
                              uint     transport_id,
                              // tsid exists with other sistandards,
                              // but we only trust it in dvb-land.
                              uint     network_id,
                              // must check polarity for dvb-s
                              signed char polarity)
{
    QString qstr =
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE sourceid     = :SOURCEID   "
        "  AND sistandard   = :SISTANDARD ";

    if (sistandard.toLower() != "dvb")
        qstr += "AND frequency    = :FREQUENCY   ";
    else
    {
        qstr += "AND transportid  = :TRANSPORTID ";
        qstr += "AND networkid    = :NETWORKID   ";
        qstr += "AND polarity     = :POLARITY    ";
    }


    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(qstr);

    query.bindValue(":SOURCEID",          db_source_id);
    query.bindValue(":SISTANDARD",        sistandard);

    if (sistandard.toLower() != "dvb")
        query.bindValue(":FREQUENCY",   QString::number(frequency));
    else
    {
        query.bindValue(":TRANSPORTID", transport_id);
        query.bindValue(":NETWORKID",   network_id);
        query.bindValue(":POLARITY",    QString(polarity));
    }

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("get_dtv_multiplex", query);
        return 0;
    }

    if (query.next())
        return query.value(0).toUInt();

    return 0;
}

static uint insert_dtv_multiplex(
    int         db_source_id,  QString     sistandard,
    uint64_t    frequency,     QString     modulation,
    // DVB specific
    int         transport_id,  int         network_id,
    int         symbol_rate,   signed char bandwidth,
    signed char polarity,      signed char inversion,
    signed char trans_mode,
    QString     inner_FEC,     QString      constellation,
    signed char hierarchy,     QString      hp_code_rate,
    QString     lp_code_rate,  QString      guard_interval,
    QString     mod_sys,       QString      rolloff)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // If transport is already present, skip insert
    uint mplex = get_dtv_multiplex(
        db_source_id,  sistandard,    frequency,
        // DVB specific
        transport_id,  network_id, polarity);

    LOG(VB_CHANSCAN, LOG_INFO, QString(
                "insert_dtv_multiplex(db_source_id: %1, sistandard: '%2', "
                "frequency: %3, modulation: %4, transport_id: %5, "
                "network_id: %6, polarity: %7...) mplexid:%8")
            .arg(db_source_id).arg(sistandard)
            .arg(frequency).arg(modulation)
            .arg(transport_id).arg(network_id)
            .arg(polarity).arg(mplex));

    bool isDVB = (sistandard.toLower() == "dvb");

    QString updateStr =
        "UPDATE dtv_multiplex "
        "SET frequency    = :FREQUENCY1, ";

    updateStr += (!modulation.isNull()) ?
        "modulation       = :MODULATION, " : "";
    updateStr += (symbol_rate >= 0) ?
        "symbolrate       = :SYMBOLRATE, " : "";
    updateStr += (bandwidth   >= 0) ?
        "bandwidth        = :BANDWIDTH, " : "";
    updateStr += (polarity    >= 0) ?
        "polarity         = :POLARITY, " : "";
    updateStr += (inversion   >= 0) ?
        "inversion        = :INVERSION, " : "";
    updateStr += (trans_mode  >= 0) ?
        "transmission_mode= :TRANS_MODE, " : "";
    updateStr += (!inner_FEC.isNull()) ?
        "fec              = :INNER_FEC, " : "";
    updateStr += (!constellation.isNull()) ?
        "constellation    = :CONSTELLATION, " : "";
    updateStr += (hierarchy  >= 0) ?
        "hierarchy        = :HIERARCHY, " : "";
    updateStr += (!hp_code_rate.isNull()) ?
        "hp_code_rate     = :HP_CODE_RATE, " : "";
    updateStr += (!lp_code_rate.isNull()) ?
        "lp_code_rate     = :LP_CODE_RATE, " : "";
    updateStr += (!guard_interval.isNull()) ?
        "guard_interval   = :GUARD_INTERVAL, " : "";
    updateStr += (!mod_sys.isNull()) ?
        "mod_sys          = :MOD_SYS, " : "";
    updateStr += (symbol_rate >= 0) ?
        "rolloff          = :ROLLOFF, " : "";
    updateStr += (transport_id && !isDVB) ?
        "transportid      = :TRANSPORTID, " : "";

    updateStr = updateStr.left(updateStr.length()-2) + " ";

    updateStr +=
        "WHERE sourceid    = :SOURCEID      AND "
        "      sistandard  = :SISTANDARD    AND ";

    updateStr += (isDVB) ?
        " polarity     = :WHEREPOLARITY      AND "
        " transportid = :TRANSPORTID AND networkid = :NETWORKID " :
        " frequency = :FREQUENCY2 ";

    QString insertStr =
        "INSERT INTO dtv_multiplex "
        "  (sourceid,        sistandard,        frequency,  ";

    insertStr += (!modulation.isNull())     ? "modulation, "        : "";
    insertStr += (transport_id || isDVB)    ? "transportid, "       : "";
    insertStr += (isDVB)                    ? "networkid, "         : "";
    insertStr += (symbol_rate >= 0)         ? "symbolrate, "        : "";
    insertStr += (bandwidth   >= 0)         ? "bandwidth, "         : "";
    insertStr += (polarity    >= 0)         ? "polarity, "          : "";
    insertStr += (inversion   >= 0)         ? "inversion, "         : "";
    insertStr += (trans_mode  >= 0)         ? "transmission_mode, " : "";
    insertStr += (!inner_FEC.isNull())      ? "fec, "               : "";
    insertStr += (!constellation.isNull())  ? "constellation, "     : "";
    insertStr += (hierarchy  >= 0)          ? "hierarchy, "         : "";
    insertStr += (!hp_code_rate.isNull())   ? "hp_code_rate, "      : "";
    insertStr += (!lp_code_rate.isNull())   ? "lp_code_rate, "      : "";
    insertStr += (!guard_interval.isNull()) ? "guard_interval, "    : "";
    insertStr += (!mod_sys.isNull())        ? "mod_sys, "           : "";
    insertStr += (!rolloff.isNull())        ? "rolloff, "           : "";
    insertStr = insertStr.left(insertStr.length()-2) + ") ";

    insertStr +=
        "VALUES "
        "  (:SOURCEID,      :SISTANDARD,       :FREQUENCY1, ";
    insertStr += (!modulation.isNull())     ? ":MODULATION, "       : "";
    insertStr += (transport_id || isDVB)    ? ":TRANSPORTID, "      : "";
    insertStr += (isDVB)                    ? ":NETWORKID, "        : "";
    insertStr += (symbol_rate >= 0)         ? ":SYMBOLRATE, "       : "";
    insertStr += (bandwidth   >= 0)         ? ":BANDWIDTH, "        : "";
    insertStr += (polarity    >= 0)         ? ":POLARITY, "         : "";
    insertStr += (inversion   >= 0)         ? ":INVERSION, "        : "";
    insertStr += (trans_mode  >= 0)         ? ":TRANS_MODE, "       : "";
    insertStr += (!inner_FEC.isNull())      ? ":INNER_FEC, "        : "";
    insertStr += (!constellation.isNull())  ? ":CONSTELLATION, "    : "";
    insertStr += (hierarchy   >= 0)         ? ":HIERARCHY, "        : "";
    insertStr += (!hp_code_rate.isNull())   ? ":HP_CODE_RATE, "     : "";
    insertStr += (!lp_code_rate.isNull())   ? ":LP_CODE_RATE, "     : "";
    insertStr += (!guard_interval.isNull()) ? ":GUARD_INTERVAL, "   : "";
    insertStr += (!mod_sys.isNull())        ? ":MOD_SYS, "          : "";
    insertStr += (!rolloff.isNull())        ? ":ROLLOFF, "          : "";
    insertStr = insertStr.left(insertStr.length()-2) + ");";

    query.prepare((mplex) ? updateStr : insertStr);

    query.bindValue(":SOURCEID",          db_source_id);
    query.bindValue(":SISTANDARD",        sistandard);
    query.bindValue(":FREQUENCY1",        QString::number(frequency));

    if (mplex)
    {
        if (isDVB)
        {
            query.bindValue(":TRANSPORTID",   transport_id);
            query.bindValue(":NETWORKID",    network_id);
            query.bindValue(":WHEREPOLARITY", QString(polarity));
        }
        else
        {
            query.bindValue(":FREQUENCY2",    QString::number(frequency));
            if (transport_id)
                query.bindValue(":TRANSPORTID", transport_id);
        }
    }
    else
    {
        if (transport_id || isDVB)
            query.bindValue(":TRANSPORTID",   transport_id);
        if (isDVB)
            query.bindValue(":NETWORKID",    network_id);
    }

    if (!modulation.isNull())
        query.bindValue(":MODULATION",    modulation);

    if (symbol_rate >= 0)
        query.bindValue(":SYMBOLRATE",    symbol_rate);
    if (bandwidth >= 0)
        query.bindValue(":BANDWIDTH",     QString("%1").arg((char)bandwidth));
    if (polarity >= 0)
        query.bindValue(":POLARITY",      QString("%1").arg((char)polarity));
    if (inversion >= 0)
        query.bindValue(":INVERSION",     QString("%1").arg((char)inversion));
    if (trans_mode >= 0)
        query.bindValue(":TRANS_MODE",    QString("%1").arg((char)trans_mode));

    if (!inner_FEC.isNull())
        query.bindValue(":INNER_FEC",     inner_FEC);
    if (!constellation.isNull())
        query.bindValue(":CONSTELLATION", constellation);
    if (hierarchy >= 0)
        query.bindValue(":HIERARCHY",     QString("%1").arg((char)hierarchy));
    if (!hp_code_rate.isNull())
        query.bindValue(":HP_CODE_RATE",  hp_code_rate);
    if (!lp_code_rate.isNull())
        query.bindValue(":LP_CODE_RATE",  lp_code_rate);
    if (!guard_interval.isNull())
        query.bindValue(":GUARD_INTERVAL",guard_interval);
    if (!mod_sys.isNull())
        query.bindValue(":MOD_SYS",       mod_sys);
    if (!rolloff.isNull())
        query.bindValue(":ROLLOFF",       rolloff);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Adding transport to Database.", query);
        return 0;
    }

    if (mplex)
        return mplex;

    mplex = get_dtv_multiplex(
        db_source_id,  sistandard,    frequency,
        // DVB specific
        transport_id,  network_id, polarity);

    LOG(VB_CHANSCAN, LOG_INFO, QString("insert_dtv_multiplex -- ") +
            QString("inserted %1").arg(mplex));

    return mplex;
}

static void handle_transport_desc(vector<uint> &muxes,
                                  const MPEGDescriptor &desc,
                                  uint sourceid, uint tsid, uint netid)
{
    uint tag = desc.DescriptorTag();

    if (tag == DescriptorID::terrestrial_delivery_system)
    {
        const TerrestrialDeliverySystemDescriptor cd(desc);
        uint64_t freq = cd.FrequencyHz();

        // Use the frequency we already have for this mplex
        // as it may be one of the other_frequencies for this mplex
        int mux = ChannelUtil::GetMplexID(sourceid, tsid, netid);
        if (mux > 0)
        {
            QString dummy_mod;
            QString dummy_sistd;
            uint dummy_tsid, dummy_netid;
            ChannelUtil::GetTuningParams(mux, dummy_mod, freq,
                                         dummy_tsid, dummy_netid, dummy_sistd);
        }

        mux = ChannelUtil::CreateMultiplex(
            (int)sourceid,        "dvb",
            freq,                  QString(),
            // DVB specific
            (int)tsid,            (int)netid,
            -1,                   cd.BandwidthString()[0].toLatin1(),
            -1,                   'a',
            cd.TransmissionModeString()[0].toLatin1(),
            QString(),                         cd.ConstellationString(),
            cd.HierarchyString()[0].toLatin1(), cd.CodeRateHPString(),
            cd.CodeRateLPString(),             cd.GuardIntervalString(),
            QString(),                         QString());

        if (mux)
            muxes.push_back(mux);

        /* unused
           HighPriority()
           IsTimeSlicingIndicatorUsed()
           IsMPE_FECUsed()
           NativeInterleaver()
           Alpha()
        */
    }
    else if (tag == DescriptorID::satellite_delivery_system)
    {
        const SatelliteDeliverySystemDescriptor cd(desc);

        uint mux = ChannelUtil::CreateMultiplex(
            sourceid,             "dvb",
            cd.FrequencyHz(),     cd.ModulationString(),
            // DVB specific
            tsid,                 netid,
            cd.SymbolRateHz(),    -1,
            cd.PolarizationString()[0].toLatin1(), 'a',
            -1,
            cd.FECInnerString(),  QString(),
            -1,                   QString(),
            QString(),            QString(),
            cd.ModulationSystemString(), cd.RollOffString());

        if (mux)
            muxes.push_back(mux);

        /* unused
           OrbitalPositionString() == OrbitalLocation
        */
    }
    else if (tag == DescriptorID::cable_delivery_system)
    {
        const CableDeliverySystemDescriptor cd(desc);

        uint mux = ChannelUtil::CreateMultiplex(
            sourceid,             "dvb",
            cd.FrequencyHz(),     cd.ModulationString(),
            // DVB specific
            tsid,                 netid,
            cd.SymbolRateHz(),    -1,
            -1,                   'a',
            -1,
            cd.FECInnerString(),  QString::null,
            -1,                   QString::null,
            QString::null,        QString::null,
            QString::null,        QString::null);

        if (mux)
            muxes.push_back(mux);
    }
}

uint ChannelUtil::CreateMultiplex(int  sourceid,      QString sistandard,
                                  uint64_t frequency, QString modulation,
                                  int  transport_id,  int     network_id)
{
    return CreateMultiplex(
        sourceid,           sistandard,
        frequency,          modulation,
        transport_id,       network_id,
        -1,                 -1,
        -1,                 -1,
        -1,
        QString::null,      QString::null,
        -1,                 QString::null,
        QString::null,      QString::null,
        QString::null,      QString::null);
}

uint ChannelUtil::CreateMultiplex(
    int         sourceid,     QString     sistandard,
    uint64_t    freq,         QString     modulation,
    // DVB specific
    int         transport_id, int         network_id,
    int         symbol_rate,  signed char bandwidth,
    signed char polarity,     signed char inversion,
    signed char trans_mode,
    QString     inner_FEC,    QString     constellation,
    signed char hierarchy,    QString     hp_code_rate,
    QString     lp_code_rate, QString     guard_interval,
    QString     mod_sys,      QString     rolloff)
{
    return insert_dtv_multiplex(
        sourceid,           sistandard,
        freq,               modulation,
        // DVB specific
        transport_id,       network_id,
        symbol_rate,        bandwidth,
        polarity,           inversion,
        trans_mode,
        inner_FEC,          constellation,
        hierarchy,          hp_code_rate,
        lp_code_rate,       guard_interval,
        mod_sys,            rolloff);
}

uint ChannelUtil::CreateMultiplex(uint sourceid, const DTVMultiplex &mux,
                                  int transport_id, int network_id)
{
    return insert_dtv_multiplex(
        sourceid,                         mux.sistandard,
        mux.frequency,                    mux.modulation.toString(),
        // DVB specific
        transport_id,                     network_id,
        mux.symbolrate,                   mux.bandwidth.toChar().toLatin1(),
        mux.polarity.toChar().toLatin1(),  mux.inversion.toChar().toLatin1(),
        mux.trans_mode.toChar().toLatin1(),
        mux.fec.toString(),               mux.modulation.toString(),
        mux.hierarchy.toChar().toLatin1(), mux.hp_code_rate.toString(),
        mux.lp_code_rate.toString(),      mux.guard_interval.toString(),
        mux.mod_sys.toString(),           mux.rolloff.toString());
}


/** \fn ChannelUtil::CreateMultiplexes(int, const NetworkInformationTable*)
 *
 */
vector<uint> ChannelUtil::CreateMultiplexes(
    int sourceid, const NetworkInformationTable *nit)
{
    vector<uint> muxes;

    if (sourceid <= 0)
        return muxes;

    for (uint i = 0; i < nit->TransportStreamCount(); ++i)
    {
        const desc_list_t& list =
            MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                  nit->TransportDescriptorsLength(i));

        uint tsid  = nit->TSID(i);
        uint netid = nit->OriginalNetworkID(i);
        for (uint j = 0; j < list.size(); ++j)
        {
            const MPEGDescriptor desc(list[j]);
            handle_transport_desc(muxes, desc, sourceid, tsid, netid);
        }
    }
    return muxes;
}

uint ChannelUtil::GetMplexID(uint sourceid, const QString &channum)
{
    MSqlQuery query(MSqlQuery::InitCon());
    /* See if mplexid is already in the database */
    query.prepare(
        "SELECT mplexid "
        "FROM channel "
        "WHERE sourceid  = :SOURCEID  AND "
        "      channum   = :CHANNUM");

    query.bindValue(":SOURCEID",  sourceid);
    query.bindValue(":CHANNUM",   channum);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("GetMplexID 0", query);
    else if (query.next())
        return query.value(0).toInt();

    return 0;
}

int ChannelUtil::GetMplexID(uint sourceid, uint64_t frequency)
{
    MSqlQuery query(MSqlQuery::InitCon());
    /* See if mplexid is already in the database */
    query.prepare(
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE sourceid  = :SOURCEID  AND "
        "      frequency = :FREQUENCY");

    query.bindValue(":SOURCEID",  sourceid);
    query.bindValue(":FREQUENCY", QString::number(frequency));

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetMplexID 1", query);
        return -1;
    }

    if (query.next())
        return query.value(0).toInt();

    return -1;
}

int ChannelUtil::GetMplexID(uint sourceid,     uint64_t frequency,
                            uint transport_id, uint     network_id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // See if transport already in database
    query.prepare(
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE networkid   = :NETWORKID   AND "
        "      transportid = :TRANSPORTID AND "
        "      frequency   = :FREQUENCY   AND "
        "      sourceid    = :SOURCEID");

    query.bindValue(":SOURCEID",    sourceid);
    query.bindValue(":NETWORKID",   network_id);
    query.bindValue(":TRANSPORTID", transport_id);
    query.bindValue(":FREQUENCY",   QString::number(frequency));

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetMplexID 2", query);
        return -1;
    }

    if (query.next())
        return query.value(0).toInt();

    return -1;
}

int ChannelUtil::GetMplexID(uint sourceid,
                            uint transport_id, uint network_id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // See if transport already in database
    query.prepare(
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE networkid   = :NETWORKID   AND "
        "      transportid = :TRANSPORTID AND "
        "      sourceid    = :SOURCEID");

    query.bindValue(":SOURCEID",    sourceid);
    query.bindValue(":NETWORKID",   network_id);
    query.bindValue(":TRANSPORTID", transport_id);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetMplexID 3", query);
        return -1;
    }

    if (query.next())
        return query.value(0).toInt();

    return -1;
}

uint ChannelUtil::GetMplexID(uint chanid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    /* See if mplexid is already in the database */
    query.prepare(
        "SELECT mplexid "
        "FROM channel "
        "WHERE chanid = :CHANID");

    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("GetMplexID 4", query);
    else if (query.next())
        return query.value(0).toInt();

    return 0;
}

/** \fn ChannelUtil::GetBetterMplexID(int, int, int)
 *  \brief Returns best match multiplex ID, creating one if needed.
 *
 *   First, see if you can get an exact match based on the current
 *   mplexid's sourceID and the NetworkID/TransportID.
 *
 *   Next, see if current one is NULL, if so update those
 *   values and return current mplexid.
 *
 *   Next, if values were set, see where you can find this
 *   NetworkID/TransportID. If we get an exact match just return it,
 *   since there is no question what mplexid this NetworkId/TransportId
 *   is for. If we get many matches, return CurrentMplexID.
 *
 *   Next, try to repeat query without currentmplexid as source id.
 *   If we get a singe match return it, if we get many matches we
 *   return the first one.
 *
 *   If none of these work return -1.
 *  \return mplexid on success, -1 on failure.
 */

// current_mplexid always exists in scanner, see ScanTranport()
//
int ChannelUtil::GetBetterMplexID(int current_mplexid,
                                  int transport_id,
                                  int network_id)
{
    LOG(VB_CHANSCAN, LOG_INFO,
        QString("GetBetterMplexID(mplexId %1, tId %2, netId %3)")
            .arg(current_mplexid).arg(transport_id).arg(network_id));

    int q_networkid = 0, q_transportid = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("SELECT networkid, transportid "
                          "FROM dtv_multiplex "
                          "WHERE mplexid = %1").arg(current_mplexid));

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Getting mplexid global search", query);
    else if (query.next())
    {
        q_networkid   = query.value(0).toInt();
        q_transportid = query.value(1).toInt();
    }

    // Got a match, return it.
    if ((q_networkid == network_id) && (q_transportid == transport_id))
    {
        LOG(VB_CHANSCAN, LOG_INFO,
            QString("GetBetterMplexID(): Returning perfect match %1")
                .arg(current_mplexid));
        return current_mplexid;
    }

    // Not in DB at all, insert it
    if (!q_networkid && !q_transportid)
    {
        int qsize = query.size();
        query.prepare(QString("UPDATE dtv_multiplex "
                              "SET networkid = %1, transportid = %2 "
                              "WHERE mplexid = %3")
                      .arg(network_id).arg(transport_id).arg(current_mplexid));

        if (!query.exec() || !query.isActive())
            MythDB::DBError("Getting mplexid global search", query);

        LOG(VB_CHANSCAN, LOG_INFO,
            QString("GetBetterMplexID(): net id and transport id "
                    "are null, qsize(%1), Returning %2")
                .arg(qsize).arg(current_mplexid));
        return current_mplexid;
    }

    // We have a partial match, so we try to do better...
    QString theQueries[2] =
    {
        QString("SELECT a.mplexid "
                "FROM dtv_multiplex a, dtv_multiplex b "
                "WHERE a.networkid   = %1 AND "
                "      a.transportid = %2 AND "
                "      a.sourceid    = b.sourceid AND "
                "      b.mplexid     = %3")
        .arg(network_id).arg(transport_id).arg(current_mplexid),

        QString("SELECT mplexid "
                "FROM dtv_multiplex "
                "WHERE networkid = %1 AND "
                "      transportid = %2")
        .arg(network_id).arg(transport_id),
    };

    for (uint i=0; i<2; i++)
    {
        query.prepare(theQueries[i]);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("Finding matching mplexid", query);

        if (query.size() == 1 && query.next())
        {
            LOG(VB_CHANSCAN, LOG_INFO,
                QString("GetBetterMplexID(): query#%1 qsize(%2) "
                        "Returning %3")
                    .arg(i).arg(query.size()).arg(current_mplexid));
            return query.value(0).toInt();
        }

        if (query.next())
        {
            int ret = (i==0) ? current_mplexid : query.value(0).toInt();
            LOG(VB_CHANSCAN, LOG_INFO,
                QString("GetBetterMplexID(): query#%1 qsize(%2) "
                        "Returning %3")
                    .arg(i).arg(query.size()).arg(ret));
            return ret;
        }
    }

    // If you still didn't find this combo return -1 (failure)
    LOG(VB_CHANSCAN, LOG_INFO, "GetBetterMplexID(): Returning -1");
    return -1;
}

bool ChannelUtil::GetTuningParams(uint      mplexid,
                                  QString  &modulation,
                                  uint64_t &frequency,
                                  uint     &dvb_transportid,
                                  uint     &dvb_networkid,
                                  QString  &si_std)
{
    if (!mplexid || (mplexid == 32767)) /* 32767 deals with old lineups */
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT transportid, networkid, frequency, modulation, sistandard "
        "FROM dtv_multiplex "
        "WHERE mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec())
    {
        MythDB::DBError("GetTuningParams failed ", query);
        return false;
    }

    if (!query.next())
        return false;

    dvb_transportid = query.value(0).toUInt();
    dvb_networkid   = query.value(1).toUInt();
    frequency       = query.value(2).toULongLong();
    modulation      = query.value(3).toString();
    si_std          = query.value(4).toString();

    return true;
}

QString ChannelUtil::GetChannelStringField(int chan_id, const QString &field)
{
    if (chan_id < 0)
        return QString::null;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT %1 FROM channel "
            "WHERE chanid=%2").arg(field).arg(chan_id));
    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex 1", query);
        return QString::null;
    }
    
    if (!query.next())
        return QString::null;
    
    return query.value(0).toString();
}

QString ChannelUtil::GetChanNum(int chan_id)
{
    return GetChannelStringField(chan_id, QString("channum"));
}

int ChannelUtil::GetTimeOffset(int chan_id)
{
    return GetChannelStringField(chan_id, QString("tmoffset")).toInt();
}

int ChannelUtil::GetSourceID(int db_mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("SELECT sourceid "
                          "FROM dtv_multiplex "
                          "WHERE mplexid = %1").arg(db_mplexid));

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex", query);
        return -1;
    }

    if (query.next())
        return query.value(0).toInt();
        
    return -1;
}

uint ChannelUtil::GetSourceIDForChannel(uint chanid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT sourceid "
        "FROM channel "
        "WHERE chanid = :CHANID");
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("Selecting channel/dtv_multiplex", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

int ChannelUtil::GetInputID(int source_id, int card_id)
{
    int input_id = -1;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardinputid"
                  " FROM cardinput"
                  " WHERE sourceid = :SOURCEID"
                  " AND cardid = :CARDID");
    query.bindValue(":SOURCEID", source_id);
    query.bindValue(":CARDID", card_id);

    if (query.exec() && query.isActive() && query.next())
        input_id = query.value(0).toInt();

    return input_id;
}

QStringList ChannelUtil::GetCardTypes(uint chanid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype "
                  "FROM capturecard, cardinput, channel "
                  "WHERE channel.chanid   = :CHANID            AND "
                  "      channel.sourceid = cardinput.sourceid AND "
                  "      cardinput.cardid = capturecard.cardid "
                  "GROUP BY cardtype");
    query.bindValue(":CHANID", chanid);

    QStringList list;
    if (!query.exec())
    {
        MythDB::DBError("ChannelUtil::GetCardTypes", query);
        return list;
    }
    while (query.next())
        list.push_back(query.value(0).toString());
    return list;
}

static bool lt_pidcache(
    const pid_cache_item_t &a, const pid_cache_item_t &b)
{
    return a.GetPID() < b.GetPID();
}

/** \brief Returns cached MPEG PIDs when given a Channel ID.
 *
 *  \param chanid   Channel ID to fetch cached pids for.
 *  \param pid_cache List of PIDs with their TableID
 *                   types is returned in pid_cache.
 */
bool ChannelUtil::GetCachedPids(uint chanid,
                                pid_cache_t &pid_cache)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("SELECT pid, tableid FROM pidcache "
                               "WHERE chanid='%1'").arg(chanid);
    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetCachedPids: fetching pids", query);
        return false;
    }

    while (query.next())
    {
        int pid = query.value(0).toInt(), tid = query.value(1).toInt();
        if ((pid >= 0) && (tid >= 0))
            pid_cache.push_back(pid_cache_item_t(pid, tid));
    }
    stable_sort(pid_cache.begin(), pid_cache.end(), lt_pidcache);

    return true;
}

/** \brief Saves PIDs for PSIP tables to database.
 *
 *  \param chanid     Channel ID to fetch cached pids for.
 *  \param pid_cache  List of PIDs with their TableID types to be saved.
 *  \param delete_all If true delete both permanent and transient pids first.
 */
bool ChannelUtil::SaveCachedPids(uint chanid,
                                 const pid_cache_t &_pid_cache,
                                 bool delete_all)
{
    MSqlQuery query(MSqlQuery::InitCon());

    /// delete
    if (delete_all)
        query.prepare("DELETE FROM pidcache WHERE chanid = :CHANID");
    else
        query.prepare(
            "DELETE FROM pidcache "
            "WHERE chanid = :CHANID AND tableid < 65536");

    query.bindValue(":CHANID", chanid);

    if (!query.exec())
    {
        MythDB::DBError("GetCachedPids -- delete", query);
        return false;
    }

    pid_cache_t old_cache;
    GetCachedPids(chanid, old_cache);
    pid_cache_t pid_cache = _pid_cache;
    stable_sort(pid_cache.begin(), pid_cache.end(), lt_pidcache);

    /// insert
    query.prepare(
        "INSERT INTO pidcache "
        "SET chanid = :CHANID, pid = :PID, tableid = :TABLEID");
    query.bindValue(":CHANID", chanid);

    bool ok = true;
    pid_cache_t::const_iterator ito = old_cache.begin();
    pid_cache_t::const_iterator itn = pid_cache.begin();
    for (; itn != pid_cache.end(); ++itn)
    {
        // if old pid smaller than current new pid, skip this old pid
        for (; ito != old_cache.end() && ito->GetPID() < itn->GetPID(); ++ito);

        // if already in DB, skip DB insert
        if (ito != old_cache.end() && ito->GetPID() == itn->GetPID())
            continue;

        query.bindValue(":PID",     itn->GetPID());
        query.bindValue(":TABLEID", itn->GetComposite());

        if (!query.exec())
        {
            MythDB::DBError("GetCachedPids -- insert", query);
            ok = false;
        }
    }

    return ok;
}

QString ChannelUtil::GetChannelValueStr(const QString &channel_field,
                                        uint           cardid,
                                        const QString &input,
                                        const QString &channum)
{
    QString retval = QString::null;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        QString(
            "SELECT channel.%1 "
            "FROM channel, capturecard, cardinput "
            "WHERE channel.channum      = :CHANNUM           AND "
            "      channel.sourceid     = cardinput.sourceid AND "
            "      cardinput.inputname  = :INPUT             AND "
            "      cardinput.cardid     = capturecard.cardid AND "
            "      capturecard.cardid   = :CARDID ")
        .arg(channel_field));

    query.bindValue(":CARDID",   cardid);
    query.bindValue(":INPUT",    input);
    query.bindValue(":CHANNUM",  channum);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("getchannelvalue", query);
    else if (query.next())
        retval = query.value(0).toString();

    return retval;
}

QString ChannelUtil::GetChannelValueStr(const QString &channel_field,
                                        uint           sourceid,
                                        const QString &channum)
{
    QString retval = QString::null;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        QString(
            "SELECT channel.%1 "
            "FROM channel "
            "WHERE channum  = :CHANNUM AND "
            "      sourceid = :SOURCEID")
        .arg(channel_field));

    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CHANNUM",  channum);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("getchannelvalue", query);
    else if (query.next())
        retval = query.value(0).toString();

    return retval;
}

int ChannelUtil::GetChannelValueInt(const QString &channel_field,
                                    uint           cardid,
                                    const QString &input,
                                    const QString &channum)
{
    QString val = GetChannelValueStr(channel_field, cardid, input, channum);

    int retval = 0;
    if (!val.isEmpty())
        retval = val.toInt();

    return (retval) ? retval : -1;
}

int ChannelUtil::GetChannelValueInt(const QString &channel_field,
                                    uint           sourceid,
                                    const QString &channum)
{
    QString val = GetChannelValueStr(channel_field, sourceid, channum);

    int retval = 0;
    if (!val.isEmpty())
        retval = val.toInt();

    return (retval) ? retval : -1;
}

bool ChannelUtil::IsOnSameMultiplex(uint srcid,
                                    const QString &new_channum,
                                    const QString &old_channum)
{
    if (new_channum.isEmpty() || old_channum.isEmpty())
        return false;

    if (new_channum == old_channum)
        return true;

    uint old_mplexid = GetMplexID(srcid, old_channum);
    if (!old_mplexid)
        return false;

    uint new_mplexid = GetMplexID(srcid, new_channum);
    if (!new_mplexid)
        return false;

    LOG(VB_CHANNEL, LOG_INFO, QString("IsOnSameMultiplex? %1==%2 -> %3")
            .arg(old_mplexid).arg(new_mplexid)
            .arg(old_mplexid == new_mplexid));

    return old_mplexid == new_mplexid;
}

/** \fn get_valid_recorder_list(uint)
 *  \brief Returns list of the recorders that have chanid in their sources.
 *  \param chanid  Channel ID of channel we are querying recorders for.
 *  \return List of cardid's for recorders with channel.
 */
static QStringList get_valid_recorder_list(uint chanid)
{
    QStringList reclist;

    // Query the database to determine which source is being used currently.
    // set the EPG so that it only displays the channels of the current source
    MSqlQuery query(MSqlQuery::InitCon());
    // We want to get the current source id for this recorder
    query.prepare(
            "SELECT cardinput.cardid "
            "FROM channel "
            "LEFT JOIN cardinput ON channel.sourceid = cardinput.sourceid "
            "WHERE channel.chanid = :CHANID AND "
            "      cardinput.livetvorder > 0 "
            "ORDER BY cardinput.livetvorder, cardinput.cardinputid");
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("get_valid_recorder_list ChanID", query);
        return reclist;
    }

    while (query.next())
        reclist << query.value(0).toString();

    return reclist;
}

/** \fn get_valid_recorder_list(const QString&)
 *  \brief Returns list of the recorders that have channum in their sources.
 *  \param channum  Channel "number" we are querying recorders for.
 *  \return List of cardid's for recorders with channel.
 */
static QStringList get_valid_recorder_list(const QString &channum)
{
    QStringList reclist;

    // Query the database to determine which source is being used currently.
    // set the EPG so that it only displays the channels of the current source
    MSqlQuery query(MSqlQuery::InitCon());
    // We want to get the current source id for this recorder
    query.prepare(
        "SELECT cardinput.cardid "
        "FROM channel "
        "LEFT JOIN cardinput ON channel.sourceid = cardinput.sourceid "
        "WHERE channel.channum = :CHANNUM AND "
        "      cardinput.livetvorder > 0 "
        "ORDER BY cardinput.livetvorder, cardinput.cardinputid");
    query.bindValue(":CHANNUM", channum);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("get_valid_recorder_list ChanNum", query);
        return reclist;
    }

    while (query.next())
        reclist << query.value(0).toString();

    return reclist;
}

/** \fn TV::GetValidRecorderList(uint, const QString&)
 *  \brief Returns list of the recorders that have chanid or channum
 *         in their sources.
 *  \param chanid   Channel ID of channel we are querying recorders for.
 *  \param channum  Channel "number" we are querying recorders for.
 *  \return List of cardid's for recorders with channel.
 */
QStringList ChannelUtil::GetValidRecorderList(
    uint chanid, const QString &channum)
{
    if (chanid)
        return get_valid_recorder_list(chanid);
    else if (!channum.isEmpty())
        return get_valid_recorder_list(channum);
    return QStringList();
}


vector<uint> ChannelUtil::GetConflicting(const QString &channum, uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    vector<uint> conflicting;

    if (sourceid)
    {
        query.prepare(
            "SELECT chanid from channel "
            "WHERE sourceid = :SOURCEID AND "
            "      channum  = :CHANNUM");
        query.bindValue(":SOURCEID", sourceid);
    }
    else
    {
        query.prepare(
            "SELECT chanid from channel "
            "WHERE channum = :CHANNUM");
    }

    query.bindValue(":CHANNUM",  channum);
    if (!query.exec())
    {
        MythDB::DBError("IsConflicting", query);
        conflicting.push_back(0);
        return conflicting;
    }

    while (query.next())
        conflicting.push_back(query.value(0).toUInt());

    return conflicting;
}

bool ChannelUtil::SetChannelValue(const QString &field_name,
                                  QString        value,
                                  uint           sourceid,
                                  const QString &channum)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        QString("UPDATE channel SET channel.%1=:VALUE "
                "WHERE channel.channum  = :CHANNUM AND "
                "      channel.sourceid = :SOURCEID").arg(field_name));

    query.bindValue(":VALUE",    value);
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", sourceid);

    return query.exec();
}

bool ChannelUtil::SetChannelValue(const QString &field_name,
                                  QString        value,
                                  int            chanid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        QString("UPDATE channel SET channel.%1=:VALUE "
                "WHERE channel.chanid = :CHANID").arg(field_name));

    query.bindValue(":VALUE",  value);
    query.bindValue(":CHANID", chanid);

    return query.exec();
}

/** Returns the DVB default authority for the chanid given. */
QString ChannelUtil::GetDefaultAuthority(uint chanid)
{
    static QReadWriteLock channel_default_authority_map_lock;
    static QMap<uint,QString> channel_default_authority_map;
    static bool run_init = true;

    channel_default_authority_map_lock.lockForRead();

    if (run_init)
    {
        channel_default_authority_map_lock.unlock();
        channel_default_authority_map_lock.lockForWrite();
        if (run_init)
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(
                "SELECT chanid, m.default_authority "
                "FROM channel c "
                "LEFT JOIN dtv_multiplex m "
                "ON (c.mplexid = m.mplexid)");
            if (query.exec())
            {
                while (query.next())
                {
                    if (!query.value(1).toString().isEmpty())
                    {
                        channel_default_authority_map[query.value(0).toUInt()] =
                            query.value(1).toString();
                    }
                }
                run_init = false;
            }
            else
            {
                MythDB::DBError("GetDefaultAuthority 1", query);
            }

            query.prepare(
                "SELECT chanid, default_authority "
                "FROM channel");
            if (query.exec())
            {
                while (query.next())
                {
                    if (!query.value(1).toString().isEmpty())
                    {
                        channel_default_authority_map[query.value(0).toUInt()] =
                            query.value(1).toString();
                    }
                }
                run_init = false;
            }
            else
            {
                MythDB::DBError("GetDefaultAuthority 2", query);
            }

        }
    }

    QMap<uint,QString>::iterator it = channel_default_authority_map.find(chanid);
    QString ret = QString::null;
    if (it != channel_default_authority_map.end())
    {
        ret = *it;
        ret.detach();
    }
    channel_default_authority_map_lock.unlock();

    return ret;
}

QString ChannelUtil::GetIcon(uint chanid)
{
    static QReadWriteLock channel_icon_map_lock;
    static QHash<uint,QString> channel_icon_map;
    static bool run_init = true;

    channel_icon_map_lock.lockForRead();

    QString ret(channel_icon_map.value(chanid, "_cold_"));

    channel_icon_map_lock.unlock();

    if (ret != "_cold_")
        return ret;

    channel_icon_map_lock.lockForWrite();

    MSqlQuery query(MSqlQuery::InitCon());
    QString iconquery = "SELECT chanid, icon FROM channel";

    if (run_init)
        iconquery += " WHERE visible = 1";
    else
        iconquery += " WHERE chanid = :CHANID";

    query.prepare(iconquery);

    if (!run_init)
        query.bindValue(":CHANID", chanid);

    if (query.exec())
    {
        if (run_init)
        {
            channel_icon_map.reserve(query.size());
            while (query.next())
            {
                channel_icon_map[query.value(0).toUInt()] =
                    query.value(1).toString();
            }
            run_init = false;
        }
        else
        {
            channel_icon_map[chanid] = (query.next()) ?
                query.value(1).toString() : "";
        }
    }
    else
    {
        MythDB::DBError("GetIcon", query);
    }

    ret = channel_icon_map.value(chanid, "");

    channel_icon_map_lock.unlock();

    return ret;
}

QString ChannelUtil::GetUnknownCallsign(void)
{
    QString tmp = tr("UNKNOWN", "Synthesized callsign");
    tmp.detach();
    return tmp;
}

int ChannelUtil::GetChanID(int mplexid,       int service_transport_id,
                           int major_channel, int minor_channel,
                           int program_number)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // find source id, so we can find manually inserted ATSC channels
    query.prepare("SELECT sourceid "
                  "FROM dtv_multiplex "
                  "WHERE mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);
    if (!query.exec())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex 2", query);
        return -1;
    }
    if (!query.next())
        return -1;

    int source_id = query.value(0).toInt();

    QStringList qstr;

    // find a proper ATSC channel
    qstr.push_back(
        QString("SELECT chanid FROM channel,dtv_multiplex "
                "WHERE channel.sourceid          = %1 AND "
                "      atsc_major_chan           = %2 AND "
                "      atsc_minor_chan           = %3 AND "
                "      dtv_multiplex.transportid = %4 AND "
                "      dtv_multiplex.mplexid     = %5 AND "
                "      dtv_multiplex.sourceid    = channel.sourceid AND "
                "      dtv_multiplex.mplexid     = channel.mplexid")
        .arg(source_id).arg(major_channel).arg(minor_channel)
        .arg(service_transport_id).arg(mplexid));

    // Find manually inserted/edited channels in order of scariness.
    // find renamed channel, where atsc is valid
    qstr.push_back(
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND "
                "atsc_major_chan=%2 AND "
                "atsc_minor_chan=%3")
        .arg(source_id).arg(major_channel).arg(minor_channel));

        // find based on mpeg program number and mplexid alone
    qstr.push_back(
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND serviceID=%2 AND mplexid=%3")
        .arg(source_id).arg(program_number).arg(mplexid));

    for (int i = 0; i < qstr.size(); i++)
    {
        query.prepare(qstr[i]);
        if (!query.exec())
            MythDB::DBError("Selecting channel/dtv_multiplex 3", query);
        else if (query.next())
            return query.value(0).toInt();
    }

    return -1;
}

uint ChannelUtil::FindChannel(uint sourceid, const QString &freqid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid "
                  "FROM channel "
                  "WHERE sourceid = :SOURCEID AND "
                  "      freqid   = :FREQID");

    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":FREQID",   freqid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("FindChannel", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}


static uint get_max_chanid(uint sourceid)
{
    QString qstr = "SELECT MAX(chanid) FROM channel ";
    qstr += (sourceid) ? "WHERE sourceid = :SOURCEID" : "";

    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(qstr);

    if (sourceid)
        query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Getting chanid for new channel (2)", query);
    else if (!query.next())
        LOG(VB_GENERAL, LOG_ERR, "Error getting chanid for new channel.");
    else
        return query.value(0).toUInt();

    return 0;
}

static bool chanid_available(uint chanid)
{
    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(
        "SELECT chanid "
        "FROM channel "
        "WHERE chanid = :CHANID");
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("is_chan_id_available", query);
    else if (query.size() == 0)
        return true;

    return false;
}

/** \fn ChannelUtil::CreateChanID(uint, const QString&)
 *  \brief Creates a unique channel ID for database use.
 *  \return chanid if successful, -1 if not
 */
int ChannelUtil::CreateChanID(uint sourceid, const QString &chan_num)
{
    // first try to base it on the channel number for human readability
    uint chanid = 0;
    int chansep = chan_num.indexOf(QRegExp("\\D"));
    if (chansep > 0)
    {
        chanid =
            sourceid * 1000 +
            chan_num.left(chansep).toInt() * 10 +
            chan_num.right(chan_num.length()-chansep-1).toInt();
    }
    else
    {
        chanid = sourceid * 1000 + chan_num.toInt();
    }

    if ((chanid > sourceid * 1000) && (chanid_available(chanid)))
        return chanid;

    // try to at least base it on the sourceid for human readability
    chanid = max(get_max_chanid(sourceid) + 1, sourceid * 1000);

    if (chanid_available(chanid))
        return chanid;

    // just get a chanid we know should work
    chanid = get_max_chanid(0) + 1;

    if (chanid_available(chanid))
        return chanid;

    // failure
    return -1;
}

bool ChannelUtil::CreateChannel(uint db_mplexid,
                                uint db_sourceid,
                                uint new_channel_id,
                                const QString &callsign,
                                const QString &service_name,
                                const QString &chan_num,
                                uint service_id,
                                uint atsc_major_channel,
                                uint atsc_minor_channel,
                                bool use_on_air_guide,
                                bool hidden,
                                bool hidden_in_guide,
                                const QString &freqid,
                                QString icon,
                                QString format,
                                QString xmltvid,
                                QString default_authority)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString chanNum = (chan_num == "-1") ?
        QString::number(service_id) : chan_num;

    QString qstr =
        "INSERT INTO channel "
        "  (chanid,        channum,    sourceid,          "
        "   callsign,      name,       serviceid,         ";
    qstr += (db_mplexid > 0)    ? "mplexid, " : "";
    qstr += (!freqid.isEmpty()) ? "freqid, "  : "";
    qstr +=
        "   atsc_major_chan,           atsc_minor_chan,   "
        "   useonairguide, visible,    tvformat,          "
        "   icon,          xmltvid,    default_authority) "
        "VALUES "
        "  (:CHANID,       :CHANNUM,   :SOURCEID,         "
        "   :CALLSIGN,     :NAME,      :SERVICEID,        ";
    qstr += (db_mplexid > 0)    ? ":MPLEXID, " : "";
    qstr += (!freqid.isEmpty()) ? ":FREQID, "  : "";
    qstr +=
        "   :MAJORCHAN,                :MINORCHAN,        "
        "   :USEOAG,       :VISIBLE,   :TVFORMAT,         "
        "   :ICON,         :XMLTVID,   :AUTHORITY)        ";

    query.prepare(qstr);

    query.bindValue(":CHANID",    new_channel_id);
    query.bindValue(":CHANNUM",   chanNum);
    query.bindValue(":SOURCEID",  db_sourceid);
    query.bindValue(":CALLSIGN",  callsign);
    query.bindValue(":NAME",      service_name);

    if (db_mplexid > 0)
        query.bindValue(":MPLEXID",   db_mplexid);

    query.bindValue(":SERVICEID", service_id);
    query.bindValue(":MAJORCHAN", atsc_major_channel);
    query.bindValue(":MINORCHAN", atsc_minor_channel);
    query.bindValue(":USEOAG",    use_on_air_guide);
    query.bindValue(":VISIBLE",   !hidden);
    (void) hidden_in_guide; // MythTV can't hide the channel in just the guide.

    if (!freqid.isEmpty())
        query.bindValue(":FREQID",    freqid);

    QString tvformat = (atsc_minor_channel > 0) ? "ATSC" : format;
    tvformat = tvformat.isNull() ? "" : tvformat;
    query.bindValue(":TVFORMAT", tvformat);

    icon = (icon.isNull()) ? "" : icon;
    query.bindValue(":ICON", icon);

    xmltvid = (xmltvid.isNull()) ? "" : xmltvid;
    query.bindValue(":XMLTVID", xmltvid);

    default_authority = (default_authority.isNull()) ? "" : default_authority;
    query.bindValue(":AUTHORITY", default_authority);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Adding Service", query);
        return false;
    }
    return true;
}

bool ChannelUtil::UpdateChannel(uint db_mplexid,
                                uint source_id,
                                uint channel_id,
                                const QString &callsign,
                                const QString &service_name,
                                const QString &chan_num,
                                uint service_id,
                                uint atsc_major_channel,
                                uint atsc_minor_channel,
                                bool use_on_air_guide,
                                bool hidden,
                                bool hidden_in_guide,
                                QString freqid,
                                QString icon,
                                QString format,
                                QString xmltvid,
                                QString default_authority)
{
    if (!channel_id)
        return false;

    QString tvformat = (atsc_minor_channel > 0) ? "ATSC" : format;
    bool set_channum = !chan_num.isEmpty() && chan_num != "-1";
    QString qstr = QString(
        "UPDATE channel "
        "SET %1 %2 %3 %4 %5 %6"
        "    mplexid         = :MPLEXID,   serviceid       = :SERVICEID, "
        "    atsc_major_chan = :MAJORCHAN, atsc_minor_chan = :MINORCHAN, "
        "    callsign        = :CALLSIGN,  name            = :NAME,      "
        "    sourceid        = :SOURCEID,  useonairguide   = :USEOAG,    "
        "    visible         = :VISIBLE "
        "WHERE chanid=:CHANID")
        .arg((!set_channum)       ? "" : "channum  = :CHANNUM,  ")
        .arg((freqid.isEmpty())   ? "" : "freqid   = :FREQID,   ")
        .arg((icon.isEmpty())     ? "" : "icon     = :ICON,     ")
        .arg((tvformat.isEmpty()) ? "" : "tvformat = :TVFORMAT, ")
        .arg((xmltvid.isEmpty())  ? "" : "xmltvid  = :XMLTVID,  ")
        .arg((default_authority.isEmpty()) ?
             "" : "default_authority = :AUTHORITY,");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(qstr);

    query.bindValue(":CHANID", channel_id);

    if (set_channum)
        query.bindValue(":CHANNUM", chan_num);

    query.bindValue(":SOURCEID",  source_id);
    query.bindValue(":CALLSIGN",  callsign);
    query.bindValue(":NAME",      service_name);

    query.bindValue(":MPLEXID",   db_mplexid);

    query.bindValue(":SERVICEID", service_id);
    query.bindValue(":MAJORCHAN", atsc_major_channel);
    query.bindValue(":MINORCHAN", atsc_minor_channel);
    query.bindValue(":USEOAG",    use_on_air_guide);
    query.bindValue(":VISIBLE",   !hidden);
    (void) hidden_in_guide; // MythTV can't hide the channel in just the guide.

    if (!freqid.isEmpty())
        query.bindValue(":FREQID",    freqid);

    if (!tvformat.isEmpty())
        query.bindValue(":TVFORMAT",  tvformat);

    if (!icon.isEmpty())
        query.bindValue(":ICON",      icon);
    if (!xmltvid.isEmpty())
        query.bindValue(":XMLTVID",   xmltvid);
    if (!default_authority.isEmpty())
        query.bindValue(":AUTHORITY",   default_authority);

    if (!query.exec())
    {
        MythDB::DBError("Updating Service", query);
        return false;
    }
    return true;
}

void ChannelUtil::UpdateInsertInfoFromDB(ChannelInsertInfo &chan)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT xmltvid, useonairguide "
        "FROM channel "
        "WHERE chanid = :ID");
    query.bindValue(":ID", chan.channel_id);

    if (!query.exec())
    {
        MythDB::DBError("UpdateInsertInfoFromDB", query);
        return;
    }

    if (query.next())
    {
        QString xmltvid = query.value(0).toString();
        bool useeit     = query.value(1).toInt();

        if (!xmltvid.isEmpty())
        {
            if (useeit)
                LOG(VB_GENERAL, LOG_ERR,
                    "Using EIT and xmltv for the same channel "
                    "is a unsupported configuration.");
            chan.xmltvid = xmltvid;
            chan.use_on_air_guide = useeit;
        }
    }
}

bool ChannelUtil::UpdateIPTVTuningData(
    uint channel_id, const IPTVTuningData &tuning)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "DELETE FROM iptv_channel "
        "WHERE chanid=:CHANID");
    query.bindValue(":CHANID", channel_id);

    if (!query.exec())
    {
        MythDB::DBError("UpdateIPTVTuningData -- delete", query);
        return false;
    }

    query.prepare(
        "INSERT INTO iptv_channel (chanid, url, type, bitrate) "
        "VALUES (:CHANID, :URL, :TYPE, :BITRATE)");
    query.bindValue(":CHANID", channel_id);

    query.bindValue(":URL", tuning.GetDataURL().toString());
    query.bindValue(":TYPE", tuning.GetFECTypeString(0));
    query.bindValue(":BITRATE", tuning.GetBitrate(0));

    if (!query.exec())
    {
        MythDB::DBError("UpdateIPTVTuningData -- data", query);
        return false;
    }

    if (tuning.GetFECURL0().port() >= 0)
    {
        query.bindValue(":URL", tuning.GetFECURL0().toString());
        query.bindValue(":TYPE", tuning.GetFECTypeString(1));
        query.bindValue(":BITRATE", tuning.GetBitrate(1));
        if (!query.exec())
        {
            MythDB::DBError("UpdateIPTVTuningData -- fec 0", query);
            return false;
        }
    }

    if (tuning.GetFECURL1().port() >= 0)
    {
        query.bindValue(":URL", tuning.GetFECURL1().toString());
        query.bindValue(":TYPE", tuning.GetFECTypeString(2));
        query.bindValue(":BITRATE", tuning.GetBitrate(2));
        if (!query.exec())
        {
            MythDB::DBError("UpdateIPTVTuningData -- fec 1", query);
            return false;
        }
    }

    return true;
}

bool ChannelUtil::DeleteChannel(uint channel_id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM channel "
        "WHERE chanid = :ID");
    query.bindValue(":ID", channel_id);

    if (!query.exec())
    {
        MythDB::DBError("Delete Channel", query);
        return false;
    }

    query.prepare(
        "DELETE FROM iptv_channel "
        "WHERE chanid = :ID");
    query.bindValue(":ID", channel_id);

    if (!query.exec())
    {
        MythDB::DBError("Delete Channel 2", query);
        return false;
    }

    return true;
}

bool ChannelUtil::SetVisible(uint channel_id, bool visible)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "UPDATE channel "
        "SET   visible = :VISIBLE "
        "WHERE chanid  = :ID");
    query.bindValue(":ID", channel_id);
    query.bindValue(":VISIBLE", visible);

    if (!query.exec())
    {
        MythDB::DBError("ChannelUtil::SetVisible", query);
        return false;
    }

    return true;
}

bool ChannelUtil::SetServiceVersion(int mplexid, int version)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        QString("UPDATE dtv_multiplex "
                "SET serviceversion = %1 "
                "WHERE mplexid = %2").arg(version).arg(mplexid));

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex", query);
        return false;
    }
    return true;
}

int ChannelUtil::GetServiceVersion(int mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("SELECT serviceversion "
                          "FROM dtv_multiplex "
                          "WHERE mplexid = %1").arg(mplexid));

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex", query);
        return false;
    }

    if (query.next())
        return query.value(0).toInt();

    return -1;
}

bool ChannelUtil::GetATSCChannel(uint sourceid, const QString &channum,
                                 uint &major,   uint          &minor)
{
    major = minor = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT atsc_major_chan, atsc_minor_chan "
        "FROM channel "
        "WHERE channum  = :CHANNUM AND "
        "      sourceid = :SOURCEID");

    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CHANNUM",  channum);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("getatscchannel", query);
    else if (query.next())
    {
        major = query.value(0).toUInt();
        minor = query.value(1).toUInt();
        return true;
    }

    return false;
}

bool ChannelUtil::GetChannelData(
    uint    sourceid,
    uint    &chanid,          const QString &channum,
    QString &tvformat,        QString       &modulation,
    QString &freqtable,       QString       &freqid,
    int     &finetune,        uint64_t      &frequency,
    QString &dtv_si_std,      int           &mpeg_prog_num,
    uint    &atsc_major,      uint          &atsc_minor,
    uint    &dvb_transportid, uint          &dvb_networkid,
    uint    &mplexid,
    bool    &commfree)
{
    tvformat      = modulation = freqtable = QString::null;
    freqid        = dtv_si_std = QString::null;
    finetune      = 0;
    frequency     = 0;
    mpeg_prog_num = -1;
    atsc_major    = atsc_minor = mplexid = 0;
    dvb_networkid = dvb_transportid = 0;
    commfree      = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT finetune, freqid, tvformat, freqtable, "
        "       commmethod, mplexid, "
        "       atsc_major_chan, atsc_minor_chan, serviceid, chanid "
        "FROM channel, videosource "
        "WHERE videosource.sourceid = channel.sourceid AND "
        "      channum              = :CHANNUM         AND "
        "      channel.sourceid     = :SOURCEID");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetChannelData", query);
        return false;
    }
    else if (!query.next())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetChannelData() failed because it could not\n"
                    "\t\t\tfind channel number '%1' in DB for source '%2'.")
                .arg(channum).arg(sourceid));
        return false;
    }

    finetune      = query.value(0).toInt();
    freqid        = query.value(1).toString();
    tvformat      = query.value(2).toString();
    freqtable     = query.value(3).toString();
    commfree      = (query.value(4).toInt() == -2);
    mplexid       = query.value(5).toUInt();
    atsc_major    = query.value(6).toUInt();
    atsc_minor    = query.value(7).toUInt();
    mpeg_prog_num = (query.value(8).isNull()) ? -1 : query.value(8).toInt();
    chanid        = query.value(9).toUInt();

    if (!mplexid || (mplexid == 32767)) /* 32767 deals with old lineups */
        return true;

    return GetTuningParams(mplexid, modulation, frequency,
                           dvb_transportid, dvb_networkid, dtv_si_std);
}

IPTVTuningData ChannelUtil::GetIPTVTuningData(uint chanid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT type+0, url, bitrate "
        "FROM iptv_channel "
        "WHERE chanid = :CHANID "
        "ORDER BY type+0");
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
    {
        MythDB::DBError("GetChannelData -- iptv", query);
        return IPTVTuningData();
    }

    QString data_url, fec_url0, fec_url1;
    IPTVTuningData::FECType fec_type = IPTVTuningData::kNone;
    uint bitrate[3] = { 0, 0, 0, };
    while (query.next())
    {
        IPTVTuningData::IPTVType type = (IPTVTuningData::IPTVType)
            query.value(0).toUInt();
        switch (type)
        {
            case IPTVTuningData::kData:
                data_url = query.value(1).toString();
                bitrate[0] = query.value(2).toUInt();
                break;
            case IPTVTuningData::kRFC2733_1:
            case IPTVTuningData::kRFC5109_1:
            case IPTVTuningData::kSMPTE2022_1:
                fec_url0 = query.value(1).toString();
                bitrate[1] = query.value(2).toUInt();
                break;
            case IPTVTuningData::kRFC2733_2:
            case IPTVTuningData::kRFC5109_2:
            case IPTVTuningData::kSMPTE2022_2:
                fec_url1 = query.value(1).toString();
                bitrate[2] = query.value(2).toUInt();
                break;
        }
        switch (type)
        {
            case IPTVTuningData::kData:
                break;
            case IPTVTuningData::kRFC2733_1:
                fec_type = IPTVTuningData::kRFC2733;
                break;
            case IPTVTuningData::kRFC5109_1:
                fec_type = IPTVTuningData::kRFC5109;
                break;
            case IPTVTuningData::kSMPTE2022_1:
                fec_type = IPTVTuningData::kSMPTE2022;
                break;
            case IPTVTuningData::kRFC2733_2:
            case IPTVTuningData::kRFC5109_2:
            case IPTVTuningData::kSMPTE2022_2:
                break; // will be handled by type of first FEC stream
        }
    }

    IPTVTuningData tuning(data_url, bitrate[0], fec_type,
                          fec_url0, bitrate[1], fec_url1, bitrate[2]);
    LOG(VB_GENERAL, LOG_INFO, QString("Loaded %1 for %2")
        .arg(tuning.GetDeviceName()).arg(chanid));
    return tuning;
}

// TODO This should be modified to load a complete channelinfo object including
//      all fields from the database
/**
 * \deprecated Use ChannelInfo::LoadChannels() instead
 */
ChannelInfoList ChannelUtil::GetChannelsInternal(
    uint sourceid, bool vis_only, bool include_disconnected,
    const QString &grp, uint changrpid)
{
    ChannelInfoList list;

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr = QString(
        "SELECT channum, callsign, channel.chanid, "
        "       atsc_major_chan, atsc_minor_chan, "
        "       name, icon, mplexid, visible, "
        "       channel.sourceid, GROUP_CONCAT(DISTINCT cardinput.cardid),"
        "       GROUP_CONCAT(DISTINCT channelgroup.grpid), "
        "       xmltvid "
        "FROM channel "
        "LEFT JOIN channelgroup ON channel.chanid     = channelgroup.chanid "
        " %1  JOIN cardinput    ON cardinput.sourceid = channel.sourceid "
        " %2  JOIN capturecard  ON cardinput.cardid   = capturecard.cardid ")
        .arg((include_disconnected) ? "LEFT" : "")
        .arg((include_disconnected) ? "LEFT" : "");

    QString cond = " WHERE ";

    if (sourceid)
    {
        qstr += QString("WHERE channel.sourceid='%1' ").arg(sourceid);
        cond = " AND ";
    }

    // Select only channels from the specified channel group
    if (changrpid > 0)
    {
        qstr += QString("%1 channelgroup.grpid = '%2' ")
            .arg(cond).arg(changrpid);
        cond = " AND ";
    }

    if (vis_only)
    {
        qstr += QString("%1 visible=1 ").arg(cond);
        cond = " AND ";
    }

    qstr += " GROUP BY chanid";

    if (!grp.isEmpty())
        qstr += QString(", %1").arg(grp);

    query.prepare(qstr);
    if (!query.exec())
    {
        MythDB::DBError("ChannelUtil::GetChannels()", query);
        return list;
    }

    while (query.next())
    {
        if (query.value(0).toString().isEmpty() || !query.value(2).toUInt())
            continue; // skip if channum blank, or chanid empty

        ChannelInfo chan(
            query.value(0).toString(),                    /* channum    */
            query.value(1).toString(),                    /* callsign   */
            query.value(2).toUInt(),                      /* chanid     */
            query.value(3).toUInt(),                      /* ATSC major */
            query.value(4).toUInt(),                      /* ATSC minor */
            query.value(7).toUInt(),                      /* mplexid    */
            query.value(8).toBool(),                      /* visible    */
            query.value(5).toString(),                    /* name       */
            query.value(6).toString(),                    /* icon       */
            query.value(9).toUInt());                     /* sourceid   */

        chan.xmltvid = query.value(12).toString();        /* xmltvid    */

        QStringList cardIDs = query.value(11).toString().split(",");
        QString cardid;
        while (!cardIDs.isEmpty())
                chan.AddCardId(cardIDs.takeFirst().toUInt());

        QStringList groupIDs = query.value(10).toString().split(",");
        QString groupid;
        while (!groupIDs.isEmpty())
                chan.AddGroupId(groupIDs.takeFirst().toUInt());
        
        list.push_back(chan);

    }

    return list;
}

vector<uint> ChannelUtil::GetChanIDs(int sourceid, bool onlyVisible)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString select = "SELECT chanid FROM channel ";
    // Yes, this a little ugly
    if (onlyVisible || sourceid > 0)
    {
        select += "WHERE ";
        if (onlyVisible)
            select += "visible = 1 ";
        if (sourceid > 0)
        {
            if (onlyVisible)
                select += "AND ";
            select += "sourceid=" + QString::number(sourceid);
        }
    }

    vector<uint> list;
    if (!query.exec(select))
    {
        MythDB::DBError("SourceUtil::GetChanIDs()", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

inline bool lt_callsign(const ChannelInfo &a, const ChannelInfo &b)
{
    return naturalCompare(a.callsign, b.callsign) < 0;
}

inline bool lt_smart(const ChannelInfo &a, const ChannelInfo &b)
{
    static QMutex sepExprLock;
    static const QRegExp sepExpr(ChannelUtil::kATSCSeparators);

    int cmp = 0;

    bool isIntA, isIntB;
    int a_int = a.channum.toUInt(&isIntA);
    int b_int = b.channum.toUInt(&isIntB);
    int a_major = a.atsc_major_chan;
    int b_major = b.atsc_major_chan;
    int a_minor = a.atsc_minor_chan;
    int b_minor = b.atsc_minor_chan;

    // Extract minor and major numbers from channum..
    bool tmp1, tmp2;
    int idxA, idxB;
    {
        QMutexLocker locker(&sepExprLock);
        idxA = a.channum.indexOf(sepExpr);
        idxB = b.channum.indexOf(sepExpr);
    }
    if (idxA >= 0)
    {
        int major = a.channum.left(idxA).toUInt(&tmp1);
        int minor = a.channum.mid(idxA+1).toUInt(&tmp2);
        if (tmp1 && tmp2)
            (a_major = major), (a_minor = minor), (isIntA = false);
    }

    if (idxB >= 0)
    {
        int major = b.channum.left(idxB).toUInt(&tmp1);
        int minor = b.channum.mid(idxB+1).toUInt(&tmp2);
        if (tmp1 && tmp2)
            (b_major = major), (b_minor = minor), (isIntB = false);
    }

    // If ATSC channel has been renumbered, sort by new channel number
    if ((a_minor > 0) && isIntA)
    {
        int atsc_int = (QString("%1%2").arg(a_major).arg(a_minor)).toInt();
        a_minor = (atsc_int == a_int) ? a_minor : 0;
    }

    if ((b_minor > 0) && isIntB)
    {
        int atsc_int = (QString("%1%2").arg(b_major).arg(b_minor)).toInt();
        b_minor = (atsc_int == b_int) ? b_minor : 0;
    }

    // one of the channels is an ATSC channel, and the other
    // is either ATSC or is numeric.
    if ((a_minor || b_minor) &&
        (a_minor || isIntA) && (b_minor || isIntB))
    {
        int a_maj = (!a_minor && isIntA) ? a_int : a_major;
        int b_maj = (!b_minor && isIntB) ? b_int : b_major;
        if ((cmp = a_maj - b_maj))
            return cmp < 0;

        if ((cmp = a_minor - b_minor))
            return cmp < 0;
    }

    if (isIntA && isIntB)
    {
        // both channels have a numeric channum
        cmp = a_int - b_int;
        if (cmp)
            return cmp < 0;
    }
    else if (isIntA ^ isIntB)
    {
        // if only one is channel numeric always consider it less than
        return isIntA;
    }
    else
    {
        // neither of channels have a numeric channum
        cmp = naturalCompare(a.channum, b.channum);
        if (cmp)
            return cmp < 0;
    }

    return lt_callsign(a,b);
}

uint ChannelUtil::GetChannelCount(int sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString   select;


    select = "SELECT chanid FROM channel";
    if (sourceid >= 0)
        select += " WHERE sourceid=" + QString::number(sourceid);
    select += ';';

    query.prepare(select);

    if (!query.exec() || !query.isActive())
        return 0;

    return query.size();
}

void ChannelUtil::SortChannels(ChannelInfoList &list, const QString &order,
                               bool eliminate_duplicates)
{
    bool cs = order.toLower() == "callsign";
    if (cs)
        stable_sort(list.begin(), list.end(), lt_callsign);
    else /* if (sortorder == "channum") */
        stable_sort(list.begin(), list.end(), lt_smart);

    if (eliminate_duplicates && !list.empty())
    {
        ChannelInfoList tmp;
        tmp.push_back(list[0]);
        for (uint i = 1; i < list.size(); i++)
        {
            if ((cs && lt_callsign(tmp.back(), list[i])) ||
                (!cs && lt_smart(tmp.back(), list[i])))
            {
                tmp.push_back(list[i]);
            }
        }

        list = tmp;
    }
}

// Return the array index of the best matching channel.  An exact
// match is the best match.  Otherwise, find the closest numerical
// value greater than channum.  E.g., if the channel list is {2_1,
// 2_2, 4_1, 4_2, 300} then input 3 returns 2_2, input 4 returns 2_2,
// and input 5 returns 4_2.
//
// The list does not need to be sorted.
int ChannelUtil::GetNearestChannel(const ChannelInfoList &list,
                                   const QString &channum)
{
    ChannelInfo target;
    target.channum = channum;
    int b = -1; // index of best seen so far
    for (int i = 0; i < (int)list.size(); ++i)
    {
        // Index i is a better result if any of the following hold:
        //   i is the first element seen
        //   i < target < best (i.e., i is the first one less than the target)
        //   best < i < target
        //   target < i < best
        if ((b < 0) ||
            (lt_smart(list[i], target) && lt_smart(target,  list[b])) ||
            (lt_smart(list[b], list[i]) && lt_smart(list[i], target)) ||
            (lt_smart(target,  list[i]) && lt_smart(list[i], list[b])))
        {
            b = i;
        }
    }
    return b;
}

uint ChannelUtil::GetNextChannel(
    const ChannelInfoList &sorted,
    uint              old_chanid,
    uint              mplexid_restriction,
    uint              chanid_restriction,
    ChannelChangeDirection direction,
    bool              skip_non_visible,
    bool              skip_same_channum_and_callsign)
{
    ChannelInfoList::const_iterator it =
        find(sorted.begin(), sorted.end(), old_chanid);

    if (it == sorted.end())
        it = sorted.begin(); // not in list, pretend we are on first channel

    if (it == sorted.end())
        return 0; // no channels..

    ChannelInfoList::const_iterator start = it;

    if (CHANNEL_DIRECTION_DOWN == direction)
    {
        do
        {
            if (it == sorted.begin())
            {
                it = find(sorted.begin(), sorted.end(),
                          sorted.rbegin()->chanid);
                if (it == sorted.end())
                {
                    --it;
                }
            }
            else
                --it;
        }
        while ((it != start) &&
               ((skip_non_visible && !it->visible) ||
                (skip_same_channum_and_callsign &&
                 it->channum  == start->channum &&
                 it->callsign == start->callsign) ||
                (mplexid_restriction &&
                 (mplexid_restriction != it->mplexid)) ||
                (chanid_restriction &&
                 (chanid_restriction != it->chanid))));
    }
    else if ((CHANNEL_DIRECTION_UP == direction) ||
             (CHANNEL_DIRECTION_FAVORITE == direction))
    {
        do
        {
            ++it;
            if (it == sorted.end())
                it = sorted.begin();
        }
        while ((it != start) &&
               ((skip_non_visible && !it->visible) ||
                (skip_same_channum_and_callsign &&
                 it->channum  == start->channum &&
                 it->callsign == start->callsign) ||
                (mplexid_restriction &&
                 (mplexid_restriction != it->mplexid)) ||
                (chanid_restriction &&
                 (chanid_restriction != it->chanid))));
    }

    return it->chanid;
}

/**
 * \brief Load channels from database into a list of ChannelInfo objects
 *
 * \note This replaces all previous methods e.g. GetChannels() and
 *       GetAllChannels() in channelutil.h
 */
ChannelInfoList ChannelUtil::LoadChannels(uint startIndex, uint count,
                                          ChannelUtil::OrderBy orderBy,
                                          bool ignoreHidden, uint sourceID,
                                          uint channelGroupID)
{
    ChannelInfoList channelList;

    MSqlQuery query(MSqlQuery::InitCon());
    QString sql = "SELECT channum, freqid, channel.sourceid, "
                  "callsign, name, icon, finetune, videofilters, xmltvid, "
                  "channel.recpriority, channel.contrast, channel.brightness, "
                  "channel.colour, channel.hue, tvformat, "
                  "visible, outputfilters, useonairguide, mplexid, "
                  "serviceid, atsc_major_chan, atsc_minor_chan, last_record, "
                  "default_authority, commmethod, tmoffset, iptvid, "
                  "channel.chanid, "
                  "GROUP_CONCAT(DISTINCT channelgroup.grpid), " // Creates a CSV list of channel groupids for this channel
                  "GROUP_CONCAT(DISTINCT cardinput.cardid) " // Creates a CSV list of cardids for this channel
                  "FROM channel "
                  "LEFT JOIN channelgroup ON channel.chanid = channelgroup.chanid "
                  "LEFT JOIN cardinput    ON cardinput.sourceid = channel.sourceid "
                  "LEFT JOIN capturecard  ON cardinput.cardid   = capturecard.cardid ";

    QStringList cond;
    if (ignoreHidden)
        cond << "channel.visible = :VISIBLE ";

    if (channelGroupID > 0)
        cond << "channelgroup.grpid = :CHANGROUPID ";

    if (sourceID > 0)
        cond << "channel.sourceid = :SOURCEID ";

    if (!cond.isEmpty())
        sql += QString("WHERE %1").arg(cond.join("AND "));

    sql += "GROUP BY channel.chanid ";

    sql += "ORDER BY :ORDERBY ";

    if (count > 0)
        sql += "LIMIT :LIMIT ";

    if (startIndex > 0)
        sql += "OFFSET :STARTINDEX ";

    query.prepare(sql);

    if (ignoreHidden)
        query.bindValue(":VISIBLE", 1);

    if (channelGroupID > 0)
        query.bindValue(":CHANGROUPID", channelGroupID);

    if (sourceID > 0)
        query.bindValue(":SOURCEID", sourceID);

    if (orderBy & kChanOrderByName)
        query.bindValue(":ORDERBY", "channel.name");
    else // kChanOrderByChanNum
        query.bindValue(":ORDERBY", "channel.channum");

    if (count > 0)
        query.bindValue(":LIMIT", count);

    if (startIndex > 0)
        query.bindValue(":LIMIT", startIndex);

    if (!query.exec())
    {
        MythDB::DBError("ChannelInfo::Load()", query);
        return channelList;
    }

    while (query.next())
    {
        ChannelInfo channelInfo;
        channelInfo.channum       = query.value(0).toString();
        channelInfo.freqid        = query.value(1).toString();
        channelInfo.sourceid      = query.value(2).toUInt();
        channelInfo.callsign      = query.value(3).toString();
        channelInfo.name          = query.value(4).toString();
        channelInfo.icon          = query.value(5).toString();
        channelInfo.finetune      = query.value(6).toInt();
        channelInfo.videofilters  = query.value(7).toString();
        channelInfo.xmltvid       = query.value(8).toString();
        channelInfo.recpriority   = query.value(9).toInt();
        channelInfo.contrast      = query.value(10).toUInt();
        channelInfo.brightness    = query.value(11).toUInt();
        channelInfo.colour        = query.value(12).toUInt();
        channelInfo.hue           = query.value(13).toUInt();
        channelInfo.tvformat      = query.value(14).toString();
        channelInfo.visible       = query.value(15).toBool();
        channelInfo.outputfilters = query.value(16).toString();
        channelInfo.useonairguide = query.value(17).toBool();
        channelInfo.mplexid       = query.value(18).toUInt();
        channelInfo.serviceid     = query.value(19).toUInt();
        channelInfo.atsc_major_chan = query.value(20).toUInt();
        channelInfo.atsc_minor_chan = query.value(21).toUInt();
        channelInfo.last_record   = query.value(22).toDateTime();
        channelInfo.default_authority = query.value(23).toString();
        channelInfo.commmethod    = query.value(24).toUInt();
        channelInfo.tmoffset      = query.value(25).toUInt();
        channelInfo.iptvid        = query.value(26).toUInt();
        channelInfo.chanid        = query.value(27).toUInt();

        QStringList groupIDs = query.value(28).toString().split(",");
        QString groupid;
        while (!groupIDs.isEmpty())
                channelInfo.AddGroupId(groupIDs.takeFirst().toUInt());

        QStringList cardIDs = query.value(29).toString().split(",");
        QString cardid;
        while (!cardIDs.isEmpty())
                channelInfo.AddCardId(cardIDs.takeFirst().toUInt());

        channelList.push_back(channelInfo);
    }

    return channelList;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
