
#include "dtvchannel.h"

// MythTV headers
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "cardutil.h"
#include "mpeg/mpegtables.h"
#include "tv_rec.h"

#define LOC QString("DTVChan[%1](%2): ").arg(m_inputId).arg(GetDevice())

QReadWriteLock DTVChannel::s_master_map_lock(QReadWriteLock::Recursive);
using MasterMap = QMap<QString,QList<DTVChannel*> >;
MasterMap DTVChannel::s_master_map;

DTVChannel::~DTVChannel()
{
    if (m_genPAT)
    {
        delete m_genPAT;
        m_genPAT = nullptr;
    }

    if (m_genPMT)
    {
        delete m_genPMT;
        m_genPMT = nullptr;
    }
}

void DTVChannel::SetDTVInfo(uint atsc_major, uint atsc_minor,
                            uint dvb_orig_netid,
                            uint mpeg_tsid, int mpeg_pnum)
{
    QMutexLocker locker(&m_dtvinfoLock);
    m_currentProgramNum        = mpeg_pnum;
    m_currentATSCMajorChannel  = atsc_major;
    m_currentATSCMinorChannel  = atsc_minor;
    m_currentTransportID       = mpeg_tsid;
    m_currentOriginalNetworkID = dvb_orig_netid;
}

QString DTVChannel::GetSIStandard(void) const
{
    QMutexLocker locker(&m_dtvinfoLock);
    return m_sistandard;
}

void DTVChannel::SetSIStandard(const QString &si_std)
{
    QMutexLocker locker(&m_dtvinfoLock);
    m_sistandard = si_std.toLower();
}

QString DTVChannel::GetSuggestedTuningMode(bool is_live_tv) const
{
    QString input = GetInputName();

    uint quickTuning = 0;
    if (m_inputId && !input.isEmpty())
        quickTuning = CardUtil::GetQuickTuning(m_inputId, input);

    bool useQuickTuning = ((quickTuning != 0U) && is_live_tv) || (quickTuning > 1);

    QMutexLocker locker(&m_dtvinfoLock);
    if (!useQuickTuning && ((m_sistandard == "atsc") || (m_sistandard == "dvb")))
        return m_sistandard;
    return "mpeg";
}

QString DTVChannel::GetTuningMode(void) const
{
    QMutexLocker locker(&m_dtvinfoLock);
    return m_tuningMode;
}

std::vector<DTVTunerType> DTVChannel::GetTunerTypes(void) const
{
    std::vector<DTVTunerType> tts;
    if (m_tunerType != DTVTunerType::kTunerTypeUnknown)
        tts.push_back(m_tunerType);
    return tts;
}

void DTVChannel::SetTuningMode(const QString &tuning_mode)
{
    QMutexLocker locker(&m_dtvinfoLock);
    m_tuningMode = tuning_mode.toLower();
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
    s_master_map_lock.lockForWrite();
    s_master_map[key].push_back(this);
    s_master_map_lock.unlock();
}

void DTVChannel::DeregisterForMaster(const QString &key)
{
    s_master_map_lock.lockForWrite();
    MasterMap::iterator mit = s_master_map.find(key);
    if (mit == s_master_map.end())
        mit = s_master_map.begin();
    for (; mit != s_master_map.end(); ++mit)
    {
        (*mit).removeAll(this);
        if (mit.key() == key)
            break;
    }
    s_master_map_lock.unlock();
}

DTVChannel *DTVChannel::GetMasterLock(const QString &key)
{
    s_master_map_lock.lockForRead();
    MasterMap::iterator mit = s_master_map.find(key);
    if (mit == s_master_map.end() || (*mit).empty())
    {
        s_master_map_lock.unlock();
        return nullptr;
    }
    return (*mit).front();
}

void DTVChannel::ReturnMasterLock(DTVChannelP &chan)
{
    if (chan != nullptr)
    {
        chan = nullptr;
        s_master_map_lock.unlock();
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

    if (!m_inputId)
        return false;

    uint mplexid_restriction = 0;
    uint chanid_restriction = 0;
    if (!IsInputAvailable(mplexid_restriction, chanid_restriction))
    {
        LOG(VB_GENERAL, LOG_INFO, loc +
            QString("Requested channel '%1' is on input '%2' "
                    "which is in a busy input group")
                .arg(channum).arg(m_inputId));

        return false;
    }

    // Fetch tuning data from the database.
    QString tvformat;
    QString modulation;
    QString freqtable;
    QString freqid;
    QString si_std;
    int finetune = 0;
    uint64_t frequency = 0;
    int mpeg_prog_num = 0;
    uint atsc_major = 0;
    uint atsc_minor = 0;
    uint mplexid = 0;
    uint chanid = 0;
    uint tsid = 0;
    uint netid = 0;

    if (!ChannelUtil::GetChannelData(
        m_sourceId, chanid, channum,
        tvformat, modulation, freqtable, freqid,
        finetune, frequency,
        si_std, mpeg_prog_num, atsc_major, atsc_minor, tsid, netid,
        mplexid, m_commFree))
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
    if (m_genPAT)
    {
        version = (m_genPAT->Version()+1) & 0x1f;
        delete m_genPAT; m_genPAT = nullptr;
        delete m_genPMT; m_genPMT = nullptr;
    }

    bool ok = true;
    if (!IsExternalChannelChangeInUse())
    {
        if (IsIPTV())
        {
            int chanid2 = ChannelUtil::GetChanID(m_sourceId, channum);
            IPTVTuningData tuning = ChannelUtil::GetIPTVTuningData(chanid2);
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
            if (!mplexid || !tuning.FillFromDB(m_tunerType, mplexid))
            {
                LOG(VB_GENERAL, LOG_ERR, loc +
                        "Failed to initialize multiplex options");
                ok = false;
            }
            else
            {
                LOG(VB_GENERAL, LOG_DEBUG, loc +
                    QString("Initialize multiplex options m_tunerType:%1 mplexid:%2")
                        .arg(m_tunerType).arg(mplexid));

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
        int chanid3 = ChannelUtil::GetChanID(m_sourceId, channum);
        ChannelUtil::GetCachedPids(chanid3, pid_cache);
        if (pid_cache.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, loc + "PID cache is empty");
            return false;
        }

        // Now we construct the PAT & PMT
        std::vector<uint> pnum; pnum.push_back(1);
        std::vector<uint> pid;  pid.push_back(9999);
        m_genPAT = ProgramAssociationTable::Create(0,version,pnum,pid);

        int pcrpid = -1;
        std::vector<uint> pids;
        std::vector<uint> types;
        for (auto & pit : pid_cache)
        {
            if (!pit.GetStreamID())
                continue;
            pids.push_back(pit.GetPID());
            types.push_back(pit.GetStreamID());
            if (pit.IsPCRPID())
                pcrpid = pit.GetPID();
            if ((pcrpid < 0) && StreamID::IsVideo(pit.GetStreamID()))
                pcrpid = pit.GetPID();
        }
        if (pcrpid < 0)
            pcrpid = pid_cache[0].GetPID();

        m_genPMT = ProgramMapTable::Create(
            pnum.back(), pid.back(), pcrpid, version, pids, types);

        SetSIStandard("mpeg");
        SetDTVInfo(0,0,0,0,-1);
    }

    // Set the current channum to the new channel's channum
    m_curChannelName = channum;

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

bool DTVChannel::TuneMultiplex(uint mplexid, const QString& /*inputname*/)
{
    DTVMultiplex tuning;
    if (!tuning.FillFromDB(m_tunerType, mplexid))
        return false;

    CheckOptions(tuning);

    return Tune(tuning);
}
