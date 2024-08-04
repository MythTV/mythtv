#include <utility>

// MythTV headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "dtvmultiplex.h"
#include "mpeg/dvbdescriptors.h"
#include "cardutil.h"

#define LOC      QString("DTVMux: ")

bool DTVMultiplex::operator==(const DTVMultiplex &m) const
{
    return ((m_frequency      == m.m_frequency) &&
            (m_modulation     == m.m_modulation) &&
            (m_inversion      == m.m_inversion) &&
            (m_bandwidth      == m.m_bandwidth) &&
            (m_hpCodeRate     == m.m_hpCodeRate) &&
            (m_lpCodeRate     == m.m_lpCodeRate) &&
            (m_transMode      == m.m_transMode) &&
            (m_guardInterval  == m.m_guardInterval) &&
            (m_fec            == m.m_fec) &&
            (m_modSys         == m.m_modSys)  &&
            (m_rolloff        == m.m_rolloff)  &&
            (m_polarity       == m.m_polarity) &&
            (m_hierarchy      == m.m_hierarchy) &&
            (m_iptvTuning     == m.m_iptvTuning)
            );
}

///////////////////////////////////////////////////////////////////////////////
// Gets

QString DTVMultiplex::toString() const
{
    QString ret = QString("%1 %2 %3 %4")
        .arg(QString::number(m_frequency), m_modulation.toString(),
             m_inversion.toString(), QString::number(m_symbolRate));

    ret += QString(" %1 %2 %3 %4 %5 %6 %7")
        .arg(m_hpCodeRate.toString(),    m_lpCodeRate.toString(),
             m_bandwidth.toString(),     m_transMode.toString(),
             m_guardInterval.toString(), m_hierarchy.toString(),
             m_polarity.toString());
    ret += QString(" fec:%1 msys:%2 ro:%3")
        .arg(m_fec.toString(),-4).arg(m_modSys.toString(),-6).arg(m_rolloff.toString());

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
        {
            return
                m_inversion.IsCompatible(other.m_inversion)   &&
                (m_symbolRate == other.m_symbolRate)          &&
                m_fec.IsCompatible(other.m_fec)               &&
                m_modulation.IsCompatible(other.m_modulation);
        }
        return
            (m_inversion  == other.m_inversion)  &&
            (m_symbolRate == other.m_symbolRate) &&
            (m_fec        == other.m_fec)        &&
            (m_modulation == other.m_modulation);
    }

    if ((DTVTunerType::kTunerTypeDVBT == type) ||
        (DTVTunerType::kTunerTypeDVBT2 == type))
    {
        if (fuzzy)
        {
            return
                m_inversion.IsCompatible(other.m_inversion)           &&
                m_bandwidth.IsCompatible(other.m_bandwidth)           &&
                m_hpCodeRate.IsCompatible(other.m_hpCodeRate)     &&
                m_lpCodeRate.IsCompatible(other.m_lpCodeRate)     &&
                m_modulation.IsCompatible(other.m_modulation)         &&
                m_guardInterval.IsCompatible(other.m_guardInterval) &&
                m_transMode.IsCompatible(other.m_transMode)         &&
                m_hierarchy.IsCompatible(other.m_hierarchy)           &&
                m_modSys.IsCompatible(other.m_modSys);
        }
        return
            (m_inversion      == other.m_inversion)      &&
            (m_bandwidth      == other.m_bandwidth)      &&
            (m_hpCodeRate     == other.m_hpCodeRate)   &&
            (m_lpCodeRate     == other.m_lpCodeRate)   &&
            (m_modulation     == other.m_modulation)     &&
            (m_guardInterval  == other.m_guardInterval) &&
            (m_transMode      == other.m_transMode)     &&
            (m_hierarchy      == other.m_hierarchy)      &&
            (m_modSys         == other.m_modSys);
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
            (m_symbolRate == other.m_symbolRate)        &&
            (m_polarity   == other.m_polarity)          &&
            (m_modSys    == other.m_modSys);

        if (fuzzy)
        {
            return ret &&
                m_inversion.IsCompatible(other.m_inversion) &&
                m_fec.IsCompatible(other.m_fec)             &&
                m_rolloff.IsCompatible(other.m_rolloff);
        }
        return ret &&
            (m_inversion  == other.m_inversion)  &&
            (m_fec        == other.m_fec)        &&
            (m_rolloff    == other.m_rolloff);
    }

    if (DTVTunerType::kTunerTypeIPTV == type)
    {
        return (m_iptvTuning == other.m_iptvTuning);
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

    m_frequency = _frequency.toUInt(&ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid frequency parameter " + _frequency);
    }

    if (ok)
    {
        ok &= m_modSys.Parse("DVB-T");
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid modulation system parameter " + "DVB-T");
        }
    }

    if (ok)
    {
        ok &= m_bandwidth.Parse(_bandwidth);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid modulation system parameter " + _bandwidth);
        }
    }

    if (ok)
    {
        ok &= m_hpCodeRate.Parse(_coderate_hp);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid coderate_hp parameter " + _coderate_hp);
        }
    }

    if (ok)
    {
        ok &= m_lpCodeRate.Parse(_coderate_lp);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid coderate_lp parameter " + _coderate_lp);
        }
    }

    if (ok)
    {
        ok &= m_modulation.Parse(_modulation);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid modulation parameter " + _modulation);
        }
    }

    if (ok)
    {
        ok &= m_transMode.Parse(_trans_mode);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid transmission mode parameter " + _trans_mode);
        }
    }

    if (ok)
    {
        ok &= m_hierarchy.Parse(_hierarchy);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid hierarchy parameter " + _hierarchy);
        }
    }

    if (ok)
    {
        ok &= m_guardInterval.Parse(_guard_interval);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid guard interval parameter " + _guard_interval);
        }
    }

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

    m_symbolRate = _symbol_rate.toUInt();
    if (!m_symbolRate)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid symbol rate " +
            QString("parameter '%1', aborting.").arg(_symbol_rate));

        return false;
    }

    if (ok)
    {
        ok &= m_fec.Parse(_fec_inner);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid fec parameter " + _fec_inner);
        }
    }

    if (ok)
    {
        ok &= m_modulation.Parse(_modulation.toLower());
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid modulation parameter " + _modulation);
        }
    }

    if (ok)
    {
        if (!_polarity.isEmpty())
        {
            ok = m_polarity.Parse(_polarity.toLower());
            if (!ok)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid polarity parameter " + _polarity);
            }
        }
    }

    if (ok)
    {
        m_frequency = _frequency.toUInt(&ok);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid frequency parameter " + _frequency);
        }
    }

    return ok;
}

bool DTVMultiplex::ParseDVB_S(
    const QString &_frequency,   const QString &_inversion,
    const QString &_symbol_rate, const QString &_fec_inner,
    const QString &_modulation,  const QString &_polarity)
{
    bool ok = ParseDVB_S_and_C(_frequency, _inversion, _symbol_rate,
                               _fec_inner, _modulation, _polarity);
    m_modSys = DTVModulationSystem::kModulationSystem_DVBS;
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

    m_modSys.Parse(_mod_sys);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED == m_modSys)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Undefined modulation system " +
                QString("parameter '%1', using DVB-C/A.").arg(_mod_sys));
        m_modSys = DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A;
    }

    // Only DVB-C variants can be used with a DVB-C tuner.
    if ((DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A != m_modSys) &&
        (DTVModulationSystem::kModulationSystem_DVBC_ANNEX_C != m_modSys))
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
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "ParseDVB_S2" +
        " frequency:"   + _frequency +
        " inversion:"   + _inversion +
        " symbol_rate:" + _symbol_rate +
        " fec_inner:"   + _fec_inner +
        " modulation:"  + _modulation +
        " polarity:"    + _polarity +
        " mod_sys:"     + _mod_sys +
        " rolloff:"     + _rolloff);

    bool ok = ParseDVB_S_and_C(_frequency, _inversion, _symbol_rate,
                               _fec_inner, _modulation, _polarity);

    m_modSys = DTVModulationSystem::kModulationSystem_UNDEFINED;
    m_modSys.Parse(_mod_sys);

    // For #10153, guess at modulation system based on modulation
    if (DTVModulationSystem::kModulationSystem_UNDEFINED == m_modSys)
    {
        m_modSys = (DTVModulation::kModulationQPSK == m_modulation) ?
            DTVModulationSystem::kModulationSystem_DVBS :
            DTVModulationSystem::kModulationSystem_DVBS2;
    }

    // Only DVB-S and DVB_S2 can be used with a DVB-S2 tuner.
    if ((DTVModulationSystem::kModulationSystem_DVBS  != m_modSys) &&
        (DTVModulationSystem::kModulationSystem_DVBS2 != m_modSys))
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

    m_modSys = DTVModulationSystem::kModulationSystem_UNDEFINED;
    m_modSys.Parse(_mod_sys);

    // We have a DVB-T2 tuner, change undefined modulation system to DVB-T2
    if (DTVModulationSystem::kModulationSystem_UNDEFINED == m_modSys)
    {
        m_modSys = DTVModulationSystem::kModulationSystem_DVBT2;
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Undefined modulation system, " +
                QString("using %1.").arg(m_modSys.toString()));
    }

    // Only DVB-T and DVB-T2 can be used with a DVB-T2 tuner.
    if ((DTVModulationSystem::kModulationSystem_DVBT  != m_modSys) &&
        (DTVModulationSystem::kModulationSystem_DVBT2 != m_modSys))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unsupported DVB-T2 modulation system " +
            QString("%1, aborting.").arg(m_modSys.toString()));
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
    if ((DTVTunerType::kTunerTypeDVBT  == type) ||
        (DTVTunerType::kTunerTypeDVBT2 == type))
    {
        return ParseDVB_T2(
            _frequency,       _inversion,       _bandwidth,
            _hp_code_rate,    _lp_code_rate,    _ofdm_modulation,
            _trans_mode,      _guard_interval,  _hierarchy,
            _mod_sys);
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

                return ParseDVB_S(
                    QString::number(cd.FrequencykHz()),  "a",
                    QString::number(cd.SymbolRateHz()),
                    cd.FECInnerString(),
                    cd.ModulationString(),
                    cd.PolarizationString());
            }

            if (type == DTVTunerType::kTunerTypeDVBS2)
            {
                return ParseDVB_S2(
                    QString::number(cd.FrequencykHz()),  "a",
                    QString::number(cd.SymbolRateHz()),
                    cd.FECInnerString(),
                    cd.ModulationString(),
                    cd.PolarizationString(),
                    cd.ModulationSystemString(),
                    cd.RollOffString());
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
                QString("Unknown delivery system descriptor 0x%1").arg(tag,0,16));
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
            query.value(18).toUInt(),   /* service_type */
            0,                          /* logical_channel */
            0);                         /* simulcast_channel */

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
        "    mplexid,            frequency,       inversion,      "
        "    symbolrate,         fec,             polarity,       "
        "    hp_code_rate,       lp_code_rate,    modulation,     "
        "    transmission_mode,  guard_interval,  hierarchy,      "
        "    mod_sys,            rolloff,         bandwidth,      "
        "    sistandard,         tuner_type,      signal_strength "
        " ) "
        "VALUES "
        " ( :SCANID, "
        "   :MPLEXID,           :FREQUENCY,      :INVERSION,      "
        "   :SYMBOLRATE,        :FEC,            :POLARITY,       "
        "   :HP_CODE_RATE,      :LP_CODE_RATE,   :MODULATION,     "
        "   :TRANSMISSION_MODE, :GUARD_INTERVAL, :HIERARCHY,      "
        "   :MOD_SYS,           :ROLLOFF,        :BANDWIDTH,      "
        "   :SISTANDARD,        :TUNER_TYPE,     :SIGNAL_STRENGTH "
        " );");

    query.bindValue(":SCANID", scanid);
    query.bindValue(":MPLEXID", m_mplex);
    query.bindValue(":FREQUENCY", QString::number(m_frequency));
    query.bindValue(":INVERSION", m_inversion.toString());
    query.bindValue(":SYMBOLRATE", QString::number(m_symbolRate));
    query.bindValue(":FEC", m_fec.toString());
    query.bindValue(":POLARITY", m_polarity.toString());
    query.bindValue(":HP_CODE_RATE", m_hpCodeRate.toString());
    query.bindValue(":LP_CODE_RATE", m_lpCodeRate.toString());
    query.bindValue(":MODULATION", m_modulation.toString());
    query.bindValue(":TRANSMISSION_MODE", m_transMode.toString());
    query.bindValue(":GUARD_INTERVAL", m_guardInterval.toString());
    query.bindValue(":HIERARCHY", m_hierarchy.toString());
    query.bindValue(":MOD_SYS", m_modSys.toString());
    query.bindValue(":ROLLOFF", m_rolloff.toString());
    query.bindValue(":BANDWIDTH", m_bandwidth.toString());
    query.bindValue(":SISTANDARD", m_sistandard);
    query.bindValue(":TUNER_TYPE", m_tunerType.toUInt());
    query.bindValue(":SIGNAL_STRENGTH", m_signalStrength);

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

    for (const auto & channel : m_channels)
        channel.SaveScan(scanid, transportid);

    return transportid;
}

bool ScanDTVTransport::ParseTuningParams(
    DTVTunerType type,
    const QString& _frequency,    const QString& _inversion,      const QString& _symbolrate,
    const QString& _fec,          const QString& _polarity,
    const QString& _hp_code_rate, const QString& _lp_code_rate,   const QString& _ofdm_modulation,
    const QString& _trans_mode,   const QString& _guard_interval, const QString& _hierarchy,
    const QString& _modulation,   const QString& _bandwidth,      const QString& _mod_sys,
    const QString& _rolloff,      const QString& signal_strength)
{
    m_tunerType = type;
    m_signalStrength = signal_strength.toInt();

    return DTVMultiplex::ParseTuningParams(
        type,
        _frequency,     _inversion,       _symbolrate,
        _fec,           _polarity,
        _hp_code_rate,  _lp_code_rate,    _ofdm_modulation,
        _trans_mode,    _guard_interval,  _hierarchy,
        _modulation,    _bandwidth,       _mod_sys,
        _rolloff);
}
