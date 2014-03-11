// MythTV headers
#include "dtvmultiplex.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mpeg/dvbdescriptors.h"

#define LOC      QString("DTVMux: ")

DTVMultiplex::DTVMultiplex(const DTVMultiplex &other) :
    frequency(other.frequency),
    symbolrate(other.symbolrate),
    inversion(other.inversion),
    bandwidth(other.bandwidth),
    hp_code_rate(other.hp_code_rate),
    lp_code_rate(other.lp_code_rate),
    modulation(other.modulation),
    trans_mode(other.trans_mode),
    guard_interval(other.guard_interval),
    hierarchy(other.hierarchy),
    polarity(other.polarity),
    fec(other.fec),
    mod_sys(other.mod_sys),
    rolloff(other.rolloff),
    mplex(other.mplex),
    sistandard(other.sistandard),
    iptv_tuning(other.iptv_tuning)
{
}

DTVMultiplex &DTVMultiplex::operator=(const DTVMultiplex &other)
{
    frequency      = other.frequency;
    symbolrate     = other.symbolrate;
    inversion      = other.inversion;
    bandwidth      = other.bandwidth;
    hp_code_rate   = other.hp_code_rate;
    lp_code_rate   = other.lp_code_rate;
    modulation     = other.modulation;
    trans_mode     = other.trans_mode;
    guard_interval = other.guard_interval;
    hierarchy      = other.hierarchy;
    polarity       = other.polarity;
    fec            = other.fec;
    mod_sys        = other.mod_sys;
    rolloff        = other.rolloff;
    mplex          = other.mplex;
    sistandard     = other.sistandard;
    iptv_tuning    = other.iptv_tuning;
    return *this;
}

bool DTVMultiplex::operator==(const DTVMultiplex &m) const
{
    return ((frequency == m.frequency) &&
            (modulation == m.modulation) &&
            (inversion == m.inversion) &&
            (bandwidth == m.bandwidth) &&
            (hp_code_rate == m.hp_code_rate) &&
            (lp_code_rate == m.lp_code_rate) &&
            (trans_mode == m.trans_mode) &&
            (guard_interval == m.guard_interval) &&
            (fec == m.fec) &&
            (mod_sys  == m.mod_sys)  &&
            (rolloff  == m.rolloff)  &&
            (polarity == m.polarity) &&
            (hierarchy == m.hierarchy) &&
            (iptv_tuning == m.iptv_tuning)
            );
}

///////////////////////////////////////////////////////////////////////////////
// Gets

QString DTVMultiplex::toString() const
{
    QString ret = QString("%1 %2 %3 ")
        .arg(frequency).arg(modulation.toString()).arg(inversion.toString());

    ret += QString("%1 %2 %3 %4 %5 %6 %7")
        .arg(hp_code_rate.toString()).arg(lp_code_rate.toString())
        .arg(bandwidth.toString()).arg(trans_mode.toString())
        .arg(guard_interval.toString()).arg(hierarchy.toString())
        .arg(polarity.toString());
    ret += QString(" fec: %1 msys: %2 rolloff: %3")
        .arg(fec.toString()).arg(mod_sys.toString()).arg(rolloff.toString());

    return ret;
}

bool DTVMultiplex::IsEqual(DTVTunerType type, const DTVMultiplex &other,
                           uint freq_range, bool fuzzy) const
{
    if ((frequency + freq_range  < other.frequency             ) ||
        (frequency               > other.frequency + freq_range))
    {
        return false;
    }

    if (DTVTunerType::kTunerTypeDVBC == type)
    {
        if (fuzzy)
            return
                inversion.IsCompatible(other.inversion)   &&
                (symbolrate == other.symbolrate)          &&
                fec.IsCompatible(other.fec)               &&
                modulation.IsCompatible(other.modulation);
        return
            (inversion  == other.inversion)  &&
            (symbolrate == other.symbolrate) &&
            (fec        == other.fec)        &&
            (modulation == other.modulation);
    }

    if (DTVTunerType::kTunerTypeDVBT == type)
    {
        if (fuzzy)
            return
                inversion.IsCompatible(other.inversion)           &&
                bandwidth.IsCompatible(other.bandwidth)           &&
                hp_code_rate.IsCompatible(other.hp_code_rate)     &&
                lp_code_rate.IsCompatible(other.lp_code_rate)     &&
                modulation.IsCompatible(other.modulation)         &&
                guard_interval.IsCompatible(other.guard_interval) &&
                trans_mode.IsCompatible(other.trans_mode)         &&
                hierarchy.IsCompatible(other.hierarchy);
        return
            (inversion      == other.inversion)      &&
            (bandwidth      == other.bandwidth)      &&
            (hp_code_rate   == other.hp_code_rate)   &&
            (lp_code_rate   == other.lp_code_rate)   &&
            (modulation     == other.modulation)     &&
            (guard_interval == other.guard_interval) &&
            (trans_mode     == other.trans_mode)     &&
            (hierarchy      == other.hierarchy);
    }

    if (DTVTunerType::kTunerTypeATSC == type)
    {
        if (fuzzy)
            modulation.IsCompatible(other.modulation);
        return (modulation == other.modulation);
    }

    if ((DTVTunerType::kTunerTypeDVBS1 == type) ||
        (DTVTunerType::kTunerTypeDVBS2 == type))
    {
        bool ret =
            (symbolrate == other.symbolrate)        &&
            (polarity   == other.polarity)          &&
            (mod_sys    == other.mod_sys);

        if (fuzzy)
            return ret &&
                inversion.IsCompatible(other.inversion) &&
                fec.IsCompatible(other.fec)             &&
                rolloff.IsCompatible(other.rolloff);
        return ret &&
            (inversion  == other.inversion)  &&
            (fec        == other.fec)        &&
            (rolloff    == other.rolloff);
    }

    if (DTVTunerType::kTunerTypeIPTV == type)
    {
        return (iptv_tuning == other.iptv_tuning);
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Parsers

bool DTVMultiplex::ParseATSC(const QString &_frequency,
                             const QString &_modulation)
{
    bool ok = true;
    frequency = _frequency.toULongLong(&ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to parse ATSC frequency %1").arg(_frequency));
        return false;
    }

    ok = modulation.Parse(_modulation);
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
    bool ok = inversion.Parse(_inversion);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Invalid inversion parameter '%1', falling back to 'auto'.")
                .arg(_inversion));
        ok = true;
    }

    ok &= bandwidth.Parse(_bandwidth);
    ok &= hp_code_rate.Parse(_coderate_hp);
    ok &= lp_code_rate.Parse(_coderate_lp);
    ok &= modulation.Parse(_modulation);
    ok &= trans_mode.Parse(_trans_mode);
    ok &= hierarchy.Parse(_hierarchy);
    ok &= guard_interval.Parse(_guard_interval);
    if (ok)
        frequency = _frequency.toInt(&ok);

    return ok;
}

bool DTVMultiplex::ParseDVB_S_and_C(
    const QString &_frequency,   const QString &_inversion,
    const QString &_symbol_rate, const QString &_fec_inner,
    const QString &_modulation,  const QString &_polarity)
{
    bool ok = inversion.Parse(_inversion);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Invalid inversion parameter '%1', falling back to 'auto'.")
                .arg(_inversion));

        ok = true;
    }

    symbolrate = _symbol_rate.toInt();
    if (!symbolrate)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid symbol rate " +
            QString("parameter '%1', aborting.").arg(_symbol_rate));

        return false;
    }

    ok &= fec.Parse(_fec_inner);
    ok &= modulation.Parse(_modulation);

    if (!_polarity.isEmpty())
        polarity.Parse(_polarity.toLower());

    if (ok)
        frequency = _frequency.toInt(&ok);

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

    if (!mod_sys.Parse(_mod_sys))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid S2 modulation system " +
                QString("parameter '%1', aborting.").arg(_mod_sys));
        return false;
    }

    // For #10153, guess at modulation system based on modulation
    if (DTVModulationSystem::kModulationSystem_UNDEFINED == mod_sys)
    {
        mod_sys = (DTVModulation::kModulationQPSK == modulation) ?
            DTVModulationSystem::kModulationSystem_DVBS :
            DTVModulationSystem::kModulationSystem_DVBS2;
    }

    if ((DTVModulationSystem::kModulationSystem_DVBS  != mod_sys) &&
        (DTVModulationSystem::kModulationSystem_DVBS2 != mod_sys))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unsupported S2 modulation system " +
            QString("parameter '%1', aborting.").arg(_mod_sys));
        return false;
    }

    if (!_rolloff.isEmpty())
        ok &= rolloff.Parse(_rolloff);

    return ok;
}

bool DTVMultiplex::ParseTuningParams(
    DTVTunerType type,
    QString _frequency,    QString _inversion,      QString _symbolrate,
    QString _fec,          QString _polarity,
    QString _hp_code_rate, QString _lp_code_rate,   QString _ofdm_modulation,
    QString _trans_mode,   QString _guard_interval, QString _hierarchy,
    QString _modulation,   QString _bandwidth,
    QString _mod_sys,      QString _rolloff)
{
    if (DTVTunerType::kTunerTypeDVBT == type)
    {
        return ParseDVB_T(
            _frequency,       _inversion,       _bandwidth,    _hp_code_rate,
            _lp_code_rate,    _ofdm_modulation, _trans_mode,   _guard_interval,
            _hierarchy);
    }

    if ((DTVTunerType::kTunerTypeDVBS1 == type) ||
        (DTVTunerType::kTunerTypeDVBC  == type))
    {
        return ParseDVB_S_and_C(
            _frequency,       _inversion,     _symbolrate,
            _fec,             _modulation,    _polarity);
    }

    if (DTVTunerType::kTunerTypeDVBS2 == type)
    {
        return ParseDVB_S2(
            _frequency,       _inversion,     _symbolrate,
            _fec,             _modulation,    _polarity,
            _mod_sys,         _rolloff);
    }

    if (DTVTunerType::kTunerTypeATSC == type)
        return ParseATSC(_frequency, _modulation);

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

    mplex = mplexid;
    sistandard = query.value(13).toString();

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
    sistandard = "dvb";

    switch (tag)
    {
        case DescriptorID::terrestrial_delivery_system:
        {
            if (type != DTVTunerType::kTunerTypeDVBT)
                break;

            const TerrestrialDeliverySystemDescriptor cd(desc);

            return ParseDVB_T(
                QString::number(cd.FrequencyHz()), "a",
                cd.BandwidthString(),               cd.CodeRateHPString(),
                cd.CodeRateLPString(),              cd.ConstellationString(),
                cd.TransmissionModeString(),        cd.GuardIntervalString(),
                cd.HierarchyString());
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
                    QString::number(cd.FrequencyHz()),  "a",
                    QString::number(cd.SymbolRateHz()), cd.FECInnerString(),
                    cd.ModulationString(),
                    cd.PolarizationString());
            }

            if (type == DTVTunerType::kTunerTypeDVBS2)
            {
                return ParseDVB_S2(
                    QString::number(cd.FrequencyHz()),  "a",
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

            return ParseDVB_S_and_C(
                    QString::number(cd.FrequencyHz()),  "a",
                    QString::number(cd.SymbolRateHz()), cd.FECInnerString(),
                    cd.ModulationString(),               QString());
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
        "       d.transportid,   d.networkid,       c.default_authority "
        "FROM channel AS c, dtv_multiplex AS d "
        "WHERE c.mplexid = :MPLEXID AND"
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
            query.value(9).toUInt(),    !query.value(10).toUInt(),
            false,
            query.value(11).toString(),  query.value(12).toString(),
            query.value(13).toString(),  query.value(14).toString(),
            0, 0, 0,
            query.value(15).toUInt(),    query.value(16).toUInt(),
            0,
            QString::null,
            false, false, false, false,
            false, false, false, false,
            false, false, false, 0,
            query.value(17).toString() /* default_authority */);

        channels.push_back(chan);
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
    query.bindValue(":MPLEXID", mplex);
    query.bindValue(":FREQUENCY", QString::number(frequency));
    query.bindValue(":INVERSION", inversion.toString());
    query.bindValue(":SYMBOLRATE", QString::number(symbolrate));
    query.bindValue(":FEC", fec.toString());
    query.bindValue(":POLARITY", polarity.toString());
    query.bindValue(":HP_CODE_RATE", hp_code_rate.toString());
    query.bindValue(":LP_CODE_RATE", lp_code_rate.toString());
    query.bindValue(":MODULATION", modulation.toString());
    query.bindValue(":TRANSMISSION_MODE", trans_mode.toString());
    query.bindValue(":GUARD_INTERVAL", guard_interval.toString());
    query.bindValue(":HIERARCHY", hierarchy.toString());
    query.bindValue(":MOD_SYS", mod_sys.toString());
    query.bindValue(":ROLLOFF", rolloff.toString());
    query.bindValue(":BANDWIDTH", bandwidth.toString());
    query.bindValue(":SISTANDARD", sistandard);
    query.bindValue(":TUNER_TYPE", (uint)tuner_type);

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

    for (uint i = 0; i < channels.size(); i++)
        channels[i].SaveScan(scanid, transportid);

    return transportid;
}

bool ScanDTVTransport::ParseTuningParams(
    DTVTunerType type,
    QString _frequency,    QString _inversion,      QString _symbolrate,
    QString _fec,          QString _polarity,
    QString _hp_code_rate, QString _lp_code_rate,   QString _ofdm_modulation,
    QString _trans_mode,   QString _guard_interval, QString _hierarchy,
    QString _modulation,   QString _bandwidth,      QString _mod_sys,
    QString _rolloff)
{
    tuner_type = type;

    return DTVMultiplex::ParseTuningParams(
        type,
        _frequency,     _inversion,       _symbolrate,
        _fec,           _polarity,
        _hp_code_rate,  _lp_code_rate,    _ofdm_modulation,
        _trans_mode,    _guard_interval,  _hierarchy,
        _modulation,    _bandwidth,       _mod_sys,
        _rolloff);
}
