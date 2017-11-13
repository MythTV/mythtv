
#include "dtvchannel.h"

// MythTV headers
#include "mythdb.h"
#include "tv_rec.h"
#include "cardutil.h"
#include "mpegtables.h"
#include "mythlogging.h"

#define LOC QString("DTVChan[%1](%2): ").arg(m_inputid).arg(GetDevice())

QReadWriteLock DTVChannel::master_map_lock(QReadWriteLock::Recursive);
typedef QMap<QString,QList<DTVChannel*> > MasterMap;
MasterMap DTVChannel::master_map;

DTVChannel::DTVChannel(TVRec *parent)
    : ChannelBase(parent),
      tunerType(DTVTunerType::kTunerTypeUnknown),
      sistandard("mpeg"),         tuningMode(QString::null),
      currentProgramNum(-1),
      currentATSCMajorChannel(0), currentATSCMinorChannel(0),
      currentTransportID(0),      currentOriginalNetworkID(0),
      genPAT(NULL),               genPMT(NULL)
{
}

DTVChannel::~DTVChannel()
{
    if (genPAT)
    {
        delete genPAT;
        genPAT = NULL;
    }

    if (genPMT)
    {
        delete genPMT;
        genPMT = NULL;
    }
}

void DTVChannel::SetDTVInfo(uint atsc_major, uint atsc_minor,
                            uint dvb_orig_netid,
                            uint mpeg_tsid, int mpeg_pnum)
{
    QMutexLocker locker(&dtvinfo_lock);
    currentProgramNum        = mpeg_pnum;
    currentATSCMajorChannel  = atsc_major;
    currentATSCMinorChannel  = atsc_minor;
    currentTransportID       = mpeg_tsid;
    currentOriginalNetworkID = dvb_orig_netid;
}

QString DTVChannel::GetSIStandard(void) const
{
    QMutexLocker locker(&dtvinfo_lock);
    QString tmp = sistandard; tmp.detach();
    return tmp;
}

void DTVChannel::SetSIStandard(const QString &si_std)
{
    QMutexLocker locker(&dtvinfo_lock);
    sistandard = si_std.toLower();
    sistandard.detach();
}

QString DTVChannel::GetSuggestedTuningMode(bool is_live_tv) const
{
    QString input = GetInputName();

    uint quickTuning = 0;
    if (m_inputid && !input.isEmpty())
        quickTuning = CardUtil::GetQuickTuning(m_inputid, input);

    bool useQuickTuning = (quickTuning && is_live_tv) || (quickTuning > 1);

    QMutexLocker locker(&dtvinfo_lock);
    if (!useQuickTuning && ((sistandard == "atsc") || (sistandard == "dvb")))
    {
        QString tmp = sistandard; tmp.detach();
        return tmp;
    }

    return "mpeg";
}

QString DTVChannel::GetTuningMode(void) const
{
    QMutexLocker locker(&dtvinfo_lock);
    QString tmp = tuningMode; tmp.detach();
    return tmp;
}

vector<DTVTunerType> DTVChannel::GetTunerTypes(void) const
{
    vector<DTVTunerType> tts;
    if (tunerType != DTVTunerType::kTunerTypeUnknown)
        tts.push_back(tunerType);
    return tts;
}

void DTVChannel::SetTuningMode(const QString &tuning_mode)
{
    QMutexLocker locker(&dtvinfo_lock);
    tuningMode = tuning_mode.toLower();
    tuningMode.detach();
}

/** \brief Returns cached MPEG PIDs for last tuned channel.
 *  \param pid_cache List of PIDs with their TableID
 *                   types is returned in pid_cache.
 */
void DTVChannel::GetCachedPids(pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid > 0)
        ChannelUtil::GetCachedPids(chanid, pid_cache);
}

/** \brief Saves MPEG PIDs to cache to database
 * \param pid_cache List of PIDs with their TableID types to be saved.
 */
void DTVChannel::SaveCachedPids(const pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid > 0)
        ChannelUtil::SaveCachedPids(chanid, pid_cache);
}

void DTVChannel::RegisterForMaster(const QString &key)
{
    master_map_lock.lockForWrite();
    master_map[key].push_back(this);
    master_map_lock.unlock();
}

void DTVChannel::DeregisterForMaster(const QString &key)
{
    master_map_lock.lockForWrite();
    MasterMap::iterator mit = master_map.find(key);
    if (mit == master_map.end())
        mit = master_map.begin();
    for (; mit != master_map.end(); ++mit)
    {
        (*mit).removeAll(this);
        if (mit.key() == key)
            break;
    }
    master_map_lock.unlock();
}

DTVChannel *DTVChannel::GetMasterLock(const QString &key)
{
    master_map_lock.lockForRead();
    MasterMap::iterator mit = master_map.find(key);
    if (mit == master_map.end() || (*mit).empty())
    {
        master_map_lock.unlock();
        return NULL;
    }
    return (*mit).front();
}

void DTVChannel::ReturnMasterLock(DTVChannelP &chan)
{
    if (chan != NULL)
    {
        chan = NULL;
        master_map_lock.unlock();
    }
}

bool DTVChannel::SetChannelByString(const QString &channum)
{
    QString loc = LOC + QString("SetChannelByString(%1): ").arg(channum);
    LOG(VB_CHANNEL, LOG_INFO, loc);

    ClearDTVInfo();

    if (!IsOpen() && !Open())
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Channel object "
                "will not open, can not change channels.");

        return false;
    }

    if (!m_inputid)
        return false;

    uint mplexid_restriction;
    uint chanid_restriction;
    if (!IsInputAvailable(mplexid_restriction, chanid_restriction))
    {
        LOG(VB_GENERAL, LOG_INFO, loc +
            QString("Requested channel '%1' is on input '%2' "
                    "which is in a busy input group")
                .arg(channum).arg(m_inputid));

        return false;
    }

    // Fetch tuning data from the database.
    QString tvformat, modulation, freqtable, freqid, si_std;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, chanid, tsid, netid;

    if (!ChannelUtil::GetChannelData(
        m_sourceid, chanid, channum,
        tvformat, modulation, freqtable, freqid,
        finetune, frequency,
        si_std, mpeg_prog_num, atsc_major, atsc_minor, tsid, netid,
        mplexid, m_commfree))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "Unable to find channel in database.");

        return false;
    }

    if ((mplexid_restriction && (mplexid != mplexid_restriction)) ||
        (!mplexid_restriction &&
         chanid_restriction && (chanid != chanid_restriction)))
    {
        LOG(VB_GENERAL, LOG_ERR, loc +
            QString("Requested channel '%1' is not available because "
                    "the tuner is currently in use on another transport.")
                .arg(channum));

        return false;
    }

    // If the frequency is zeroed out, don't use it directly.
    if (frequency == 0)
    {
        frequency = (freqid.toULongLong() + finetune) * 1000;
        mplexid = 0;
    }
    bool isFrequency = (frequency > 10000000);
    bool hasTuneToChan =
        !m_tuneToChannel.isEmpty() && m_tuneToChannel != "Undefined";

    // If we are tuning to a freqid, rather than an actual frequency,
    // we may need to set the frequency table to use.
    if (!isFrequency || hasTuneToChan)
        SetFreqTable(freqtable);

    // Set NTSC, PAL, ATSC, etc.
    SetFormat(tvformat);

    // If a tuneToChannel is set make sure we're still on it
    if (hasTuneToChan)
        Tune(m_tuneToChannel, 0);

    // Clear out any old PAT or PMT & save version info
    uint version = 0;
    if (genPAT)
    {
        version = (genPAT->Version()+1) & 0x1f;
        delete genPAT; genPAT = NULL;
        delete genPMT; genPMT = NULL;
    }

    bool ok = true;
    if (m_externalChanger.isEmpty())
    {
        if (IsIPTV())
        {
            int chanid = ChannelUtil::GetChanID(m_sourceid, channum);
            IPTVTuningData tuning = ChannelUtil::GetIPTVTuningData(chanid);
            if (!Tune(tuning, false))
            {
                LOG(VB_GENERAL, LOG_ERR, loc + "Tuning to IPTV URL");
                ClearDTVInfo();
                ok = false;
            }
        }
        else if (m_name.contains("composite", Qt::CaseInsensitive) ||
                 m_name.contains("component", Qt::CaseInsensitive) ||
                 m_name.contains("s-video", Qt::CaseInsensitive))
        {
            LOG(VB_GENERAL, LOG_WARNING, loc + "You have not set "
                    "an external channel changing"
                    "\n\t\t\tscript for a component|composite|s-video "
                    "input. Channel changing will do nothing.");
        }
        else if (isFrequency && Tune(frequency))
        {
        }
        else if (isFrequency)
        {
            // Initialize basic the tuning parameters
            DTVMultiplex tuning;
            if (!mplexid || !tuning.FillFromDB(tunerType, mplexid))
            {
                LOG(VB_GENERAL, LOG_ERR, loc +
                        "Failed to initialize multiplex options");
                ok = false;
            }
            else
            {
                // Try to fix any problems with the multiplex
                CheckOptions(tuning);

                // Tune to proper multiplex
                if (!Tune(tuning))
                {
                    LOG(VB_GENERAL, LOG_ERR, loc + "Tuning to frequency.");

                    ClearDTVInfo();
                    ok = false;
                }
            }
        }
        else
        {
            // ExternalChannel justs wants the channum
            ok = freqid.isEmpty() && finetune == 0 ?
                 Tune(channum) : Tune(freqid, finetune);
        }
    }

    LOG(VB_CHANNEL, LOG_INFO, loc + ((ok) ? "success" : "failure"));

    if (!ok)
        return false;

    if (atsc_minor || (mpeg_prog_num>=0))
    {
        SetSIStandard(si_std);
        SetDTVInfo(atsc_major, atsc_minor, netid, tsid, mpeg_prog_num);
    }
    else if (IsPIDTuningSupported())
    {
        // We need to pull the pid_cache since there are no tuning tables
        pid_cache_t pid_cache;
        int chanid = ChannelUtil::GetChanID(m_sourceid, channum);
        ChannelUtil::GetCachedPids(chanid, pid_cache);
        if (pid_cache.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, loc + "PID cache is empty");
            return false;
        }

        // Now we construct the PAT & PMT
        vector<uint> pnum; pnum.push_back(1);
        vector<uint> pid;  pid.push_back(9999);
        genPAT = ProgramAssociationTable::Create(0,version,pnum,pid);

        int pcrpid = -1;
        vector<uint> pids;
        vector<uint> types;
        pid_cache_t::iterator pit = pid_cache.begin();
        for (; pit != pid_cache.end(); ++pit)
        {
            if (!pit->GetStreamID())
                continue;
            pids.push_back(pit->GetPID());
            types.push_back(pit->GetStreamID());
            if (pit->IsPCRPID())
                pcrpid = pit->GetPID();
            if ((pcrpid < 0) && StreamID::IsVideo(pit->GetStreamID()))
                pcrpid = pit->GetPID();
        }
        if (pcrpid < 0)
            pcrpid = pid_cache[0].GetPID();

        genPMT = ProgramMapTable::Create(
            pnum.back(), pid.back(), pcrpid, version, pids, types);

        SetSIStandard("mpeg");
        SetDTVInfo(0,0,0,0,-1);
    }

    // Set the current channum to the new channel's channum
    m_curchannelname = channum;

    // Setup filters & recording picture attributes for framegrabing recorders
    // now that the new curchannelname has been established.
    if (m_pParent)
        m_pParent->SetVideoFiltersForChannel(GetSourceID(), channum);
    InitPictureAttributes();

    HandleScript(freqid);

    return ok;
}

void DTVChannel::HandleScriptEnd(bool /*ok*/)
{
    // MAYBE TODO? need to tell signal monitor to throw out any tables
    // it saw on the last mux...

    // We do not want to call ChannelBase::HandleScript() as it
    // will save the current channel to (*it)->startChanNum
}

bool DTVChannel::TuneMultiplex(uint mplexid, QString /*inputname*/)
{
    DTVMultiplex tuning;
    if (!tuning.FillFromDB(tunerType, mplexid))
        return false;

    CheckOptions(tuning);

    return Tune(tuning);
}
