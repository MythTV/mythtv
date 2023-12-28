/* -*- Mode: c++ -*-
 *  DTVChannel
 *  Copyright (c) 2005,2006 by Daniel Kristjansson
 *  Contains base class for digital channels.
 */

#ifndef DTVCHANNEL_H
#define DTVCHANNEL_H

// C++ headers
#include <cstdint>
#include <vector>

// Qt headers
#include <QReadWriteLock>
#include <QString>
#include <QMutex>
#include <QList>                        // for QList
#include <QMap>                         // for QMap

// MythTV headers
#include "dtvconfparserhelpers.h" // for DTVTunerType
#include "channelbase.h"
#include "channelutil.h" // for pid_cache_t, IPTVTuningData

class ProgramAssociationTable;
class ProgramMapTable;
class TVRec;

/** \class DTVChannel
 *  \brief Class providing a generic interface to digital tuning hardware.
 */
class DTVChannel : public ChannelBase
{
  public:
    explicit DTVChannel(TVRec *parent) : ChannelBase(parent) {}
    ~DTVChannel() override;

    // Commands
    bool SetChannelByString(const QString &chan) override; // ChannelBase

    /* Allow 'MPTS' format to be set, so we know when to process the
       full, unfiltered MPTS from the transport stream. */
    void SetFormat(const QString & format) override // ChannelBase
        { m_tvFormat = format; }
    QString GetFormat(void) { return m_tvFormat; }

    /// \brief To be used by the channel scanner and possibly the EIT scanner.
    virtual bool TuneMultiplex(uint mplexid, const QString& inputname);
    /// \brief This performs the actual frequency tuning and in some cases
    ///        input switching.
    ///
    /// In rare cases such as ASI this does nothing since all the channels
    /// are in the same MPTS stream on the same input. But generally you
    /// will need to implement this when adding support for new hardware.
    virtual bool Tune(const DTVMultiplex &tuning) = 0;
    /// \brief Performs IPTV Tuning. Only implemented by IPTVChannel.
    virtual bool Tune(const IPTVTuningData &/*tuning*/, bool /*scanning*/) { return false; }
    /// \brief Leave it up to the implementation to map the channnum
    /// appropriately.
    ///
    /// Used by the ExternalRecorder.
    virtual bool Tune(const QString &/*channum*/) { return true; }
    /// \brief Enters power saving mode if the card supports it
    virtual bool EnterPowerSavingMode(void)
    {
        return true;
    }
    /// \brief This tunes on the frequency Identification parameter for
    ///        hardware that supports it.
    ///
    /// This is only called when there is no frequency set. This is used
    /// to implement "Channel Numbers" in analog tuning scenarios and to
    /// implement "Virtual Channels" in the OCUR and Firewire tuners.
    bool Tune([[maybe_unused]] const QString &freqid,
              [[maybe_unused]] int finetune) override // ChannelBase
    {
        return false;
    }

    virtual bool Tune([[maybe_unused]] uint64_t frequency)
    {
        return false;
    }

    // Gets

    /// \brief Returns program number in PAT, -1 if unknown.
    int GetProgramNumber(void) const
        { return m_currentProgramNum; };

    /// \brief Returns major channel, 0 if unknown.
    uint GetMajorChannel(void) const
        { return m_currentATSCMajorChannel; };

    /// \brief Returns minor channel, 0 if unknown.
    uint GetMinorChannel(void) const
        { return m_currentATSCMinorChannel; };

    /// \brief Returns DVB original_network_id, 0 if unknown.
    uint GetOriginalNetworkID(void) const
        { return m_currentOriginalNetworkID; };

    /// \brief Returns DVB transport_stream_id, 0 if unknown.
    uint GetTransportID(void) const
        { return m_currentTransportID; };

    /// \brief Returns PSIP table standard: MPEG, DVB, ATSC, or OpenCable
    QString GetSIStandard(void) const;

    /// \brief Returns suggested tuning mode: "mpeg", "dvb", or "atsc"
    QString GetSuggestedTuningMode(bool is_live_tv) const;

    /// \brief Returns tuning mode last set by SetTuningMode().
    QString GetTuningMode(void) const;

    /// \brief Returns a vector of supported tuning types.
    virtual std::vector<DTVTunerType> GetTunerTypes(void) const;

    void GetCachedPids(pid_cache_t &pid_cache) const;

    void RegisterForMaster(const QString &key);
    void DeregisterForMaster(const QString &key);
    static DTVChannel *GetMasterLock(const QString &key);
    using DTVChannelP = DTVChannel*;
    static void ReturnMasterLock(DTVChannelP &chan);

    /// \brief Returns true if this is the first of a number of multi-rec devs
    virtual bool IsMaster(void) const { return false; }

    virtual bool IsPIDTuningSupported(void) const { return false; }

    virtual bool IsIPTV(void) const { return false; }

    bool HasGeneratedPAT(void) const { return m_genPAT != nullptr; }
    bool HasGeneratedPMT(void) const { return m_genPMT != nullptr; }
    const ProgramAssociationTable *GetGeneratedPAT(void) const {return m_genPAT;}
    const ProgramMapTable         *GetGeneratedPMT(void) const {return m_genPMT;}

    // Sets

    /// \brief Sets tuning mode: "mpeg", "dvb", "atsc", etc.
    void SetTuningMode(const QString &tuning_mode);

    void SaveCachedPids(const pid_cache_t &pid_cache) const;

  protected:
    /// \brief Sets PSIP table standard: MPEG, DVB, ATSC, or OpenCable
    void SetSIStandard(const QString &si_std);
    void SetDTVInfo(uint atsc_major, uint atsc_minor,
                    uint dvb_orig_netid,
                    uint mpeg_tsid, int mpeg_pnum);
    void ClearDTVInfo(void) { SetDTVInfo(0, 0, 0, 0, -1); }
    /// \brief Checks tuning for problems, and tries to fix them.
    virtual void CheckOptions(DTVMultiplex &/*tuning*/) const {}
    void HandleScriptEnd(bool ok) override; // ChannelBase

  protected:
    mutable QMutex m_dtvinfoLock;

    DTVTunerType   m_tunerType {DTVTunerType::kTunerTypeUnknown};
                   /// PSIP table standard: MPEG, DVB, ATSC, OpenCable
    QString        m_sistandard               {"mpeg"};
    QString        m_tuningMode;
    QString        m_tvFormat;
    int            m_currentProgramNum        {-1};
    uint           m_currentATSCMajorChannel  {0};
    uint           m_currentATSCMinorChannel  {0};
    uint           m_currentTransportID       {0};
    uint           m_currentOriginalNetworkID {0};

    /// This is a generated PAT for RAW pid tuning
    ProgramAssociationTable *m_genPAT {nullptr};
    /// This is a generated PMT for RAW pid tuning
    ProgramMapTable         *m_genPMT {nullptr};

    using MasterMap = QMap<QString,QList<DTVChannel*> >;
    static QReadWriteLock    s_master_map_lock;
    static MasterMap         s_master_map;
};

#endif // DTVCHANNEL_H
