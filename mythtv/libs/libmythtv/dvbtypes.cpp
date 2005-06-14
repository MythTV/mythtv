#include <mythcontext.h>
#include "dvbtypes.h"

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
    "1", // INVERSION_OFF,
    "0", // INVERSION_ON,
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

bool DVBTuning::parseATSC(const QString& frequency, const QString modulation)
{
#if (DVB_API_VERSION_MINOR == 1)
    params.frequency = frequency.toInt();

    dvb_vsb_parameters& p = params.u.vsb;

    if (modulation == "qam_256")   p.modulation = QAM_256;
    else if (modulation == "qam_64")    p.modulation = QAM_64;
    else if (modulation == "8vsb")    p.modulation = VSB_8;
    else if (modulation == "16vsb")    p.modulation = VSB_16;
    else {
        WARNING_TUNING("Invalid modulationulation parameter '"<<
                modulation << "', falling back to '8vsb'.");
        p.modulation = VSB_8;
    }
#else
   (void)frequency;
   (void)modulation;
#endif
    return true;
}

bool DVBTuning::parseOFDM(const QString& frequency,const QString& inversion,
                          const QString& bandwidth,const QString& coderate_hp,
                          const QString& coderate_lp,const QString& constellation,
                          const QString& trans_mode,const QString& guard_interval,
                           const QString& hierarchy)
{
    dvb_ofdm_parameters& p = params.u.ofdm;

    params.frequency = frequency.toInt();

    switch(inversion[0]) {
        case '1': params.inversion = INVERSION_ON;    break;
        case '0': params.inversion = INVERSION_OFF;   break;
        case 'a': params.inversion = INVERSION_AUTO;  break;
        default : ERROR_TUNING("Invalid inversion, aborting.");
                  return false;
    }

    switch(bandwidth[0]) {
        case 'a': p.bandwidth = BANDWIDTH_AUTO; break;
        case '8': p.bandwidth = BANDWIDTH_8_MHZ; break;
        case '7': p.bandwidth = BANDWIDTH_7_MHZ; break;
        case '6': p.bandwidth = BANDWIDTH_6_MHZ; break;
        default: WARNING_TUNING("Invalid bandwidth parameter '" << bandwidth
                         << "', falling back to 'auto'.");
                 p.bandwidth = BANDWIDTH_AUTO;
    }

    if (coderate_hp == "none")      p.code_rate_HP = FEC_NONE;
    else if (coderate_hp == "auto") p.code_rate_HP = FEC_AUTO;
    else if (coderate_hp == "8/9")  p.code_rate_HP = FEC_8_9;
    else if (coderate_hp == "7/8")  p.code_rate_HP = FEC_7_8;
    else if (coderate_hp == "6/7")  p.code_rate_HP = FEC_6_7;
    else if (coderate_hp == "5/6")  p.code_rate_HP = FEC_5_6;
    else if (coderate_hp == "4/5")  p.code_rate_HP = FEC_4_5;
    else if (coderate_hp == "3/4")  p.code_rate_HP = FEC_3_4;
    else if (coderate_hp == "2/3")  p.code_rate_HP = FEC_2_3;
    else if (coderate_hp == "1/2")  p.code_rate_HP = FEC_1_2;
    else {
        WARNING_TUNING("Invalid hp code rate parameter '" << coderate_hp
                << "', falling back to 'auto'.");
        p.code_rate_HP = FEC_AUTO;
    }

    if (coderate_lp == "none")      p.code_rate_LP = FEC_NONE;
    else if (coderate_lp == "auto") p.code_rate_LP = FEC_AUTO;
    else if (coderate_lp == "8/9")  p.code_rate_LP = FEC_8_9;
    else if (coderate_lp == "7/8")  p.code_rate_LP = FEC_7_8;
    else if (coderate_lp == "6/7")  p.code_rate_LP = FEC_6_7;
    else if (coderate_lp == "5/6")  p.code_rate_LP = FEC_5_6;
    else if (coderate_lp == "4/5")  p.code_rate_LP = FEC_4_5;
    else if (coderate_lp == "3/4")  p.code_rate_LP = FEC_3_4;
    else if (coderate_lp == "2/3")  p.code_rate_LP = FEC_2_3;
    else if (coderate_lp == "1/2")  p.code_rate_LP = FEC_1_2;
    else {
        WARNING_TUNING("Invalid lp code rate parameter '" << coderate_lp
                << "', falling back to 'auto'.");
        p.code_rate_LP = FEC_AUTO;
    }

    if (constellation == "auto")            p.constellation = QAM_AUTO;
    else if (constellation == "qpsk")       p.constellation = QPSK;
    else if (constellation == "qam_16")     p.constellation = QAM_16;
    else if (constellation == "qam_32")     p.constellation = QAM_32;
    else if (constellation == "qam_64")     p.constellation = QAM_64;
    else if (constellation == "qam_128")    p.constellation = QAM_128;
    else if (constellation == "qam_256")    p.constellation = QAM_256;
    else {
        WARNING_TUNING("Invalid constellation parameter '" << constellation
                << "', falling bac to 'auto'.");
        p.constellation = QAM_AUTO;
    }

    switch (trans_mode[0])
    {
        case 'a': p.transmission_mode = TRANSMISSION_MODE_AUTO; break;
        case '2': p.transmission_mode = TRANSMISSION_MODE_2K; break;
        case '8': p.transmission_mode = TRANSMISSION_MODE_8K; break;
        default: WARNING_TUNING("invalid transmission mode parameter '" << trans_mode
                         << "', falling back to 'auto'.");
                  p.transmission_mode = TRANSMISSION_MODE_AUTO;
    }

    switch (hierarchy[0]) {
        case 'a': p.hierarchy_information = HIERARCHY_AUTO; break;
        case 'n': p.hierarchy_information = HIERARCHY_NONE; break;
        case '1': p.hierarchy_information = HIERARCHY_1; break;
        case '2': p.hierarchy_information = HIERARCHY_2; break;
        case '4': p.hierarchy_information = HIERARCHY_4; break;
        default: WARNING_TUNING("invalid hierarchy parameter '" << hierarchy
                         << "', falling back to 'auto'.");
                 p.hierarchy_information = HIERARCHY_AUTO;
    }

    if (guard_interval == "auto")       p.guard_interval = GUARD_INTERVAL_AUTO;
    else if (guard_interval == "1/4")     p.guard_interval = GUARD_INTERVAL_1_4;
    else if (guard_interval == "1/8")     p.guard_interval = GUARD_INTERVAL_1_8;
    else if (guard_interval == "1/16")    p.guard_interval = GUARD_INTERVAL_1_16;
    else if (guard_interval == "1/32")    p.guard_interval = GUARD_INTERVAL_1_32;
    else {
        WARNING_TUNING("invalid guard interval parameter '" << guard_interval
                << "', falling back to 'auto'.");
        p.guard_interval = GUARD_INTERVAL_AUTO;
    }
    return true;
}

// TODO: Add in DiseqcPos when diseqc class supports it
bool DVBTuning::parseQPSK(const QString& frequency, const QString& inversion,
                          const QString& symbol_rate, const QString& fec_inner,
                          const QString& pol, const QString& _diseqc_type,
                          const QString& _diseqc_port,
                          const QString& _diseqc_pos,
                          const QString& _lnb_lof_switch, 
                          const QString& _lnb_lof_hi,
                          const QString& _lnb_lof_lo)
{
    dvb_qpsk_parameters& p = params.u.qpsk;

    params.frequency = frequency.toInt();

    switch(inversion[0]) {
        case '1': params.inversion = INVERSION_ON;    break;
        case '0': params.inversion = INVERSION_OFF;   break;
        case 'a': params.inversion = INVERSION_AUTO;  break;
        default : ERROR_TUNING("Invalid inversion, aborting.");
                  return false;
    }

    p.symbol_rate = symbol_rate.toInt();
    if (p.symbol_rate == 0) {
        ERROR_TUNING("Invalid symbol rate parameter '" << symbol_rate
              << "', aborting.");
        return false;
    }

    if (fec_inner == "none")        p.fec_inner = FEC_NONE;
    else if (fec_inner == "auto")   p.fec_inner = FEC_AUTO;
    else if (fec_inner == "8/9")    p.fec_inner = FEC_8_9;
    else if (fec_inner == "7/8")    p.fec_inner = FEC_7_8;
    else if (fec_inner == "6/7")    p.fec_inner = FEC_6_7;
    else if (fec_inner == "5/6")    p.fec_inner = FEC_5_6;
    else if (fec_inner == "4/5")    p.fec_inner = FEC_4_5;
    else if (fec_inner == "3/4")    p.fec_inner = FEC_3_4;
    else if (fec_inner == "2/3")    p.fec_inner = FEC_2_3;
    else if (fec_inner == "1/2")    p.fec_inner = FEC_1_2;
    else {
        WARNING_TUNING("Invalid fec_inner parameter '" << fec_inner
                << "', falling back to 'auto'.");
        p.fec_inner = FEC_AUTO;
    }

    switch(pol[0]) {
        case 'v': case 'r': voltage = SEC_VOLTAGE_13; break;
        case 'h': case 'l': voltage = SEC_VOLTAGE_18; break;
        default : ERROR_TUNING("Invalid polarization, aborting.");
                  return false;
    }

    diseqc_type = _diseqc_type.toInt();
    diseqc_port = _diseqc_port.toInt();
    diseqc_pos = _diseqc_pos.toFloat();
    lnb_lof_switch = _lnb_lof_switch.toInt();
    lnb_lof_hi = _lnb_lof_hi.toInt();
    lnb_lof_lo = _lnb_lof_lo.toInt();
    return true;
}

bool DVBTuning::parseQAM(const QString& frequency, const QString& inversion,
                         const QString& symbol_rate, const QString& fec_inner,
                         const QString& modulation)
{
    dvb_qam_parameters& p = params.u.qam;

    params.frequency = frequency.toInt();

    switch(inversion[0]) {
        case '1': params.inversion = INVERSION_ON;    break;
        case '0': params.inversion = INVERSION_OFF;   break;
        case 'a': params.inversion = INVERSION_AUTO;  break;
        default : ERROR_TUNING("Invalid inversion, aborting.");
                  return false;
    }

    p.symbol_rate = symbol_rate.toInt();
    if (p.symbol_rate == 0) {
        ERROR_TUNING("Invalid symbol rate parameter '" << symbol_rate
              << "', aborting.");
        return false;
    }

    if (fec_inner == "none")        p.fec_inner = FEC_NONE;
    else if (fec_inner == "auto")   p.fec_inner = FEC_AUTO;
    else if (fec_inner == "8/9")    p.fec_inner = FEC_8_9;
    else if (fec_inner == "7/8")    p.fec_inner = FEC_7_8;
    else if (fec_inner == "6/7")    p.fec_inner = FEC_6_7;
    else if (fec_inner == "5/6")    p.fec_inner = FEC_5_6;
    else if (fec_inner == "4/5")    p.fec_inner = FEC_4_5;
    else if (fec_inner == "3/4")    p.fec_inner = FEC_3_4;
    else if (fec_inner == "2/3")    p.fec_inner = FEC_2_3;
    else if (fec_inner == "1/2")    p.fec_inner = FEC_1_2;
    else {
        WARNING_TUNING("Invalid fec_inner parameter '" << fec_inner
                << "', falling back to 'auto'.");
        p.fec_inner = FEC_AUTO;
    }

    if (modulation == "qpsk")           p.modulation = QPSK;
    else if (modulation == "auto")      p.modulation = QAM_AUTO;
    else if (modulation == "qam_256")   p.modulation = QAM_256;
    else if (modulation == "qam_128")   p.modulation = QAM_128;
    else if (modulation == "qam_64")    p.modulation = QAM_64;
    else if (modulation == "qam_32")    p.modulation = QAM_32;
    else if (modulation == "qam_16")    p.modulation = QAM_16;
    else {
        WARNING_TUNING("Invalid modulationulation parameter '" << modulation
                << "', falling bac to 'auto'.");
        p.modulation = QAM_AUTO;
    }

    return true;
}



