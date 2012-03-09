/* -*- Mode: c++ -*-
 *  DTVChannel
 *  Copyright (c) 2005,2006 by Daniel Kristjansson
 *  Contains base class for digital channels.
 */

#ifndef _DTVCHANNEL_H_
#define _DTVCHANNEL_H_

// POSIX headers
#include <stdint.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QReadWriteLock>
#include <QString>
#include <QMutex>

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
    DTVChannel(TVRec *parent);
    virtual ~DTVChannel();

    // Commands
    virtual bool SetChannelByString(const QString &chan);

    /// \brief To be used by the channel scanner and possibly the EIT scanner.
    virtual bool TuneMultiplex(uint mplexid, QString inputname);
    /// \brief This performs the actual frequency tuning and in some cases
    ///        input switching.
    ///
    /// In rare cases such as ASI this does nothing since all the channels
    /// are in the same MPTS stream on the same input. But generally you
    /// will need to implement this when adding support for new hardware.
    virtual bool Tune(const DTVMultiplex &tuning, QString inputname) = 0;
    /// \brief Performs IPTV Tuning. Only implemented by IPTVChannel.
    virtual bool Tune(const IPTVTuningData&) { return false; }
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
    virtual bool Tune(const QString &freqid, int finetune)
    {
        (void) freqid; (void) finetune;
        return false;
    }

    virtual bool Tune(uint64_t frequency, QString inputname)
    {
        (void) frequency; (void) inputname;
        return false;
    }

    // Gets

    /// \brief Returns program number in PAT, -1 if unknown.
    int GetProgramNumber(void) const
        { return currentProgramNum; };

    /// \brief Returns major channel, 0 if unknown.
    uint GetMajorChannel(void) const
        { return currentATSCMajorChannel; };

    /// \brief Returns minor channel, 0 if unknown.
    uint GetMinorChannel(void) const
        { return currentATSCMinorChannel; };

    /// \brief Returns DVB original_network_id, 0 if unknown.
    uint GetOriginalNetworkID(void) const
        { return currentOriginalNetworkID; };

    /// \brief Returns DVB transport_stream_id, 0 if unknown.
    uint GetTransportID(void) const
        { return currentTransportID; };

    /// \brief Returns PSIP table standard: MPEG, DVB, ATSC, or OpenCable
    QString GetSIStandard(void) const;

    /// \brief Returns suggested tuning mode: "mpeg", "dvb", or "atsc"
    QString GetSuggestedTuningMode(bool is_live_tv) const;

    /// \brief Returns tuning mode last set by SetTuningMode().
    QString GetTuningMode(void) const;

    /// \brief Returns a vector of supported tuning types.
    virtual vector<DTVTunerType> GetTunerTypes(void) const;

    void GetCachedPids(pid_cache_t &pid_cache) const;

    void RegisterForMaster(const QString &key);
    void DeregisterForMaster(const QString &key);
    static DTVChannel *GetMasterLock(const QString &key);
    typedef DTVChannel* DTVChannelP;
    static void ReturnMasterLock(DTVChannelP&);

    /// \brief Returns true if this is the first of a number of multi-rec devs
    virtual bool IsMaster(void) const { return false; }

    virtual bool IsPIDTuningSupported(void) const { return false; }

    virtual bool IsIPTV(void) const { return false; }

    bool HasGeneratedPAT(void) const { return genPAT != NULL; }
    bool HasGeneratedPMT(void) const { return genPMT != NULL; }
    const ProgramAssociationTable *GetGeneratedPAT(void) const {return genPAT;}
    const ProgramMapTable         *GetGeneratedPMT(void) const {return genPMT;}

    // Sets

    /// \brief Sets tuning mode: "mpeg", "dvb", "atsc", etc.
    void SetTuningMode(const QString &tuningmode);

    void SaveCachedPids(const pid_cache_t &pid_cache) const;

  protected:
    /// \brief Sets PSIP table standard: MPEG, DVB, ATSC, or OpenCable
    void SetSIStandard(const QString&);
    void SetDTVInfo(uint atsc_major, uint atsc_minor,
                    uint dvb_orig_netid,
                    uint mpeg_tsid, int mpeg_pnum);
    void ClearDTVInfo(void) { SetDTVInfo(0, 0, 0, 0, -1); }
    /// \brief Checks tuning for problems, and tries to fix them.
    virtual void CheckOptions(DTVMultiplex &tuning) const {}
    virtual void HandleScriptEnd(bool ok);

  protected:
    mutable QMutex dtvinfo_lock;

    DTVTunerType tunerType;
    QString sistandard; ///< PSIP table standard: MPEG, DVB, ATSC, OpenCable
    QString tuningMode;
    int     currentProgramNum;
    uint    currentATSCMajorChannel;
    uint    currentATSCMinorChannel;
    uint    currentTransportID;
    uint    currentOriginalNetworkID;

    /// This is a generated PAT for RAW pid tuning
    ProgramAssociationTable *genPAT;
    /// This is a generated PMT for RAW pid tuning
    ProgramMapTable         *genPMT;

    typedef QMap<QString,QList<DTVChannel*> > MasterMap;
    static QReadWriteLock    master_map_lock;
    static MasterMap         master_map;
};

#endif // _DTVCHANNEL_H_
