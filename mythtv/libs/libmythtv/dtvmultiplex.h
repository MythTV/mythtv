/* -*- Mode: c++ -*-
 *  DTVMultiplex
 *  Copyright (c) 2005,2006 by Daniel Kristjansson
 *  Digital multiplexes info class
 */

#ifndef DTVMULTIPLEX_H
#define DTVMULTIPLEX_H

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
    uint64_t            m_frequency  {0};
    uint64_t            m_symbolRate {0};
    DTVInversion        m_inversion;
    DTVBandwidth        m_bandwidth;
    DTVCodeRate         m_hpCodeRate;       // High Priority FEC rate
    DTVCodeRate         m_lpCodeRate;       // Low Priority FEC rate
    DTVModulation       m_modulation;
    DTVTransmitMode     m_transMode;
    DTVGuardInterval    m_guardInterval;
    DTVHierarchy        m_hierarchy;
    DTVPolarity         m_polarity;
    DTVCodeRate         m_fec;              // Inner Forward Error Correction rate
    DTVModulationSystem m_modSys;           // Modulation system
    DTVRollOff          m_rolloff;

    // Optional additional info
    uint             m_mplex      {0};
    QString          m_sistandard;
    IPTVTuningData   m_iptvTuning;
};

class MTV_PUBLIC ScanDTVTransport : public DTVMultiplex
{
  public:
    ScanDTVTransport() = default;
    ScanDTVTransport(const DTVMultiplex &mplex, DTVTunerType tt, uint cid) :
        DTVMultiplex(mplex), m_tunerType(tt), m_cardid(cid) { }
    ~ScanDTVTransport() override = default;

    bool FillFromDB(DTVTunerType type, uint mplexid) override; // DTVMultiplex
    uint SaveScan(uint scanid) const;

    bool ParseTuningParams(
        DTVTunerType type,
        const QString& frequency,    const QString& inversion,      const QString& symbolrate,
        const QString& fec,          const QString& polarity,
        const QString& hp_code_rate, const QString& lp_code_rate,   const QString& ofdm_modulation,
        const QString& trans_mode,   const QString& guard_interval, const QString& hierarchy,
        const QString& modulation,   const QString& bandwidth,
        const QString& mod_sys,      const QString& rolloff,        const QString& signal_strength);

  public:
    DTVTunerType          m_tunerType      {DTVTunerType::kTunerTypeUnknown};
    uint                  m_cardid         {0};
    ChannelInsertInfoList m_channels;
    uint                  m_networkID      {0};
    uint                  m_transportID    {0};
    int                   m_signalStrength {0};
};
using ScanDTVTransportList = std::vector<ScanDTVTransport>;

#endif // DTVMULTIPLEX_H
