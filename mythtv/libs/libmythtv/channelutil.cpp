// -*- Mode: c++ -*-

#include <algorithm>
#include <cstdint>
#include <set>
#include <utility>

#include <QFile>
#include <QHash>
#include <QImage>
#include <QReadWriteLock>
#include <QRegularExpression>

#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/stringutil.h"

#include "channelutil.h"
#include "mpeg/dvbtables.h"
#include "recorders/HLS/HLSReader.h"
#include "sourceutil.h"

#define LOC QString("ChanUtil: ")

const QString ChannelUtil::kATSCSeparators = "(_|-|#|\\.)";

static uint get_dtv_multiplex(uint     db_source_id,  const QString& sistandard,
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
        query.bindValue(":POLARITY",    QChar(static_cast<uint>(polarity)));
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
    int         db_source_id,  const QString& sistandard,
    uint64_t    frequency,     const QString& modulation,
    // DVB specific
    int         transport_id,  int         network_id,
    int         symbol_rate,   signed char bandwidth,
    signed char polarity,      signed char inversion,
    signed char trans_mode,
    const QString& inner_FEC,   const QString& constellation,
    signed char    hierarchy,   const QString& hp_code_rate,
    const QString& lp_code_rate, const QString& guard_interval,
    const QString& mod_sys,     const QString& rolloff)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // If transport is already present, skip insert
    uint mplex = get_dtv_multiplex(
        db_source_id,  sistandard,    frequency,
        // DVB specific
        transport_id,  network_id, polarity);

    LOG(VB_CHANSCAN, LOG_INFO, "insert_dtv_multiplex(" +
        QString("dbid:%1 std:'%2' ").arg(db_source_id).arg(sistandard) +
        QString("freq:%1 mod:%2 ").arg(frequency).arg(modulation) +
        QString("tid:%1 nid:%2 ").arg(transport_id).arg(network_id) +
        QString("pol:%1 msys:%2 ...)").arg(QChar(static_cast<uint>(polarity))).arg(mod_sys) +
        QString("mplexid:%1").arg(mplex));

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
            query.bindValue(":NETWORKID",     network_id);
            query.bindValue(":WHEREPOLARITY", QChar(static_cast<uint>(polarity)));
        }
        else
        {
            query.bindValue(":FREQUENCY2",    QString::number(frequency));
            if (transport_id)
                query.bindValue(":TRANSPORTID",   transport_id);
        }
    }
    else
    {
        if (transport_id || isDVB)
            query.bindValue(":TRANSPORTID",   transport_id);
        if (isDVB)
            query.bindValue(":NETWORKID",     network_id);
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
            QString("inserted mplexid %1").arg(mplex));

    return mplex;
}

static void handle_transport_desc(std::vector<uint> &muxes,
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
            uint dummy_tsid = 0;
            uint dummy_netid = 0;
            ChannelUtil::GetTuningParams(mux, dummy_mod, freq,
                                         dummy_tsid, dummy_netid, dummy_sistd);
        }

        mux = ChannelUtil::CreateMultiplex(
            (int)sourceid,        "dvb",
            freq,                  QString(),
            // DVB specific
            (int)tsid,            (int)netid,
            -1,                   cd.BandwidthString().at(0).toLatin1(),
            -1,                   'a',
            cd.TransmissionModeString().at(0).toLatin1(),
            QString(),                         cd.ConstellationString(),
            cd.HierarchyString().at(0).toLatin1(), cd.CodeRateHPString(),
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
            cd.FrequencykHz(),    cd.ModulationString(),
            // DVB specific
            tsid,                 netid,
            cd.SymbolRateHz(),    -1,
            cd.PolarizationString().at(0).toLatin1(), 'a',
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
            cd.FECInnerString(),  QString(),
            -1,                   QString(),
            QString(),            QString(),
            QString(),            QString());

        if (mux)
            muxes.push_back(mux);
    }
}

uint ChannelUtil::CreateMultiplex(int      sourceid,     const QString& sistandard,
                                  uint64_t frequency,    const QString& modulation,
                                  int      transport_id, int            network_id)
{
    return CreateMultiplex(
        sourceid,           sistandard,
        frequency,          modulation,
        transport_id,       network_id,
        -1,                 -1,
        -1,                 -1,
        -1,
        QString(),          QString(),
        -1,                 QString(),
        QString(),          QString(),
        QString(),          QString());
}

uint ChannelUtil::CreateMultiplex(
    int            sourceid,     const QString& sistandard,
    uint64_t       freq,         const QString& modulation,
    // DVB specific
    int            transport_id, int            network_id,
    int            symbol_rate,  signed char    bandwidth,
    signed char    polarity,     signed char    inversion,
    signed char    trans_mode,
    const QString& inner_FEC,    const QString& constellation,
    signed char    hierarchy,    const QString& hp_code_rate,
    const QString& lp_code_rate, const QString& guard_interval,
    const QString& mod_sys,      const QString& rolloff)
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
        sourceid,                            mux.m_sistandard,
        mux.m_frequency,                     mux.m_modulation.toString(),
        // DVB specific
        transport_id,                        network_id,
        mux.m_symbolRate,                    mux.m_bandwidth.toChar().toLatin1(),
        mux.m_polarity.toChar().toLatin1(),  mux.m_inversion.toChar().toLatin1(),
        mux.m_transMode.toChar().toLatin1(),
        mux.m_fec.toString(),                mux.m_modulation.toString(),
        mux.m_hierarchy.toChar().toLatin1(), mux.m_hpCodeRate.toString(),
        mux.m_lpCodeRate.toString(),         mux.m_guardInterval.toString(),
        mux.m_modSys.toString(),             mux.m_rolloff.toString());
}


/** \fn ChannelUtil::CreateMultiplexes(int, const NetworkInformationTable*)
 *
 */
std::vector<uint> ChannelUtil::CreateMultiplexes(
    int sourceid, const NetworkInformationTable *nit)
{
    std::vector<uint> muxes;

    if (sourceid <= 0)
        return muxes;

    for (uint i = 0; i < nit->TransportStreamCount(); ++i)
    {
        const desc_list_t& list =
            MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                  nit->TransportDescriptorsLength(i));

        uint tsid  = nit->TSID(i);
        uint netid = nit->OriginalNetworkID(i);
        for (const auto *j : list)
        {
            const MPEGDescriptor desc(j);
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
        "WHERE deleted   IS NULL AND "
        "      sourceid  = :SOURCEID  AND "
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

// current_mplexid always exists in scanner, see ScanTransport()
//
int ChannelUtil::GetBetterMplexID(int current_mplexid,
                                  int transport_id,
                                  int network_id)
{
    LOG(VB_CHANSCAN, LOG_INFO,
        QString("GetBetterMplexID(mplexId %1, tId %2, netId %3)")
            .arg(current_mplexid).arg(transport_id).arg(network_id));

    int q_networkid = 0;
    int q_transportid = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT networkid, transportid "
                  "FROM dtv_multiplex "
                  "WHERE mplexid = :MPLEX_ID");

    query.bindValue(":MPLEX_ID", current_mplexid);

    if (!query.exec())
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
        query.prepare("UPDATE dtv_multiplex "
                      "SET networkid = :NETWORK_ID, "
                      "    transportid = :TRANSPORT_ID "
                      "WHERE mplexid = :MPLEX_ID");

        query.bindValue(":NETWORK_ID", network_id);
        query.bindValue(":TRANSPORT_ID", transport_id);
        query.bindValue(":MPLEX_ID", current_mplexid);

        if (!query.exec())
            MythDB::DBError("Getting mplexid global search", query);

        LOG(VB_CHANSCAN, LOG_INFO,
            QString("GetBetterMplexID(): net id and transport id "
                    "are null, qsize(%1), Returning %2")
                .arg(qsize).arg(current_mplexid));
        return current_mplexid;
    }

    // We have a partial match, so we try to do better...
    std::array<QString,2> theQueries
    {
        QString("SELECT a.mplexid "
                "FROM dtv_multiplex a, dtv_multiplex b "
                "WHERE a.networkid   = :NETWORK_ID AND "
                "      a.transportid = :TRANSPORT_ID AND "
                "      a.sourceid    = b.sourceid AND "
                "      b.mplexid     = :MPLEX_ID"),

        QString("SELECT mplexid "
                "FROM dtv_multiplex "
                "WHERE networkid = :NETWORK_ID AND "
                "      transportid = :TRANSPORT_ID"),
    };

    for (uint i=0; i<2; i++)
    {
        query.prepare(theQueries[i]);

        query.bindValue(":NETWORK_ID", network_id);
        query.bindValue(":TRANSPORT_ID", transport_id);
        if (i == 0)
            query.bindValue(":MPLEX_ID", current_mplexid);

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
        return {};

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT %1 FROM channel "
                          "WHERE chanid = :CHANID").arg(field));
    query.bindValue(":CHANID", chan_id);
    if (!query.exec())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex 1", query);
        return {};
    }

    if (!query.next())
        return {};

    return query.value(0).toString();
}

QString ChannelUtil::GetChanNum(int chan_id)
{
    return GetChannelStringField(chan_id, QString("channum"));
}

std::chrono::minutes ChannelUtil::GetTimeOffset(int chan_id)
{
    return std::chrono::minutes(GetChannelStringField(chan_id, QString("tmoffset")).toInt());
}

int ChannelUtil::GetSourceID(int db_mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT sourceid "
                  "FROM dtv_multiplex "
                  "WHERE mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", db_mplexid);
    if (!query.exec())
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

QStringList ChannelUtil::GetInputTypes(uint chanid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype "
                  "FROM capturecard, channel "
                  "WHERE channel.chanid   = :CHANID              AND "
                  "      channel.sourceid = capturecard.sourceid "
                  "GROUP BY cardtype");
    query.bindValue(":CHANID", chanid);

    QStringList list;
    if (!query.exec())
    {
        MythDB::DBError("ChannelUtil::GetInputTypes", query);
        return list;
    }
    while (query.next())
        list.push_back(query.value(0).toString());
    return list;
}

static bool lt_pidcache(
    const pid_cache_item_t a, const pid_cache_item_t b)
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
    query.prepare("SELECT pid, tableid FROM pidcache "
                  "WHERE chanid = :CHANID");

    query.bindValue(":CHANID", chanid);

    if (!query.exec())
    {
        MythDB::DBError("GetCachedPids: fetching pids", query);
        return false;
    }

    while (query.next())
    {
        int pid = query.value(0).toInt();
        int tid = query.value(1).toInt();
        if ((pid >= 0) && (tid >= 0))
            pid_cache.emplace_back(pid, tid);
    }
    stable_sort(pid_cache.begin(), pid_cache.end(), lt_pidcache);

    return true;
}

/** \brief Saves PIDs for PSIP tables to database.
 *
 *  \param chanid      Channel ID to fetch cached pids for.
 *  \param _pid_cache  List of PIDs with their TableID types to be saved.
 *  \param delete_all  If true delete both permanent and transient pids first.
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
    {
        query.prepare(
            "DELETE FROM pidcache "
            "WHERE chanid = :CHANID AND tableid < 65536");
    }

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
    auto ito = old_cache.begin();
    for (const auto& itn : pid_cache)
    {
        // if old pid smaller than current new pid, skip this old pid
        for (; ito != old_cache.end() && ito->GetPID() < itn.GetPID(); ++ito);

        // if already in DB, skip DB insert
        if (ito != old_cache.end() && ito->GetPID() == itn.GetPID())
            continue;

        query.bindValue(":PID",     itn.GetPID());
        query.bindValue(":TABLEID", itn.GetComposite());

        if (!query.exec())
        {
            MythDB::DBError("GetCachedPids -- insert", query);
            ok = false;
        }
    }

    return ok;
}

QString ChannelUtil::GetChannelValueStr(const QString &channel_field,
                                        uint           sourceid,
                                        const QString &channum)
{
    QString retval;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        QString(
            "SELECT channel.%1 "
            "FROM channel "
            "WHERE deleted  IS NULL AND "
            "      channum  = :CHANNUM AND "
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
                                    uint           sourceid,
                                    const QString &channum)
{
    QString val = GetChannelValueStr(channel_field, sourceid, channum);

    int retval = 0;
    if (!val.isEmpty())
        retval = val.toInt();

    return (retval) ? retval : -1;
}

QString ChannelUtil::GetChannelNumber(uint sourceid, const QString &channel_name)
{
    if (channel_name.isEmpty())
        return {};

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT channum FROM channel WHERE sourceid = :SOURCEID "
                  "AND name = :NAME "
                  "AND deleted IS NULL;" );
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":NAME", channel_name.left(64));    // Field channel.name is 64 characters
    if (!query.exec())
    {
        MythDB::DBError("GetChannelNumber", query);
        return {};
    }

    if (!query.next())
        return {};

    return query.value(0).toString();
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
 *  \return List of inputid's for recorders with channel.
 */
static QStringList get_valid_recorder_list(uint chanid)
{
    QStringList reclist;

    // Query the database to determine which source is being used currently.
    // set the EPG so that it only displays the channels of the current source
    MSqlQuery query(MSqlQuery::InitCon());
    // We want to get the current source id for this recorder
    query.prepare(
            "SELECT capturecard.cardid "
            "FROM channel "
            "LEFT JOIN capturecard ON channel.sourceid = capturecard.sourceid "
            "WHERE channel.chanid = :CHANID AND "
            "      capturecard.livetvorder > 0 "
            "ORDER BY capturecard.livetvorder, capturecard.cardid");
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
 *  \return List of inputid's for recorders with channel.
 */
static QStringList get_valid_recorder_list(const QString &channum)
{
    QStringList reclist;

    // Query the database to determine which source is being used currently.
    // set the EPG so that it only displays the channels of the current source
    MSqlQuery query(MSqlQuery::InitCon());
    // We want to get the current source id for this recorder
    query.prepare(
        "SELECT capturecard.cardid "
        "FROM channel "
        "LEFT JOIN capturecard ON channel.sourceid = capturecard.sourceid "
        "WHERE channel.deleted IS NULL AND "
        "      channel.channum = :CHANNUM AND "
        "      capturecard.livetvorder > 0 "
        "ORDER BY capturecard.livetvorder, capturecard.cardid");
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

/**
 *  \brief Returns list of the recorders that have chanid or channum
 *         in their sources.
 *  \param chanid   Channel ID of channel we are querying recorders for.
 *  \param channum  Channel "number" we are querying recorders for.
 *  \return List of inputid's for recorders with channel.
 */
QStringList ChannelUtil::GetValidRecorderList(
    uint chanid, const QString &channum)
{
    if (chanid)
        return get_valid_recorder_list(chanid);
    if (!channum.isEmpty())
        return get_valid_recorder_list(channum);
    return {};
}


std::vector<uint> ChannelUtil::GetConflicting(const QString &channum, uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    std::vector<uint> conflicting;

    if (sourceid)
    {
        query.prepare(
            "SELECT chanid from channel "
            "WHERE deleted  IS NULL AND "
            "      sourceid = :SOURCEID AND "
            "      channum  = :CHANNUM");
        query.bindValue(":SOURCEID", sourceid);
    }
    else
    {
        query.prepare(
            "SELECT chanid from channel "
            "WHERE deleted IS NULL AND "
            "      channum = :CHANNUM");
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
                                  const QString& value,
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
                                  const QString& value,
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

QReadWriteLock ChannelUtil::s_channelDefaultAuthorityMapLock;
QMap<uint,QString> ChannelUtil::s_channelDefaultAuthorityMap;
bool ChannelUtil::s_channelDefaultAuthority_runInit = true;

/** Returns the DVB default authority for the chanid given. */
QString ChannelUtil::GetDefaultAuthority(uint chanid)
{
    s_channelDefaultAuthorityMapLock.lockForRead();

    if (s_channelDefaultAuthority_runInit)
    {
        s_channelDefaultAuthorityMapLock.unlock();
        s_channelDefaultAuthorityMapLock.lockForWrite();
        // cppcheck-suppress knownConditionTrueFalse
        if (s_channelDefaultAuthority_runInit)
        {
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare(
                "SELECT chanid, m.default_authority "
                "FROM channel c "
                "LEFT JOIN dtv_multiplex m "
                "ON (c.mplexid = m.mplexid) "
                "WHERE deleted IS NULL");
            if (query.exec())
            {
                while (query.next())
                {
                    if (!query.value(1).toString().isEmpty())
                    {
                        s_channelDefaultAuthorityMap[query.value(0).toUInt()] =
                            query.value(1).toString();
                    }
                }
                s_channelDefaultAuthority_runInit = false;
            }
            else
            {
                MythDB::DBError("GetDefaultAuthority 1", query);
            }

            query.prepare(
                "SELECT chanid, default_authority "
                "FROM channel "
                "WHERE deleted IS NULL");
            if (query.exec())
            {
                while (query.next())
                {
                    if (!query.value(1).toString().isEmpty())
                    {
                        s_channelDefaultAuthorityMap[query.value(0).toUInt()] =
                            query.value(1).toString();
                    }
                }
                s_channelDefaultAuthority_runInit = false;
            }
            else
            {
                MythDB::DBError("GetDefaultAuthority 2", query);
            }

        }
    }

    QMap<uint,QString>::iterator it = s_channelDefaultAuthorityMap.find(chanid);
    QString ret;
    if (it != s_channelDefaultAuthorityMap.end())
        ret = *it;
    s_channelDefaultAuthorityMapLock.unlock();

    return ret;
}

QString ChannelUtil::GetIcon(uint chanid)
{
    static QReadWriteLock s_channelIconMapLock;
    static QHash<uint,QString> s_channelIconMap;
    static bool s_runInit = true;

    s_channelIconMapLock.lockForRead();

    QString ret(s_channelIconMap.value(chanid, "_cold_"));

    s_channelIconMapLock.unlock();

    if (ret != "_cold_")
        return ret;

    s_channelIconMapLock.lockForWrite();

    MSqlQuery query(MSqlQuery::InitCon());
    QString iconquery = "SELECT chanid, icon FROM channel";

    if (s_runInit)
        iconquery += " WHERE visible > 0";
    else
        iconquery += " WHERE chanid = :CHANID";

    query.prepare(iconquery);

    if (!s_runInit)
        query.bindValue(":CHANID", chanid);

    if (query.exec())
    {
        if (s_runInit)
        {
            s_channelIconMap.reserve(query.size());
            while (query.next())
            {
                s_channelIconMap[query.value(0).toUInt()] =
                    query.value(1).toString();
            }
            s_runInit = false;
        }
        else
        {
            s_channelIconMap[chanid] = (query.next()) ?
                query.value(1).toString() : "";
        }
    }
    else
    {
        MythDB::DBError("GetIcon", query);
    }

    ret = s_channelIconMap.value(chanid, "");

    s_channelIconMapLock.unlock();

    return ret;
}

QString ChannelUtil::GetUnknownCallsign(void)
{
    return tr("UNKNOWN", "Synthesized callsign");
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

    // find a proper ATSC channel
    query.prepare("SELECT chanid FROM channel,dtv_multiplex "
                  "WHERE channel.deleted           IS NULL AND "
                  "      channel.sourceid          = :SOURCEID AND "
                  "      atsc_major_chan           = :MAJORCHAN AND "
                  "      atsc_minor_chan           = :MINORCHAN AND "
                  "      dtv_multiplex.transportid = :TRANSPORTID AND "
                  "      dtv_multiplex.mplexid     = :MPLEXID AND "
                  "      dtv_multiplex.sourceid    = channel.sourceid AND "
                  "      dtv_multiplex.mplexid     = channel.mplexid");

    query.bindValue(":SOURCEID", source_id);
    query.bindValue(":MAJORCHAN", major_channel);
    query.bindValue(":MINORCHAN", minor_channel);
    query.bindValue(":TRANSPORTID", service_transport_id);
    query.bindValue(":MPLEXID", mplexid);

    if (query.exec() && query.next())
        return query.value(0).toInt();

    // Find manually inserted/edited channels in order of scariness.
    // find renamed channel, where atsc is valid
    query.prepare("SELECT chanid FROM channel "
                  "WHERE deleted IS NULL AND "
                  "sourceid = :SOURCEID AND "
                  "atsc_major_chan = :MAJORCHAN AND "
                  "atsc_minor_chan = :MINORCHAN");

    query.bindValue(":SOURCEID", source_id);
    query.bindValue(":MAJORCHAN", major_channel);
    query.bindValue(":MINORCHAN", minor_channel);

    if (query.exec() && query.next())
        return query.value(0).toInt();

        // find based on mpeg program number and mplexid alone
    query.prepare("SELECT chanid FROM channel "
                  "WHERE deleted IS NULL AND "
                  "sourceid = :SOURCEID AND "
                  "serviceID = :SERVICEID AND "
                  "mplexid = :MPLEXID");

    query.bindValue(":SOURCEID", source_id);
    query.bindValue(":SERVICEID", program_number);
    query.bindValue(":MPLEXID", mplexid);

    if (query.exec() && query.next())
        return query.value(0).toInt();

    return -1;
}

uint ChannelUtil::FindChannel(uint sourceid, const QString &freqid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid "
                  "FROM channel "
                  "WHERE deleted  IS NULL AND "
                  "      sourceid = :SOURCEID AND "
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

    MSqlQuery query(MSqlQuery::ChannelCon());
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
    MSqlQuery query(MSqlQuery::ChannelCon());
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
    static const QRegularExpression kNonDigitRE { R"(\D)" };
    uint chanid = 0;
    int chansep = chan_num.indexOf(kNonDigitRE);
    if (chansep > 0)
    {
        chanid =
            (sourceid * 10000) +
            (chan_num.left(chansep).toInt() * 100) +
            chan_num.right(chan_num.length() - chansep - 1).toInt();
    }
    else
    {
        chanid = (sourceid * 10000) + chan_num.toInt();
    }

    if ((chanid > sourceid * 10000) && (chanid_available(chanid)))
        return chanid;

    // try to at least base it on the sourceid for human readability
    chanid = std::max(get_max_chanid(sourceid) + 1, sourceid * 10000);

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
                                ChannelVisibleType visible,
                                const QString &freqid,
                                const QString& icon,
                                QString format,
                                const QString& xmltvid,
                                const QString& default_authority,
                                uint service_type,
                                int  recpriority,
                                int  tmOffset,
                                int  commMethod )
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
        "   icon,          xmltvid,    default_authority, "
        "   service_type,  recpriority, tmoffset,         "
        "   commmethod ) "
        "VALUES "
        "  (:CHANID,       :CHANNUM,   :SOURCEID,         "
        "   :CALLSIGN,     :NAME,      :SERVICEID,        ";
    qstr += (db_mplexid > 0)    ? ":MPLEXID, " : "";
    qstr += (!freqid.isEmpty()) ? ":FREQID, "  : "";
    qstr +=
        "   :MAJORCHAN,                :MINORCHAN,        "
        "   :USEOAG,       :VISIBLE,   :TVFORMAT,         "
        "   :ICON,         :XMLTVID,   :AUTHORITY,        "
        "   :SERVICETYPE, :RECPRIORITY, :TMOFFSET,        "
        "   :COMMMETHOD ) ";

    query.prepare(qstr);

    query.bindValue      (":CHANID",    new_channel_id);
    query.bindValueNoNull(":CHANNUM",   chanNum);
    query.bindValue      (":SOURCEID",  db_sourceid);
    query.bindValueNoNull(":CALLSIGN",  callsign);
    query.bindValueNoNull(":NAME",      service_name);

    if (db_mplexid > 0)
        query.bindValue(":MPLEXID",   db_mplexid);

    query.bindValue(":SERVICEID", service_id);
    query.bindValue(":MAJORCHAN", atsc_major_channel);
    query.bindValue(":MINORCHAN", atsc_minor_channel);
    query.bindValue(":USEOAG",    use_on_air_guide);
    query.bindValue(":VISIBLE",   visible);

    if (!freqid.isEmpty())
        query.bindValue(":FREQID",    freqid);

    QString tvformat = (atsc_minor_channel > 0) ? "ATSC" : std::move(format);
    query.bindValueNoNull(":TVFORMAT",    tvformat);
    query.bindValueNoNull(":ICON",        icon);
    query.bindValueNoNull(":XMLTVID",     xmltvid);
    query.bindValueNoNull(":AUTHORITY",   default_authority);
    query.bindValue      (":SERVICETYPE", service_type);
    query.bindValue      (":RECPRIORITY", recpriority);
    query.bindValue      (":TMOFFSET",    tmOffset);
    query.bindValue      (":COMMMETHOD",  commMethod);

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
                                ChannelVisibleType visible,
                                const QString& freqid,
                                const QString& icon,
                                QString format,
                                const QString& xmltvid,
                                const QString& default_authority,
                                uint service_type,
                                int  recpriority,
                                int  tmOffset,
                                int  commMethod )
{
    if (!channel_id)
        return false;

    QString tvformat = (atsc_minor_channel > 0) ? "ATSC" : std::move(format);
    bool set_channum = !chan_num.isEmpty() && chan_num != "-1";
    QString qstr = QString(
        "UPDATE channel "
        "SET %1 %2 %3 %4 %5 %6 %7 %8 %9 "
        "    mplexid         = :MPLEXID,   serviceid       = :SERVICEID, "
        "    atsc_major_chan = :MAJORCHAN, atsc_minor_chan = :MINORCHAN, "
        "    callsign        = :CALLSIGN,  name            = :NAME,      "
        "    sourceid        = :SOURCEID,  useonairguide   = :USEOAG,    "
        "    visible         = :VISIBLE,   service_type    = :SERVICETYPE "
        "WHERE chanid=:CHANID")
        .arg((!set_channum)       ? "" : "channum  = :CHANNUM,  ",
             (freqid.isEmpty())   ? "" : "freqid   = :FREQID,   ",
             (icon.isEmpty())     ? "" : "icon     = :ICON,     ",
             (tvformat.isEmpty()) ? "" : "tvformat = :TVFORMAT, ",
             (xmltvid.isEmpty())  ? "" : "xmltvid  = :XMLTVID,  ",
             (default_authority.isEmpty()) ?
                "" : "default_authority = :AUTHORITY,",
             (recpriority == INT_MIN) ? "" : "recpriority = :RECPRIORITY, ",
             (tmOffset    == INT_MIN) ? "" : "tmOffset    = :TMOFFSET, ",
             (commMethod  == INT_MIN) ? "" : "commmethod  = :COMMMETHOD, ");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(qstr);

    query.bindValue(":CHANID", channel_id);

    if (set_channum)
        query.bindValue(":CHANNUM", chan_num);

    query.bindValue      (":SOURCEID",  source_id);
    query.bindValueNoNull(":CALLSIGN",  callsign);
    query.bindValueNoNull(":NAME",      service_name);

    query.bindValue(":MPLEXID",     db_mplexid);
    query.bindValue(":SERVICEID",   service_id);
    query.bindValue(":MAJORCHAN",   atsc_major_channel);
    query.bindValue(":MINORCHAN",   atsc_minor_channel);
    query.bindValue(":USEOAG",      use_on_air_guide);
    query.bindValue(":VISIBLE",     visible);
    query.bindValue(":SERVICETYPE", service_type);

    if (!freqid.isNull())
        query.bindValue(":FREQID",    freqid);
    if (!tvformat.isNull())
        query.bindValue(":TVFORMAT",  tvformat);
    if (!icon.isNull())
        query.bindValue(":ICON",      icon);
    if (!xmltvid.isNull())
        query.bindValue(":XMLTVID",   xmltvid);
    if (!default_authority.isNull())
        query.bindValue(":AUTHORITY",   default_authority);
    if (recpriority != INT_MIN)
        query.bindValue(":RECPRIORITY", recpriority);
    if (tmOffset != INT_MIN)
        query.bindValue(":TMOFFSET", tmOffset);
    if (commMethod != INT_MIN)
        query.bindValue(":COMMMETHOD", commMethod);

    if (!query.exec())
    {
        MythDB::DBError("Updating Service", query);
        return false;
    }
    return true;
}

void ChannelUtil::UpdateChannelNumberFromDB(ChannelInsertInfo &chan)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channum "
        "FROM channel "
        "WHERE chanid = :ID");
    query.bindValue(":ID", chan.m_channelId);

    if (!query.exec())
    {
        MythDB::DBError("UpdateChannelNumberFromDB", query);
        return;
    }

    if (query.next())
    {
        QString channum = query.value(0).toString();

        if (!channum.isEmpty())
        {
            chan.m_chanNum = channum;
        }
    }
}

void ChannelUtil::UpdateInsertInfoFromDB(ChannelInsertInfo &chan)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT xmltvid, useonairguide, visible "
        "FROM channel "
        "WHERE chanid = :ID");
    query.bindValue(":ID", chan.m_channelId);

    if (!query.exec())
    {
        MythDB::DBError("UpdateInsertInfoFromDB", query);
        return;
    }

    if (query.next())
    {
        QString xmltvid = query.value(0).toString();
        bool useeit     = query.value(1).toBool();
        ChannelVisibleType visible =
            static_cast<ChannelVisibleType>(query.value(2).toInt());

        if (!xmltvid.isEmpty())
        {
            if (useeit)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "Using EIT and xmltv for the same channel "
                    "is an unsupported configuration.");
            }
            chan.m_xmltvId = xmltvid;
        }
        chan.m_useOnAirGuide = useeit;
        chan.m_hidden = (visible == kChannelNotVisible ||
                         visible == kChannelNeverVisible);
        chan.m_visible = visible;
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
        "UPDATE channel "
        "SET deleted = NOW() "
        "WHERE chanid = :ID");
    query.bindValue(":ID", channel_id);

    if (!query.exec())
    {
        MythDB::DBError("Delete Channel", query);
        return false;
    }

    return true;
}

bool ChannelUtil::SetVisible(uint channel_id, ChannelVisibleType visible)
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

    query.prepare("UPDATE dtv_multiplex "
                  "SET serviceversion = :VERSION "
                  "WHERE mplexid = :MPLEXID");

    query.bindValue(":VERSION", version);
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex", query);
        return false;
    }
    return true;
}

int ChannelUtil::GetServiceVersion(int mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT serviceversion "
                  "FROM dtv_multiplex "
                  "WHERE mplexid = :MPLEXID");

    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec())
    {
        MythDB::DBError("Selecting channel/dtv_multiplex", query);
        return 0;
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
        "WHERE deleted  IS NULL AND "
        "      channum  = :CHANNUM AND "
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
    chanid        = 0;
    tvformat.clear();
    modulation.clear();
    freqtable.clear();;
    freqid.clear();
    dtv_si_std.clear();
    finetune      = 0;
    frequency     = 0;
    mpeg_prog_num = -1;
    atsc_major    = atsc_minor = mplexid = 0;
    dvb_networkid = dvb_transportid = 0;
    commfree      = false;

    int found = 0;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT finetune, freqid, tvformat, freqtable, "
        "       commmethod, mplexid, "
        "       atsc_major_chan, atsc_minor_chan, serviceid, "
        "       chanid,  visible "
        "FROM channel, videosource "
        "WHERE channel.deleted      IS NULL            AND "
        "      videosource.sourceid = channel.sourceid AND "
        "      channum              = :CHANNUM         AND "
        "      channel.sourceid     = :SOURCEID "
        "ORDER BY channel.visible > 0 DESC, channel.chanid ");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("GetChannelData", query);
        return false;
    }

    if (query.next())
    {
        finetune      = query.value(0).toInt();
        freqid        = query.value(1).toString();
        tvformat      = query.value(2).toString();
        freqtable     = query.value(3).toString();
        commfree      = (query.value(4).toInt() == -2);
        mplexid       = query.value(5).toUInt();
        atsc_major    = query.value(6).toUInt();
        atsc_minor    = query.value(7).toUInt();
        mpeg_prog_num = (query.value(8).isNull()) ? -1
                        : query.value(8).toInt();
        chanid        = query.value(9).toUInt();

        if (query.value(10).toInt() > kChannelNotVisible)
            found++;
    }

    while (query.next())
        if (query.value(10).toInt() > kChannelNotVisible)
            found++;

    if (found == 0 && chanid)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("No visible channels for %1, using invisble chanid %2")
            .arg(channum).arg(chanid));
    }

    if (found > 1)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Found multiple visible channels for %1, using chanid %2")
            .arg(channum).arg(chanid));
    }

    if (!chanid)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Could not find channel '%1' in DB for source %2 '%3'.")
                .arg(channum).arg(sourceid).arg(SourceUtil::GetSourceName(sourceid)));
        return false;
    }

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
        return {};
    }

    QString data_url;
    QString fec_url0;
    QString fec_url1;
    IPTVTuningData::FECType fec_type = IPTVTuningData::kNone;
    std::array<uint,3> bitrate { 0, 0, 0, };
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
    uint sourceid, bool visible_only, bool include_disconnected,
    const QString &group_by, uint channel_groupid)
{
    ChannelInfoList list;

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr = QString(
        "SELECT videosource.sourceid, GROUP_CONCAT(capturecard.cardid) "
        "FROM videosource "
        "%1 JOIN capturecard ON capturecard.sourceid = videosource.sourceid "
        "GROUP BY videosource.sourceid")
        .arg((include_disconnected) ? "LEFT" : "");

    query.prepare(qstr);
    if (!query.exec())
    {
        MythDB::DBError("ChannelUtil::GetChannels()", query);
        return list;
    }

    QMap<uint, QList<uint>> inputIdLists;
    while (query.next())
    {
        uint qSourceId = query.value(0).toUInt();
        QList<uint> &inputIdList = inputIdLists[qSourceId];
        QStringList inputIds = query.value(1).toString().split(",");
        while (!inputIds.isEmpty())
            inputIdList.append(inputIds.takeFirst().toUInt());
    }

    qstr = QString(
        "SELECT channum, callsign, channel.chanid, "
        "       atsc_major_chan, atsc_minor_chan, "
        "       name, icon, mplexid, visible, "
        "       channel.sourceid, "
        "       GROUP_CONCAT(DISTINCT channelgroup.grpid), "
        "       xmltvid "
        "FROM channel "
        "LEFT JOIN channelgroup ON channel.chanid = channelgroup.chanid ");

    qstr += "WHERE deleted IS NULL ";

    if (sourceid)
        qstr += QString("AND channel.sourceid='%1' ").arg(sourceid);

    // Select only channels from the specified channel group
    if (channel_groupid > 0)
        qstr += QString("AND channelgroup.grpid = '%1' ").arg(channel_groupid);

    if (visible_only)
        qstr += QString("AND visible > 0 ");

    qstr += " GROUP BY chanid";

    if (!group_by.isEmpty())
        qstr += QString(", %1").arg(group_by);

    query.prepare(qstr);
    if (!query.exec())
    {
        MythDB::DBError("ChannelUtil::GetChannels()", query);
        return list;
    }

    while (query.next())
    {
        if (query.value(0).toString().isEmpty() || !query.value(2).toBool())
            continue; // skip if channum blank, or chanid empty

        uint qSourceID = query.value(9).toUInt();
        ChannelInfo chan(
            query.value(0).toString(),                    /* channum    */
            query.value(1).toString(),                    /* callsign   */
            query.value(2).toUInt(),                      /* chanid     */
            query.value(3).toUInt(),                      /* ATSC major */
            query.value(4).toUInt(),                      /* ATSC minor */
            query.value(7).toUInt(),                      /* mplexid    */
            static_cast<ChannelVisibleType>(query.value(8).toInt()),
                                                          /* visible    */
            query.value(5).toString(),                    /* name       */
            query.value(6).toString(),                    /* icon       */
            qSourceID);                                   /* sourceid   */

        chan.m_xmltvId = query.value(11).toString();      /* xmltvid    */

        for (auto inputId : std::as_const(inputIdLists[qSourceID]))
            chan.AddInputId(inputId);

        QStringList groupIDs = query.value(10).toString().split(",");
        while (!groupIDs.isEmpty())
                chan.AddGroupId(groupIDs.takeFirst().toUInt());

        list.push_back(chan);

    }

    return list;
}

std::vector<uint> ChannelUtil::GetChanIDs(int sourceid, bool onlyVisible)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString select = "SELECT chanid FROM channel WHERE deleted IS NULL ";
    // Yes, this a little ugly
    if (onlyVisible || sourceid > 0)
    {
        if (onlyVisible)
            select += "AND visible > 0 ";
        if (sourceid > 0)
            select += "AND sourceid=" + QString::number(sourceid);
    }

    std::vector<uint> list;
    query.prepare(select);
    if (!query.exec())
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
    return StringUtil::naturalCompare(a.m_callSign, b.m_callSign) < 0;
}

inline bool lt_smart(const ChannelInfo &a, const ChannelInfo &b)
{
    static QMutex s_sepExprLock;
    static const QRegularExpression kSepExpr(ChannelUtil::kATSCSeparators);

    bool isIntA = false;
    bool isIntB = false;
    int a_int   = a.m_chanNum.toUInt(&isIntA);
    int b_int   = b.m_chanNum.toUInt(&isIntB);
    int a_major = a.m_atscMajorChan;
    int b_major = b.m_atscMajorChan;
    int a_minor = a.m_atscMinorChan;
    int b_minor = b.m_atscMinorChan;

    // Extract minor and major numbers from channum..
    int idxA = 0;
    int idxB = 0;
    {
        QMutexLocker locker(&s_sepExprLock);
        idxA = a.m_chanNum.indexOf(kSepExpr);
        idxB = b.m_chanNum.indexOf(kSepExpr);
    }
    if (idxA >= 0)
    {
        bool tmp1 = false;
        bool tmp2 = false;
        int major = a.m_chanNum.left(idxA).toUInt(&tmp1);
        int minor = a.m_chanNum.mid(idxA+1).toUInt(&tmp2);
        if (tmp1 && tmp2)
            (a_major = major), (a_minor = minor), (isIntA = false);
    }

    if (idxB >= 0)
    {
        bool tmp1 = false;
        bool tmp2 = false;
        int major = b.m_chanNum.left(idxB).toUInt(&tmp1);
        int minor = b.m_chanNum.mid(idxB+1).toUInt(&tmp2);
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
        int cmp = a_maj - b_maj;
        if (cmp != 0)
            return cmp < 0;

        cmp = a_minor - b_minor;
        if (cmp != 0)
            return cmp < 0;
    }

    if (isIntA && isIntB)
    {
        // both channels have a numeric channum
        int cmp = a_int - b_int;
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
        int cmp = StringUtil::naturalCompare(a.m_chanNum, b.m_chanNum);
        if (cmp)
            return cmp < 0;
    }

    return lt_callsign(a,b);
}

uint ChannelUtil::GetChannelCount(int sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString   select;


    select = "SELECT chanid FROM channel WHERE deleted IS NULL ";
    if (sourceid >= 0)
        select += "AND sourceid=" + QString::number(sourceid);
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
        for (size_t i = 1; i < list.size(); i++)
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
    target.m_chanNum = channum;
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
    bool              skip_same_channum_and_callsign,
    bool              skip_other_sources)
{
    auto it = find(sorted.cbegin(), sorted.cend(), old_chanid);

    if (it == sorted.end())
        it = sorted.begin(); // not in list, pretend we are on first channel

    if (it == sorted.end())
        return 0; // no channels..

    auto start = it;

    if (CHANNEL_DIRECTION_DOWN == direction)
    {
        do
        {
            if (it == sorted.begin())
            {
                it = find(sorted.begin(), sorted.end(),
                          sorted.rbegin()->m_chanId);
                if (it == sorted.end())
                {
                    --it;
                }
            }
            else
            {
                --it;
            }
        }
        while ((it != start) &&
               ((skip_non_visible && it->m_visible < kChannelVisible) ||
                (skip_other_sources &&
                 it->m_sourceId != start->m_sourceId) ||
                (skip_same_channum_and_callsign &&
                 it->m_chanNum  == start->m_chanNum &&
                 it->m_callSign == start->m_callSign) ||
                ((mplexid_restriction != 0U) &&
                 (mplexid_restriction != it->m_mplexId)) ||
                ((chanid_restriction != 0U) &&
                 (chanid_restriction != it->m_chanId))));
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
               ((skip_non_visible && it->m_visible < kChannelVisible) ||
                (skip_other_sources &&
                 it->m_sourceId != start->m_sourceId) ||
                (skip_same_channum_and_callsign &&
                 it->m_chanNum  == start->m_chanNum &&
                 it->m_callSign == start->m_callSign) ||
                ((mplexid_restriction != 0U) &&
                 (mplexid_restriction != it->m_mplexId)) ||
                ((chanid_restriction != 0U) &&
                 (chanid_restriction != it->m_chanId))));
    }

    return it->m_chanId;
}

ChannelInfoList ChannelUtil::LoadChannels(uint startIndex, uint count,
                                          uint &totalAvailable,
                                          bool ignoreHidden,
                                          ChannelUtil::OrderBy orderBy,
                                          ChannelUtil::GroupBy groupBy,
                                          uint sourceID,
                                          uint channelGroupID,
                                          bool liveTVOnly,
                                          const QString& callsign,
                                          const QString& channum,
                                          bool ignoreUntunable)
{
    ChannelInfoList channelList;

    MSqlQuery query(MSqlQuery::InitCon());

    QString sql = QString(
        "SELECT parentid, GROUP_CONCAT(cardid ORDER BY cardid) "
        "FROM capturecard "
        "WHERE parentid <> 0 "
        "GROUP BY parentid ");

    query.prepare(sql);
    if (!query.exec())
    {
        MythDB::DBError("ChannelUtil::GetChannels()", query);
        return channelList;
    }

    QMap<uint, QList<uint>> childIdLists;
    while (query.next())
    {
        auto parentId = query.value(0).toUInt();
        auto &childIdList = childIdLists[parentId];
        auto childIds = query.value(1).toString().split(",");
        while (!childIds.isEmpty())
            childIdList.append(childIds.takeFirst().toUInt());
    }

    sql = "SELECT %1 channum, freqid, channel.sourceid, "
                  "callsign, name, icon, finetune, videofilters, xmltvid, "
                  "channel.recpriority, channel.contrast, channel.brightness, "
                  "channel.colour, channel.hue, tvformat, "
                  "visible, outputfilters, useonairguide, mplexid, "
                  "serviceid, atsc_major_chan, atsc_minor_chan, last_record, "
                  "default_authority, commmethod, tmoffset, iptvid, "
                  "channel.chanid, "
                  "GROUP_CONCAT(DISTINCT `groups`.`groupids`), " // Creates a CSV list of channel groupids for this channel
                  "GROUP_CONCAT(DISTINCT capturecard.cardid "
                  "             ORDER BY livetvorder), " // Creates a CSV list of inputids for this channel
                  "MIN(livetvorder) livetvorder "
                  "FROM channel ";
    if (!channelGroupID)
        sql +=    "LEFT ";
    sql +=        "JOIN ( "
                  "    SELECT chanid ,"
                  "           GROUP_CONCAT(grpid ORDER BY grpid) groupids "
                  "    FROM channelgroup ";
    if (channelGroupID)
        sql +=    "    WHERE grpid = :CHANGROUPID ";
    sql +=        "    GROUP BY chanid "
                  ") `groups` "
                  "    ON channel.chanid = `groups`.`chanid` ";
    if (!ignoreUntunable && !liveTVOnly)
        sql +=    "LEFT ";
    sql +=        "JOIN capturecard "
                  "    ON capturecard.sourceid = channel.sourceid "
                  "       AND capturecard.parentid = 0 ";
    if (liveTVOnly)
        sql +=    "       AND capturecard.livetvorder > 0 ";

    sql += "WHERE channel.deleted IS NULL ";
    if (ignoreHidden)
        sql += "AND channel.visible > 0 ";

    if (sourceID > 0)
        sql += "AND channel.sourceid = :SOURCEID ";

    if (groupBy == kChanGroupByCallsign)
        sql += "GROUP BY channel.callsign ";
    else if (groupBy == kChanGroupByCallsignAndChannum)
        sql += "GROUP BY channel.callsign, channel.channum ";
    else
        sql += "GROUP BY channel.chanid "; // We must always group for this query

    if (orderBy == kChanOrderByName)
        sql += "ORDER BY channel.name ";
    else if (orderBy == kChanOrderByChanNum)
    {
        // Natural sorting including subchannels e.g. 2_4, 1.3
        sql += "ORDER BY LPAD(CAST(channel.channum AS UNSIGNED), 10, 0), "
               "         LPAD(channel.channum,  10, 0) ";
    }
    else // kChanOrderByLiveTV
    {
        sql += "ORDER BY callsign = :CALLSIGN1 AND channum = :CHANNUM DESC, "
               "         callsign = :CALLSIGN2 DESC, "
               "         livetvorder, "
               "         channel.recpriority DESC, "
               "         chanid ";
    }

    if (count > 0)
        sql += "LIMIT :LIMIT ";

    if (startIndex > 0)
        sql += "OFFSET :STARTINDEX ";


    if (startIndex > 0 || count > 0)
        sql = sql.arg("SQL_CALC_FOUND_ROWS");
    else
        sql = sql.arg(""); // remove place holder

    query.prepare(sql);

    if (channelGroupID > 0)
        query.bindValue(":CHANGROUPID", channelGroupID);

    if (sourceID > 0)
        query.bindValue(":SOURCEID", sourceID);

    if (count > 0)
        query.bindValue(":LIMIT", count);

    if (startIndex > 0)
        query.bindValue(":STARTINDEX", startIndex);

    if (orderBy == kChanOrderByLiveTV)
    {
        query.bindValue(":CALLSIGN1", callsign);
        query.bindValue(":CHANNUM", channum);
        query.bindValue(":CALLSIGN2", callsign);
    }

    if (!query.exec())
    {
        MythDB::DBError("ChannelInfo::Load()", query);
        return channelList;
    }

    QList<uint> groupIdList;
    while (query.next())
    {
        ChannelInfo channelInfo;
        channelInfo.m_chanNum           = query.value(0).toString();
        channelInfo.m_freqId            = query.value(1).toString();
        channelInfo.m_sourceId          = query.value(2).toUInt();
        channelInfo.m_callSign          = query.value(3).toString();
        channelInfo.m_name              = query.value(4).toString();
        channelInfo.m_icon              = query.value(5).toString();
        channelInfo.m_fineTune          = query.value(6).toInt();
        channelInfo.m_videoFilters      = query.value(7).toString();
        channelInfo.m_xmltvId           = query.value(8).toString();
        channelInfo.m_recPriority       = query.value(9).toInt();
        channelInfo.m_contrast          = query.value(10).toUInt();
        channelInfo.m_brightness        = query.value(11).toUInt();
        channelInfo.m_colour            = query.value(12).toUInt();
        channelInfo.m_hue               = query.value(13).toUInt();
        channelInfo.m_tvFormat          = query.value(14).toString();
        channelInfo.m_visible           =
            static_cast<ChannelVisibleType>(query.value(15).toInt());
        channelInfo.m_outputFilters     = query.value(16).toString();
        channelInfo.m_useOnAirGuide     = query.value(17).toBool();
        channelInfo.m_mplexId           = query.value(18).toUInt();
        channelInfo.m_serviceId         = query.value(19).toUInt();
        channelInfo.m_atscMajorChan     = query.value(20).toUInt();
        channelInfo.m_atscMinorChan     = query.value(21).toUInt();
        channelInfo.m_lastRecord        = query.value(22).toDateTime();
        channelInfo.m_defaultAuthority  = query.value(23).toString();
        channelInfo.m_commMethod        = query.value(24).toUInt();
        channelInfo.m_tmOffset          = query.value(25).toUInt();
        channelInfo.m_iptvId            = query.value(26).toUInt();
        channelInfo.m_chanId            = query.value(27).toUInt();

        QStringList groupIDs = query.value(28).toString().split(",");
        groupIdList.clear();
        while (!groupIDs.isEmpty())
                groupIdList.push_back(groupIDs.takeFirst().toUInt());
        std::sort(groupIdList.begin(), groupIdList.end());
        for (auto groupId : groupIdList)
            channelInfo.AddGroupId(groupId);

        QStringList parentIDs = query.value(29).toString().split(",");
        while (!parentIDs.isEmpty())
        {
            auto parentId = parentIDs.takeFirst().toUInt();
            channelInfo.AddInputId(parentId);
            auto childIdList = childIdLists[parentId];
            for (auto childId : childIdList)
                channelInfo.AddInputId(childId);
        }

        channelList.push_back(channelInfo);
    }

    if ((startIndex > 0 || count > 0) &&
        query.exec("SELECT FOUND_ROWS()") && query.next())
        totalAvailable = query.value(0).toUInt();
    else
        totalAvailable = query.size();

    return channelList;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
