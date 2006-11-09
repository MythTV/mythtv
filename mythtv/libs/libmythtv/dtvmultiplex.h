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
#include <qstring.h>

// MythTV headers
#include "dtvconfparserhelpers.h"

class DTVMultiplex
{
  public:
    DTVMultiplex()
        : frequency(0), symbolrate(0), mplex(0), sistandard(QString::null) { }
    DTVMultiplex(const DTVMultiplex &other) { (*this) = other; }

    DTVMultiplex &operator=(const DTVMultiplex &other);

    bool operator==(const DTVMultiplex &m) const;
 
    void Clear(void) { DTVMultiplex mux; (*this) = mux; }

    bool FillFromDB(DTVTunerType type, uint mplexid);

    bool IsEqual(DTVTunerType type, const DTVMultiplex& other,
                 uint freq_range = 0) const;

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

    bool ParseTuningParams(
        DTVTunerType type,
        QString frequency,    QString inversion,      QString symbolrate,
        QString fec,          QString polarity,
        QString hp_code_rate, QString lp_code_rate,   QString constellation,
        QString trans_mode,   QString guard_interval, QString hierarchy,
        QString modulation,   QString bandwidth);

    QString toString() const;

  public:
    // Basic tuning
    uint64_t         frequency;
    uint64_t         symbolrate;
    DTVInversion     inversion;
    DTVBandwidth     bandwidth;
    DTVCodeRate      hp_code_rate;  ///< High Priority FEC rate
    DTVCodeRate      lp_code_rate;  ///< Low Priority FEC rate
    //DTVModulation    constellation; ///< Modulation for OFDM, TODO Remove
    DTVModulation    modulation;
    DTVTransmitMode  trans_mode;
    DTVGuardInterval guard_interval;
    DTVHierarchy     hierarchy;
    DTVPolarity      polarity;
    DTVCodeRate      fec; ///< Inner Forward Error Correction rate

    // Optional additional info
    uint             mplex;
    QString          sistandard;
};

#endif // _DTVMULTIPLEX_H_
