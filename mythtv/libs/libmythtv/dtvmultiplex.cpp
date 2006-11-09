// Qt headers
#include <qdeepcopy.h>

// MythTV headers
#include "dtvmultiplex.h"
#include "mythcontext.h"
#include "mythdbcon.h"

#define LOC QString("DTVMux: ")
#define LOC_ERR QString("DTVMux, Error: ")
#define LOC_WARN QString("DTVMux, Warning: ")


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
    mplex          = other.mplex;
    sistandard     = QDeepCopy<QString>(other.sistandard);
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
            (polarity == m.polarity) &&
            (hierarchy == m.hierarchy));
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

    return ret;
}

bool DTVMultiplex::IsEqual(DTVTunerType type, const DTVMultiplex &other,
                           uint freq_range) const
{
    if ((frequency + freq_range  < other.frequency             ) ||
        (frequency               > other.frequency + freq_range))
    {
        return false;
    }

    if (DTVTunerType::kTunerTypeQAM == type)
    {
        return
            (inversion  == other.inversion)  &&
            (symbolrate == other.symbolrate) &&
            (fec        == other.fec)        &&
            (modulation == other.modulation);
    }

    if (DTVTunerType::kTunerTypeOFDM == type)
    {
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
        return (modulation == other.modulation);
    }

    if ((DTVTunerType::kTunerTypeDVB_S2 == type) ||
        (DTVTunerType::kTunerTypeQPSK   == type))
    {
        return
            (inversion  == other.inversion)  &&
            (symbolrate == other.symbolrate) &&
            (fec        == other.fec);
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////
// Parsers

bool DTVMultiplex::ParseATSC(const QString &_frequency,
                             const QString &_modulation)
{
    bool ok = true;
    frequency = _frequency.toUInt(&ok);
    return ok && modulation.Parse(_modulation);
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
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Invalid inversion, falling back to 'auto'.");
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
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Invalid inversion, falling back to 'auto'");

        ok = true;
    }

    symbolrate = _symbol_rate.toInt();
    if (!symbolrate)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Invalid symbol rate " +
                QString("parameter '%1', aborting.").arg(_symbol_rate));

        return false;
    }

    ok &= fec.Parse(_fec_inner);
    ok &= modulation.Parse(_modulation);

    if (!_polarity.isEmpty())
        polarity.Parse(_polarity.lower());

    if (ok)
        frequency = _frequency.toInt(&ok);

    return ok;
}

bool DTVMultiplex::ParseTuningParams(
    DTVTunerType type,
    QString _frequency,    QString _inversion,      QString _symbolrate,
    QString _fec,          QString _polarity,
    QString _hp_code_rate, QString _lp_code_rate,   QString _ofdm_modulation,
    QString _trans_mode,   QString _guard_interval, QString _hierarchy,
    QString _modulation,   QString _bandwidth)
{
    if (DTVTunerType::kTunerTypeOFDM == type)
    {
        return ParseDVB_T(
            _frequency,       _inversion,       _bandwidth,    _hp_code_rate,
            _lp_code_rate,    _ofdm_modulation, _trans_mode,   _guard_interval,
            _hierarchy);
    }

    if ((DTVTunerType::kTunerTypeQPSK   == type) ||
        (DTVTunerType::kTunerTypeDVB_S2 == type) ||
        (DTVTunerType::kTunerTypeQAM    == type))
    {
        return ParseDVB_S_and_C(
            _frequency,       _inversion,     _symbolrate,
            _fec,             _modulation,    _polarity);
    }

    if (DTVTunerType::kTunerTypeATSC == type)
        return ParseATSC(_frequency, _modulation);
   
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
        "       modulation,        bandwidth,      sistandard "
        "FROM dtv_multiplex "
        "WHERE dtv_multiplex.mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("DVBTuning::FillFromDB", query);
        return false;
    }

    if (!query.next())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Could not find tuning parameters for mplex %1")
                .arg(mplexid));

        return false;
    }

    mplex = mplexid;
    sistandard = QDeepCopy<QString>(query.value(13).toString());

    // Parse the query into our DVBTuning class
    return ParseTuningParams(
        type,
        query.value(0).toString(),  query.value(1).toString(),
        query.value(2).toString(),  query.value(3).toString(),
        query.value(4).toString(),  query.value(5).toString(),
        query.value(6).toString(),  query.value(7).toString(),
        query.value(8).toString(),  query.value(9).toString(),
        query.value(10).toString(), query.value(11).toString(),
        query.value(12).toString());
}
