/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBTYPES_H
#define DVBTYPES_H

// POSIX headers
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <stdint.h>
#include <unistd.h>

// C++ headers
#include <vector>
#include <map>
using namespace std;

// Qt headers
#include <qdatetime.h>
#include <qstringlist.h>
#include <qmutex.h>

#include <linux/dvb/version.h>
#if (DVB_API_VERSION != 3)
#error "DVB driver includes with API version 3 not found!"
#endif

#ifndef DVB_API_VERSION_MINOR
#define DVB_API_VERSION_MINOR 0
#endif

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#if (DVB_API_VERSION >= 3 && DVB_API_VERSION_MINOR >= 1)
#    define USE_ATSC
#else
#warning DVB API version < 3.1
#warning ATSC will not be supported using the Linux DVB drivers
#    define FE_ATSC       (FE_OFDM+1)
#    define FE_CAN_8VSB   0x200000
#    define FE_CAN_16VSB  0x400000
#    define VSB_8         (fe_modulation)(QAM_AUTO+1)
#    define VSB_16        (fe_modulation)(QAM_AUTO+2)
#endif

#include "mpegdescriptors.h"
#include "mpegtables.h"

#define MPEG_TS_PKT_SIZE 188
#define DEF_DMX_BUF_SIZE  64 * 1024
#define MAX_SECTION_SIZE 4096
#define DMX_DONT_FILTER 0x2000

#ifdef FE_GET_EXTENDED_INFO
  #define dvb_fe_params dvb_frontend_parameters_new
#else
  #define dvb_fe_params dvb_frontend_parameters
#endif

QString toString(const fe_type_t);
QString toString(const struct dvb_fe_params&, const fe_type_t);
QString toString(fe_status);
QString toString(const struct dvb_frontend_event&, const fe_type_t);

//The following are a set of helper classes to allow easy translation
//between the actual dvb enums and db strings.

//Helper abstract template to do some of the mundain bits
//of translating the DVBParamHelpers
template <typename V> class DVBParamHelper 
{
protected:
    V value;

    struct Table
    {
        QString symbol;
        V value;
    };

    static bool parseParam(QString& symbol, V& value, Table *table)
    {
        Table *p = table;
        while (p->symbol!=NULL)
        {
            if (p->symbol==symbol.left(p->symbol.length()))
            {
                 symbol=symbol.mid(p->symbol.length());
                 value = p->value;
                 return true;
            }
            p++;
        }
        return false;
    }

public:
    DVBParamHelper(V _value) : value(_value) {}

    operator V() const { return value; }
    V operator=(V _value) {return value = _value;}
    bool operator==(const V& v) const {return value == v;}
};

class DVBInversion : public DVBParamHelper<fe_spectral_inversion_t>
{
protected:
    static Table confTable[];
    static Table vdrTable[];
    static Table parseTable[];
    static char* stringLookup[];

public:
    DVBInversion() : DVBParamHelper<fe_spectral_inversion_t>(INVERSION_AUTO) {}
    bool parseConf(QString& _value) 
           {return parseParam(_value,value,confTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,vdrTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(fe_spectral_inversion_t _value)
           {return stringLookup[_value];}
};

class DVBBandwidth : public DVBParamHelper<fe_bandwidth_t>
{
protected:
    static Table confTable[];
    static Table vdrTable[];
    static Table parseTable[];
    static char* stringLookup[];

public:
    DVBBandwidth() : DVBParamHelper<fe_bandwidth_t>(BANDWIDTH_AUTO) {}
    bool parseConf(QString& _value) 
           {return parseParam(_value,value,confTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,vdrTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(fe_bandwidth_t _value)
           {return stringLookup[_value];}
};

class DVBCodeRate : public DVBParamHelper<fe_code_rate_t>
{
protected:
    static Table confTable[];
    static Table vdrTable[];
    static Table parseTable[];
    static char* stringLookup[];

public:
    DVBCodeRate() : DVBParamHelper<fe_code_rate_t>(FEC_AUTO) {}

    bool parseConf(QString& _value) 
           {return parseParam(_value,value,confTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,vdrTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(fe_code_rate_t _value)
           {return stringLookup[_value];}
};

class DVBModulation : public DVBParamHelper<fe_modulation_t>
{
protected:
    static Table confTable[];
    static Table vdrTable[];
    static Table parseTable[];
    static char* stringLookup[];

public:
    DVBModulation() : DVBParamHelper<fe_modulation_t>(QAM_AUTO) {}

    bool parseConf(QString& _value) 
           {return parseParam(_value,value,confTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,vdrTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(fe_modulation_t _value)
           {return stringLookup[_value];}
};

class DVBTransmitMode : public DVBParamHelper<fe_transmit_mode_t>
{
protected:
    static Table confTable[];
    static Table vdrTable[];
    static Table parseTable[];
    static char* stringLookup[];

public:
    DVBTransmitMode() : DVBParamHelper<fe_transmit_mode_t>(TRANSMISSION_MODE_AUTO) {}

    bool parseConf(QString& _value) 
           {return parseParam(_value,value,confTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,vdrTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(fe_transmit_mode_t _value)
           {return stringLookup[_value];}
};

class DVBGuardInterval : public DVBParamHelper<fe_guard_interval_t>
{
protected:
    static Table confTable[];
    static Table vdrTable[];
    static Table parseTable[];
    static char* stringLookup[];

public:
    DVBGuardInterval() : DVBParamHelper<fe_guard_interval_t>(GUARD_INTERVAL_AUTO) {}

    bool parseConf(QString& _value) 
           {return parseParam(_value,value,confTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,vdrTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(fe_guard_interval_t _value)
           {return stringLookup[_value];}
};

class DVBHierarchy : public DVBParamHelper<fe_hierarchy_t>
{
protected:
    static Table confTable[];
    static Table vdrTable[];
    static Table parseTable[];
    static char* stringLookup[];

public:
    DVBHierarchy() : DVBParamHelper<fe_hierarchy_t>(HIERARCHY_AUTO) {}

    bool parseConf(QString& _value) 
           {return parseParam(_value,value,confTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,vdrTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(fe_hierarchy_t _value)
           {return stringLookup[_value];}
};

enum PolarityValues {Vertical,Horizontal,Right,Left};
class DVBPolarity : public DVBParamHelper<PolarityValues>
{
protected:
    static Table parseTable[];
    static char* stringLookup[];
public:
    DVBPolarity() :  DVBParamHelper<PolarityValues>(Vertical) { }

    bool parseConf(QString& _value) 
           {return parseParam(_value,value,parseTable);}
    bool parseVDR(QString& _value)
           {return parseParam(_value,value,parseTable);}
    bool parse(QString& _value)
           {return parseParam(_value,value,parseTable);}

    QString toString() const {return toString(value);}
    static QString toString(PolarityValues _value) 
           {return stringLookup[_value];}
};

typedef vector<uint16_t> dvb_pid_t;
// needs to add provider id so dvbcam doesnt require parsing
// of the pmt and or the pmtcache
typedef vector<uint16_t> dvb_caid_t;

extern bool equal_qpsk(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
#ifdef FE_GET_EXTENDED_INFO
extern bool equal_dvbs2(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
#endif
extern bool equal_atsc(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
extern bool equal_qam(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
extern bool equal_ofdm(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op, uint range);
extern bool equal_type(
    const struct dvb_fe_params &p,
    const struct dvb_fe_params &op,
    fe_type_t type, uint freq_range);

class DVBTuning
{
  public:
    DVBTuning()
      : voltage(SEC_VOLTAGE_OFF), tone(SEC_TONE_OFF), 
        diseqc_type(0), diseqc_port(0), diseqc_pos(0.0f),
        lnb_lof_switch(0), lnb_lof_hi(0), lnb_lof_lo(0)
    {
        bzero(&params, sizeof(dvb_fe_params));
    }

    struct dvb_fe_params params;
    fe_sec_voltage_t    voltage;
    fe_sec_tone_mode_t  tone;
    unsigned int        diseqc_type;
    unsigned int        diseqc_port;
    float               diseqc_pos;
    unsigned int        lnb_lof_switch;
    unsigned int        lnb_lof_hi;
    unsigned int        lnb_lof_lo;

    bool equalQPSK(const DVBTuning& other, uint range = 0) const
        { return equal_qpsk(params, other.params, range);  }
    bool equalATSC(const DVBTuning& other, uint range = 0) const
        { return equal_atsc(params, other.params, range);  }
    bool equalQAM( const DVBTuning& other, uint range = 0) const
        { return equal_qam(params, other.params, range);   }
    bool equalOFDM(const DVBTuning& other, uint range = 0) const
        { return equal_ofdm(params, other.params, range);  }
    bool equal(fe_type_t type, const DVBTuning& other, uint range = 0) const
        { return equal_type(params, other.params, type, range); }

    // Helper functions to get the paramaters as DB friendly strings
    char InversionChar() const;
    char TransmissionModeChar() const;
    char BandwidthChar() const;
    char HierarchyChar() const;
    QString ConstellationDB() const;
    QString ModulationDB() const;

    // Helper functions to parse params from DB friendly strings
    static fe_bandwidth      parseBandwidth(    const QString&, bool &ok);
    static fe_sec_voltage    parsePolarity(     const QString&, bool &ok);
    static fe_guard_interval parseGuardInterval(const QString&, bool &ok);
    static fe_transmit_mode  parseTransmission( const QString&, bool &ok);
    static fe_hierarchy      parseHierarchy(    const QString&, bool &ok);
    static fe_spectral_inversion parseInversion(const QString&, bool &ok);
    static fe_code_rate      parseCodeRate(     const QString&, bool &ok);
    static fe_modulation     parseModulation(   const QString&, bool &ok);

    // Helper functions for UI and DB
    uint Frequency()      const { return params.frequency; }
    uint QPSKSymbolRate() const { return params.u.qpsk.symbol_rate; }
    uint QAMSymbolRate()  const { return params.u.qam.symbol_rate; }
    QString GuardIntervalString() const;
    QString InversionString() const;
    QString BandwidthString() const;
    QString TransmissionModeString() const;
    QString HPCodeRateString() const;
    QString LPCodeRateString() const;
    QString QAMInnerFECString() const;
    QString ModulationString() const;
    QString ConstellationString() const;
    QString HierarchyString() const;
    QString toString(fe_type_t type) const;

    bool parseATSC(const QString& frequency,      const QString modulation);

    bool parseOFDM(const QString& frequency,      const QString& inversion,
                   const QString& bandwidth,      const QString& coderate_hp,
                   const QString& coderate_lp,    const QString& constellation,
                   const QString& trans_mode,     const QString& guard_interval,
                   const QString& hierarchy);

    bool parseQPSK(const QString& frequency,      const QString& inversion,
                   const QString& symbol_rate,    const QString& fec_inner,
                   const QString& pol,            const QString& diseqc_type,
                   const QString& diseqc_port,    const QString& diseqc_pos,
                   const QString& lnb_lof_switch, const QString& lnb_lof_hi,
                   const QString& lnb_lof_lo);

    bool parseQAM(const QString& frequency,       const QString& inversion,
                  const QString& symbol_rate,     const QString& fec_inner,
                  const QString& modulation);

#ifdef FE_GET_EXTENDED_INFO
    bool equalDVBS2(const DVBTuning& other, uint range = 0) const
        { return equal_dvbs2(params, other.params, range);  }
    uint DVBS2SymbolRate() const { return params.u.qpsk2.symbol_rate; }
    bool parseDVBS2(const QString& frequency,      const QString& inversion,
                   const QString& symbol_rate,    const QString& fec_inner,
                   const QString& pol,            const QString& diseqc_type,
                   const QString& diseqc_port,    const QString& diseqc_pos,
                   const QString& lnb_lof_switch, const QString& lnb_lof_hi,
                   const QString& lnb_lof_lo,     const QString& modulation);
#endif
};

#endif // DVB_TYPES_H
