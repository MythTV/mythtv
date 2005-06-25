#include "frequencytables.h"
#include "channelutil.h"

freq_table_map_t frequencies;

static void init_freq_tables(freq_table_map_t&);

TransportScanItem::TransportScanItem()
    : mplexid(-1),      standard("dvb"),
      FriendlyName(""), SourceID(0),         UseTimer(false),
      scanning(false),  timeoutTune(ATSC_TUNINGTIMEOUT)
{ 
    bzero(freq_offsets, sizeof(int)*3);
}

TransportScanItem::TransportScanItem(int sourceid,
                                     int _mplexid,
                                     const QString &fn)
    : mplexid(_mplexid), standard("dvb"),
      FriendlyName(fn),  SourceID(sourceid), UseTimer(false),
      scanning(false),   timeoutTune(ATSC_TUNINGTIMEOUT)
{
    bzero(freq_offsets, sizeof(int)*3);
}

TransportScanItem::TransportScanItem(int sourceid,
                                     const QString &std,
                                     const QString &fn,
                                     uint fnum,
                                     uint freq,
                                     const FrequencyTable &ft) :
    mplexid(-1),        standard(std),       FriendlyName(fn),
    friendlyNum(fnum),  SourceID(sourceid),  UseTimer(false),
    scanning(false)
{
    bzero(freq_offsets, sizeof(int)*3);

    // set timeout
    timeoutTune = (standard == "dvb") ?
        DVBT_TUNINGTIMEOUT : ATSC_TUNINGTIMEOUT;

#ifdef USING_DVB
    // setup tuning params
    tuning.params.frequency                    = freq;
    const DVBFrequencyTable *dvbft =
        dynamic_cast<const DVBFrequencyTable*>(&ft);
    if (standard == "dvb" && dvbft)
    {
        tuning.params.inversion                    = dvbft->inversion;
        freq_offsets[1]                            = dvbft->offset1;
        freq_offsets[2]                            = dvbft->offset2;
        tuning.params.u.ofdm.bandwidth             = dvbft->bandwidth;
        tuning.params.u.ofdm.code_rate_HP          = dvbft->coderate_hp;
        tuning.params.u.ofdm.code_rate_LP          = dvbft->coderate_lp;
        tuning.params.u.ofdm.constellation         = dvbft->constellation;
        tuning.params.u.ofdm.transmission_mode     = dvbft->trans_mode;
        tuning.params.u.ofdm.guard_interval        = dvbft->guard_interval;
        tuning.params.u.ofdm.hierarchy_information = dvbft->hierarchy;
    }
    else if (standard == "atsc")
    {
#if (DVB_API_VERSION_MINOR == 1)
        tuning.params.u.vsb.modulation = (fe_modulation) ft.modulation;
#endif
    }
#else
    frequency  = freq;
    modulation = ft.modulation;
#endif // USING_DVB
    mplexid = GetMultiplexIdFromDB();
}

/** \fn TransportScanItem::GetMultiplexIdFromDB() const
 *  \brief Fetches mplexid if it exists, based on the frequency and sourceid
 */
int TransportScanItem::GetMultiplexIdFromDB() const
{
    int mplexid = -1;

    for (uint i = 0; (i < offset_cnt()) && (mplexid <= 0); i++)
        mplexid = ChannelUtil::GetMplexID(SourceID, freq_offset(i));

    return mplexid;
}

uint TransportScanItem::freq_offset(uint i) const
{
#ifdef USING_DVB
    int freq = (int) tuning.params.frequency;
#else
    int freq = (int) frequency;
#endif

    return (uint) (freq + freq_offsets[i]);
}

void init_freq_tables()
{
    static bool statics_initialized = false;
    static QMutex statics_lock;
    statics_lock.lock();
    if (!statics_initialized)
    {
        init_freq_tables(frequencies);
        statics_initialized = true;
    }
    statics_lock.unlock();
}

freq_table_list_t get_matching_freq_tables(
    QString format, QString modulation, QString country)
{
    const freq_table_map_t &fmap = frequencies;

    freq_table_list_t list;

    QString lookup = QString("%1_%2_%3%4")
        .arg(format).arg(modulation).arg(country);

    for (uint i = 0; true; i++)
    {
        const FrequencyTable* ft = fmap[lookup.arg(i)];
        if (!ft)
            break;
        list.push_back(ft);
    }

    return list;
}

static void init_freq_tables(freq_table_map_t &fmap)
{
#ifdef USING_DVB
    // United Kingdom
    fmap["dvbt_ofdm_uk0"] = new DVBFrequencyTable(
        474000000, 850000000, 8000000, "" , 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_2K,
        GUARD_INTERVAL_1_32, HIERARCHY_NONE, QAM_AUTO, 166670, -166670);

    // Finland
    fmap["dvbt_ofdm_fi0"] = new DVBFrequencyTable(
        474000000, 850000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 0, 0);

    // Sweden
    fmap["dvbt_ofdm_se0"] = new DVBFrequencyTable(
        474000000, 850000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 0, 0);

    // Australia
    fmap["dvbt_ofdm_au0"] = new DVBFrequencyTable(
        177500000, 226500000, 7000000, "", 0, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // VHF 6-12
    fmap["dvbt_ofdm_au1"] = new DVBFrequencyTable(
        529500000, 816500000, 7000000, "", 0, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_64, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // UHF 28-69

    // Germany (Deuschland)
    fmap["dvbt_ofdm_de0"] = new DVBFrequencyTable(
        177500000, 226500000, 7000000, "", 0, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // VHF 6-12
    fmap["dvbt_ofdm_de1"] = new DVBFrequencyTable(
        474000000, 826000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // UHF 21-65
#endif // USING_DVB

    // USA Terrestrial (center frequency, subtract 1.75 Mhz for visual carrier)
    fmap["atsc_vsb8_us0"] = new FrequencyTable(
        "ATSC Channel %1",  2,  57000000,  85000000, 6000000, VSB_8); // VHF 2-6
    fmap["atsc_vsb8_us1"] = new FrequencyTable(
        "ATSC Channel %1",  7, 177000000, 213000000, 6000000, VSB_8); // VHF 7-13
    fmap["atsc_vsb8_us2"] = new FrequencyTable(
        "ATSC Channel %1", 14, 473000000, 803000000, 6000000, VSB_8); // UHF 14-69
    fmap["atsc_vsb8_us3"] = new FrequencyTable(
        "ATSC Channel %1", 70, 809000000, 887000000, 6000000, VSB_8); // UHF 70-83

    // USA Cable, QAM 256
    fmap["atsc_qam256_us0"] = new FrequencyTable(
        "QAM-256 Channel %1",   1, 75000000, 801000000, 6000000, QAM_256);
    fmap["atsc_qam256_us1"] = new FrequencyTable(
        "QAM-256 Channel T-%1", 7, 10000000,  52000000, 6000000, QAM_256);

    // USA Cable, QAM 128
    fmap["atsc_qam128_us0"] = new FrequencyTable(
        "QAM-128 Channel %1",   1, 75000000, 801000000, 6000000, QAM_128);

    fmap["atsc_qam128_us1"] = new FrequencyTable(
        "QAM-128 Channel T-%1", 7, 10000000,  52000000, 6000000, QAM_128);

    // USA Cable, QAM 64
    fmap["atsc_qam64_us0"] = new FrequencyTable(
        "QAM-64 Channel %1",    1, 75000000, 801000000, 6000000, QAM_64);
    fmap["atsc_qam64_us1"] = new FrequencyTable(
        "QAM-64 Channel T-%1",  7, 10000000,  52000000, 6000000, QAM_64);
}
