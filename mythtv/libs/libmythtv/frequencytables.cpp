#include "frequencytables.h"
#include "channelutil.h"

freq_table_map_t frequencies;

static void init_freq_tables(freq_table_map_t&);

TransportScanItem::TransportScanItem()
    : mplexid(-1),        standard("dvb"),      FriendlyName(""),
      friendlyNum(0),     SourceID(0),          UseTimer(false),
      scanning(false),    timeoutTune(1000)
{ 
    bzero(freq_offsets, sizeof(int)*3);

#ifdef USING_DVB
    bzero(&tuning, sizeof(DVBTuning));
#else
    frequency  = 0;
    modulation = 0;
#endif
}

TransportScanItem::TransportScanItem(int sourceid,
                                     const QString &std,
                                     const QString &fn,
                                     int _mplexid,
                                     uint tuneTO)
    : mplexid(_mplexid),  standard(std),        FriendlyName(fn),
      friendlyNum(0),     SourceID(sourceid),   UseTimer(false),
      scanning(false),    timeoutTune(tuneTO)
{
    bzero(freq_offsets, sizeof(int)*3);

#ifdef USING_DVB
    bzero(&tuning, sizeof(DVBTuning));
#else
    frequency  = 0;
    modulation = 0;
#endif
}

TransportScanItem::TransportScanItem(int sourceid,
                                     const QString &std,
                                     const QString &fn,
                                     uint fnum,
                                     uint freq,
                                     const FrequencyTable &ft,
                                     uint tuneTO)
    : mplexid(-1),        standard(std),        FriendlyName(fn),
      friendlyNum(fnum),  SourceID(sourceid),   UseTimer(false),
      scanning(false),    timeoutTune(tuneTO)
{
    bzero(freq_offsets, sizeof(int)*3);
#ifdef USING_DVB
    bzero(&tuning, sizeof(DVBTuning));

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
        if (dvbft)
        {
            freq_offsets[1]                        = dvbft->offset1;
            freq_offsets[2]                        = dvbft->offset2;
        }
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

QString TransportScanItem::toString() const
{
    QString str = QString("Transport Scan Item '%1' #%2\n")
        .arg(FriendlyName).arg(friendlyNum);
    str += QString("\tmplexid(%1) standard(%2) sourceid(%3)\n")
        .arg(mplexid).arg(standard).arg(SourceID);
    str += QString("\tUseTimer(%1) scanning(%2)\n")
        .arg(UseTimer).arg(scanning);
    str += QString("\ttimeoutTune(%3 msec)\n").arg(timeoutTune);
#ifdef USING_DVB
#if (DVB_API_VERSION_MINOR == 1)
    if (standard == "atsc")
    {
        str += QString("\tfrequency(%1) modulation(%2)\n")
            .arg(tuning.params.frequency)
            .arg(tuning.params.u.vsb.modulation);
    }
    else
#endif // (DVB_API_VERSION_MINOR == 1)
    {
        str += QString("\tfrequency(%1) constellation(%2)\n")
            .arg(tuning.params.frequency)
            .arg(tuning.params.u.ofdm.constellation);
        str += QString("\t  inv(%1) bandwidth(%2) hp(%3) lp(%4)\n")
            .arg(tuning.params.inversion)
            .arg(tuning.params.u.ofdm.bandwidth)
            .arg(tuning.params.u.ofdm.code_rate_HP)
            .arg(tuning.params.u.ofdm.code_rate_LP);
        str += QString("\t  trans_mode(%1) guard_int(%2) hierarchy(%3)\n")
            .arg(tuning.params.u.ofdm.transmission_mode)
            .arg(tuning.params.u.ofdm.guard_interval)
            .arg(tuning.params.u.ofdm.hierarchy_information);
    }
#else
    str += QString("\tfrequency(%1) modulation(%2)")
        .arg(frequency).arg(modulation);
#endif // USING_DVB
    str += QString("\t offset[0..2]: %1 %2 %3")
        .arg(freq_offsets[0]).arg(freq_offsets[1]).arg(freq_offsets[2]);
    return str;
}

bool init_freq_tables()
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

    return true;
}
bool just_here_to_force_init = init_freq_tables();

freq_table_list_t get_matching_freq_tables(
    QString format, QString modulation, QString country)
{
    const freq_table_map_t &fmap = frequencies;

    freq_table_list_t list;

    QString lookup = QString("%1_%2_%3%4")
        .arg(format).arg(modulation).arg(country);

    freq_table_map_t::const_iterator it = fmap.begin();
    for (uint i = 0; it != fmap.end(); i++)
    {
        it = fmap.find(lookup.arg(i));
        if (it != fmap.end())
            list.push_back(*it);
    }

    return list;
}

long long get_center_frequency(
    QString format, QString modulation, QString country, int freqid)
{
    freq_table_list_t list =
        get_matching_freq_tables(format, modulation, country);

    for (uint i = 0; i < list.size(); ++i)
    {
        int min_freqid = list[i]->name_offset;
        int max_freqid = min_freqid +
            ((list[i]->frequencyEnd - list[i]->frequencyStart) /
             list[i]->frequencyStep);

        if ((min_freqid <= freqid) && (freqid <= max_freqid))
            return list[i]->frequencyStart +
                list[i]->frequencyStep * (freqid - min_freqid);
    }
    return -1;
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

    // Spain
    fmap["dvbt_ofdm_es0"] = new DVBFrequencyTable(
        474000000, 858000000, 8000000, "", 0, INVERSION_OFF,
        BANDWIDTH_8_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_AUTO,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, QAM_AUTO, 125000, 0); // UHF 21-69
#endif // USING_DVB

//#define DEBUG_DVB_OFFSETS
#ifdef DEBUG_DVB_OFFSETS
    // UHF 14-69
    fmap["atsc_vsb8_us0"] = new DVBFrequencyTable(
        533000000, 803000000, 6000000, "xATSC Channel %1", 24, INVERSION_OFF,
        BANDWIDTH_7_MHZ, FEC_AUTO, FEC_AUTO, QAM_AUTO, TRANSMISSION_MODE_8K,
        GUARD_INTERVAL_AUTO, HIERARCHY_NONE, VSB_8, -100000, 100000);
#else
    // USA Terrestrial (center frequency, subtract 1.75 Mhz for visual carrier)
    // VHF 2-6
    fmap["atsc_vsb8_us0"] = new FrequencyTable(
        "ATSC Channel %1",  2,  57000000,  85000000, 6000000, VSB_8);
    // VHF 7-13
    fmap["atsc_vsb8_us1"] = new FrequencyTable(
        "ATSC Channel %1",  7, 177000000, 213000000, 6000000, VSB_8);
    // UHF 14-69
    fmap["atsc_vsb8_us2"] = new FrequencyTable(
        "ATSC Channel %1", 14, 473000000, 803000000, 6000000, VSB_8);
    // UHF 70-83
    fmap["atsc_vsb8_us3"] = new FrequencyTable(
        "ATSC Channel %1", 70, 809000000, 887000000, 6000000, VSB_8);
#endif // USING_DVB

    // USA Cable, QAM 256
    fmap["atsc_qam256_uscable0"] = new FrequencyTable(
        "QAM-256 Channel %1",   1, 75000000,1005000000, 6000000, QAM_256);
    fmap["atsc_qam256_uscable1"] = new FrequencyTable(
        "QAM-256 Channel T-%1", 7, 10000000,  52000000, 6000000, QAM_256);

    // USA Cable, QAM 256 ch 78+
    fmap["atsc_qam256_uscablehigh0"] = new FrequencyTable(
        "QAM-256 Channel %1",  78,472000000,1005000000, 6000000, QAM_256);

    // USA Cable HRC, QAM 256
    fmap["atsc_qam256_ushrc0"] = new FrequencyTable(
        "QAM-256 HRC %1",   1,  73750000,  73750001, 6000000, QAM_256);
    fmap["atsc_qam256_ushrc1"] = new FrequencyTable(
        "QAM-256 HRC %1",   2,  55750000,  67750000, 6000000, QAM_256);
    fmap["atsc_qam256_ushrc2"] = new FrequencyTable(
        "QAM-256 HRC %1",   5,  79750000,  85750000, 6000000, QAM_256);
    fmap["atsc_qam256_ushrc3"] = new FrequencyTable(
        "QAM-256 HRC %1",   7, 175750000, 643750000, 6000000, QAM_256);
    fmap["atsc_qam256_ushrc4"] = new FrequencyTable(
        "QAM-256 HRC %1",  95,  91750000, 114000000, 6000000, QAM_256);
    fmap["atsc_qam256_ushrc5"] = new FrequencyTable(
        "QAM-256 HRC %1", 100, 649750000, 799750000, 6000000, QAM_256);
    fmap["atsc_qam256_ushrc6"] = new FrequencyTable(
        "QAM-256 HRC T-%1", 7,   8175000,  50750000, 6000000, QAM_256);

    // USA Cable HRC, QAM 256 ch 78+
    fmap["atsc_qam256_ushrchigh0"] = new FrequencyTable(
        "QAM-256 HRC %1",  78, 601750000, 643750000, 6000000, QAM_256);
    fmap["atsc_qam256_ushrchigh1"] = new FrequencyTable(
        "QAM-256 HRC %1", 100, 649750000, 799750000, 6000000, QAM_256);



    // USA Cable, QAM 128
    fmap["atsc_qam128_uscable0"] = new FrequencyTable(
        "QAM-128 Channel %1",   1, 75000000,1005000000, 6000000, QAM_128);
    fmap["atsc_qam128_uscable1"] = new FrequencyTable(
        "QAM-128 Channel T-%1", 7, 10000000,  52000000, 6000000, QAM_128);

    // USA Cable, QAM 128 ch 78+
    fmap["atsc_qam128_uscablehigh0"] = new FrequencyTable(
        "QAM-128 Channel %1",  78,472000000,1005000000, 6000000, QAM_128);

    // USA Cable HRC, QAM 128
    fmap["atsc_qam128_ushrc0"] = new FrequencyTable(
        "QAM-128 HRC %1",   1,  73750000,  73750001, 6000000, QAM_128);
    fmap["atsc_qam128_ushrc1"] = new FrequencyTable(
        "QAM-128 HRC %1",   2,  55750000,  67750000, 6000000, QAM_128);
    fmap["atsc_qam128_ushrc2"] = new FrequencyTable(
        "QAM-128 HRC %1",   5,  79750000,  85750000, 6000000, QAM_128);
    fmap["atsc_qam128_ushrc3"] = new FrequencyTable(
        "QAM-128 HRC %1",   7, 175750000, 643750000, 6000000, QAM_128);
    fmap["atsc_qam128_ushrc4"] = new FrequencyTable(
        "QAM-128 HRC %1",  95,  91750000, 114000000, 6000000, QAM_128);
    fmap["atsc_qam128_ushrc5"] = new FrequencyTable(
        "QAM-128 HRC %1", 100, 649750000, 799750000, 6000000, QAM_128);
    fmap["atsc_qam128_ushrc6"] = new FrequencyTable(
        "QAM-128 HRC T-%1", 7,   8175000,  50750000, 6000000, QAM_128);

    // USA Cable HRC, QAM 128 ch 78+
    fmap["atsc_qam128_ushrchigh0"] = new FrequencyTable(
        "QAM-128 HRC %1",  78, 601750000, 643750000, 6000000, QAM_128);
    fmap["atsc_qam128_ushrchigh1"] = new FrequencyTable(
        "QAM-128 HRC %1", 100, 649750000, 799750000, 6000000, QAM_128);




    // USA Cable, QAM 64
    fmap["atsc_qam64_uscable0"] = new FrequencyTable(
        "QAM-64 Channel %1",    1, 75000000,1005000000, 6000000, QAM_64);
    fmap["atsc_qam64_uscable1"] = new FrequencyTable(
        "QAM-64 Channel T-%1",  7, 10000000,  52000000, 6000000, QAM_64);

    // USA Cable, QAM 64 ch 78+
    fmap["atsc_qam64_uscablehigh0"] = new FrequencyTable(
        "QAM-64 Channel %1",   78,472000000,1005000000, 6000000, QAM_64);

    // USA Cable HRC, QAM 64
    fmap["atsc_qam64_ushrc0"] = new FrequencyTable(
        "QAM-64 HRC %1",   1,  73750000,  73750001, 6000000, QAM_64);
    fmap["atsc_qam64_ushrc1"] = new FrequencyTable(
        "QAM-64 HRC %1",   2,  55750000,  67750000, 6000000, QAM_64);
    fmap["atsc_qam64_ushrc2"] = new FrequencyTable(
        "QAM-64 HRC %1",   5,  79750000,  85750000, 6000000, QAM_64);
    fmap["atsc_qam64_ushrc3"] = new FrequencyTable(
        "QAM-64 HRC %1",   7, 175750000, 643750000, 6000000, QAM_64);
    fmap["atsc_qam64_ushrc4"] = new FrequencyTable(
        "QAM-64 HRC %1",  95,  91750000, 114000000, 6000000, QAM_64);
    fmap["atsc_qam64_ushrc5"] = new FrequencyTable(
        "QAM-64 HRC %1", 100, 649750000, 799750000, 6000000, QAM_64);
    fmap["atsc_qam64_ushrc6"] = new FrequencyTable(
        "QAM-64 HRC T-%1", 7,   8175000,  50750000, 6000000, QAM_64);

    // USA Cable HRC, QAM 64 ch 78+
    fmap["atsc_qam64_ushrchigh0"] = new FrequencyTable(
        "QAM-64 HRC %1",  78, 601750000, 643750000, 6000000, QAM_64);
    fmap["atsc_qam64_ushrchigh1"] = new FrequencyTable(
        "QAM-64 HRC %1", 100, 649750000, 799750000, 6000000, QAM_64);
}
