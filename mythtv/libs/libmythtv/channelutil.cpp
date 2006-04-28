// -*- Mode: c++ -*-

#include <qregexp.h>
#include <stdint.h>

#include "channelutil.h"
#include "mythdbcon.h"
#include "dvbtables.h"

static uint get_dtv_multiplex(int  db_source_id,  QString sistandard,
                              uint frequency,
                              // DVB specific
                              int  transport_id,  int     network_id)
{
    QString qstr = 
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE sourceid     = :SOURCEID   "
        "  AND sistandard   = :SISTANDARD ";

    if (sistandard.lower() != "dvb")
        qstr += "AND frequency    = :FREQUENCY   ";
    else
    {
        qstr += "AND transportid  = :TRANSPORTID ";
        qstr += "AND networkid    = :NETWORKID   ";
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(qstr);

    query.bindValue(":SOURCEID",          db_source_id);
    query.bindValue(":SISTANDARD",        sistandard);

    if (sistandard.lower() != "dvb")
        query.bindValue(":FREQUENCY",     frequency);
    else
    {
        query.bindValue(":TRANSPORTID",   transport_id);
        query.bindValue(":NETWORKID",     network_id);
    }

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("get_dtv_multiplex", query);
        return 0;
    }

    if (query.next())
        return query.value(0).toUInt();

    return 0;
}

static uint insert_dtv_multiplex(
    int         db_source_id,  QString     sistandard,
    uint        frequency,     QString     modulation,
    // DVB specific
    int         transport_id,  int         network_id,
    int         symbol_rate,   signed char bandwidth,
    signed char polarity,      signed char inversion,
    signed char trans_mode,
    QString     inner_FEC,     QString      constellation,
    signed char hierarchy,     QString      hp_code_rate,
    QString     lp_code_rate,  QString      guard_interval)
{
    MSqlQuery query(MSqlQuery::InitCon());

    VERBOSE(VB_SIPARSER, QString("insert_dtv_multiplex(%1, %2, %3, %4...)")
            .arg(db_source_id).arg(sistandard)
            .arg(frequency).arg(modulation));

    // If transport is already present, skip insert
    uint mplex = get_dtv_multiplex(
        db_source_id,  sistandard,    frequency,
        // DVB specific
        transport_id,  network_id);

    QString updateStr =
        "UPDATE dtv_multiplex "
        "SET sourceid         = :SOURCEID,      sistandard   = :SISTANDARD, "
        "    frequency        = :FREQUENCY,     modulation   = :MODULATION, "
        "    transportid      = :TRANSPORTID,   networkid    = :NETWORKID, "
        "    symbolrate       = :SYMBOLRATE,    bandwidth    = :BANDWIDTH, "
        "    polarity         = :POLARITY,      inversion    = :INVERSION, "
        "    transmission_mode= :TRANS_MODE,    fec          = :INNER_FEC, "
        "    constellation    = :CONSTELLATION, hierarchy    = :HIERARCHY, "
        "    hp_code_rate     = :HP_CODE_RATE,  lp_code_rate = :LP_CODE_RATE, "
        "    guard_interval   = :GUARD_INTERVAL "
        "WHERE sourceid    = :SOURCEID     AND "
        "      sistandard  = :SISTANDARD   AND "
        "      transportid = :TRANSPORTID  AND "
        "      networkid   = :NETWORKID ";

    if (sistandard.lower() != "dvb")
        updateStr += " AND frequency = :FREQUENCY ";

    QString insertStr =
        "INSERT INTO dtv_multiplex "
        "  (sourceid,        sistandard,        frequency,  "
        "   modulation,      transportid,       networkid,  "
        "   symbolrate,      bandwidth,         polarity,   "
        "   inversion,       transmission_mode,             "
        "   fec,             constellation,     hierarchy,  "
        "   hp_code_rate,    lp_code_rate,      guard_interval) "
        "VALUES "
        "  (:SOURCEID,       :SISTANDARD,       :FREQUENCY, "
        "   :MODULATION,     :TRANSPORTID,      :NETWORKID, "
        "   :SYMBOLRATE,     :BANDWIDTH,        :POLARITY,  "
        "   :INVERSION,      :TRANS_MODE,                   "
        "   :INNER_FEC,      :CONSTELLATION,    :HIERARCHY, "
        "   :HP_CODE_RATE,   :LP_CODE_RATE,     :GUARD_INTERVAL);";

    query.prepare((mplex) ? updateStr : insertStr);

    VERBOSE(VB_SIPARSER, "insert_dtv_multiplex -- "
            <<((mplex) ? "update" : "insert") << " " << mplex);

    query.bindValue(":SOURCEID",          db_source_id);
    query.bindValue(":SISTANDARD",        sistandard);
    query.bindValue(":FREQUENCY",         frequency);

    if (!modulation.isNull())
        query.bindValue(":MODULATION",    modulation);
    if (transport_id > 0)
        query.bindValue(":TRANSPORTID",   transport_id);
    if (network_id > 0)
        query.bindValue(":NETWORKID",     network_id);

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

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Adding transport to Database.", query);
        return 0;
    }

    if (mplex)
        return mplex;

    mplex = get_dtv_multiplex(
        db_source_id,  sistandard,    frequency,
        // DVB specific
        transport_id,  network_id);

    VERBOSE(VB_SIPARSER, QString("insert_dtv_multiplex -- ") +
            QString("inserted %1").arg(mplex));

    return mplex;
}

void handle_transport_desc(vector<uint> &muxes, const MPEGDescriptor &desc,
                           uint sourceid, uint tsid, uint netid)
{
    uint tag = desc.DescriptorTag();

    if (tag == DescriptorID::terrestrial_delivery_system)
    {
        const TerrestrialDeliverySystemDescriptor cd(desc);
        uint64_t freq = cd.FrequencyHz();

        // Use the frequency we already have for this mplex
        // as it may be one of the other_frequencies for this mplex
        if (cd.OtherFrequencyInUse())
        {
            QString modulation;
            uint mux = ChannelUtil::GetMplexID(sourceid, tsid, netid);
            freq     = ChannelUtil::GetTuningParams(mux, modulation);
        }

        uint mux = ChannelUtil::CreateMultiplex(
            sourceid,            "dvb",
            freq,                 QString::null,
            // DVB specific
            tsid,                 netid,
            -1,                   QChar(cd.BandwidthString()[0]),
            -1,                   'a',
            QChar(cd.TransmissionModeString()[0]),
            QString::null,                  cd.ConstellationString(),
            QChar(cd.HierarchyString()[0]), cd.CodeRateHPString(),
            cd.CodeRateLPString(),          cd.GuardIntervalString());

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
            QChar(cd.PolarizationString()[0]), -1,
            -1,
            cd.FECInnerString(),  QString::null,
            -1,                   QString::null,
            QString::null,        QString::null);

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
            -1,                   -1,
            -1,
            cd.FECInnerString(),  QString::null,
            -1,                   QString::null,
            QString::null,        QString::null);

        if (mux)
            muxes.push_back(mux);
    }
#if 0
    else if (tag == DescriptorID::frequency_list)
    {
        const FrequencyListDescriptor cd(desc);
        //uint ct = cd.CodingType(); //nd,sat,cable,terra
        for (uint i = 0; i < cd.FrequencyCount(); i++)
        {
            uint mux = ChannelUtil::CreateMultiplex(
                sourceid,             "dvb",
                cd.FrequencyHz(i),    QString::null/*modulation*/,
                tsid,                 netid);

            if (mux)
                muxes.push_back(mux);
        }
    }
#endif
}

uint ChannelUtil::CreateMultiplex(int  sourceid,     QString sistandard,
                                  uint frequency,    QString modulation,
                                  int  transport_id, int     network_id)
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
        QString::null,      QString::null);
}

uint ChannelUtil::CreateMultiplex(
    int         sourceid,     QString     sistandard,
    uint        freq,         QString     modulation,
    // DVB specific
    int         transport_id, int         network_id,
    int         symbol_rate,  signed char bandwidth,
    signed char polarity,     signed char inversion,
    signed char trans_mode,
    QString     inner_FEC,    QString     constellation,
    signed char hierarchy,    QString     hp_code_rate,
    QString     lp_code_rate, QString     guard_interval)
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
        lp_code_rate,       guard_interval);
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

int ChannelUtil::GetMplexID(uint sourceid, uint frequency)
{
    MSqlQuery query(MSqlQuery::InitCon());
    /* See if mplexid is already in the database */
    query.prepare(
        "SELECT mplexid "
        "FROM dtv_multiplex "
        "WHERE sourceid  = :SOURCEID  AND "
        "      frequency = :FREQUENCY");

    query.bindValue(":SOURCEID",  sourceid);
    query.bindValue(":FREQUENCY", frequency);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetMplexID 1", query);
        return -1;
    }

    if (query.next())
        return query.value(0).toInt();

    return -1;
}

int ChannelUtil::GetMplexID(uint sourceid,     uint frequency,
                            uint transport_id, uint network_id)
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
    query.bindValue(":FREQUENCY",   frequency);
    
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetMplexID 2", query);
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
        MythContext::DBError("GetMplexID 3", query);
        return -1;
    }

    if (query.next())
        return query.value(0).toInt();

    return -1;
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
    VERBOSE(VB_SIPARSER,
            QString("GetBetterMplexID(mplexId %1, tId %2, netId %3)")
            .arg(current_mplexid).arg(transport_id).arg(network_id));

    int q_networkid = 0, q_transportid = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("SELECT networkid, transportid "
                          "FROM dtv_multiplex "
                          "WHERE mplexid = %1").arg(current_mplexid));

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Getting mplexid global search", query);
    else if (query.size())
    {
        query.next();
        q_networkid   = query.value(0).toInt();
        q_transportid = query.value(1).toInt();
    }

    // Got a match, return it.
    if ((q_networkid == network_id) && (q_transportid == transport_id))
    {
        VERBOSE(VB_SIPARSER,
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
            MythContext::DBError("Getting mplexid global search", query);

        VERBOSE(VB_SIPARSER, QString(
                    "GetBetterMplexID(): net id and transport id "
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
            MythContext::DBError("Finding matching mplexid", query);

        if (query.size() == 1)
        {
            VERBOSE(VB_SIPARSER, QString(
                        "GetBetterMplexID(): query#%1 qsize(%2) "
                        "Returning %3")
                    .arg(i).arg(query.size()).arg(current_mplexid));
            query.next();
            return query.value(0).toInt();
        }

        if (query.size() > 1)
        {
            query.next();
            int ret = (i==0) ? current_mplexid : query.value(0).toInt();
            VERBOSE(VB_SIPARSER, QString(
                        "GetBetterMplexID(): query#%1 qsize(%2) "
                        "Returning %3")
                    .arg(i).arg(query.size()).arg(ret));
            return ret;
        }
    }

    // If you still didn't find this combo return -1 (failure)
    VERBOSE(VB_SIPARSER, QString("GetBetterMplexID(): Returning -1"));
    return -1;
}

int ChannelUtil::GetTuningParams(int mplexid, QString &modulation)
{
    if (mplexid <= 0)
        return -1;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT frequency, modulation "
                          "FROM dtv_multiplex "
                          "WHERE mplexid=%1").arg(mplexid));

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetTuningParams failed ", query);
        return -1;
    }

    if (!query.size())
        return -1;

    query.next();

    modulation = query.value(1).toString();
    return query.value(0).toInt(); 
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
        MythContext::DBError("Selecting channel/dtv_multiplex 1", query);
        return QString::null;
    }
    if (!query.size())
        return QString::null;

    query.next();
    return query.value(0).toString();
}

QString ChannelUtil::GetChanNum(int chan_id)
{
    return GetChannelStringField(chan_id, QString("channum"));
}

QString ChannelUtil::GetCallsign(int chan_id)
{
    return GetChannelStringField(chan_id, QString("callsign"));
}

QString ChannelUtil::GetServiceName(int chan_id)
{
    return GetChannelStringField(chan_id, QString("name"));
}

int ChannelUtil::GetSourceID(int db_mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("SELECT sourceid "
                          "FROM dtv_multiplex "
                          "WHERE mplexid = %1").arg(db_mplexid));

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Selecting channel/dtv_multiplex", query);
        return -1;
    }

    if (query.size() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }
    return -1;
}

/** \fn ChannelUtil::GetInputName(int)
 *  \brief Returns input name for a card input
 */
QString ChannelUtil::GetInputName(int source_id)
{
    QString inputname = QString::null;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT inputname "
                  "FROM cardinput "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", source_id);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        inputname = query.value(0).toString();
    }
    return inputname;
}

int ChannelUtil::GetChanID(int mplexid,       int service_transport_id,
                           int major_channel, int minor_channel,
                           int program_number)
{
    // Currently we don't use the service transport id,
    // but we should use it according to ATSC std 65.
    (void) service_transport_id;
    
    MSqlQuery query(MSqlQuery::InitCon());

    // This works for transports inserted by a scan
    query.prepare(QString("SELECT chanid FROM channel "
                          "WHERE serviceID=%1 AND mplexid=%2")
                  .arg(program_number).arg(mplexid));
    
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Selecting channel/dtv_multiplex 1", query);
        return -1;
    }
    if (query.size())
    {
        query.next();
        return query.value(0).toInt();
    }

    if (major_channel <= 0)
        return -1;

    // find source id, so we can find manually inserted ATSC channels
    query.prepare(QString("SELECT sourceid FROM dtv_multiplex "
                          "WHERE mplexid=%2").arg(mplexid));
    if (!query.exec() || !query.isActive() || !query.size())
    {
        MythContext::DBError("Selecting channel/dtv_multiplex 2", query);
        return -1;
    }
    query.next();
    int source_id = query.value(0).toInt();

    uint atsc_src_id = (major_channel << 8) | (minor_channel & 0xff);

    // Find manually inserted/edited channels...
    QString qstr[] = 
    {
        // find based on pcHDTV formatted major and minor channels
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND channum='%2_%3'")
        .arg(source_id).arg(major_channel).arg(minor_channel),
        // find based on pcHDTV formatted major channel and program number
        // really old format, still used in freq_id, but we don't check that.
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND channum='%2-%3'")
        .arg(source_id).arg(major_channel).arg(program_number),
        // find based on DVB formatted major and minor channels
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND channum='%2%3'")
        .arg(source_id).arg(major_channel).arg(minor_channel),
        // find renamed channel, where atscsrcid is valid
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND atscsrcid=%2")
        .arg(source_id).arg(atsc_src_id),
    };

    for (uint i = 0; i < 4; i++)
    {
        query.prepare(qstr[i]);
        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Selecting channel/dtv_multiplex 3", query);
            return -1;
        }
        if (query.size())
        {
            query.next();
            return query.value(0).toInt();
        }
    }

    return -1;
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
        MythContext::DBError("Getting chanid for new channel (2)", query);
    else if (!query.next())
        VERBOSE(VB_IMPORTANT, "Error getting chanid for new channel.");
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
        MythContext::DBError("is_chan_id_available", query);
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
    int chansep = chan_num.find(QRegExp("\\D"));
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
                                int  freqid,
                                QString icon,
                                QString format,
                                QString xmltvid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString chanNum = (chan_num == "-1") ?
        QString::number(service_id) : chan_num;

    uint atsc_src_id = (atsc_major_channel << 8) | (atsc_minor_channel & 0xff);

    query.prepare(
        "INSERT INTO channel "
        "  (chanid,        channum,    sourceid,   callsign,  "
        "   name,          mplexid,    serviceid,  atscsrcid, "
        "   useonairguide, visible,    freqid,     tvformat,  "
        "   icon,          xmltvid) "
        "VALUES "
        "  (:CHANID,       :CHANNUM,   :SOURCEID,  :CALLSIGN,  "
        "   :NAME,         :MPLEXID,   :SERVICEID, :ATSCSRCID, "
        "   :USEOAG,       :VISIBLE,   :FREQID,    :TVFORMAT,  "
        "   :ICON,         :XMLTVID)");

    query.bindValue(":CHANID",    new_channel_id);
    query.bindValue(":CHANNUM",   chanNum);
    query.bindValue(":SOURCEID",  db_sourceid);
    query.bindValue(":CALLSIGN",  callsign.utf8());
    query.bindValue(":NAME",      service_name.utf8());

    if (db_mplexid > 0)
        query.bindValue(":MPLEXID",   db_mplexid);

    query.bindValue(":SERVICEID", service_id);
    query.bindValue(":ATSCSRCID", atsc_src_id);
    query.bindValue(":USEOAG",    use_on_air_guide);
    query.bindValue(":VISIBLE",   !hidden);
    (void) hidden_in_guide; // MythTV can't hide the channel in just the guide.

    if (freqid > 0)
        query.bindValue(":FREQID",    freqid);

    QString tvformat = (atsc_minor_channel > 0) ? "ATSC" : format;
    query.bindValue(":TVFORMAT", tvformat);

    query.bindValue(":ICON",      icon);
    query.bindValue(":XMLTVID",   xmltvid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Adding Service", query);
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
                                int freqid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    uint atsc_src_id = (atsc_major_channel << 8) | (atsc_minor_channel & 0xff);

    query.prepare("UPDATE channel "
                  "SET mplexid=:MPLEXID,     serviceid=:SERVICEID, "
                  "    atscsrcid=:ATSCSRCID, callsign=:CALLSIGN, "
                  "    name=:NAME,           channum=:CHANNUM, "
                  "    freqid=:FREQID,       tvformat=:TVFORMAT, "
                  "    sourceid=:SOURCEID "
                  "WHERE chanid=:CHANID");

    query.bindValue(":MPLEXID",   db_mplexid);
    query.bindValue(":SERVICEID", service_id);
    query.bindValue(":ATSCSRCID", atsc_src_id);
    query.bindValue(":CALLSIGN",  callsign.utf8());
    query.bindValue(":NAME",      service_name.utf8());
    query.bindValue(":SOURCEID",  source_id);
    query.bindValue(":CHANID",    channel_id);
    if (chan_num != "-1")
        query.bindValue(":CHANNUM",   chan_num);
    if (freqid > 0)
        query.bindValue(":FREQID",    freqid);
    if (atsc_major_channel > 0)
        query.bindValue(":TVFORMAT",  "ATSC");

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Updating Service", query);
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
        MythContext::DBError("Selecting channel/dtv_multiplex", query);
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
        MythContext::DBError("Selecting channel/dtv_multiplex", query);
        return false;
    }

    if (query.size() > 0)
    {
        query.next();
        return query.value(0).toInt();
    }
    return -1;
}

QString ChannelUtil::GetDTVPrivateType(uint network_id,
                                       const QString &key,
                                       const QString table_standard)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString theQuery = QString(
        "SELECT private_value "
        "FROM dtv_privatetypes "
        "WHERE networkid = %1 AND sitype = '%2' AND private_type = '%3'")
        .arg(network_id).arg(table_standard).arg(key);

    query.prepare(theQuery);

    if (!query.exec() && !query.isActive())
    {
        MythContext::DBError(
            QString("Error loading DTV private type %1").arg(key), query);

        return QString::null;
    }

    if (query.size() <= 0)
        return QString::null;

    return query.value(0).toString();
}
