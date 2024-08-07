#include <utility>

#include <QMutex>

#include "libmythbase/compat.h"

#include "frequencies.h"
#include "frequencytables.h"
#include "channelutil.h"

static bool             frequencies_initialized = false;
static QMutex           frequencies_lock;
static freq_table_map_t frequencies;

static void init_freq_tables(freq_table_map_t& /*fmap*/);
static freq_table_list_t get_matching_freq_tables_internal(
    const QString &format, const QString &modulation, const QString &country);

TransportScanItem::TransportScanItem()
{
    m_tuning.Clear();
}

TransportScanItem::TransportScanItem(uint           sourceid,
                                     const QString &_si_std,
                                     QString        _name,
                                     uint           _mplexid,
                                     std::chrono::milliseconds _timeoutTune)
    : m_mplexid(_mplexid),  m_friendlyName(std::move(_name)),
      m_sourceID(sourceid),
      m_timeoutTune(_timeoutTune)
{
    m_tuning.Clear();
    m_tuning.m_sistandard = _si_std;

    if (_si_std == "analog")
    {
        m_tuning.m_sistandard = "analog";
        m_tuning.m_modulation = DTVModulation::kModulationAnalog;
    }
}

TransportScanItem::TransportScanItem(uint           _sourceid,
                                     QString        _name,
                                     const  DTVMultiplex &_tuning,
                                     std::chrono::milliseconds _timeoutTune)
    : m_mplexid(0),
      m_friendlyName(std::move(_name)),
      m_sourceID(_sourceid),
      m_timeoutTune(_timeoutTune),
      m_tuning(_tuning)
{
}

TransportScanItem::TransportScanItem(uint                _sourceid,
                                     QString             _name,
                                     DTVTunerType        _tuner_type,
                                     const DTVTransport &_tuning,
                                     std::chrono::milliseconds _timeoutTune)
    : m_mplexid(0),
      m_friendlyName(std::move(_name)),
      m_sourceID(_sourceid),
      m_timeoutTune(_timeoutTune),
      m_expectedChannels(_tuning.channels)
{
    m_tuning.Clear();

    m_tuning.ParseTuningParams(
        _tuner_type,
        QString::number(_tuning.m_frequency),  _tuning.m_inversion.toString(),
        QString::number(_tuning.m_symbolRate), _tuning.m_fec.toString(),
        _tuning.m_polarity.toString(),         _tuning.m_hpCodeRate.toString(),
        _tuning.m_lpCodeRate.toString(),       _tuning.m_modulation.toString(),
        _tuning.m_transMode.toString(),        _tuning.m_guardInterval.toString(),
        _tuning.m_hierarchy.toString(),        _tuning.m_modulation.toString(),
        _tuning.m_bandwidth.toString(),        _tuning.m_modSys.toString(),
        _tuning.m_rolloff.toString());
}

TransportScanItem::TransportScanItem(uint sourceid,
                                     const QString &std,
                                     QString strFmt,
                                     uint freqNum,
                                     uint freq,
                                     const FrequencyTable &ft,
                                     std::chrono::milliseconds timeoutTune)
    : m_mplexid(0),           m_friendlyName(std::move(strFmt)),
      m_friendlyNum(freqNum), m_sourceID(sourceid),
      m_timeoutTune(timeoutTune)
{
    m_tuning.Clear();

    // Setup tuning parameters
    m_tuning.m_frequency  = freq;
    m_tuning.m_sistandard = "dvb";
    m_tuning.m_modulation = ft.m_modulation;

    if (std.toLower() == "atsc")
        m_tuning.m_sistandard = "atsc";
    else if (std.toLower() == "analog")
    {
        m_tuning.m_sistandard = "analog";
        m_tuning.m_modulation = DTVModulation::kModulationAnalog;
    }

    m_freqOffsets[1]   = ft.m_offset1;
    m_freqOffsets[2]   = ft.m_offset2;

    if (std == "dvbt")
    {
        m_tuning.m_inversion      = ft.m_inversion;
        m_tuning.m_bandwidth      = ft.m_bandwidth;
        m_tuning.m_hpCodeRate     = ft.m_coderateHp;
        m_tuning.m_lpCodeRate     = ft.m_coderateLp;
        m_tuning.m_transMode      = ft.m_transMode;
        m_tuning.m_guardInterval  = ft.m_guardInterval;
        m_tuning.m_hierarchy      = ft.m_hierarchy;
    }
    else if (std == "dvbc" || std == "dvbs")
    {
        m_tuning.m_symbolRate     = ft.m_symbolRate;
        m_tuning.m_fec            = ft.m_fecInner;
    }

    // Do not use tuning information from database for the "Full Scan"
    // m_mplexid = GetMultiplexIdFromDB();
}

TransportScanItem::TransportScanItem(uint _sourceid,
                                     QString _name,
                                     IPTVTuningData _tuning,
                                     QString _channel,
                                     std::chrono::milliseconds _timeoutTune) :
    m_mplexid(0),
    m_friendlyName(std::move(_name)),
    m_sourceID(_sourceid),
    m_timeoutTune(_timeoutTune),
    m_iptvTuning(std::move(_tuning)), m_iptvChannel(std::move(_channel))
{
    m_tuning.Clear();
    m_tuning.m_sistandard = "MPEG";
}

/** \fn TransportScanItem::GetMultiplexIdFromDB(void) const
 *  \brief Fetches mplexid if it exists, based on the frequency and sourceid
 */
uint TransportScanItem::GetMultiplexIdFromDB(void) const
{
    int mplexid = 0;

    for (uint i = 0; (i < offset_cnt()) && (mplexid <= 0); i++)
        mplexid = ChannelUtil::GetMplexID(m_sourceID, freq_offset(i));

    return mplexid < 0 ? 0 : mplexid;
}

uint64_t TransportScanItem::freq_offset(uint i) const
{
    auto freq = (int64_t) m_tuning.m_frequency;

    return (uint64_t) (freq + m_freqOffsets[i]);
}

QString TransportScanItem::toString() const
{
    if (m_tuning.m_sistandard == "MPEG")
    {
        return m_iptvChannel + ": " + m_iptvTuning.GetDeviceKey();
    }

    QString str = QString("Transport Scan Item '%1' #%2").arg(m_friendlyName).arg(m_friendlyNum);
    str += "\n\t";
    str += QString("mplexid(%1) "           ).arg(m_mplexid);
    str += QString("standard(%1) "          ).arg(m_tuning.m_sistandard);
    str += QString("sourceid(%1) "          ).arg(m_sourceID);
    str += QString("useTimer(%1) "          ).arg(static_cast<int>(m_useTimer));
    str += QString("scanning(%1) "          ).arg(static_cast<int>(m_scanning));
    str += QString("timeoutTune(%3 msec)  " ).arg(m_timeoutTune.count());
    str += "\n\t";
    str += QString("frequency(%1) "         ).arg(m_tuning.m_frequency);
    str += QString("offset[0..2]: %1 %2 %3 ").arg(m_freqOffsets[0]).arg(m_freqOffsets[1]).arg(m_freqOffsets[2]);

    if (m_tuning.m_sistandard == "atsc" || m_tuning.m_sistandard == "analog")
    {
        str += QString("modulation(%1) "    ).arg(m_tuning.m_modulation.toString());
    }
    else
    {
        str += QString("constellation(%1) " ).arg(m_tuning.m_modulation.toString());
        str += QString("inv(%1) "           ).arg(m_tuning.m_inversion.toString());
        str += QString("bandwidth(%1) "     ).arg(m_tuning.m_bandwidth.toString());
        str += QString("hp(%1) "            ).arg(m_tuning.m_hpCodeRate.toString());
        str += QString("lp(%1) "            ).arg(m_tuning.m_lpCodeRate.toString());
        str += "\n\t";
        str += QString("trans_mode(%1) "    ).arg(m_tuning.m_transMode.toString());
        str += QString("guard_int(%1) "     ).arg(m_tuning.m_guardInterval.toString());
        str += QString("hierarchy(%1) "     ).arg(m_tuning.m_hierarchy.toString());
        str += QString("symbol_rate(%1) "   ).arg(m_tuning.m_symbolRate);
        str += QString("fec(%1) "           ).arg(m_tuning.m_fec.toString());
    }
    return str;
}

static bool init_freq_tables(void)
{
    if (!frequencies_initialized)
    {
        init_freq_tables(frequencies);
        frequencies_initialized = true;
    }
    return true;
}

bool teardown_frequency_tables(void)
{
    QMutexLocker locker(&frequencies_lock);
    if (frequencies_initialized)
    {
        frequencies.clear();
        frequencies_initialized = false;
    }
    return true;
}

static freq_table_list_t get_matching_freq_tables_internal(
    const QString &format, const QString &modulation, const QString &country)
{
    const freq_table_map_t &fmap = frequencies;

    freq_table_list_t list;

    QString lookup = QString("%1_%2_%3%4")
        .arg(format, modulation, country);

    freq_table_map_t::const_iterator it = fmap.begin();
    for (uint i = 0; it != fmap.end(); i++)
    {
        it = fmap.find(lookup.arg(i));
        if (it != fmap.end())
            list.push_back(*it);
    }

    return list;
}

freq_table_list_t get_matching_freq_tables(
    const QString &format, const QString &modulation, const QString &country)
{
    QMutexLocker locker(&frequencies_lock);
    init_freq_tables();

    freq_table_list_t list =
        get_matching_freq_tables_internal(format, modulation, country);

    freq_table_list_t new_list;
    for (auto & ft : list)
        new_list.push_back(new FrequencyTable(*ft));

    return new_list;
}

long long get_center_frequency(
    const QString& format, const QString& modulation, const QString& country, int freqid)
{
    QMutexLocker locker(&frequencies_lock);
    init_freq_tables();

    freq_table_list_t list =
        get_matching_freq_tables_internal(format, modulation, country);

    for (auto & ft : list)
    {
        int min_freqid = ft->m_nameOffset;
        int max_freqid = min_freqid +
            ((ft->m_frequencyEnd - ft->m_frequencyStart) /
             ft->m_frequencyStep);

        if ((min_freqid <= freqid) && (freqid <= max_freqid))
            return ft->m_frequencyStart +
                (ft->m_frequencyStep * (freqid - min_freqid));
    }
    return -1;
}

int get_closest_freqid(
    const QString& format, QString modulation, const QString& country, long long centerfreq)
{
    modulation = (modulation == "8vsb") ? "vsb8" : modulation;

    freq_table_list_t list =
        get_matching_freq_tables_internal(format, modulation, country);

    for (auto & ft : list)
    {
        int min_freqid = ft->m_nameOffset;
        int max_freqid = min_freqid +
            ((ft->m_frequencyEnd - ft->m_frequencyStart) /
             ft->m_frequencyStep);
        int freqid =
            ((centerfreq - ft->m_frequencyStart) /
             ft->m_frequencyStep) + min_freqid;

        if ((min_freqid <= freqid) && (freqid <= max_freqid))
            return freqid;
    }
#if 0
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("get_closest_freqid(%1, %2, %3, %4) Failed sz(%5)")
            .arg(format) .arg(modulation) .arg(country) .arg(centerfreq)
            .arg(list.size()));
#endif
    return -1;
}


//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FREQ(A,B, C,D, E,F,G, H, I)               \
    fmap[QString("atsc_%1_us%2").arg(A,B)] =      \
        new FrequencyTable((C)+(D), E, F, G, H, I);

// The maximum channel defined in the US frequency tables (standard, HRC, IRC)
static constexpr uint8_t US_MAX_CHAN { 158 };
// Equation for computing EIA-542 frequency of channels > 99
static constexpr uint64_t EIA_542_FREQUENCY(uint64_t bandwidth,
                                            uint64_t offset,
                                            uint64_t channel)
{ return (bandwidth * (8 + channel)) + offset;}
static_assert(EIA_542_FREQUENCY(6000000, 3000000, US_MAX_CHAN) == 999000000);
static_assert(EIA_542_FREQUENCY(6000300, 1750000, US_MAX_CHAN) == 997799800);

static void init_freq_tables(freq_table_map_t &fmap)
{
    // United Kingdom
    fmap["dvbt_ofdm_gb0"] = new FrequencyTable(
        474000000, 698000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardInterval_1_32, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 166670, -166670);

    // Finland
    fmap["dvbt_ofdm_fi0"] = new FrequencyTable(
        474000000, 850000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAM64,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0);

    // Sweden
    fmap["dvbt_ofdm_se0"] = new FrequencyTable(
        474000000, 850000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAM64,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0);

    // Australia
    fmap["dvbt_ofdm_au0"] = new FrequencyTable(
        177500000, 226500000, 7000000, "Channel %1", 5,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth7MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAM64,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 125000, 0); // VHF 5-12
    fmap["dvbt_ofdm_au1"] = new FrequencyTable(
        529500000, 816500000, 7000000, "Channel %1", 28,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth7MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAM64,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 125000, 0); // UHF 28-69

    // Germany (Deutschland)
    fmap["dvbt_ofdm_de0"] = new FrequencyTable(
        177500000, 226500000, 7000000, "Channel %1", 5,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth7MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // VHF 5-12, deprecated
    fmap["dvbt_ofdm_de1"] = new FrequencyTable(
        474000000, 826000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // UHF 21-65

    // Israel
    fmap["dvbt_ofdm_il0"] = new FrequencyTable(
        514000000, 514000000+1, 8000000, "Channel %1", 26,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // UHF 26 - central Israel
    fmap["dvbt_ofdm_il1"] = new FrequencyTable(
        538000000, 538000000+1, 8000000, "Channel %1", 29,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // UHF 29 - North and Eilat area

    // Italy (Italia)
    fmap["dvbt_ofdm_it0"] = new FrequencyTable(
        177500000, 226500000, 7000000, "Channel %1", 5,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth7MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // VHF 5-12, deprecated
    fmap["dvbt_ofdm_it1"] = new FrequencyTable(
        474000000, 858000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // UHF 21-65

    // Czech Republic
    fmap["dvbt_ofdm_cz0"] = new FrequencyTable(
        474000000, 858000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFEC_2_3,
        DTVCodeRate::kFEC_2_3, DTVModulation::kModulationQAM64,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAM64, 0, 0); // UHF 21-69

    // Greece (Hellas)
    fmap["dvbt_ofdm_gr0"] = new FrequencyTable(
        174000000, 230000000, 7000000, "Channel %1", 5,
        DTVInversion::kInversionAuto,
        DTVBandwidth::kBandwidth7MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyAuto,
        DTVModulation::kModulationQAMAuto, 0, 0); // VHF 5-12, deprecated
    fmap["dvbt_ofdm_gr1"] = new FrequencyTable(
        474000000, 866000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionAuto,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // UHF 21-65

    // Spain
    fmap["dvbt_ofdm_es0"] = new FrequencyTable(
        474000000, 858000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 125000, 0); // UHF 21-69

    // New Zealand
    fmap["dvbt_ofdm_nz0"] = new FrequencyTable(
        474000000, 690000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFEC_3_4,
        DTVCodeRate::kFEC_3_4, DTVModulation::kModulationQAM64,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardInterval_1_16, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAM64, 0 , 0); // UHF 21-48

    // France
    fmap["dvbt_ofdm_fr0"] = new FrequencyTable(
        474000000, 850000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 167000, -166000);

    // Denmark
    fmap["dvbt_ofdm_dk0"] = new FrequencyTable(
        474000000, 858000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFEC_2_3,
        DTVCodeRate::kFECNone, DTVModulation::kModulationQAM64,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardInterval_1_4, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAM64, 0, 0);

    // Netherlands
    fmap["dvbt_ofdm_nl0"] = new FrequencyTable(
        474000000, 690000000, 8000000, "Channel %1", 21,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth8MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 0, 0); // UHF 21-48

    // Chile (ISDB-Tb)
    fmap["dvbt_ofdm_cl0"] = new FrequencyTable(
        473000000, 803000000, 6000000, "Channel %1", 14,
        DTVInversion::kInversionAuto,
        DTVBandwidth::kBandwidthAuto, DTVCodeRate::kFEC_3_4,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionModeAuto,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulationQAMAuto, 143000, 0);

    // DVB-C Germany
    fmap["dvbc_qam_de0"] = new FrequencyTable(
         73000000,  73000000, 8000000, "Channel D%1", 73,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        6900000, 0, 0);
    fmap["dvbc_qam_de1"] = new FrequencyTable(
         81000000,  81000000, 8000000, "Channel D%1", 81,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        6900000, 0, 0);
    fmap["dvbc_qam_de2"] = new FrequencyTable(
        113000000, 121000000, 8000000, "Channel S0%1", 2,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        6900000, 0, 0);
    fmap["dvbc_qam_de3"] = new FrequencyTable(
        306000000, 466000000, 8000000, "Channel S%1", 21,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        6900000, 0, 0);
    fmap["dvbc_qam_de4"] = new FrequencyTable(
        474000000, 858000000, 8000000, "Channel %1", 21,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        6900000, 0, 0);

    // DVB-C Netherlands
    fmap["dvbc_qam_nl0"] = new FrequencyTable(
         474000000,  474000000, 8000000, "Channel %1", 21,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAM64,
        6875000, 0, 0);

    // DVB-C United Kingdom
    fmap["dvbc_qam_gb0"] = new FrequencyTable(
        12324000, 12324000+1, 10, "Channel %1", 1,
        DTVCodeRate::kFEC_3_4, DTVModulation::kModulationQAMAuto,
        29500000, 0, 0);
    fmap["dvbc_qam_gb1"] = new FrequencyTable(
        459000000, 459000000+1, 10, "Channel %1", 2,
        DTVCodeRate::kFEC_3_4, DTVModulation::kModulationQAM64,
        6952000, 0, 0);

    // DVB-C Unknown (British Forces ?)
    fmap["dvbc_qam_bf0"] = new FrequencyTable(
        203000000, 795000000, 100000, "BF Channel %1", 1,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        6900000, 0, 0);
    fmap["dvbc_qam_bf1"] = new FrequencyTable(
        194750000, 794750000, 100000, "BF Channel %1", 1 + ((795000-203000) / 100),
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        6900000, 0, 0);

//#define DEBUG_DVB_OFFSETS
#ifdef DEBUG_DVB_OFFSETS
    // UHF 24-36
    fmap["atsc_vsb8_us0"] = new FrequencyTable(
        533000000, 605000000, 6000000, "xATSC Channel %1", 24,
        DTVInversion::kInversionOff,
        DTVBandwidth::kBandwidth7MHz, DTVCodeRate::kFECAuto,
        DTVCodeRate::kFECAuto, DTVModulation::kModulationQAMAuto,
        DTVTransmitMode::kTransmissionMode8K,
        DTVGuardInterval::kGuardIntervalAuto, DTVHierarchy::kHierarchyNone,
        DTVModulation::kModulation8VSB, -100000, 100000);
#else // if !DEBUG_DVB_OFFSETS
    // USA Terrestrial (center frequency, subtract 1.75 MHz for visual carrier)
    // VHF 2-4
    fmap["atsc_vsb8_us0"] = new FrequencyTable(
        "ATSC Channel %1",  2,  57000000,  69000000, 6000000,
        DTVModulation::kModulation8VSB);
    // VHF 5-6
    fmap["atsc_vsb8_us1"] = new FrequencyTable(
        "ATSC Channel %1",  5,  79000000,  85000000, 6000000,
        DTVModulation::kModulation8VSB);
    // VHF 7-13
    fmap["atsc_vsb8_us2"] = new FrequencyTable(
        "ATSC Channel %1",  7, 177000000, 213000000, 6000000,
        DTVModulation::kModulation8VSB);
    // UHF 14-36
    fmap["atsc_vsb8_us3"] = new FrequencyTable(
        "ATSC Channel %1", 14, 473000000, 605000000, 6000000,
        DTVModulation::kModulation8VSB);
#endif // !DEBUG_DVB_OFFSETS

    const std::array<const QString,4> modStr {
        "vsb8", "qam256", "qam128", "qam64" };
    const std::array<const DTVModulation::Types,4> mod {
                         DTVModulation::kModulation8VSB,
                         DTVModulation::kModulationQAM256,
                         DTVModulation::kModulationQAM128,
                         DTVModulation::kModulationQAM64, };
    const std::array<const QString,4> desc {
        "ATSC ", "QAM-256 ", "QAM-128 ", "QAM-64 ", };

    for (uint i = 0; i < 4; i++)
    {
        // USA Cable, T13 to T14 and ch 2 to US_MAX_CHAN
        FREQ(modStr[i], "cable0", desc[i], "Channel T-%1",
             13,   44750000,   50750000, 6000000, mod[i]); // T13-T14
        FREQ(modStr[i], "cable1", desc[i], "Channel %1",
             2,    57000000,   69000000, 6000000, mod[i]); // 2-4
        FREQ(modStr[i], "cable2", desc[i], "Channel %1",
             5,    79000000,   85000000, 6000000, mod[i]); // 5-6
        FREQ(modStr[i], "cable3", desc[i], "Channel %1",
             7,   177000000,  213000000, 6000000, mod[i]); // 7-13
        FREQ(modStr[i], "cable4", desc[i], "Channel %1",
             14,  123000000,  171000000, 6000000, mod[i]); // 14-22
        FREQ(modStr[i], "cable5", desc[i], "Channel %1",
             23,  219000000,  645000000, 6000000, mod[i]); // 23-94
        FREQ(modStr[i], "cable6", desc[i], "Channel %1",
             95,   93000000,  117000000, 6000000, mod[i]); // 95-99
        // The center frequency of any EIA-542 std cable channel over 99 is
        // Frequency_MHz = ( 6 * ( 8 + channel_designation ) ) + 3
        FREQ(modStr[i], "cable7", desc[i], "Channel %1",
            100, 651000000,
            EIA_542_FREQUENCY(6000000, 3000000, US_MAX_CHAN),
            6000000, mod[i]);                             // 100-US_MAX_CHAN

        // USA Cable, QAM 256 ch 78 to US_MAX_CHAN
        FREQ(modStr[i], "cablehigh0", desc[i], "Channel %1",
             78,  549000000,  645000000, 6000000, mod[i]); // 78-94
        FREQ(modStr[i], "cablehigh1", desc[i], "Channel %1",
             100, 651000000,
             EIA_542_FREQUENCY(6000000, 3000000, US_MAX_CHAN),
             6000000, mod[i]);                             // 100-US_MAX_CHAN

        // USA Cable HRC, ch 1 to US_MAX_CHAN
        FREQ(modStr[i], "hrc0", desc[i], "HRC %1",
             1,    73753600,  73753601, 6000300, mod[i]); // 1
        FREQ(modStr[i], "hrc1", desc[i], "HRC %1",
             2,    55752700,  67753300, 6000300, mod[i]); // 2-4
        FREQ(modStr[i], "hrc2", desc[i], "HRC %1",
             5,    79753900,  85754200, 6000300, mod[i]); // 5-6
        FREQ(modStr[i], "hrc3", desc[i], "HRC %1",
             7,   175758700, 211760500, 6000300, mod[i]); // 7-13
        FREQ(modStr[i], "hrc4", desc[i], "HRC %1",
             14,  121756000, 169758400, 6000300, mod[i]); // 14-22
        FREQ(modStr[i], "hrc5", desc[i], "HRC %1",
             23,  217760800, 643782100, 6000300, mod[i]); // 23-94
        FREQ(modStr[i], "hrc6", desc[i], "HRC %1",
             95,   91754500, 115755700, 6000300, mod[i]); // 95-99
        // The center frequency of any EIA-542 HRC cable channel over 99 is
        // Frequency_MHz = ( 6.0003 * ( 8 + channel_designation ) ) + 1.75
        FREQ(modStr[i], "hrc7", desc[i], "HRC %1",
             100, 649782400,
             EIA_542_FREQUENCY(6000300, 1750000, US_MAX_CHAN),
             6000300, mod[i]); // 100-US_MAX_CHAN

        // USA Cable HRC, ch 76-94 and 100-US_MAX_CHAN
        // Channels 95-99 are low frequency despite high channel numbers
        FREQ(modStr[i], "hrchigh0", desc[i], "HRC %1",
             76,  535776700, 643782100, 6000300, mod[i]); // 76-94
        FREQ(modStr[i], "hrchigh1", desc[i], "HRC %1",
             100, 649782400,
             EIA_542_FREQUENCY(6000300, 1750000, US_MAX_CHAN),
             6000300, mod[i]); // 100-US_MAX_CHAN

        // USA Cable IRC, ch 1 to US_MAX_CHAN
        FREQ(modStr[i], "irc0", desc[i], "IRC %1",
             1,    75012500,  75012501, 6000000, mod[i]); // 1
        FREQ(modStr[i], "irc1", desc[i], "IRC %1",
             2,    57012500,  69012500, 6000000, mod[i]); // 2-4
        FREQ(modStr[i], "irc2", desc[i], "IRC %1",
             5,    81012500,  87012500, 6000000, mod[i]); // 5-6
        FREQ(modStr[i], "irc3", desc[i], "IRC %1",
             7,   177012500, 213012500, 6000000, mod[i]); // 7-13
        FREQ(modStr[i], "irc4", desc[i], "IRC %1",
             14,  123012500, 171012500, 6000000, mod[i]); // 14-22
        FREQ(modStr[i], "irc5", desc[i], "IRC %1",
             23,  219012500, 327012500, 6000000, mod[i]); // 23-41
        FREQ(modStr[i], "irc6", desc[i], "IRC %1",
             42,  333025000, 333025001, 6000000, mod[i]); // 42
        FREQ(modStr[i], "irc7", desc[i], "IRC %1",
             43,  339012500, 645012500, 6000000, mod[i]); // 43-94
        FREQ(modStr[i], "irc8", desc[i], "IRC %1",
             95,   93012500, 105012500, 6000000, mod[i]); // 95-97
        FREQ(modStr[i], "irc9", desc[i], "IRC %1",
             98,  111025000, 117025000, 6000000, mod[i]); // 98-99
        // The center frequency of any EIA-542 IRC cable channel over 99 is
        // Frequency_MHz = ( 6 * ( 8 + channel_designation ) ) + 3.0125
        FREQ(modStr[i], "irc10", desc[i], "IRC %1",
             100, 651012500,
             EIA_542_FREQUENCY(6000000, 3012500, US_MAX_CHAN),
             6000000, mod[i]); // 100-US_MAX_CHAN

        // USA Cable IRC, ch 76-94 and 100-125
        // Channels 95-99 are low frequency despite high channel numbers
        FREQ(modStr[i], "irchigh0", desc[i], "IRC %1",
             76,  537012500, 645012500, 6000000, mod[i]); // 76-94
        FREQ(modStr[i], "irchigh1", desc[i], "IRC %1",
             100, 651012500,
             EIA_542_FREQUENCY(6000000, 3012500, US_MAX_CHAN),
             6000000, mod[i]); // 100-US_MAX_CHAN
    }

    // create old school frequency tables...
    for (const auto & [name, list] : gChanLists)
    {
        for (uint i = 0; i < (uint)list.size(); i++)
        {
            uint64_t freq = (list[i].freq * 1000LL) + 1750000;
            fmap[QString("analog_analog_%1%2").arg(name).arg(i)] =
                new FrequencyTable(
                    QString("%1 %2").arg(name, list[i].name), i+2,
                    freq, freq + 3000000,
                    6000000, DTVModulation::kModulationAnalog);
        }
    }
}
