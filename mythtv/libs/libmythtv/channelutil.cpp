// -*- Mode: c++ -*-

#include <algorithm>
#include <set>
using namespace std;

#include <qdeepcopy.h>
#include <qregexp.h>
#include <stdint.h>

#include "channelutil.h"
#include "mythdbcon.h"
#include "dvbtables.h"
#include "tv.h" // for CHANNEL_DIRECTION

#define LOC QString("ChanUtil: ")
#define LOC_ERR QString("ChanUtil, Error: ")

DBChannel::DBChannel(const DBChannel &other)
{
    (*this) = other;
}

DBChannel& DBChannel::operator=(const DBChannel &other)
{
    channum    = QDeepCopy<QString>(other.channum);
    callsign   = QDeepCopy<QString>(other.callsign);
    chanid     = other.chanid;
    major_chan = other.major_chan;
    minor_chan = other.minor_chan;
    favorite   = other.favorite;
    visible    = other.visible;
    name       = QDeepCopy<QString>(other.name);
    icon       = QDeepCopy<QString>(other.icon);

    return *this;
}

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
    int mplex = get_dtv_multiplex(
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
        int mux = ChannelUtil::GetMplexID(sourceid, tsid, netid);
        if (mux > 0)
        {
            QString dummy_mod;
            uint dummy_tsid, dummy_netid;
            ChannelUtil::GetTuningParams(mux, dummy_mod, freq,
                                         dummy_tsid, dummy_netid);
        }

        mux = ChannelUtil::CreateMultiplex(
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
            QChar(cd.PolarizationString()[0]), 'a',
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
        MythContext::DBError("GetMplexID 0", query);
    else if (query.next())
        return query.value(0).toInt();

    return 0;
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

bool ChannelUtil::GetTuningParams(uint      mplexid,
                                  QString  &modulation,
                                  uint64_t &frequency,
                                  uint     &dvb_transportid,
                                  uint     &dvb_networkid)
{
    if (!mplexid || (mplexid == 32767)) /* 32767 deals with old lineups */
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT transportid, networkid, frequency, modulation "
        "FROM dtv_multiplex "
        "WHERE mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetTuningParams failed ", query);
        return false;
    }

    if (!query.next())
        return false;

    dvb_transportid = query.value(0).toUInt();
    dvb_networkid   = query.value(1).toUInt();
    frequency       = (uint64_t) query.value(2).toDouble(); // Qt 3.1 compat
    modulation      = query.value(3).toString();

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
 *  NOTE: This is BROKEN, it does not specify which card the input is on.
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
        MythContext::DBError("getchannelvalue", query);
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
        MythContext::DBError("getchannelvalue", query);
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

    return old_mplexid == new_mplexid;
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
    if (!query.exec() || !query.isActive() || !query.size())
    {
        MythContext::DBError("Selecting channel/dtv_multiplex 2", query);
        return -1;
    }
    if (!query.next())
        return -1;

    int source_id = query.value(0).toInt();

    QString qstr[] = 
    {
        // find a proper ATSC channel
        QString("SELECT chanid FROM channel,dtv_multiplex "
                "WHERE channel.sourceid          = %1 AND "
                "      atsc_major_chan           = %2 AND "
                "      atsc_minor_chan           = %3 AND "
                "      dtv_multiplex.transportid = %4 AND "
                "      dtv_multiplex.mplexid     = %5 AND "
                "      dtv_multiplex.sourceid    = channel.sourceid AND "
                "      dtv_multiplex.mplexid     = channel.mplexid")
        .arg(source_id).arg(major_channel).arg(minor_channel)
        .arg(service_transport_id).arg(mplexid),

        // Find manually inserted/edited channels in order of scariness.

        // find renamed channel, where atsc is valid
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND "
                "atsc_major_chan=%2 AND "
                "atsc_minor_chan=%3")
        .arg(source_id).arg(major_channel).arg(minor_channel),
        // find based on mpeg program number and mplexid alone
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND serviceID=%1 AND mplexid=%2")
        .arg(source_id).arg(program_number).arg(mplexid),
        // find based on OLD pcHDTV formatted major and minor channels
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND channum='%2_%3'")
        .arg(source_id).arg(major_channel).arg(minor_channel),
        // find based on OLD pcHDTV formatted major channel and program number
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND channum='%2-%3'")
        .arg(source_id).arg(major_channel).arg(program_number),
        // find based on OLD DVB formatted major and minor channels
        QString("SELECT chanid FROM channel "
                "WHERE sourceid=%1 AND channum='%2%3'")
        .arg(source_id).arg(major_channel).arg(minor_channel),
    };

    for (uint i = 0; i < 6; i++)
    {
        query.prepare(qstr[i]);
        if (!query.exec() || !query.isActive())
            MythContext::DBError("Selecting channel/dtv_multiplex 3", query);
        if (query.next())
            return query.value(0).toInt();
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

    query.prepare(
        "INSERT INTO channel "
        "  (chanid,        channum,    sourceid,   callsign,  "
        "   name,          mplexid,    serviceid,             "
        "   atsc_major_chan,           atsc_minor_chan,       "
        "   useonairguide, visible,    freqid,     tvformat,  "
        "   icon,          xmltvid) "
        "VALUES "
        "  (:CHANID,       :CHANNUM,   :SOURCEID,  :CALLSIGN,  "
        "   :NAME,         :MPLEXID,   :SERVICEID,             "
        "   :MAJORCHAN,                :MINORCHAN,             "
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
    query.bindValue(":MAJORCHAN", atsc_major_channel);
    query.bindValue(":MINORCHAN", atsc_minor_channel);
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

    query.prepare(
        "UPDATE channel "
        "SET mplexid         = :MPLEXID,   serviceid       = :SERVICEID, "
        "    atsc_major_chan = :MAJORCHAN, atsc_minor_chan = :MINORCHAN, "
        "    callsign        = :CALLSIGN,  name            = :NAME,      "
        "    channum         = :CHANNUM,   freqid          = :FREQID,    "
        "    tvformat        = :TVFORMAT,  sourceid        = :SOURCEID   "
        "WHERE chanid=:CHANID");

    query.bindValue(":MPLEXID",   db_mplexid);
    query.bindValue(":SERVICEID", service_id);
    query.bindValue(":MAJORCHAN", atsc_major_channel);
    query.bindValue(":MINORCHAN", atsc_minor_channel);
    query.bindValue(":CALLSIGN",  callsign.utf8());
    query.bindValue(":NAME",      service_name.utf8());
    query.bindValue(":SOURCEID",  source_id);
    query.bindValue(":CHANID",    channel_id);
    if (chan_num != "-1")
        query.bindValue(":CHANNUM",   chan_num);
    if (freqid > 0)
        query.bindValue(":FREQID",    freqid);
    if (atsc_minor_channel > 0)
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
        MythContext::DBError("getatscchannel", query);
    else if (query.next())
    {
        major = query.value(0).toUInt();
        minor = query.value(1).toUInt();
        return true;
    }

    return false;
}

bool ChannelUtil::GetChannelData(
    uint    sourceid,         const QString &channum,
    QString &tvformat,        QString       &modulation,
    QString &freqtable,       QString       &freqid,
    int     &finetune,        uint64_t      &frequency,
    int     &mpeg_prog_num,
    uint    &atsc_major,      uint          &atsc_minor,
    uint    &dvb_transportid, uint          &dvb_networkid,
    uint    &mplexid,
    bool    &commfree)
{
    tvformat      = modulation = freqtable = freqid = QString::null;
    finetune      = 0;
    frequency     = 0;
    mpeg_prog_num = -1;
    atsc_major    = atsc_minor = mplexid = 0;
    commfree      = false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT finetune, freqid, tvformat, freqtable, "
        "       commfree, mplexid, "
        "       atsc_major_chan, atsc_minor_chan, serviceid "
        "FROM channel, videosource "
        "WHERE videosource.sourceid = channel.sourceid AND "
        "      channum              = :CHANNUM         AND "
        "      channel.sourceid     = :SOURCEID");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetChannelData", query);
        return false;
    }
    else if (!query.next())
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "GetChannelData() failed because it could not\n"
                    "\t\t\tfind channel number '%1' in DB for source '%2'.")
                .arg(channum).arg(sourceid));
        return false;
    }

    finetune      = query.value(0).toInt();
    freqid        = query.value(1).toString();
    tvformat      = query.value(2).toString();
    freqtable     = query.value(3).toString();
    commfree      = query.value(4).toBool();
    mplexid       = query.value(5).toUInt();
    atsc_major    = query.value(6).toUInt();
    atsc_minor    = query.value(7).toUInt();
    mpeg_prog_num = query.value(8).toUInt();

    if (!mplexid || (mplexid == 32767)) /* 32767 deals with old lineups */
        return true;

    return GetTuningParams(mplexid, modulation, frequency,
                           dvb_transportid, dvb_networkid);
}

DBChanList ChannelUtil::GetChannels(uint sourceid, bool vis_only, QString grp)
{
    DBChanList list;
    QMap<uint,uint> favorites;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT chanid, favid "
        "FROM favorites");
    if (!query.exec() || !query.isActive())
        MythContext::DBError("get channels -- favorites", query);
    else
    {
        while (query.next())
            favorites[query.value(0).toUInt()] = query.value(1).toUInt();
    }

    QString qstr =
        "SELECT channum, callsign, chanid, "
        "       atsc_major_chan, atsc_minor_chan, "
        "       name, icon, visible "
        "FROM channel ";

    if (sourceid)
        qstr += QString("WHERE sourceid='%1' ").arg(sourceid);
    else
        qstr += ",cardinput,capturecard "
            "WHERE cardinput.sourceid = channel.sourceid   AND "
            "      cardinput.cardid   = capturecard.cardid     ";

    if (vis_only)
        qstr += "AND visible=1 ";

    if (!grp.isEmpty())
        qstr += QString("GROUP BY %1 ").arg(grp);

    query.prepare(qstr);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("get channels -- sourceid", query);
        return list;
    }

    while (query.next())
    {
        if (query.value(0).toString().isEmpty() || !query.value(2).toUInt())
            continue; // skip if channum blank, or chanid empty

        DBChannel chan(
            query.value(0).toString(),                    /* channum    */
            QString::fromUtf8(query.value(1).toString()), /* callsign   */
            query.value(2).toUInt(),                      /* chanid     */
            query.value(3).toUInt(),                      /* ATSC major */
            query.value(4).toUInt(),                      /* ATSC minor */
            favorites[query.value(2).toUInt()],           /* favid      */
            query.value(7).toBool(),                      /* visible    */
            QString::fromUtf8(query.value(5).toString()), /* name       */
            query.value(6).toString());                   /* icon       */

        list.push_back(chan);
    }

    return list;
}

inline bool lt_callsign(const DBChannel &a, const DBChannel &b)
{
    return QString::localeAwareCompare(a.callsign, b.callsign) < 0;
}

inline bool lt_smart(const DBChannel &a, const DBChannel &b)
{
    int cmp = 0;

    bool isIntA, isIntB;
    int a_int = a.channum.toUInt(&isIntA);
    int b_int = b.channum.toUInt(&isIntB);

    // one of the channels is an ATSC channel, and the other
    // is either ATSC or is numeric.
    if ((a.minor_chan || b.minor_chan) &&
        (a.minor_chan || isIntA) && (b.minor_chan || isIntB))
    {
        int a_maj = (!a.minor_chan && isIntA) ? a_int : a.major_chan;
        int b_maj = (!b.minor_chan && isIntB) ? b_int : b.major_chan;
        if ((cmp = a_maj - b_maj))
            return cmp < 0;
        
        if ((cmp = a.minor_chan - b.minor_chan))
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
        // one of channels does not have a numeric channum
        cmp = QString::localeAwareCompare(a.channum, b.channum);
        if (cmp)
            return cmp < 0;
    }

    return lt_callsign(a,b);
}

void ChannelUtil::SortChannels(DBChanList &list, const QString &order,
                               bool eliminate_duplicates)
{
    bool cs = order.lower() == "callsign";
    if (cs)
        stable_sort(list.begin(), list.end(), lt_callsign);
    else /* if (sortorder == "channum") */
        stable_sort(list.begin(), list.end(), lt_smart);

    if (eliminate_duplicates && !list.empty())
    {
        DBChanList tmp;
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

void ChannelUtil::EliminateDuplicateChanNum(DBChanList &list)
{
    typedef std::set<QString> seen_set;
    seen_set seen;

    DBChanList::iterator it = list.begin();

    while (it != list.end())
    {
        QString tmp = QDeepCopy<QString>(it->channum);
        std::pair<seen_set::iterator, bool> insret = seen.insert(tmp);
        if (insret.second)
            ++it;
        else
            it = list.erase(it);
    }
}

uint ChannelUtil::GetNextChannel(const DBChanList &sorted,
                                 uint old_chanid, int direction)
{
    DBChanList::const_iterator it =
        find(sorted.begin(), sorted.end(), old_chanid);

    if (it == sorted.end())
        it = sorted.begin(); // not in list, pretend we are on first channel

    if (it == sorted.end())
        return 0; // no channels..

    DBChanList::const_iterator start = it;
    bool skip_non_visible = true; // TODO make DB selectable

    if (CHANNEL_DIRECTION_DOWN == direction)
    {
        do
        {
            if (it == sorted.begin())
                it = find(sorted.begin(), sorted.end(),
                          sorted.rbegin()->chanid);
            else
                it--;
        } while ((it != start) && skip_non_visible && !it->visible);

    }
    else if (CHANNEL_DIRECTION_UP == direction)
    {
        do
        {
            it++;
            if (it == sorted.end())
                it = sorted.begin();
        } while ((it != start) && skip_non_visible && !it->visible);
    }
    else if (CHANNEL_DIRECTION_FAVORITE == direction)
    {
        do
        {
            it++;
            if (it == sorted.end())
                it = sorted.begin();

        } while ((it != start) &&
                 (!it->favorite || (skip_non_visible && !it->visible)));
    }

    return it->chanid;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
