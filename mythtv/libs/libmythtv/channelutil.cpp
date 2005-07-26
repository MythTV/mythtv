// -*- Mode: c++ -*-

#include "channelutil.h"
#include "mythdbcon.h"
#include "dvbtables.h"

static bool insert_dtv_multiplex(int db_source_id, QString sistandard,
                                 uint frequency,   QString modulation,
                                 // DVB specific
                                 int transport_id,      int network_id,
                                 bool set_odfm_info,
                                 int symbol_rate,       char bandwidth,
                                 char polarity,         char inversion,
                                 char trans_mode,
                                 QString inner_FEC,     QString constellation,
                                 char hierarchy,        QString hp_code_rate,
                                 QString lp_code_rate,  QString guard_interval)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // If transport not present add it, and move on to the next
    query.prepare(
        "INSERT INTO dtv_multiplex "
        "  (transportid, networkid, frequency, symbolrate, "
        "   fec, polarity, modulation, constellation, bandwidth, "
        "   hierarchy, hp_code_rate, lp_code_rate, guard_interval, "
        "   transmission_mode, inversion, sourceid, sistandard) "
        "VALUES "
        "  (:TRANSPORTID, :NETWORKID, :FREQUENCY, :SYMBOLRATE, "
        "   :FEC, :POLARITY, :MODULATION, :CONSTELLATION, :BANDWIDTH, "
        "   :HIERARCHY, :HP_CODE_RATE, :LP_CODE_RATE, :GUARD_INTERVAL, "
        "   :TRANS_MODE, :INVERSION, :SOURCEID, :SISTANDARD);");

    query.bindValue(":SOURCEID",       db_source_id);
    if (transport_id > 0)
        query.bindValue(":TRANSPORTID",transport_id);
    if (network_id > 0)
        query.bindValue(":NETWORKID",  network_id);
    query.bindValue(":FREQUENCY",      frequency);
    if (modulation != QString::null)
        query.bindValue(":MODULATION", modulation);
    query.bindValue(":SISTANDARD",     sistandard);

    if (symbol_rate >= 0)
        query.bindValue(":SYMBOLRATE", symbol_rate);
    if (polarity >= 0)
        query.bindValue(":POLARITY",   QString("%1").arg(polarity));
    if (inner_FEC != QString::null)
        query.bindValue(":FEC",        inner_FEC);

    if (set_odfm_info)
    {
        query.bindValue(":INVERSION",      QString("%1").arg(inversion));
        query.bindValue(":BANDWIDTH",      QString("%1").arg(bandwidth));
        query.bindValue(":HP_CODE_RATE",   hp_code_rate);
        query.bindValue(":LP_CODE_RATE",   lp_code_rate);
        query.bindValue(":CONSTELLATION",  constellation);
        query.bindValue(":TRANS_MODE",     QString("%1").arg(trans_mode));
        query.bindValue(":GUARD_INTERVAL", guard_interval);
        query.bindValue(":HIERARCHY",      QString("%1").arg(hierarchy));
    }

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Adding transport to Database.", query);
        return false;
    }

    return true;
}

static int get_max_mplex_id()
{
    // Query for mplexid of new multiplex
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT max(mplexid) FROM dtv_multiplex;");

    if (!query.exec() || !query.isActive() || !query.size())
    {
        MythContext::DBError("Getting multiplex ID of new Transport", query);
        return -1;
    }

    query.next();
    return query.value(0).toInt();
}

int ChannelUtil::CreateMultiplex(int sourceid,   const QString &sistandard,
                                 uint frequency, const QString &modulation)
{
    insert_dtv_multiplex(
        sourceid,                   sistandard,
        frequency,                  modulation,
        -1,                         -1,
        false,
        -1,                         -1,
        -1,                         -1,
        -1,
        QString::null, QString::null,
        -1,            QString::null,
        QString::null, QString::null);
    
    return get_max_mplex_id();
}

int ChannelUtil::CreateMultiplex(int sourceid, const QString &sistandard,
                                 uint freq,    const QString &modulation,
                                 // DVB specific
                                 int transport_id,      int network_id,
                                 bool set_odfm_info,
                                 int symbol_rate,       signed char bandwidth,
                                 signed char polarity,  signed char inversion,
                                 signed char trans_mode,
                                 QString inner_FEC,     QString constellation,
                                 signed char hierarchy, QString hp_code_rate,
                                 QString lp_code_rate,  QString guard_interval)
{
    insert_dtv_multiplex(sourceid,           sistandard,
                         freq,               modulation,
                         // DVB specific
                         transport_id,       network_id,
                         set_odfm_info,
                         symbol_rate,        bandwidth,
                         polarity,           inversion,
                         trans_mode,
                         inner_FEC,          constellation,
                         hierarchy,          hp_code_rate,
                         lp_code_rate,       guard_interval);

    return get_max_mplex_id();
}

/** \fn CreateMultiplexes(int, const NetworkInformationTable*)
 *
 */
vector<int> ChannelUtil::CreateMultiplexes(int sourceid,
                                           const NetworkInformationTable *nit)
{
    vector<int> muxes;
    for (uint i = 0; i < nit->TransportStreamCount(); ++i)
    {        
        const desc_list_t& list = 
            MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                  nit->TransportDescriptorsLength(i));

        uint tsid  = nit->TSID(i);
        uint netid = nit->OriginalNetworkID(i);
        uint tag   = MPEGDescriptor(list[i]).DescriptorTag();

        if (tag == DescriptorID::terrestrial_delivery_system)
        {
            TerrestrialDeliverySystemDescriptor cd(list[i]);
            int mux = ChannelUtil::CreateMultiplex(
                sourceid,            "dvb",
                cd.FrequencyHz(),     QString::null,
                // DVB specific
                tsid,                 netid,
                true,
                -1,                   QChar(cd.BandwidthString()[0]),
                -1,                   -1,
                QChar(cd.TransmissionModeString()[0]),
                QString::null,        cd.ConstellationString(),
                QChar(cd.HierarchyString()[0]), cd.CodeRateHPString(),
                cd.CodeRateLPString(),cd.GuardIntervalString());

            if (mux >= 0)
                muxes.push_back(mux);

            /* unused
            HighPriority()
            IsTimeSlicingIndicatorUsed()
            IsMPE_FECUsed()
            NativeInterleaver()
            Alpha()
            OtherFrequencyInUse()
            */
        }
        else if (tag == DescriptorID::satellite_delivery_system)
        {
            SatelliteDeliverySystemDescriptor cd(list[i]);
            int mux = ChannelUtil::CreateMultiplex(
                sourceid,             "dvb",
                cd.FrequencyHz(),     cd.ModulationString(),
                // DVB specific
                tsid,                 netid,
                false,
                cd.SymbolRateHz(),    -1,
                QChar(cd.PolarizationString()[0]), -1,
                -1,
                cd.FECInnerString(),  QString::null,
                -1,                   QString::null,
                QString::null,        QString::null);

            if (mux >= 0)
                muxes.push_back(mux);

            /* unused
              OrbitalPositionString() == OrbitalLocation
             */
        }
        else if (tag == DescriptorID::cable_delivery_system)
        {
            CableDeliverySystemDescriptor cd(list[i]);
            int mux = ChannelUtil::CreateMultiplex(
                sourceid,             "dvb",
                cd.FrequencyHz(),     cd.ModulationString(),
                // DVB specific
                tsid,                 netid,
                false,
                cd.SymbolRateHz(),    -1,
                -1,                   -1,
                -1,
                cd.FECInnerString(),  QString::null,
                -1,                   QString::null,
                QString::null,        QString::null);

            if (mux >= 0)
                muxes.push_back(mux);
        }
        else if (tag == DescriptorID::frequency_list)
        {
            FrequencyListDescriptor cd(list[i]);
            //uint ct = cd.CodingType(); //nd,sat,cable,terra
            for (uint i = 0; i<cd.FrequencyCount(); i++)
            {
                int mux = ChannelUtil::CreateMultiplex(
                    sourceid,             "dvb",
                    cd.FrequencyHz(i),    QString::null/*modulation*/,
                    // DVB specific
                    tsid,                 netid,
                    false,
                    -1,                   -1,
                    -1,                   -1,
                    -1,
                    QString::null,        QString::null,
                    -1,                   QString::null,
                    QString::null,        QString::null);

                if (mux >= 0)
                    muxes.push_back(mux);
            }
        }
    }
    return muxes;
}

int ChannelUtil::GetMplexID(int sourceid, uint freq)
{
    MSqlQuery query(MSqlQuery::InitCon());
    /* See if mplexid is already in the database */
    QString theQuery = QString("SELECT mplexid FROM dtv_multiplex "
                               "WHERE sourceid = %1 AND frequency = %2")
        .arg(sourceid).arg(freq);

    query.prepare(theQuery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Check for existing transport", query);

    if (query.size() <= 0)
        return -1;

    query.next();
    return query.value(0).toInt();
}

int ChannelUtil::GetMplexID(int sourceid, uint frequency,
                            int transport_id, int network_id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // See if transport already in database
    query.prepare("SELECT mplexid FROM dtv_multiplex "
                  "WHERE networkid=:NETWORKID AND transportid=:TRANSPORTID AND "
                  "      frequency=:FREQUENCY AND sourceid=:SOURCEID");

    query.bindValue(":SOURCEID",    sourceid);
    query.bindValue(":NETWORKID",   network_id);
    query.bindValue(":TRANSPORTID", transport_id);
    query.bindValue(":FREQUENCY",   frequency);
    
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Selecting transports", query);
        return -1;
    }
    if (query.size())
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
        return current_mplexid;

    // Not in DB at all, insert it
    if (!q_networkid && !q_transportid)
    {
        query.prepare(QString("UPDATE dtv_multiplex "
                              "SET networkid = %1, transportid = %2 "
                              "WHERE mplexid = %3")
                      .arg(network_id).arg(transport_id).arg(current_mplexid));

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Getting mplexid global search", query);

        return current_mplexid;
    }

    // We have a partial match, so we try to do better...
    QString theQueries[2] =
    {
        QString("SELECT a.mplexid "
                "FROM dtv_multiplex a, dtv_multiplex b "
                "WHERE a.networkid   = %1 AND "
                "      a.transportid = %2 AND "
                "      a.sourceId    = b.sourceID AND "
                "      b.mplexid     = %3")
        .arg(network_id).arg(transport_id).arg(current_mplexid),

        QString("SELECT mplexid "
                "FROM dtv_multiplex "
                "WHERE networkID = %1 AND "
                "      transportID = %2")
        .arg(network_id).arg(transport_id),
    };

    for (uint i=0; i<2; i++)
    {
        query.prepare(theQueries[i]);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Finding matching mplexid", query);

        if (query.size() == 1)
        {
            query.next();
            return query.value(0).toInt();
        }

        if (query.size() > 1)
        {
            // Now you are in trouble.. You found more than 1 match, so
            // just guess that it is the first entry in the database..S
            VERBOSE(VB_IMPORTANT,
                    QString("Found more than 1 match for network_id %1 "
                            "transport_id %2").arg(network_id).arg(transport_id));
            query.next();
            return (i==0) ? current_mplexid : query.value(0).toInt();
        }
    }

    // If you still didn't find this combo return -1 (failure)
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

QString ChannelUtil::GetChanNum(int chan_id)
{
    if (chan_id < 0)
        return QString::null;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT channum FROM channel "
                          "WHERE chanid=%1").arg(chan_id));
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

/** \fn ChannelUtil::CreateChanID(uint, const QString&)
 *  \brief Creates a unique channel ID for database use.
 *  \return chanid if successful, -1 if not
 */
int ChannelUtil::CreateChanID(uint sourceid, const QString &chan_num)
{
    MSqlQuery query(MSqlQuery::DDCon());

    uint desired_chanid = 0;
    int chansep = chan_num.find("_");
    if (chansep > 0)
    {
        desired_chanid =
            sourceid * 1000 +
            chan_num.left(chansep).toInt() * 10 +
            chan_num.right(chan_num.length()-chansep-1).toInt();
    }
    else
    {
        desired_chanid = sourceid * 1000 + chan_num.toInt();
    }

    if (desired_chanid > sourceid * 1000)
    {
        query.prepare(
            QString("SELECT chanid FROM channel "
                    "WHERE chanid = '%1'").arg(desired_chanid));
        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Getting chanid for new channel (1)", query);
            return -1;
        }
        if (query.size() == 0)
            return desired_chanid;
    }

    query.prepare("SELECT MAX(chanid) FROM channel "
                  "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Getting chanid for new channel (2)", query);
        return -1;
    }
    if (!query.next())
    {
        VERBOSE(VB_IMPORTANT, "Error getting chanid for new channel.");
        return -1;
    }
    return query.value(0).toInt() + 1;
}

bool ChannelUtil::CreateChannel(uint db_sourceid,
                                uint new_channel_id,
                                const QString &callsign,
                                const QString &service_name,
                                const QString &chan_num,
                                uint atsc_major_channel,
                                uint atsc_minor_channel,
                                int  freqid,
                                const QString &xmltvid,
                                const QString &tvformat)
{
    uint atsc_src_id = (atsc_major_channel << 8) | (atsc_minor_channel & 0xff);

    MSqlQuery query(MSqlQuery::DDCon());
    query.prepare(
        "INSERT INTO channel "
        "       (chanid,    channum,   sourceid, "
        "        callsign,  name,      xmltvid, "
        "        freqid,    tvformat,  atscsrcid) "
        "VALUES (:CHANID,   :CHANNUM,  :SOURCEID, "
        "        :CALLSIGN, :NAME,     :XMLTVID, "
        "        :FREQID,   :TVFORMAT, :ATSCSRCID)");

    query.bindValue(":CHANID",    new_channel_id);
    query.bindValue(":CHANNUM",   chan_num);
    query.bindValue(":SOURCEID",  db_sourceid);
    query.bindValue(":CALLSIGN",  callsign);
    query.bindValue(":NAME",      service_name);
    query.bindValue(":XMLTVID",   xmltvid);
    query.bindValue(":FREQID",    freqid);
    query.bindValue(":TVFORMAT",  tvformat);
    query.bindValue(":ATSCSRCID", atsc_src_id);
       
    if (!query.exec())
    {
        MythContext::DBError("Inserting new channel", query);
        return false;
    }
    return true;
}

bool ChannelUtil::CreateChannel(uint db_mplexid,
                                uint db_sourceid,
                                uint new_channel_id,
                                const QString &service_name,
                                const QString &chan_num,
                                uint service_id,
                                uint atsc_major_channel,
                                uint atsc_minor_channel,
                                bool use_on_air_guide,
                                bool hidden,
                                bool hidden_in_guide,
                                int  freqid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString chanNum = (chan_num == "-1") ?
        QString::number(service_id) : chan_num;

    uint atsc_src_id = (atsc_major_channel << 8) | (atsc_minor_channel & 0xff);

    query.prepare(
        "INSERT INTO channel "
        "  (chanid,        channum,    sourceid,   callsign, "
        "   name,          mplexid,    serviceid,  atscsrcid, "
        "   useonairguide, visible,    freqid,     tvformat) "
        "VALUES "
        "  (:CHANID,       :CHANNUM,   :SOURCEID,  :CALLSIGN, "
        "   :NAME,         :MPLEXID,   :SERVICEID, :ATSCSRCID, "
        "   :USEOAG,       :VISIBLE,   :FREQID,    :TVFORMAT)");

    query.bindValue(":CHANID",    new_channel_id);
    query.bindValue(":CHANNUM",   chanNum);
    query.bindValue(":SOURCEID",  db_sourceid);
    query.bindValue(":CALLSIGN",  service_name.utf8());
    query.bindValue(":NAME",      service_name.utf8());

    query.bindValue(":MPLEXID",   db_mplexid);
    query.bindValue(":SERVICEID", service_id);
    query.bindValue(":ATSCSRCID", atsc_src_id);
    query.bindValue(":USEOAG",    use_on_air_guide);
    query.bindValue(":VISIBLE",   !(hidden || hidden_in_guide));
    if (freqid > 0)
        query.bindValue(":FREQID",    freqid);
    if (atsc_major_channel > 0)
        query.bindValue(":TVFORMAT",  "atsc");

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
    query.bindValue(":CALLSIGN",  service_name.utf8());
    query.bindValue(":NAME",      service_name.utf8());
    query.bindValue(":SOURCEID",  source_id);
    query.bindValue(":CHANID",    channel_id);
    if (chan_num != "-1")
        query.bindValue(":CHANNUM",   chan_num);
    if (freqid > 0)
        query.bindValue(":FREQID",    freqid);
    if (atsc_major_channel > 0)
        query.bindValue(":TVFORMAT",  "atsc");

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
