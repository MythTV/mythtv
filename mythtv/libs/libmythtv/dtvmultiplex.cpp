// MythTV headers
#include "dtvmultiplex.h"

#include "mpeg/dvbdescriptors.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "cardutil.h"
#include <utility>

#define LOC      QString("DTVMux: ")

bool DTVMultiplex::operator==(const DTVMultiplex &m) const
{
    return ((m_frequency      == m.m_frequency) &&
            (m_modulation     == m.m_modulation) &&
            (m_inversion      == m.m_inversion) &&
            (m_bandwidth      == m.m_bandwidth) &&
            (m_hp_code_rate   == m.m_hp_code_rate) &&
            (m_lp_code_rate   == m.m_lp_code_rate) &&
            (m_trans_mode     == m.m_trans_mode) &&
            (m_guard_interval == m.m_guard_interval) &&
            (m_fec            == m.m_fec) &&
            (m_mod_sys        == m.m_mod_sys)  &&
            (m_rolloff        == m.m_rolloff)  &&
            (m_polarity       == m.m_polarity) &&
            (m_hierarchy      == m.m_hierarchy) &&
            (m_iptv_tuning    == m.m_iptv_tuning)
            );
}

///////////////////////////////////////////////////////////////////////////////
// Gets

QString DTVMultiplex::toString() const
{
    QString ret = QString("%1 %2 %3 ")
        .arg(m_frequency).arg(m_modulation.toString()).arg(m_inversion.toString());

    ret += QString("%1 %2 %3 %4 %5 %6 %7")
        .arg(m_hp_code_rate.toString()).arg(m_lp_code_rate.toString())
        .arg(m_bandwidth.toString()).arg(m_trans_mode.toString())
        .arg(m_guard_interval.toString()).arg(m_hierarchy.toString())
        .arg(m_polarity.toString());
    ret += QString(" fec: %1 msys: %2 rolloff: %3")
        .arg(m_fec.toString()).arg(m_mod_sys.toString()).arg(m_rolloff.toString());

    return ret;
}

bool DTVMultiplex::IsEqual(DTVTunerType type, const DTVMultiplex &other,
                           uint freq_range, bool fuzzy) const
{
    if ((m_frequency + freq_range  < other.m_frequency             ) ||
        (m_frequency               > other.m_frequency + freq_range))
    {
        return false;
    }

    if (DTVTunerType::kTunerTypeDVBC == type)
    {
        if (fuzzy)
            return
                m_inversion.IsCompatible(other.m_inversion)   &&
                (m_symbolrate == other.m_symbolrate)          &&
                m_fec.IsCompatible(other.m_fec)               &&
                m_modulation.IsCompatible(other.m_modulation);
        return
            (m_inversion  == other.m_inversion)  &&
            (m_symbolrate == other.m_symbolrate) &&
            (m_fec        == other.m_fec)        &&
            (m_modulation == other.m_modulation);
    }

    if ((DTVTunerType::kTunerTypeDVBT == type) ||
        (DTVTunerType::kTunerTypeDVBT2 == type))
    {
        if (fuzzy)
            return
                m_inversion.IsCompatible(other.m_inversion)           &&
                m_bandwidth.IsCompatible(other.m_bandwidth)           &&
                m_hp_code_rate.IsCompatible(other.m_hp_code_rate)     &&
                m_lp_code_rate.IsCompatible(other.m_lp_code_rate)     &&
                m_modulation.IsCompatible(other.m_modulation)         &&
                m_guard_interval.IsCompatible(other.m_guard_interval) &&
                m_trans_mode.IsCompatible(other.m_trans_mode)         &&
                m_hierarchy.IsCompatible(other.m_hierarchy)           &&
                m_mod_sys.IsCompatible(other.m_mod_sys);
        return
            (m_inversion      == other.m_inversion)      &&
            (m_bandwidth      == other.m_bandwidth)      &&
            (m_hp_code_rate   == other.m_hp_code_rate)   &&
            (m_lp_code_rate   == other.m_lp_code_rate)   &&
            (m_modulation     == other.m_modulation)     &&
            (m_guard_interval == other.m_guard_interval) &&
            (m_trans_mode     == other.m_trans_mode)     &&
            (m_hierarchy      == other.m_hierarchy)      &&
            (m_mod_sys        == other.m_mod_sys);
    }

    if (DTVTunerType::kTunerTypeATSC == type)
    {
        if (fuzzy)
            return m_modulation.IsCompatible(other.m_modulation);
        return (m_modulation == other.m_modulation);
    }

    if ((DTVTunerType::kTunerTypeDVBS1 == type) ||
        (DTVTunerType::kTunerTypeDVBS2 == type))
    {
        bool ret =
            (m_symbolrate == other.m_symbolrate)        &&
            (m_polarity   == other.m_polarity)          &&
            (m_mod_sys    == other.m_mod_sys);

        if (fuzzy)
            return ret &&
                m_inversion.IsCompatible(other.m_inversion) &&
                m_fec.IsCompatible(other.m_fec)             &&
                m_rolloff.IsCompatible(other.m_rolloff);
        return ret &&
            (m_inversion  == other.m_inversion)  &&
            (m_fec        == other.m_fec)        &&
            (m_rolloff    == other.m_rolloff);
    }

    if (DTVTunerType::kTunerTypeIPTV == type)
    {
        return (m_iptv_tuning == other.m_iptv_tuning);
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Parsers

bool DTVMultiplex::ParseATSC(const QString &_frequency,
                             const QString &_modulation)
{
    bool ok = true;
    m_frequency = _frequency.toULongLong(&ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to parse ATSC frequency %1").arg(_frequency));
        return false;
    }

    ok = m_modulation.Parse(_modulation);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to parse ATSC modulation %1").arg(_modulation));
    }
    return ok;
}

bool DTVMultiplex::ParseDVB_T(
    const QString &_frequency,   const QString &_inversion,
    const QString &_bandwidth,   const QString &_coderate_hp,
    const QString &_coderate_lp, const QString &_modulation,
    const QString &_trans_mode,  const QString &_guard_interval,
    const QString &_hierarchy)
{
    bool ok = m_inversion.Parse(_inversion);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Invalid inversion parameter '%1', falling back to 'auto'.")
                .arg(_inversion));
        ok = true;
    }

    ok &= m_mod_sys.Parse("DVB-T");
    ok &= m_bandwidth.Parse(_bandwidth);
    ok &= m_hp_code_rate.Parse(_coderate_hp);
    ok &= m_lp_code_rate.Parse(_coderate_lp);
    ok &= m_modulation.Parse(_modulation);
    ok &= m_trans_mode.Parse(_trans_mode);
    ok &= m_hierarchy.Parse(_hierarchy);
    ok &= m_guard_interval.Parse(_guard_interval);
    if (ok)
        m_frequency = _frequency.toInt(&ok);

    return ok;
}

bool DTVMultiplex::ParseDVB_S_and_C(
    const QString &_frequency,   const QString &_inversion,
    const QString &_symbol_rate, const QString &_fec_inner,
    const QString &_modulation,  const QString &_polarity)
{
    bool ok = m_inversion.Parse(_inversion);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Invalid inversion parameter '%1', falling back to 'auto'.")
                .arg(_inversion));

        ok = true;
    }

    m_symbolrate = _symbol_rate.toInt();
    if (!m_symbolrate)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid symbol rate " +
            QString("parameter '%1', aborting.").arg(_symbol_rate));

        return false;
    }

    ok &= m_fec.Parse(_fec_inner);
    ok &= m_modulation.Parse(_modulation);

    if (!_polarity.isEmpty())
        m_polarity.Parse(_polarity.toLower());

    if (ok)
        m_frequency = _frequency.toInt(&ok);

    return ok;
}

bool DTVMultiplex::ParseDVB_S(
    const QString &_frequency,   const QString &_inversion,
    const QString &_symbol_rate, const QString &_fec_inner,
    const QString &_modulation,  const QString &_polarity)
{
    bool ok = ParseDVB_S_and_C(_frequency, _inversion, _symbol_rate,
                               _fec_inner, _modulation, _polarity);
    m_mod_sys = DTVModulationSystem::kModulationSystem_DVBS;
    return ok;
}

bool DTVMultiplex::ParseDVB_C(
    const QString &_frequency,   const QString &_inversion,
    const QString &_symbol_rate, const QString &_fec_inner,
    const QString &_modulation,  const QString &_polarity,
    const QString &_mod_sys)
{
    bool ok = ParseDVB_S_and_C(_frequency, _inversion, _symbol_rate,
                               _fec_inner, _modulation, _polarity);

    m_mod_sys.Parse(_mod_sys);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED == m_mod_sys)
    {
        m_mod_sys = DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A;
    }

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("%1 ").arg(__FUNCTION__) +
        QString("_mod_sys:%1 ok:%2 ").arg(_mod_sys).arg(ok) +
        QString("m_mod_sys:%1 %2 ").arg(m_mod_sys).arg(m_mod_sys.toString()));

    if ((DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A != m_mod_sys) &&
        (DTVModulationSystem::kModulationSystem_DVBC_ANNEX_B != m_mod_sys) &&
        (DTVModulationSystem::kModulationSystem_DVBC_ANNEX_C != m_mod_sys))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unsupported DVB-C modulation system " +
            QString("parameter '%1', aborting.").arg(_mod_sys));
        return false;
    }

    return ok;
}

bool DTVMultiplex::ParseDVB_S2(
    const QString &_frequency,   const QString &_inversion,
    const QString &_symbol_rate, const QString &_fec_inner,
    const QString &_modulation,  const QString &_polarity,
    const QString &_mod_sys,     const QString &_rolloff)
{
    bool ok = ParseDVB_S_and_C(_frequency, _inversion, _symbol_rate,
                               _fec_inner, _modulation, _polarity);

    if (!m_mod_sys.Parse(_mod_sys))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid DVB-S2 modulation system " +
                QString("parameter '%1', aborting.").arg(_mod_sys));
        return false;
    }

    // For #10153, guess at modulation system based on modulation
    if (DTVModulationSystem::kModulationSystem_UNDEFINED == m_mod_sys)
    {
        m_mod_sys = (DTVModulation::kModulationQPSK == m_modulation) ?
            DTVModulationSystem::kModulationSystem_DVBS :
            DTVModulationSystem::kModulationSystem_DVBS2;
    }

    if ((DTVModulationSystem::kModulationSystem_DVBS  != m_mod_sys) &&
        (DTVModulationSystem::kModulationSystem_DVBS2 != m_mod_sys))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unsupported DVB-S2 modulation system " +
            QString("parameter '%1', aborting.").arg(_mod_sys));
        return false;
    }

    if (!_rolloff.isEmpty())
        ok &= m_rolloff.Parse(_rolloff);

    return ok;
}

bool DTVMultiplex::ParseDVB_T2(
    const QString &_frequency,   const QString &_inversion,
    const QString &_bandwidth,   const QString &_coderate_hp,
    const QString &_coderate_lp, const QString &_modulation,
    const QString &_trans_mode,  const QString &_guard_interval,
    const QString &_hierarchy,   const QString &_mod_sys)
{
    bool ok = ParseDVB_T(_frequency, _inversion, _bandwidth,
                         _coderate_hp, _coderate_lp, _modulation,
                         _trans_mode, _guard_interval, _hierarchy);

    QString l_mod_sys = _mod_sys;

    // Accept "0" for "DVB-T" and "1" for "DVB-T2"
    if (_mod_sys == "1")
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Invalid DVB-T2 modulation system " +
                QString("parameter '%1', using DVB-T2.").arg(_mod_sys));
        m_mod_sys = DTVModulationSystem::kModulationSystem_DVBT2;
        l_mod_sys = m_mod_sys.toString();
    }
    else if (_mod_sys == "0")
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Invalid DVB-T modulation system " +
                QString("parameter '%1', using DVB-T.").arg(_mod_sys));
        m_mod_sys = DTVModulationSystem::kModulationSystem_DVBT;
        l_mod_sys = m_mod_sys.toString();
    }

    if (!m_mod_sys.Parse(l_mod_sys))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Invalid DVB-T/T2 modulation system " +
                QString("parameter '%1', aborting.").arg(l_mod_sys));
        return false;
    }

    if (m_mod_sys == DTVModulationSystem::kModulationSystem_UNDEFINED)
    {
        m_mod_sys = DTVModulationSystem::kModulationSystem_DVBT;
    }

    if ((DTVModulationSystem::kModulationSystem_DVBT  != m_mod_sys) &&
        (DTVModulationSystem::kModulationSystem_DVBT2 != m_mod_sys))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unsupported DVB-T2 modulation system " +
            QString("parameter '%1', aborting.").arg(l_mod_sys));
        return false;
    }

    return ok;
}

bool DTVMultiplex::ParseTuningParams(
    DTVTunerType type,
    const QString& _frequency,    const QString& _inversion,      const QString& _symbolrate,
    const QString& _fec,          const QString& _polarity,
    const QString& _hp_code_rate, const QString& _lp_code_rate,   const QString& _ofdm_modulation,
    const QString& _trans_mode,   const QString& _guard_interval, const QString& _hierarchy,
    const QString& _modulation,   const QString& _bandwidth,
    const QString& _mod_sys,      const QString& _rolloff)
{
    if (DTVTunerType::kTunerTypeDVBT == type)
    {
        return ParseDVB_T(
            _frequency,       _inversion,       _bandwidth,
            _hp_code_rate,    _lp_code_rate,    _ofdm_modulation,
            _trans_mode,      _guard_interval,  _hierarchy);
    }

    if (DTVTunerType::kTunerTypeDVBC  == type)
    {
        return ParseDVB_C(
            _frequency,       _inversion,       _symbolrate,
            _fec,             _modulation,      _polarity,
            _mod_sys);
    }

    if (DTVTunerType::kTunerTypeDVBS1 == type)
    {
        return ParseDVB_S(
            _frequency,       _inversion,       _symbolrate,
            _fec,             _modulation,      _polarity);
    }

    if (DTVTunerType::kTunerTypeDVBS2 == type)
    {
        return ParseDVB_S2(
            _frequency,       _inversion,       _symbolrate,
            _fec,             _modulation,      _polarity,
            _mod_sys,         _rolloff);
    }

    if (DTVTunerType::kTunerTypeDVBT2 == type)
    {
        return ParseDVB_T2(
            _frequency,       _inversion,       _bandwidth,
            _hp_code_rate,    _lp_code_rate,    _ofdm_modulation,
            _trans_mode,      _guard_interval,  _hierarchy,
            _mod_sys);
    }

    if (DTVTunerType::kTunerTypeATSC == type)
    {
        return ParseATSC(_frequency, _modulation);
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("ParseTuningParams -- Unknown tuner type = 0x%1")
        .arg(type,0,16,QChar('0')));

    return false;
}

bool DTVMultiplex::FillFromDB(DTVTunerType type, uint mplexid)
{
    Clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT frequency,         inversion,      symbolrate, "
        "       fec,               polarity, "
        "       hp_code_rate,      lp_code_rate,   constellation, "
        "       transmission_mode, guard_interval, hierarchy, "
        "       modulation,        bandwidth,      sistandard, "
        "       mod_sys,           rolloff "
        "FROM dtv_multiplex "
        "WHERE dtv_multiplex.mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec())
    {
        MythDB::DBError("DVBTuning::FillFromDB", query);
        return false;
    }

    if (!query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not find tuning parameters for mplex %1")
                .arg(mplexid));

        return false;
    }

    m_mplex = mplexid;
    m_sistandard = query.value(13).toString();

    // Parse the query into our DVBTuning class
    return ParseTuningParams(
        type,
        query.value(0).toString(),  query.value(1).toString(),
        query.value(2).toString(),  query.value(3).toString(),
        query.value(4).toString(),  query.value(5).toString(),
        query.value(6).toString(),  query.value(7).toString(),
        query.value(8).toString(),  query.value(9).toString(),
        query.value(10).toString(), query.value(11).toString(),
        query.value(12).toString(), query.value(14).toString(),
        query.value(15).toString());
}


bool DTVMultiplex::FillFromDeliverySystemDesc(DTVTunerType type,
                                              const MPEGDescriptor &desc)
{
    uint tag = desc.DescriptorTag();
    m_sistandard = "dvb";

    switch (tag)
    {
        case DescriptorID::terrestrial_delivery_system:
        {
            const TerrestrialDeliverySystemDescriptor cd(desc);

            if (type == DTVTunerType::kTunerTypeDVBT)
            {
                return ParseDVB_T(
                    QString::number(cd.FrequencyHz()), "a",
                    cd.BandwidthString(),               cd.CodeRateHPString(),
                    cd.CodeRateLPString(),              cd.ConstellationString(),
                    cd.TransmissionModeString(),        cd.GuardIntervalString(),
                    cd.HierarchyString());
            }

            if (type == DTVTunerType::kTunerTypeDVBT2)
            {
                return ParseDVB_T2(
                    QString::number(cd.FrequencyHz()), "a",
                    cd.BandwidthString(),               cd.CodeRateHPString(),
                    cd.CodeRateLPString(),              cd.ConstellationString(),
                    cd.TransmissionModeString(),        cd.GuardIntervalString(),
                    cd.HierarchyString(),               "DVB-T");
            }

            break;
        }
        case DescriptorID::satellite_delivery_system:
        {
            const SatelliteDeliverySystemDescriptor cd(desc);

            if (type == DTVTunerType::kTunerTypeDVBS1)
            {
                if (cd.ModulationSystem())
                {
                    LOG(VB_CHANSCAN, LOG_NOTICE, LOC +
                        "Ignoring DVB-S2 transponder with DVB-S card");
                    return false;
                }

                return ParseDVB_S_and_C(
                    QString::number(cd.FrequencykHz()),  "a",
                    QString::number(cd.SymbolRateHz()), cd.FECInnerString(),
                    cd.ModulationString(),
                    cd.PolarizationString());
            }

            if (type == DTVTunerType::kTunerTypeDVBS2)
            {
                return ParseDVB_S2(
                    QString::number(cd.FrequencykHz()),  "a",
                    QString::number(cd.SymbolRateHz()), cd.FECInnerString(),
                    cd.ModulationString(),
                    cd.PolarizationString(),
                    cd.ModulationSystemString(),         cd.RollOffString());
            }

            break;
        }
        case DescriptorID::cable_delivery_system:
        {
            if (type != DTVTunerType::kTunerTypeDVBC)
                break;

            const CableDeliverySystemDescriptor cd(desc);

            return ParseDVB_C(
                    QString::number(cd.FrequencyHz()),  "a",
                    QString::number(cd.SymbolRateHz()), cd.FECInnerString(),
                    cd.ModulationString(),               QString(),
                    "DVB-C/A");
        }
        default:
            LOG(VB_CHANSCAN, LOG_ERR, LOC +
                "unknown delivery system descriptor");
            return false;
    }

    LOG(VB_CHANSCAN, LOG_ERR, LOC +
        QString("Tuner type %1 does not match delivery system")
            .arg(type.toString()));
    return false;
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool ScanDTVTransport::FillFromDB(DTVTunerType type, uint mplexid)
{
    if (!DTVMultiplex::FillFromDB(type, mplexid))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT c.mplexid,       c.sourceid,        c.chanid,          "
        "       c.callsign,      c.name,            c.channum,         "
        "       c.serviceid,     c.atsc_major_chan, c.atsc_minor_chan, "
        "       c.useonairguide, c.visible,         c.freqid,          "
        "       c.icon,          c.tvformat,        c.xmltvid,         "
        "       d.transportid,   d.networkid,       c.default_authority,"
        "       c.service_type "
        "FROM channel AS c, dtv_multiplex AS d "
        "WHERE c.deleted IS NULL AND "
        "      c.mplexid = :MPLEXID AND"
        "      c.mplexid = d.mplexid");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec())
    {
        MythDB::DBError("ScanDTVTransport::FillFromDB", query);
        return false;
    }

    while (query.next())
    {
        ChannelInsertInfo chan(
            query.value(0).toUInt(),     query.value(1).toUInt(),
            query.value(2).toUInt(),     query.value(3).toString(),
            query.value(4).toString(),   query.value(5).toString(),
            query.value(6).toUInt(),
            query.value(7).toUInt(),     query.value(8).toUInt(),
            query.value(9).toBool(),    !query.value(10).toBool(),
            false,
            query.value(11).toString(),  query.value(12).toString(),
            query.value(13).toString(),  query.value(14).toString(),
            0, 0, 0,
            query.value(15).toUInt(),    query.value(16).toUInt(),
            0,
            QString(),
            false, false, false, false,
            false, false, false, false,
            false, false, false, 0,
            query.value(17).toString(), /* default_authority */
            query.value(18).toUInt());  /* service_type */

        m_channels.push_back(chan);
    }

    return true;
}

uint ScanDTVTransport::SaveScan(uint scanid) const
{
    uint transportid = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "INSERT INTO channelscan_dtv_multiplex "
        " (  scanid, "
        "    mplexid,            frequency,       inversion,  "
        "    symbolrate,         fec,             polarity,   "
        "    hp_code_rate,       lp_code_rate,    modulation, "
        "    transmission_mode,  guard_interval,  hierarchy,  "
        "    mod_sys,            rolloff,                     "
        "    bandwidth,          sistandard,      tuner_type  "
        " ) "
        "VALUES "
        " ( :SCANID, "
        "   :MPLEXID,           :FREQUENCY,      :INVERSION,  "
        "   :SYMBOLRATE,        :FEC,            :POLARITY,   "
        "   :HP_CODE_RATE,      :LP_CODE_RATE,   :MODULATION, "
        "   :TRANSMISSION_MODE, :GUARD_INTERVAL, :HIERARCHY,  "
        "   :MOD_SYS,           :ROLLOFF,                     "
        "   :BANDWIDTH,         :SISTANDARD,     :TUNER_TYPE  "
        " );");

    query.bindValue(":SCANID", scanid);
    query.bindValue(":MPLEXID", m_mplex);
    query.bindValue(":FREQUENCY", QString::number(m_frequency));
    query.bindValue(":INVERSION", m_inversion.toString());
    query.bindValue(":SYMBOLRATE", QString::number(m_symbolrate));
    query.bindValue(":FEC", m_fec.toString());
    query.bindValue(":POLARITY", m_polarity.toString());
    query.bindValue(":HP_CODE_RATE", m_hp_code_rate.toString());
    query.bindValue(":LP_CODE_RATE", m_lp_code_rate.toString());
    query.bindValue(":MODULATION", m_modulation.toString());
    query.bindValue(":TRANSMISSION_MODE", m_trans_mode.toString());
    query.bindValue(":GUARD_INTERVAL", m_guard_interval.toString());
    query.bindValue(":HIERARCHY", m_hierarchy.toString());
    query.bindValue(":MOD_SYS", m_mod_sys.toString());
    query.bindValue(":ROLLOFF", m_rolloff.toString());
    query.bindValue(":BANDWIDTH", m_bandwidth.toString());
    query.bindValue(":SISTANDARD", m_sistandard);
    query.bindValue(":TUNER_TYPE", (uint)m_tuner_type);

    if (!query.exec())
    {
        MythDB::DBError("ScanDTVTransport::SaveScan 1", query);
        return transportid;
    }

    query.prepare("SELECT MAX(transportid) FROM channelscan_dtv_multiplex");
    if (!query.exec())
        MythDB::DBError("ScanDTVTransport::SaveScan 2", query);
    else if (query.next())
        transportid = query.value(0).toUInt();

    if (!transportid)
        return transportid;

    for (size_t i = 0; i < m_channels.size(); i++)
        m_channels[i].SaveScan(scanid, transportid);

    return transportid;
}

bool ScanDTVTransport::ParseTuningParams(
    DTVTunerType type,
    const QString& _frequency,    const QString& _inversion,      const QString& _symbolrate,
    const QString& _fec,          const QString& _polarity,
    const QString& _hp_code_rate, const QString& _lp_code_rate,   const QString& _ofdm_modulation,
    const QString& _trans_mode,   const QString& _guard_interval, const QString& _hierarchy,
    const QString& _modulation,   const QString& _bandwidth,      const QString& _mod_sys,
    const QString& _rolloff)
{
    m_tuner_type = type;

    return DTVMultiplex::ParseTuningParams(
        type,
        _frequency,     _inversion,       _symbolrate,
        _fec,           _polarity,
        _hp_code_rate,  _lp_code_rate,    _ofdm_modulation,
        _trans_mode,    _guard_interval,  _hierarchy,
        _modulation,    _bandwidth,       _mod_sys,
        _rolloff);
}
