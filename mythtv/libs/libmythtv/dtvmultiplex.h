/* -*- Mode: c++ -*-
 *  DTVMultiplex
 *  Copyright (c) 2005,2006 by Daniel Kristjansson
 *  Digital multiplexes info class
 */

#ifndef _DTVMULTIPLEX_H_
#define _DTVMULTIPLEX_H_

// C++ headers
#include <cstdint>

// Qt headers
#include <QString>

// MythTV headers
#include "dtvconfparserhelpers.h"
#include "channelinfo.h"
#include "iptvtuningdata.h"
#include "mythtvexp.h"

class MPEGDescriptor;

class MTV_PUBLIC DTVMultiplex
{
  public:
  DTVMultiplex() = default;
    DTVMultiplex(const DTVMultiplex &/*other*/) = default;
    DTVMultiplex &operator=(const DTVMultiplex &/*other*/) = default;
    virtual ~DTVMultiplex() = default;

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
        const QString &coderate_lp, const QString &modulation,
        const QString &trans_mode,  const QString &guard_interval,
        const QString &hierarchy);

    bool ParseDVB_S_and_C(
        const QString &frequency,    const QString &inversion,
        const QString &symbol_rate,  const QString &fec_inner,
        const QString &modulation,   const QString &polarity);

    bool ParseDVB_S(
        const QString &frequency,    const QString &inversion,
        const QString &symbol_rate,  const QString &fec_inner,
        const QString &modulation,   const QString &polarity);

    bool ParseDVB_C(
        const QString &frequency,    const QString &inversion,
        const QString &symbol_rate,  const QString &fec_inner,
        const QString &modulation,   const QString &polarity,
        const QString &mod_sys);

    bool ParseDVB_S2(
        const QString &frequency,    const QString &inversion,
        const QString &symbol_rate,  const QString &fec_inner,
        const QString &modulation,   const QString &polarity,
        const QString &mod_sys,      const QString &rolloff);

    bool ParseDVB_T2(
        const QString &frequency,   const QString &inversion,
        const QString &bandwidth,   const QString &coderate_hp,
        const QString &coderate_lp, const QString &modulation,
        const QString &trans_mode,  const QString &guard_interval,
        const QString &hierarchy,   const QString &mod_sys);

    bool ParseTuningParams(
        DTVTunerType type,
        const QString& frequency,    const QString& inversion,      const QString& symbolrate,
        const QString& fec,          const QString& polarity,
        const QString& hp_code_rate, const QString& lp_code_rate,   const QString& ofdm_modulation,
        const QString& trans_mode,   const QString& guard_interval, const QString& hierarchy,
        const QString& modulation,   const QString& bandwidth,      const QString& mod_sys,
        const QString& rolloff);

    QString toString() const;

  public:
    // Basic tuning
    uint64_t         m_frequency  {0};
    uint64_t         m_symbolrate {0};
    DTVInversion     m_inversion;
    DTVBandwidth     m_bandwidth;
    DTVCodeRate      m_hp_code_rate;    ///< High Priority FEC rate
    DTVCodeRate      m_lp_code_rate;    ///< Low Priority FEC rate
    DTVModulation    m_modulation;
    DTVTransmitMode  m_trans_mode;
    DTVGuardInterval m_guard_interval;
    DTVHierarchy     m_hierarchy;
    DTVPolarity      m_polarity;
    DTVCodeRate      m_fec;             ///< Inner Forward Error Correction rate
    DTVModulationSystem m_mod_sys;      ///< Modulation system
    DTVRollOff       m_rolloff;

    // Optional additional info
    uint             m_mplex      {0};
    QString          m_sistandard;
    IPTVTuningData   m_iptv_tuning;
};

class MTV_PUBLIC ScanDTVTransport : public DTVMultiplex
{
  public:
    ScanDTVTransport() :
        DTVMultiplex() { }
    ScanDTVTransport(const DTVMultiplex &mplex, DTVTunerType tt, uint cid) :
        DTVMultiplex(mplex), m_tuner_type(tt), m_cardid(cid) { }
    virtual ~ScanDTVTransport() = default;

    bool FillFromDB(DTVTunerType type, uint mplexid) override; // DTVMultiplex
    uint SaveScan(uint scanid) const;

    bool ParseTuningParams(
        DTVTunerType type,
        const QString& frequency,    const QString& inversion,      const QString& symbolrate,
        const QString& fec,          const QString& polarity,
        const QString& hp_code_rate, const QString& lp_code_rate,   const QString& ofdm_modulation,
        const QString& trans_mode,   const QString& guard_interval, const QString& hierarchy,
        const QString& modulation,   const QString& bandwidth,
        const QString& mod_sys,      const QString& rolloff);

  public:
    DTVTunerType          m_tuner_type {DTVTunerType::kTunerTypeUnknown};
    uint                  m_cardid     {0};
    ChannelInsertInfoList m_channels;
};
typedef vector<ScanDTVTransport> ScanDTVTransportList;

#endif // _DTVMULTIPLEX_H_
