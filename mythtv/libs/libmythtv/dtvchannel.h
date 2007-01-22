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
#include <qmutex.h>
#include <qstring.h>

// MythTV headers
#include "channelbase.h"

typedef pair<uint,uint> pid_cache_item_t;
typedef vector<pid_cache_item_t> pid_cache_t;

class TVRec;

/** \class DTVChannel
 *  \brief Class providing a generic interface to digital tuning hardware.
 */
class DTVChannel : public ChannelBase
{
  public:
    DTVChannel(TVRec *parent);
    virtual ~DTVChannel() {}

    // Commands

    /// \brief To be used by the channel scanner and possibly the EIT scanner.
    virtual bool TuneMultiplex(uint mplexid, QString inputname) = 0;
    /// \brief To be used by the channel scanner and possibly the EIT scanner.
    virtual bool Tune(const DTVMultiplex &tuning, QString inputname) = 0;
    /// \brief Enters power saving mode if the card supports it
    virtual bool EnterPowerSavingMode(void) { return true; }

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

    /** \brief Returns cached MPEG PIDs for last tuned channel.
     *  \param pid_cache List of PIDs with their TableID
     *                   types is returned in pid_cache.
     */
    virtual void GetCachedPids(pid_cache_t &pid_cache) const
        { (void) pid_cache; }

    // Sets

    /// \brief Sets tuning mode: "mpeg", "dvb", "atsc", etc.
    void SetTuningMode(const QString &tuningmode);

    /** \brief Saves MPEG PIDs to cache to database
     * \param pid_cache List of PIDs with their TableID types to be saved.
     */
    virtual void SaveCachedPids(const pid_cache_t &pid_cache) const
        { (void) pid_cache; }

  protected:
    /// \brief Sets PSIP table standard: MPEG, DVB, ATSC, or OpenCable
    void SetSIStandard(const QString&);
    void SetDTVInfo(uint atsc_major, uint atsc_minor,
                    uint dvb_orig_netid,
                    uint mpeg_tsid, int mpeg_pnum);
    void ClearDTVInfo(void) { SetDTVInfo(0, 0, 0, 0, -1); }

    static void GetCachedPids(int chanid, pid_cache_t&);
    static void SaveCachedPids(int chanid, const pid_cache_t&);

  protected:
    mutable QMutex dtvinfo_lock;

    QString sistandard; ///< PSIP table standard: MPEG, DVB, ATSC, OpenCable
    QString tuningMode;
    int     currentProgramNum;
    uint    currentATSCMajorChannel;
    uint    currentATSCMinorChannel;
    uint    currentTransportID;
    uint    currentOriginalNetworkID;
};

#endif // _DTVCHANNEL_H_
