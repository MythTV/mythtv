#include <mythcontext.h>
#include "dvbtypes.h"

QString DVBTuning::inversion() const
{ 
    switch(params.inversion)
    {
    case INVERSION_ON:
        return "1";
    case INVERSION_OFF:
        return "0";
    default:
        return "a";
    }
}

QString DVBTuning::bandwidth() const
{
    switch (params.u.ofdm.bandwidth)
    {
       case BANDWIDTH_8_MHZ:
           return "8";
       case BANDWIDTH_7_MHZ:
           return "7";
       case BANDWIDTH_6_MHZ:
           return "6";
       default:
               return "auto";
    }
}

QString DVBTuning::coderate(fe_code_rate_t coderate) const
{
    switch (coderate) 
    {
    case FEC_NONE:
         return "none";
    case FEC_1_2:
        return "1/2";
    case FEC_2_3:
        return "2/3";
    case FEC_3_4:
        return "3/4";
    case FEC_4_5:
        return "4/5";
    case FEC_5_6:
        return "5/6";
    case FEC_6_7:
        return "6/7";
    case FEC_7_8:
        return "7/8";
    default:
        return "auto";
    }
}

QString DVBTuning::constellation() const
{
    switch (params.u.ofdm.constellation)
    {
       case QPSK:
               return "qpsk";
       case QAM_16:
               return "qam_16";
       case QAM_32:
               return "qam_32";
       case QAM_64:
               return "qam_64";
       case QAM_128:
               return "qam_128";
       case QAM_256:
               return "qam_256";
       default:
               return "auto";
    }
}

QString DVBTuning::transmissionMode() const
{
    //TransmissionMode
    switch (params.u.ofdm.transmission_mode) 
    {
    case TRANSMISSION_MODE_2K:
        return "2";
    case TRANSMISSION_MODE_8K:
        return "8";
    default:
        return "auto";
    }    
}

QString DVBTuning::guardInterval() const
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

QString DVBTuning::hierarchy() const
{
   // Hierarchy
    switch (params.u.ofdm.hierarchy_information) 
    {
       case HIERARCHY_NONE:
               return "n";
       case HIERARCHY_1:
               return "1";
       case HIERARCHY_2:
               return "2";
       case HIERARCHY_4:
               return "4";
       default:
               return "a";
    }
}

QString DVBTuning::modulation() const
{
    switch(params.u.vsb.modulation)
    {
        case QPSK:
            return "qpsk";
        case QAM_16:
            return "qam_16";
        case QAM_32:  
            return "qam_32";
        case QAM_64:   
            return "qam_64";
        case QAM_128:
            return "qam_128";
        case QAM_256:
            return "qam_256";
#if (DVB_API_VERSION_MINOR == 1)
        case VSB_8:
            return "8vsb";
        case VSB_16:
            return "16vsb";
#endif
        case QAM_AUTO:
        default:
            return "auto";
    }
}

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
   (void)t;
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



