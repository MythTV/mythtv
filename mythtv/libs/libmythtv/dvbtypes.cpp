#include <mythcontext.h>
#include "dvbtypes.h"

static QString mod2str(fe_modulation mod);
static QString mod2dbstr(fe_modulation mod);
static QString coderate(fe_code_rate_t coderate);

DVBParamHelper<PolarityValues>::Table DVBPolarity::parseTable[] =
{
    {"v",Vertical},
    {"h",Horizontal},
    {"r",Right},
    {"l",Left},
    {NULL,Vertical}
};

char *DVBPolarity::stringLookup[] =
{
   "v",   // Vertical
   "h",   // Horizontal
   "r",   // Right
   "l"    // Left
};

DVBParamHelper<fe_spectral_inversion_t>::Table DVBInversion::confTable[] =
{
   {"INVERSION_AUTO",INVERSION_AUTO},
   {"INVERSION_OFF",INVERSION_OFF},
   {"INVERSION_ON",INVERSION_ON},
   {NULL,INVERSION_AUTO}
};

DVBParamHelper<fe_spectral_inversion_t>::Table DVBInversion::vdrTable[] =
{
   {"999",INVERSION_AUTO},
   {"0",INVERSION_OFF},
   {"1",INVERSION_ON},
   {NULL,INVERSION_AUTO}
};

DVBParamHelper<fe_spectral_inversion_t>::Table DVBInversion::parseTable[] =
{
   {"a",INVERSION_AUTO},
   {"0",INVERSION_OFF},
   {"1",INVERSION_ON},
   {NULL,INVERSION_AUTO}
};

char* DVBInversion::stringLookup[] =
{
    "0", // INVERSION_OFF,
    "1", // INVERSION_ON,
    "a"  //INVERSION_AUTO
};

DVBParamHelper<fe_bandwidth_t>::Table DVBBandwidth::confTable[] =
{
   {"BANDWIDTH_AUTO",BANDWIDTH_AUTO},
   {"BANDWIDTH_8_MHZ",BANDWIDTH_8_MHZ},
   {"BANDWIDTH_7_MHZ",BANDWIDTH_7_MHZ},
   {"BANDWIDTH_6_MHZ",BANDWIDTH_6_MHZ},
   {NULL,BANDWIDTH_AUTO}
};

DVBParamHelper<fe_bandwidth_t>::Table DVBBandwidth::vdrTable[] =
{
   {"999",BANDWIDTH_AUTO},
   {"8",BANDWIDTH_8_MHZ},
   {"7",BANDWIDTH_7_MHZ},
   {"6",BANDWIDTH_6_MHZ},
   {NULL,BANDWIDTH_AUTO},
};

DVBParamHelper<fe_bandwidth_t>::Table DVBBandwidth::parseTable[] =
{
   {"auto",BANDWIDTH_AUTO},
   {"8",BANDWIDTH_8_MHZ},
   {"7",BANDWIDTH_7_MHZ},
   {"6",BANDWIDTH_6_MHZ},
   {NULL,BANDWIDTH_AUTO}
};

char *DVBBandwidth::stringLookup[]=
{
    "8",   //BANDWIDTH_8_MHZ,
    "7",   //BANDWIDTH_7_MHZ,
    "6",   //BANDWIDTH_6_MHZ,
    "auto" //BANDWIDTH_AUTO
};

DVBParamHelper<fe_code_rate_t>::Table DVBCodeRate::confTable[] =
{
    {"FEC_AUTO",FEC_AUTO},
    {"FEC_1_2",FEC_1_2},
    {"FEC_2_3",FEC_2_3},
    {"FEC_3_4",FEC_3_4},
    {"FEC_4_5",FEC_4_5},
    {"FEC_5_6",FEC_5_6},
    {"FEC_6_7",FEC_6_7},
    {"FEC_7_8",FEC_7_8},
    {"FEC_8_9",FEC_8_9},
    {"FEC_NONE",FEC_NONE},
    {NULL,FEC_AUTO}
};

DVBParamHelper<fe_code_rate_t>::Table DVBCodeRate::vdrTable[] =
{
    {"999",FEC_AUTO},
    {"12",FEC_1_2},
    {"23",FEC_2_3},
    {"34",FEC_3_4},
    {"45",FEC_4_5},
    {"56",FEC_5_6},
    {"67",FEC_6_7},
    {"78",FEC_7_8},
    {"89",FEC_8_9},
    {"0",FEC_NONE},
    {NULL,FEC_AUTO}
};

DVBParamHelper<fe_code_rate_t>::Table DVBCodeRate::parseTable[] =
{
    {"auto",FEC_AUTO},
    {"1/2",FEC_1_2},
    {"2/3",FEC_2_3},
    {"3/4",FEC_3_4},
    {"4/5",FEC_4_5},
    {"5/6",FEC_5_6},
    {"6/7",FEC_6_7},
    {"7/8",FEC_7_8},
    {"8/9",FEC_8_9},
    {"none",FEC_NONE},
    {NULL,FEC_AUTO}
};

char *DVBCodeRate::stringLookup[] =
{
     "none", //FEC_NONE,
     "1/2",  //FEC_1_2,
     "2/3",  //FEC_2_3,
     "3/4",  //FEC_3_4,
     "4/5",  //FEC_4_5,
     "5/6",  //FEC_5_6,
     "6/7",  //FEC_6_7,
     "7/8",  //FEC_7_8,
     "8/9",  //FEC_8_9,
     "auto"  //FEC_AUTO
};

DVBParamHelper<fe_modulation_t>::Table DVBModulation::confTable[] =
{
   {"QAM_AUTO",QAM_AUTO},
   {"QAM_16",QAM_16},
   {"QAM_32",QAM_32},
   {"QAM_64",QAM_64},
   {"QAM_128",QAM_128},
   {"QAM_256",QAM_256},
   {"QPSK",QPSK},
   {NULL,QAM_AUTO},
};

DVBParamHelper<fe_modulation_t>::Table DVBModulation::vdrTable[] =
{
   {"999",QAM_AUTO},
   {"16",QAM_16},
   {"32",QAM_32},
   {"64",QAM_64},
   {"128",QAM_128},
   {"256",QAM_256},
   {"0",QPSK},
   {NULL,QAM_AUTO},
};

DVBParamHelper<fe_modulation_t>::Table DVBModulation::parseTable[] =
{
   {"auto",QAM_AUTO},
   {"qam_16",QAM_16},
   {"qam_32",QAM_32},
   {"qam_64",QAM_64},
   {"qam_128",QAM_128},
   {"qam_256",QAM_256},
   {"qpsk",QPSK},
#if (DVB_API_VERSION_MINOR == 1)
   {"8vsb",VSB_8},
   {"16vsb",VSB_16},
#endif
   {NULL,QAM_AUTO},
};

char *DVBModulation::stringLookup[] =
{
    "qpsk",    //QPSK,
    "qam_16",  //QAM_16,
    "qam_32",  //QAM_32,
    "qam_64",  //QAM_64,
    "qam_128", //QAM_128,
    "qam_256", //QAM_256,
    "auto",    //QAM_AUTO,
    "8vsb",    //VSB_8,
    "16vsb"    //VSB_16
};

DVBParamHelper<fe_transmit_mode_t>::Table DVBTransmitMode::confTable[] =
{
   {"TRANSMISSION_MODE_AUTO",TRANSMISSION_MODE_AUTO},
   {"TRANSMISSION_MODE_2K",TRANSMISSION_MODE_2K},
   {"TRANSMISSION_MODE_8K",TRANSMISSION_MODE_2K},
   {NULL,TRANSMISSION_MODE_AUTO},
};

DVBParamHelper<fe_transmit_mode_t>::Table DVBTransmitMode::vdrTable[] =
{
   {"999",TRANSMISSION_MODE_AUTO},
   {"2",TRANSMISSION_MODE_2K},
   {"8",TRANSMISSION_MODE_2K},
   {NULL,TRANSMISSION_MODE_AUTO},
};

DVBParamHelper<fe_transmit_mode_t>::Table DVBTransmitMode::parseTable[] =
{
   {"auto",TRANSMISSION_MODE_AUTO},
   {"2",TRANSMISSION_MODE_2K},
   {"8",TRANSMISSION_MODE_2K},
   {NULL,TRANSMISSION_MODE_AUTO},
};

char *DVBTransmitMode::stringLookup[] =
{
    "2",   //TRANSMISSION_MODE_2K,
    "8",   //TRANSMISSION_MODE_8K,
    "auto" //TRANSMISSION_MODE_AUTO
};

DVBParamHelper<fe_guard_interval_t>::Table DVBGuardInterval::confTable[] =
{
   {"GUARD_INTERVAL_AUTO",GUARD_INTERVAL_AUTO},
   {"GUARD_INTERVAL_1_32",GUARD_INTERVAL_1_32},
   {"GUARD_INTERVAL_1_16",GUARD_INTERVAL_1_16},
   {"GUARD_INTERVAL_1_8",GUARD_INTERVAL_1_8},
   {"GUARD_INTERVAL_1_4",GUARD_INTERVAL_1_4},
   {NULL,GUARD_INTERVAL_AUTO},
};

DVBParamHelper<fe_guard_interval_t>::Table DVBGuardInterval::vdrTable[] =
{
   {"999",GUARD_INTERVAL_AUTO},
   {"32",GUARD_INTERVAL_1_32},
   {"16",GUARD_INTERVAL_1_16},
   {"8",GUARD_INTERVAL_1_8},
   {"4",GUARD_INTERVAL_1_4},
   {NULL,GUARD_INTERVAL_AUTO},
};

DVBParamHelper<fe_guard_interval_t>::Table DVBGuardInterval::parseTable[] =
{
   {"auto",GUARD_INTERVAL_AUTO},
   {"1/32",GUARD_INTERVAL_1_32},
   {"1/16",GUARD_INTERVAL_1_16},
   {"1/8",GUARD_INTERVAL_1_8},
   {"1/4",GUARD_INTERVAL_1_4},
   {NULL,GUARD_INTERVAL_AUTO},
};

char *DVBGuardInterval::stringLookup[] =
{
    "1/32", // GUARD_INTERVAL_1_32,
    "1/16", // GUARD_INTERVAL_1_16,
    "1/8",  // GUARD_INTERVAL_1_8,
    "1/4",  // GUARD_INTERVAL_1_4,
    "auto"  // GUARD_INTERVAL_AUTO
};

DVBParamHelper<fe_hierarchy_t>::Table DVBHierarchy::confTable[] =
{
   {"HIERARCHY_NONE",HIERARCHY_NONE},
   {"HIERARCHY_1",HIERARCHY_1},
   {"HIERARCHY_2",HIERARCHY_2},
   {"HIERARCHY_4",HIERARCHY_4},
   {"HIERARCHY_AUTO",HIERARCHY_AUTO},
   {NULL,HIERARCHY_AUTO},
};

DVBParamHelper<fe_hierarchy_t>::Table DVBHierarchy::vdrTable[] =
{
   {"0",HIERARCHY_NONE},
   {"1",HIERARCHY_1},
   {"2",HIERARCHY_2},
   {"4",HIERARCHY_4},
   {"999",HIERARCHY_AUTO},
   {NULL,HIERARCHY_AUTO},
};

DVBParamHelper<fe_hierarchy_t>::Table DVBHierarchy::parseTable[] =
{
   {"n",HIERARCHY_NONE},
   {"1",HIERARCHY_1},
   {"2",HIERARCHY_2},
   {"4",HIERARCHY_4},
   {"a",HIERARCHY_AUTO},
   {NULL,HIERARCHY_AUTO},
};

char *DVBHierarchy::stringLookup[] =
{
    "n", //HIERARCHY_NONE,
    "1", //HIERARCHY_1,
    "2", //HIERARCHY_2,
    "4", //HIERARCHY_4,
    "a"  //HIERARCHY_AUTO
};

bool equal_qpsk(const struct dvb_frontend_parameters &p,
                const struct dvb_frontend_parameters &op, uint range)
{
    return
        p.frequency + range  >= op.frequency           &&
        p.frequency          <= op.frequency + range   &&
        p.inversion          == op.inversion           &&
        p.u.qpsk.symbol_rate == op.u.qpsk.symbol_rate  &&
        p.u.qpsk.fec_inner   == op.u.qpsk.fec_inner;
}

bool equal_atsc(const struct dvb_frontend_parameters &p,
                const struct dvb_frontend_parameters &op, uint range)
{
#if (DVB_API_VERSION_MINOR == 1)
    return
        p.frequency + range  >= op.frequency           &&
        p.frequency          <= op.frequency + range   &&
        p.u.vsb.modulation   == op.u.vsb.modulation;
#else
    return
        p.frequency + range  >= op.frequency           &&
        p.frequency          <= op.frequency + range;
#endif
}

bool equal_qam(const struct dvb_frontend_parameters &p,
               const struct dvb_frontend_parameters &op, uint range)
{
    return
        p.frequency + range  >= op.frequency            &&
        p.frequency          <= op.frequency + range    &&
        p.inversion          == op.inversion            &&
        p.u.qam.symbol_rate  == op.u.qam.symbol_rate    &&
        p.u.qam.fec_inner    == op.u.qam.fec_inner      &&
        p.u.qam.modulation   == op.u.qam.modulation;
}

bool equal_ofdm(const struct dvb_frontend_parameters &p,
                const struct dvb_frontend_parameters &op,
                uint range)
{
    return 
        p.frequency + range            >= op.frequency                &&
        p.frequency                    <= op.frequency + range        &&
        p.inversion                    == op.inversion                &&
        p.u.ofdm.bandwidth             == op.u.ofdm.bandwidth         &&
        p.u.ofdm.code_rate_HP          == op.u.ofdm.code_rate_HP      &&
        p.u.ofdm.code_rate_LP          == op.u.ofdm.code_rate_LP      &&
        p.u.ofdm.constellation         == op.u.ofdm.constellation     &&
        p.u.ofdm.guard_interval        == op.u.ofdm.guard_interval    &&
        p.u.ofdm.transmission_mode     == op.u.ofdm.transmission_mode &&
        p.u.ofdm.hierarchy_information == op.u.ofdm.hierarchy_information;
}

bool equal_type(const struct dvb_frontend_parameters &p,
                const struct dvb_frontend_parameters &op,
                fe_type_t type, uint freq_range)
{
    if (FE_QAM == type)
        return equal_qam(p, op, freq_range);
    if (FE_OFDM == type)
        return equal_ofdm(p, op, freq_range);
    if (FE_QPSK == type)
        return equal_qpsk(p, op, freq_range);
#if (DVB_API_VERSION_MINOR == 1)
    if (FE_ATSC == type)
        return equal_atsc(p, op, freq_range);
#endif
    return false;
}

char DVBTuning::InversionChar() const
{
    switch (params.inversion)
    {
    case INVERSION_ON:
        return '1';
    case INVERSION_OFF:
        return '0';
    default:
        return 'a';
    }
}

char DVBTuning::TransmissionModeChar() const
{
    switch (params.u.ofdm.transmission_mode)
    {
    case TRANSMISSION_MODE_2K:
        return '2';
    case TRANSMISSION_MODE_8K:
        return '8';
    default:
        return 'a';
    }
}

char DVBTuning::BandwidthChar() const
{
    switch (params.u.ofdm.bandwidth)
    {
       case BANDWIDTH_8_MHZ:
           return '8';
       case BANDWIDTH_7_MHZ:
           return '7';
       case BANDWIDTH_6_MHZ:
           return '6';
       default:
           return 'a';
    }
}

char DVBTuning::HierarchyChar() const
{
    switch (params.u.ofdm.hierarchy_information)
    {
       case HIERARCHY_NONE:
               return 'n';
       case HIERARCHY_1:
               return '1';
       case HIERARCHY_2:
               return '2';
       case HIERARCHY_4:
               return '4';
       default:
               return 'a';
    }
}

QString DVBTuning::ConstellationDB() const
{
    return mod2dbstr(params.u.ofdm.constellation);
}

QString DVBTuning::ModulationDB() const
{
#if (DVB_API_VERSION_MINOR == 1)
    return mod2dbstr(params.u.vsb.modulation);
#else
    return mod2dbstr((fe_modulation)0);
#endif
}

QString DVBTuning::InversionString() const
{
    switch (params.inversion)
    {
        case INVERSION_ON:
            return "On";
        case INVERSION_OFF:
            return "Off";
        default:
            return "Auto";
    }
}

QString DVBTuning::ModulationString() const
{
    return mod2str(params.u.qam.modulation);
}

QString DVBTuning::ConstellationString() const
{
    return mod2str(params.u.ofdm.constellation);
}

QString DVBTuning::BandwidthString() const
{
    switch (params.u.ofdm.bandwidth)
    {
        case BANDWIDTH_AUTO:  return "Auto"; break;
        case BANDWIDTH_8_MHZ: return "8MHz"; break;
        case BANDWIDTH_7_MHZ: return "7MHz"; break;
        case BANDWIDTH_6_MHZ: return "6MHz"; break;
    }
    return "Unknown";
}

QString DVBTuning::TransmissionModeString() const
{
    //TransmissionMode
    switch (params.u.ofdm.transmission_mode)
    {
    case TRANSMISSION_MODE_2K:
        return "2K";
    case TRANSMISSION_MODE_8K:
        return "8K";
    default:
        return "Auto";
    }
}

QString DVBTuning::HPCodeRateString() const
{
    return coderate(params.u.ofdm.code_rate_HP);
}

QString DVBTuning::LPCodeRateString() const
{
    return coderate(params.u.ofdm.code_rate_LP);
}

QString DVBTuning::QAMInnerFECString() const
{
    return coderate(params.u.qam.fec_inner);
}

QString DVBTuning::GuardIntervalString() const
{
    //Guard Interval
    switch (params.u.ofdm.guard_interval)
    {
    case GUARD_INTERVAL_1_32:
        return "1/32";
    case GUARD_INTERVAL_1_16:
        return "1/16";
    case GUARD_INTERVAL_1_8:
        return "1/8";
    case GUARD_INTERVAL_1_4:
        return "1/4";
    default:
        return "auto";
    }
}

QString DVBTuning::HierarchyString() const
{
   // Hierarchy
    switch (params.u.ofdm.hierarchy_information)
    {
       case HIERARCHY_NONE:
               return "None";
       case HIERARCHY_1:
               return "1";
       case HIERARCHY_2:
               return "2";
       case HIERARCHY_4:
               return "4";
       default:
               return "Auto";
    }
}

QString DVBTuning::toString(fe_type_t type) const
{
    QString msg("");

    if (FE_QPSK == type)
    {
        msg = QString("Frequency: %1 Symbol Rate: %2 Pol: %3 Inv: %4")
            .arg(Frequency())
            .arg(QPSKSymbolRate())
            .arg((voltage == SEC_VOLTAGE_13) ? "V/R" : "H/L")
            .arg(InversionString());
    }
    else if (FE_QAM == type)
    {
        msg = QString("Frequency: %1 Symbol Rate: %2 Inversion: %3 Inner FEC: %4")
            .arg(Frequency())
            .arg(QAMSymbolRate())
            .arg(InversionString())
            .arg(QAMInnerFECString());
    }
#if (DVB_API_VERSION_MINOR == 1)
    else if (FE_ATSC == type)
    {
        msg = QString("Frequency: %1 Modulation: %2")
            .arg(Frequency())
            .arg(ModulationString());
    }
#endif
    else if (FE_OFDM == type)
    {
        msg = QString("Frequency: %1 BW: %2 HP: %3 LP: %4"
                      "C: %5 TM: %6 H: %7 GI: %8")
            .arg(Frequency())
            .arg(BandwidthString())
            .arg(HPCodeRateString())
            .arg(LPCodeRateString())
            .arg(ConstellationString())
            .arg(TransmissionModeString())
            .arg(HierarchyString())
            .arg(GuardIntervalString());
    }
    return msg;
}

bool DVBTuning::parseATSC(const QString& frequency, const QString modulation)
{
    bool ok = true;

    params.frequency = frequency.toInt();
#if (DVB_API_VERSION_MINOR == 1)
    dvb_vsb_parameters& p = params.u.vsb;

    p.modulation = parseModulation(modulation, ok);
    if (QAM_AUTO == p.modulation)
    {
        WARNING_TUNING(QString("Invalid modulationulation parameter '%1', "
                               "falling back to '8-VSB'.").arg(modulation));
        p.modulation = VSB_8;
    }
#endif

    return true;
}

bool DVBTuning::parseOFDM(const TransportObject &transport)
{
    return parseOFDM(QString::number(transport.Frequency),
                     transport.Inversion,
                     transport.Bandwidth,
                     transport.CodeRateHP,
                     transport.CodeRateLP,
                     transport.Constellation,
                     transport.TransmissionMode,
                     transport.GuardInterval,
                     transport.Hiearchy);
}

bool DVBTuning::parseOFDM(const QString& frequency,   const QString& inversion,
                          const QString& bandwidth,   const QString& coderate_hp,
                          const QString& coderate_lp, const QString& constellation,
                          const QString& trans_mode,  const QString& guard_interval,
                          const QString& hierarchy)
{
    bool ok = true;

    dvb_ofdm_parameters& p = params.u.ofdm;
    params.frequency = frequency.toInt();

    params.inversion = parseInversion(inversion, ok);
    if (!ok)
    {
        ERROR_TUNING("Invalid inversion, aborting.");
        return false;
    }

    p.bandwidth             = parseBandwidth(bandwidth,          ok);
    p.code_rate_HP          = parseCodeRate(coderate_hp,         ok);
    p.code_rate_LP          = parseCodeRate(coderate_lp,         ok);
    p.constellation         = parseModulation(constellation,     ok);
    p.transmission_mode     = parseTransmission(trans_mode,      ok);
    p.hierarchy_information = parseHierarchy(hierarchy,          ok);
    p.guard_interval        = parseGuardInterval(guard_interval, ok);

    return true;
}

// TODO: Add in DiseqcPos when diseqc class supports it
bool DVBTuning::parseQPSK(const QString& frequency,   const QString& inversion,
                          const QString& symbol_rate, const QString& fec_inner,
                          const QString& pol,         const QString& _diseqc_type,
                          const QString& _diseqc_port,
                          const QString& _diseqc_pos,
                          const QString& _lnb_lof_switch,
                          const QString& _lnb_lof_hi,
                          const QString& _lnb_lof_lo)
{
    bool ok = true;

    dvb_qpsk_parameters& p = params.u.qpsk;
    params.frequency = frequency.toInt();

    params.inversion = parseInversion(inversion, ok);
    if (!ok)
    {
        ERROR_TUNING("Invalid inversion, aborting.");
        return false;
    }

    p.symbol_rate = symbol_rate.toInt();
    if (!p.symbol_rate)
    {
        ERROR_TUNING(QString("Invalid symbol rate parameter '%1', aborting.")
                     .arg(symbol_rate));
        return false;
    }

    voltage = parsePolarity(pol, ok);
    if (SEC_VOLTAGE_OFF == voltage)
    {
        ERROR_TUNING("Invalid polarization, aborting.");
        return false;
    }

    p.fec_inner = parseCodeRate(fec_inner, ok);

    diseqc_type = _diseqc_type.toInt();
    diseqc_port = _diseqc_port.toInt();
    diseqc_pos  = _diseqc_pos.toFloat();
    lnb_lof_switch = _lnb_lof_switch.toInt();
    lnb_lof_hi  = _lnb_lof_hi.toInt();
    lnb_lof_lo  = _lnb_lof_lo.toInt();
    return true;
}

bool DVBTuning::parseQAM(const TransportObject &transport)
{
    return parseQAM(QString::number(transport.Frequency),
                    transport.Inversion,
                    QString::number(transport.SymbolRate),
                    transport.FEC_Inner,
                    transport.Modulation);
}

bool DVBTuning::parseQAM(const QString& frequency, const QString& inversion,
                         const QString& symbol_rate, const QString& fec_inner,
                         const QString& modulation)
{
    bool ok = true;

    dvb_qam_parameters& p = params.u.qam;
    params.frequency = frequency.toInt();

    params.inversion = parseInversion(inversion, ok);
    if (!ok)
    {
        ERROR_TUNING("Invalid inversion, aborting.");
        return false;
    }

    p.symbol_rate = symbol_rate.toInt();
    if (!p.symbol_rate)
    {
        ERROR_TUNING(QString("Invalid symbol rate parameter '%1', aborting.")
                     .arg(symbol_rate));
        return false;
    }

    p.fec_inner  = parseCodeRate(fec_inner, ok);
    p.modulation = parseModulation(modulation, ok);

    return true;
}

fe_bandwidth DVBTuning::parseBandwidth(const QString &bw, bool &ok)
{
    char bandwidth = QChar(bw[0]).lower();
    ok = true;
    switch (bandwidth)
    {
        case 'a': return BANDWIDTH_AUTO;
        case '8': return BANDWIDTH_8_MHZ;
        case '7': return BANDWIDTH_7_MHZ;
        case '6': return BANDWIDTH_6_MHZ;
    }
    ok = false;

    WARNING_TUNING(QString("Invalid bandwidth parameter '%1', "
                           "falling back to 'auto'.").arg(bandwidth));

    return BANDWIDTH_AUTO;
}

fe_sec_voltage DVBTuning::parsePolarity(const QString &pol, bool &ok)
{
    char polarity = QChar(pol[0]).lower();
    ok = true;
    switch (polarity)
    {
        case 'v':
        case 'r': return SEC_VOLTAGE_13;
        case 'h':
        case 'l': return SEC_VOLTAGE_18;
        default: return SEC_VOLTAGE_OFF;
    }
}

fe_guard_interval DVBTuning::parseGuardInterval(const QString &gi, bool &ok)
{
    QString guard_interval = gi.lower();
    ok = true;
    if (guard_interval == "auto")      return GUARD_INTERVAL_AUTO;
    else if (guard_interval == "1/4")  return GUARD_INTERVAL_1_4;
    else if (guard_interval == "1/8")  return GUARD_INTERVAL_1_8;
    else if (guard_interval == "1/16") return GUARD_INTERVAL_1_16;
    else if (guard_interval == "1/32") return GUARD_INTERVAL_1_32;

    ok = false;

    WARNING_TUNING(QString("Invalid guard interval parameter '%1', "
                           "falling back to 'auto'.").arg(gi));

    return GUARD_INTERVAL_AUTO;
}

fe_transmit_mode DVBTuning::parseTransmission(const QString &tm, bool &ok)
{
    char transmission_mode = QChar(tm[0]).lower();
    ok = true;
    switch (transmission_mode)
    {
        case 'a': return TRANSMISSION_MODE_AUTO;
        case '2': return TRANSMISSION_MODE_2K;
        case '8': return TRANSMISSION_MODE_8K;
    }
    ok = false;

    WARNING_TUNING(QString("Invalid transmission mode parameter '%1', "
                           "falling back to 'auto'.").arg(tm));

    return TRANSMISSION_MODE_AUTO;
}

fe_hierarchy DVBTuning::parseHierarchy(const QString &hier, bool &ok)
{
    char hierarchy = QChar(hier[0]).lower();
    ok = true;
    switch (hierarchy)
    {
        case 'a': return HIERARCHY_AUTO;
        case 'n': return HIERARCHY_NONE;
        case '1': return HIERARCHY_1;
        case '2': return HIERARCHY_2;
        case '4': return HIERARCHY_4;
    }
    ok = false;

    WARNING_TUNING(QString("Invalid hierarchy parameter '%1', "
                           "falling back to 'auto'.").arg(hier));

    return HIERARCHY_AUTO;
}

fe_spectral_inversion DVBTuning::parseInversion(const QString &inv, bool &ok)
{
    char inversion = QChar(inv[0]).lower();
    ok = true;
    switch (inversion)
    {
        case '1': return INVERSION_ON;
        case '0': return INVERSION_OFF;
        case 'a': return INVERSION_AUTO;
    }
    ok = false;
    return INVERSION_AUTO;
}

fe_code_rate DVBTuning::parseCodeRate(const QString &cr, bool &ok)
{
    QString code_rate = cr.lower();
    ok = true;
    if (     code_rate == "none") return FEC_NONE;
    else if (code_rate == "auto") return FEC_AUTO;
    else if (code_rate ==  "8/9") return FEC_8_9;
    else if (code_rate ==  "7/8") return FEC_7_8;
    else if (code_rate ==  "6/7") return FEC_6_7;
    else if (code_rate ==  "5/6") return FEC_5_6;
    else if (code_rate ==  "4/5") return FEC_4_5;
    else if (code_rate ==  "3/4") return FEC_3_4;
    else if (code_rate ==  "2/3") return FEC_2_3;
    else if (code_rate ==  "1/2") return FEC_1_2;

    ok = false;

    WARNING_TUNING(QString("Invalid code rate parameter '%1', "
                           "falling back to 'auto'.").arg(cr));

    return FEC_AUTO;
}

fe_modulation DVBTuning::parseModulation(const QString &mod, bool &ok)
{
    QString modulation = mod.lower();
    ok = true;
    if (     modulation ==    "qpsk") return QPSK; 
    else if (modulation ==    "auto") return QAM_AUTO;
    else if (modulation ==       "a") return QAM_AUTO;
    else if (modulation =="qam_auto") return QAM_AUTO;
    else if (modulation == "qam_256") return QAM_256;
    else if (modulation == "qam_128") return QAM_128;
    else if (modulation ==  "qam_64") return QAM_64;
    else if (modulation ==  "qam_32") return QAM_32;
    else if (modulation ==  "qam_16") return QAM_16;
#if (DVB_API_VERSION_MINOR == 1)
    else if (modulation ==    "8vsb") return VSB_8;
    else if (modulation ==   "16vsb") return VSB_16;
#endif
    else if (modulation == "qam-256") return QAM_256;
    else if (modulation == "qam-128") return QAM_128;
    else if (modulation ==  "qam-64") return QAM_64;
    else if (modulation ==  "qam-32") return QAM_32;
    else if (modulation ==  "qam-16") return QAM_16;
#if (DVB_API_VERSION_MINOR == 1)
    else if (modulation ==   "8-vsb") return VSB_8;
    else if (modulation ==  "16-vsb") return VSB_16;
#endif

    ok = false;

    WARNING_TUNING(QString("Invalid constellation/modulation parameter '%1', "
                           "falling back to 'auto'.").arg(mod));

    return QAM_AUTO;
}

bool dvb_channel_t::Parse(
    fe_type_t type,
    QString frequency,         QString inversion,      QString symbolrate,
    QString fec,               QString polarity,       QString dvb_diseqc_type,
    QString diseqc_port,       QString diseqc_pos,     QString lnb_lof_switch,
    QString lnb_lof_hi,        QString lnb_lof_lo,     QString _sistandard,
    QString hp_code_rate,      QString lp_code_rate,   QString constellation,
    QString trans_mode,        QString guard_interval, QString hierarchy,
    QString modulation,        QString bandwidth)
{
    lock.lock();
        
    bool ok = false;
    if (FE_QPSK == type)
        ok = tuning.parseQPSK(
            frequency,       inversion,     symbolrate,   fec,   polarity,
            dvb_diseqc_type, diseqc_port,   diseqc_pos,
            lnb_lof_switch,  lnb_lof_hi,    lnb_lof_lo);
    else if (FE_QAM == type)
        ok = tuning.parseQAM(
            frequency,       inversion,     symbolrate,   fec,   modulation);
    else if (FE_OFDM == type)
        ok = tuning.parseOFDM(
            frequency,       inversion,     bandwidth,    hp_code_rate,
            lp_code_rate,    constellation, trans_mode,   guard_interval,
            hierarchy);
#if (DVB_API_VERSION_MINOR == 1)
    else if (FE_ATSC == type)
        ok = tuning.parseATSC(frequency, modulation);
#endif
        
    sistandard = _sistandard;
        
    lock.unlock();
    return ok;
}

static QString mod2str(fe_modulation mod)
{
    switch (mod)
    {
        case QPSK:     return    "QPSK";
        case QAM_AUTO: return    "Auto";
        case QAM_256:  return "QAM-256";
        case QAM_128:  return "QAM-128";
        case QAM_64:   return  "QAM-64";
        case QAM_32:   return  "QAM-32";
        case QAM_16:   return  "QAM-16";
#if (DVB_API_VERSION_MINOR == 1)
        case VSB_8:    return   "8-VSB";
        case VSB_16:   return  "16-VSB";
#endif
        default:      return "auto";
    }
    return "Unknown";
}

static QString mod2dbstr(fe_modulation mod)
{
    switch (mod)
    {
        case QPSK:     return "qpsk";
        case QAM_AUTO: return "auto";
        case QAM_16:   return "qam_16";
        case QAM_32:   return "qam_32";
        case QAM_64:   return "qam_64";
        case QAM_128:  return "qam_128";
        case QAM_256:  return "qam_256";
#if (DVB_API_VERSION_MINOR == 1)
        case VSB_8:    return "8vsb";
        case VSB_16:   return "16vsb";
#endif
        default:       return "auto";
    }
}

static QString coderate(fe_code_rate_t coderate)
{
    switch (coderate)
    {
        case FEC_NONE: return "None";
        case FEC_1_2:  return "1/2";
        case FEC_2_3:  return "2/3";
        case FEC_3_4:  return "3/4";
        case FEC_4_5:  return "4/5";
        case FEC_5_6:  return "5/6";
        case FEC_6_7:  return "6/7";
        case FEC_7_8:  return "7/8";
        case FEC_8_9:  return "8/9";
        default:       return "Auto";
    }
}

QString toString(const fe_type_t type)
{
    if (FE_QPSK == type)
        return "QPSK";
    else if (FE_QAM == type)
        return "QAM";
    else if (FE_OFDM == type)
        return "OFDM";
    else if (FE_ATSC == type)
        return "ATSC";
    return "Unknown";
}

QString toString(const struct dvb_frontend_parameters &params,
                 const fe_type_t type)
{
    return QString("freq(%1) type(%2)")
        .arg(params.frequency).arg(toString(type));
}

QString toString(fe_status status)
{
    QString str("");
    if (FE_HAS_SIGNAL  & status) str += "Signal,";
    if (FE_HAS_CARRIER & status) str += "Carrier,";
    if (FE_HAS_VITERBI & status) str += "FEC Stable,";
    if (FE_HAS_SYNC    & status) str += "Sync,";
    if (FE_HAS_LOCK    & status) str += "Lock,";
    if (FE_TIMEDOUT    & status) str += "Timed Out,";
    if (FE_REINIT      & status) str += "Reinit,";
    return str;
}

QString toString(const struct dvb_frontend_event &event,
                 const fe_type_t type)
{
    return QString("Event status(%1) %2")
        .arg(toString(event.status)).arg(toString(event.parameters, type));
}
