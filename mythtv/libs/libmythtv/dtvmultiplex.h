/* -*- Mode: c++ -*-
 *  DTVMultiplex
 *  Copyright (c) 2005,2006 by Daniel Kristjansson
 *  Digital multiplexes info class
 */

#ifndef _DTVMULTIPLEX_H_
#define _DTVMULTIPLEX_H_

// POSIX headers
#include <stdint.h>

// Qt headers
#include <QString>

// MythTV headers
#include "dtvconfparserhelpers.h"
#include "channelinfo.h"

class MPEGDescriptor;

class DTVMultiplex
{
  public:
    DTVMultiplex()
        : frequency(0), symbolrate(0), mplex(0), sistandard(QString::null) { }
    DTVMultiplex(const DTVMultiplex &other);
    DTVMultiplex &operator=(const DTVMultiplex &other);
    virtual ~DTVMultiplex() { }

    bool operator==(const DTVMultiplex &m) const;

    void Clear(void) { (*this) = DTVMultiplex(); }

    virtual bool FillFromDB(DTVTunerType type, uint mplexid);

    bool FillFromDeliverySystemDesc(DTVTunerType type, const MPEGDescriptor &desc);

    bool IsEqual(DTVTunerType type, const DTVMultiplex& other,
                 uint freq_range = 0, bool fuzzy = false) const;

    bool ParseATSC(const QString &frequency, const QString &modulation);

    bool ParseDVB_T(
        const QString &frequency,   const QString &inversion,
        const QString &bandwidth,   const QString &coderate_hp,
        const QString &coderate_lp, const QString &constellation,
        const QString &trans_mode,  const QString &guard_interval,
        const QString &hierarchy);

    bool ParseDVB_S_and_C(
        const QString &frequency,    const QString &inversion,
        const QString &symbol_rate,  const QString &fec_inner,
        const QString &modulation,   const QString &polarity);

    bool ParseDVB_S2(
        const QString &frequency,    const QString &inversion,
        const QString &symbol_rate,  const QString &fec_inner,
        const QString &modulation,   const QString &polarity,
        const QString &mod_sys,      const QString &rolloff);

    bool ParseTuningParams(
        DTVTunerType type,
        QString frequency,    QString inversion,      QString symbolrate,
        QString fec,          QString polarity,
        QString hp_code_rate, QString lp_code_rate,   QString constellation,
        QString trans_mode,   QString guard_interval, QString hierarchy,
        QString modulation,   QString bandwidth,      QString mod_sys,
        QString rolloff);

    QString toString() const;

  public:
    // Basic tuning
    uint64_t         frequency;
    uint64_t         symbolrate;
    DTVInversion     inversion;
    DTVBandwidth     bandwidth;
    DTVCodeRate      hp_code_rate;  ///< High Priority FEC rate
    DTVCodeRate      lp_code_rate;  ///< Low Priority FEC rate
    DTVModulation    modulation;
    DTVTransmitMode  trans_mode;
    DTVGuardInterval guard_interval;
    DTVHierarchy     hierarchy;
    DTVPolarity      polarity;
    DTVCodeRate      fec; ///< Inner Forward Error Correction rate
    DTVModulationSystem mod_sys; ///< modulation system (only DVB-S or DVB-S2 atm)
    DTVRollOff       rolloff;

    // Optional additional info
    uint             mplex;
    QString          sistandard;
};

class ScanDTVTransport : public DTVMultiplex
{
  public:
    ScanDTVTransport() :
        DTVMultiplex(), tuner_type(DTVTunerType::kTunerTypeUnknown),
        cardid(0) { }
    ScanDTVTransport(const DTVMultiplex &mplex, DTVTunerType tt, uint cid) :
        DTVMultiplex(mplex), tuner_type(tt), cardid(cid) { }
    virtual ~ScanDTVTransport() {}

    virtual bool FillFromDB(DTVTunerType type, uint mplexid);
    uint SaveScan(uint scanid) const;

    bool ParseTuningParams(
        DTVTunerType type,
        QString frequency,    QString inversion,      QString symbolrate,
        QString fec,          QString polarity,
        QString hp_code_rate, QString lp_code_rate,   QString constellation,
        QString trans_mode,   QString guard_interval, QString hierarchy,
        QString modulation,   QString bandwidth,
        QString mod_sys,      QString rolloff);

  public:
    DTVTunerType          tuner_type;
    uint                  cardid;
    ChannelInsertInfoList channels;
};
typedef vector<ScanDTVTransport> ScanDTVTransportList;

#endif // _DTVMULTIPLEX_H_
