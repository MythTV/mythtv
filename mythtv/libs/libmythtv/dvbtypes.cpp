// -*- Mode: c++ -*-

// Qt headers
#include <qdeepcopy.h>

// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbtypes.h"

static QString mod2str(fe_modulation mod);
static QString mod2dbstr(fe_modulation mod);
static QString coderate(fe_code_rate_t coderate);

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
        case VSB_8:    return   "8-VSB";
        case VSB_16:   return  "16-VSB";
#ifdef FE_GET_EXTENDED_INFO
	case MOD_8PSK: return    "8PSK";
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
        case VSB_8:    return "8vsb";
        case VSB_16:   return "16vsb";
#ifdef FE_GET_EXTENDED_INFO
	case MOD_8PSK: return "8psk";
#endif
        default:       return "auto";
    }
}

static QString coderate(fe_code_rate_t coderate)
{
    switch (coderate)
    {
        case FEC_NONE: return "none";
        case FEC_1_2:  return "1/2";
        case FEC_2_3:  return "2/3";
        case FEC_3_4:  return "3/4";
        case FEC_4_5:  return "4/5";
        case FEC_5_6:  return "5/6";
        case FEC_6_7:  return "6/7";
        case FEC_7_8:  return "7/8";
        case FEC_8_9:  return "8/9";
        default:       return "auto";
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
#ifdef FE_GET_EXTENDED_INFO
    else if (FE_DVB_S2 == type)
        return "DVB_S2";
#endif
    return "Unknown";
}

QString toString(const struct dvb_fe_params &params,
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
    const struct dvb_fe_params *params;
    params = (const struct dvb_fe_params *)(&event.parameters);
    return QString("Event status(%1) %2")
        .arg(toString(event.status))
        .arg(toString(*params, type));
}
