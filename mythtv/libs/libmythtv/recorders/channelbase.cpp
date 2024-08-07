// Std C headers
#include <cstdlib>
#include <cerrno>

// POSIX headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// C++ headers
#include <iostream>
#include <algorithm>

// Qt headers
#include <QCoreApplication>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#ifdef USING_OSX_FIREWIRE
#include "darwinfirewiredevice.h"
#endif
#ifdef USING_LINUX_FIREWIRE
#include "linuxfirewiredevice.h"
#endif
#include "firewirechannel.h"
#include "cetonchannel.h"
#include "dummychannel.h"
#include "tvremoteutil.h"
#include "channelbase.h"
#include "channelutil.h"
#include "frequencies.h"
#include "hdhrchannel.h"
#include "iptvchannel.h"
#include "asichannel.h"
#include "dtvchannel.h"
#include "dvbchannel.h"
#include "v4lchannel.h"
#include "ExternalChannel.h"
#include "sourceutil.h"
#include "cardutil.h"
#include "inputinfo.h"
#include "satipchannel.h"

#define LOC QString("ChannelBase[%1]: ").arg(m_inputId)

ChannelBase::ChannelBase(TVRec *parent) : m_pParent(parent)
{
    if (m_pParent)
    {
        m_inputId = m_pParent->GetInputId();
    }
}

ChannelBase::~ChannelBase(void)
{
    QMutexLocker locker(&m_systemLock);
    if (m_system)
        KillScript();
}

bool ChannelBase::Init(QString &startchannel, bool setchan)
{
    bool ok = false;

    if (!setchan)
        ok = IsTunable(startchannel);
    else
        ok = SetChannelByString(startchannel);

    if (ok)
        return true;

    // Try to find a valid channel if given start channel fails.
    QString msg1 = QString("Setting start channel '%1' failed ").arg(startchannel);
    QString msg2 = "and no suitable channel found.";
    bool msg_error = true;

    // Attempt to find the requested startchannel
    for (auto & channel : m_channels)
    {
        if (channel.m_chanNum == startchannel &&
            IsTunable(startchannel))
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString("Found startchannel '%1'").arg(startchannel));
            return true;
        }
    }

    uint mplexid_restriction = 0;
    uint chanid_restriction = 0;

    if (!m_channels.empty() &&
        IsInputAvailable(mplexid_restriction, chanid_restriction))
    {
        uint chanid = ChannelUtil::GetNextChannel(
            m_channels, m_channels[0].m_chanId,
            mplexid_restriction, chanid_restriction, CHANNEL_DIRECTION_UP);

        auto cit = find(m_channels.begin(), m_channels.end(), chanid);

        if ((chanid != 0U) && (cit != m_channels.end()))
        {
            if (!setchan)
            {
                ok = IsTunable((mplexid_restriction || chanid_restriction)
                               ? (*cit).m_chanNum : startchannel);
            }
            else
            {
                ok = SetChannelByString((*cit).m_chanNum);
            }

            if (ok)
            {
                startchannel = (*cit).m_chanNum;
                msg2 = QString("selected '%1' instead.").arg(startchannel);
                msg_error = false;
            }
        }
    }

    LOG(VB_GENERAL, ((msg_error) ? LOG_ERR : LOG_WARNING), LOC + msg1 + msg2);

    return ok;
}

bool ChannelBase::IsTunable(const QString &channum) const
{
    QString loc = LOC + QString("IsTunable(%1)").arg(channum);

    if (!m_inputId)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested non-existant input"));

        return false;
    }

    uint mplexid_restriction = 0;
    uint chanid_restriction = 0;
    if (!IsInputAvailable(mplexid_restriction, chanid_restriction))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested channel is on input '%1' "
                    "which is in a busy input group")
            .arg(m_inputId));

        return false;
    }

    // Fetch tuning data from the database.
    QString tvformat;
    QString modulation;
    QString freqtable;
    QString freqid;
    QString dtv_si_std;
    int finetune = 0;
    uint64_t frequency = 0;
    int mpeg_prog_num = 0;
    uint atsc_major = 0;
    uint atsc_minor = 0;
    uint mplexid = 0;
    uint chanid = 0;
    uint tsid = 0;
    uint netid = 0;
    bool commfree = false;

    if (!ChannelUtil::GetChannelData(m_sourceId, chanid, channum,
                                     tvformat, modulation, freqtable, freqid,
                                     finetune, frequency, dtv_si_std,
                                     mpeg_prog_num, atsc_major, atsc_minor,
                                     tsid, netid, mplexid, commfree))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Failed to find channel in DB on input '%1' ")
            .arg(m_inputId));

        return false;
    }

    if ((mplexid_restriction && (mplexid != mplexid_restriction)) ||
        (!mplexid_restriction &&
         chanid_restriction && (chanid != chanid_restriction)))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Channel is valid, but tuner is busy "
                    "on different multiplex/channel (%1 != %2) / (%3 != %4)")
            .arg(mplexid).arg(mplexid_restriction)
            .arg(chanid).arg(chanid_restriction));

        return false;
    }

    return true;
}

uint ChannelBase::GetNextChannel(uint chanid, ChannelChangeDirection direction) const
{
    if (!chanid)
    {
        if (!m_inputId)
            return 0;

        chanid = ChannelUtil::GetChanID(m_sourceId, m_curChannelName);
    }

    uint mplexid_restriction = 0;
    uint chanid_restriction = 0;
    (void)IsInputAvailable(mplexid_restriction, chanid_restriction);

    return ChannelUtil::GetNextChannel(
        m_channels, chanid, mplexid_restriction, chanid_restriction,
        direction);
}

uint ChannelBase::GetNextChannel(const QString &channum, ChannelChangeDirection direction) const
{
    if (!m_inputId)
        return 0;

    uint chanid = ChannelUtil::GetChanID(m_sourceId, channum);
    return GetNextChannel(chanid, direction);
}

bool ChannelBase::IsInputAvailable(
    uint &mplexid_restriction, uint &chanid_restriction) const
{
    if (!m_inputId)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + QString("no m_inputId"));
        return false;
    }

    InputInfo info;

    mplexid_restriction = 0;
    chanid_restriction = 0;

    std::vector<uint> inputids = CardUtil::GetConflictingInputs(m_inputId);
    for (uint inputid : inputids)
    {
        if (RemoteIsBusy(inputid, info))
        {
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString("Input %1 is busy on %2/%3")
                .arg(info.m_inputId)
                .arg(info.m_chanId).arg(info.m_mplexId));
            if (info.m_sourceId != m_sourceId)
            {
                LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Input is busy"));
                return false;
            }
            mplexid_restriction = info.m_mplexId;
            chanid_restriction = info.m_chanId;
        }
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Input is free on %1/%2")
        .arg(mplexid_restriction).arg(chanid_restriction));
    return true;
}

/// \note m_systemLock must be held when this is called
bool ChannelBase::KillScript(void)
{
    if (!m_system)
        return true;

    m_system->Term(true);

    delete m_system;
    m_system = nullptr;
    return true;
}

/// \note m_systemLock must NOT be held when this is called
void ChannelBase::HandleScript(const QString &freqid)
{
    QMutexLocker locker(&m_systemLock);

    bool ok = true;
    m_systemStatus = 0; // unknown

    if (!m_inputId)
    {
        m_systemStatus = 2; // failed
        HandleScriptEnd(true);
        return;
    }

    if (m_externalChanger.isEmpty())
    {
        m_systemStatus = 3; // success
        HandleScriptEnd(true);
        return;
    }

    if (freqid.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "A channel changer is set, but the freqid field is empty."
            "\n\t\t\tWe will return success to ease setup pains, "
            "but no script is will actually run.");
        m_systemStatus = 3; // success
        HandleScriptEnd(true);
        return;
    }

    // It's possible we simply never reaped the process, check status first.
    if (m_system)
        GetScriptStatus(true);

    // If it's still running, try killing it. GetScriptStatus() may
    // update m_system. (cppcheck-suppress duplicateCondition)
    if (m_system)
        ok = KillScript();

    // The GetScriptStatus() call above can reset m_systemStatus with
    // the exit status of the last channel change script invocation, so
    // we must set it to pending here.
    m_systemStatus = 1; // pending

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Can not execute channel changer, previous call to script "
            "is still running.");
        m_systemStatus = 2; // failed
        HandleScriptEnd(ok);
    }
    else
    {
        if (m_externalChanger.toLower() == "internal")
        {
            ok = ChangeInternalChannel(freqid, m_inputId);
            if (!ok)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Can not execute internal channel "
                    "changer.");
                m_systemStatus = 2; // failed
            }
            else
            {
                m_systemStatus = 3; // success
            }

            HandleScriptEnd(ok);
        }
        else
        {
            ok = ChangeExternalChannel(m_externalChanger, freqid);
            if (!ok)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Can not execute channel changer.");
                m_systemStatus = 2; // failed
                HandleScriptEnd(ok);
            }
        }
    }
}

bool ChannelBase::ChangeInternalChannel([[maybe_unused]] const QString &freqid,
                                        [[maybe_unused]] uint inputid) const
{
#ifdef USING_FIREWIRE
    FirewireDevice *device = nullptr;
    QString fwnode = CardUtil::GetFirewireChangerNode(inputid);
    uint64_t guid = string_to_guid(fwnode);
    QString fwmodel = CardUtil::GetFirewireChangerModel(inputid);

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Internal channel change to %1 "
            "on inputid %2, GUID %3 (%4)").arg(freqid).arg(inputid)
            .arg(fwnode, fwmodel));

#ifdef USING_LINUX_FIREWIRE
    device = new LinuxFirewireDevice(
        guid, 0, 100, true);
#endif // USING_LINUX_FIREWIRE

#ifdef USING_OSX_FIREWIRE
    device = new DarwinFirewireDevice(guid, 0, 100);
#endif // USING_OSX_FIREWIRE

    if (!device)
        return false;

    if (!device->OpenPort())
        return false;

    if (!device->SetChannel(fwmodel, 0, freqid.toUInt()))
    {
        device->ClosePort();
        delete device;
        device = nullptr;
        return false;
    }

    device->ClosePort();
    delete device;
    device = nullptr;
    return true;
#else
    return false;
#endif
}

/// \note m_systemLock must be held when this is called
bool ChannelBase::ChangeExternalChannel(const QString &changer,
                                        const QString &freqid)
{
    if (m_system)
        return false;

    if (changer.isEmpty() || freqid.isEmpty())
        return false;

    QString command = QString("%1 %2").arg(changer, freqid);
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Running command: %1").arg(command));

    m_system = new MythSystemLegacy(command, kMSRunShell | kMSRunBackground);
    m_system->Run();

    return true;
}

uint ChannelBase::GetScriptStatus(bool holding_lock)
{
    if (!m_system)
        return m_systemStatus;

    if (!holding_lock)
        m_systemLock.lock();

    m_systemStatus = m_system->Wait();
    if (m_systemStatus != GENERIC_EXIT_RUNNING &&
        m_systemStatus != GENERIC_EXIT_START)
    {
        delete m_system;
        m_system = nullptr;

        HandleScriptEnd(m_systemStatus == GENERIC_EXIT_OK);
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("GetScriptStatus() %1")
        .arg(m_systemStatus));

    uint ret = 0;
    switch(m_systemStatus)
    {
        case GENERIC_EXIT_OK:
            ret = 3;    // success
            break;
        case GENERIC_EXIT_RUNNING:
        case GENERIC_EXIT_START:
            ret = 1;    // pending
            break;
        default:
            ret = 2;    // fail
            break;
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("GetScriptStatus() %1 -> %2")
            .arg(m_systemStatus). arg(ret));

    m_systemStatus = ret;

    if (!holding_lock)
        m_systemLock.unlock();

    return ret;
}

/// \note m_systemLock must be held when this is called
void ChannelBase::HandleScriptEnd(bool ok)
{
    if (ok)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Channel change script succeeded.");
        if (m_inputId)
        {
            // Set this as the future start channel for this source
            m_startChanNum = m_curChannelName;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Channel change script failed.");
    }
}

int ChannelBase::GetChanID(void) const
{
    if (!m_inputId)
        return -1;

    int found   = 0;
    int visible = -1;
    int id      = -1;
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid,visible FROM channel "
                  "WHERE deleted  IS NULL AND "
                  "      channum  = :CHANNUM AND "
                  "      sourceid = :SOURCEID");
    query.bindValueNoNull(":CHANNUM", m_curChannelName);
    query.bindValue(":SOURCEID", m_sourceId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("fetching chanid", query);
        return -1;
    }

    while (query.next())
    {
        if (query.value(1).toInt() > 0)
        {
            ++found;
            visible = query.value(0).toInt();
        }
        else
        {
            id = query.value(0).toInt();
        }
    }

    if (!found)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("No visible channel ids for %1 on sourceid %2")
            .arg(m_curChannelName).arg(m_sourceId));
    }

    if (found > 1)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Found multiple visible channel ids for %1 on sourceid %2")
            .arg(m_curChannelName).arg(m_sourceId));
    }

    return (visible >= 0 ? visible : id);
}

/**
 *  \brief Fills in input map from DB
 */
bool ChannelBase::InitializeInput(void)
{
    if (!m_inputId)
    {
        if (m_pParent)
            m_inputId = m_pParent->GetInputId();
        else
            m_inputId = CardUtil::GetFirstInputID(GetDevice());
    }

    if (!m_inputId)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "InitializeInput(): Programmer error, no parent.");
        return false;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sourceid,    inputname, "
        "       startchan,   externalcommand, "
        "       tunechan "
        "FROM capturecard "
        "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", m_inputId);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("ChannelBase::InitializeInput", query);
        return false;
    }
    if (!query.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("No capturecard record in database for input %1")
            .arg(m_inputId));
        return false;
    }

    query.next();

    m_sourceId = query.value(0).toUInt();
    m_name = query.value(1).toString();
    m_startChanNum = query.value(2).toString();
    m_externalChanger = query.value(3).toString();
    m_tuneToChannel = query.value(4).toString();

    if (0 == m_sourceId)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("No video source defined for input %1")
            .arg(m_inputId));
            return false;
    }

    m_channels = ChannelUtil::GetChannels(m_sourceId, false);
    QString order = gCoreContext->GetSetting("ChannelOrdering", "channum");
    ChannelUtil::SortChannels(m_channels, order);

    if (!IsExternalChannelChangeSupported() &&
        !m_externalChanger.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "External Channel changer is "
            "set, but this device does not support it.");
        m_externalChanger.clear();
    }

    // print it
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Input #%1: '%2' schan(%3) sourceid(%4)")
        .arg(m_inputId).arg(m_name, m_startChanNum)
        .arg(m_sourceId));

    return true;
}

/** \fn ChannelBase::Renumber(uint,const QString&,const QString&)
 *  \brief Changes a channum if we have it cached anywhere.
 */
void ChannelBase::Renumber(uint sourceid,
                           const QString &oldChanNum,
                           const QString &newChanNum)
{
    bool skip = (m_name.isEmpty()                ||
                 m_startChanNum.isEmpty()        ||
                 m_startChanNum != oldChanNum ||
                 m_sourceId     != sourceid);
    if (!skip)
        m_startChanNum = newChanNum;

    if (GetSourceID() == sourceid && oldChanNum == m_curChannelName)
        m_curChannelName = newChanNum;

    StoreInputChannels();
}

/**
 *  \brief Sets starting channel for the each input in the input map.
 */
void ChannelBase::StoreInputChannels(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (m_name.isEmpty() || m_startChanNum.isEmpty())
        return;

    query.prepare(
        "UPDATE capturecard "
        "SET startchan = :STARTCHAN "
        "WHERE cardid = :CARDINPUTID");
    query.bindValue(":STARTCHAN", m_startChanNum);
    query.bindValue(":CARDINPUTID", m_inputId);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("StoreInputChannels", query);
}

bool ChannelBase::CheckChannel(const QString &channum) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return false;

    query.prepare(
        "SELECT channel.chanid "
        "FROM channel, capturecard "
        "WHERE channel.deleted       IS NULL                AND "
        "      channel.channum       = :CHANNUM             AND "
        "      channel.sourceid      = capturecard.sourceid AND "
        "      capturecard.cardid    = :INPUTID             AND "
        "      capturecard.hostname  = :HOSTNAME");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":INPUTID",   m_inputId);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkchannel", query);
    }
    else if (query.size() > 0)
    {
        return true;
    }

    LOG(VB_CHANNEL, LOG_ERR, LOC +
        QString("Failed to find channel(%1) on input (%2).")
        .arg(channum).arg(m_inputId));
    return false;
}

ChannelBase *ChannelBase::CreateChannel(
    TVRec                    *tvrec,
    const GeneralDBOptions   &genOpt,
    const DVBDBOptions       &dvbOpt,
    [[maybe_unused]] const FireWireDBOptions  &fwOpt,
    const QString            &startchannel,
    bool                      enter_power_save_mode,
    QString                  &rbFileExt,
    bool                      setchan)
{
    rbFileExt = "ts";

    ChannelBase *channel = nullptr;
    if (genOpt.m_inputType == "DVB")
    {
#ifdef USING_DVB
        channel = new DVBChannel(genOpt.m_videoDev, tvrec);
        auto *dvbchannel = dynamic_cast<DVBChannel*>(channel);
        if (dvbchannel != nullptr)
            dvbchannel->SetSlowTuning(dvbOpt.m_dvbTuningDelay);
#endif
    }
    else if (genOpt.m_inputType == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
        channel = new FirewireChannel(tvrec, genOpt.m_videoDev, fwOpt);
#endif
    }
#ifdef USING_HDHOMERUN
    else if (genOpt.m_inputType == "HDHOMERUN")
    {
        channel = new HDHRChannel(tvrec, genOpt.m_videoDev);
    }
#endif
#ifdef USING_SATIP
    else if (genOpt.m_inputType == "SATIP")
    {
        channel = new SatIPChannel(tvrec, genOpt.m_videoDev);
    }
#endif
    else if ((genOpt.m_inputType == "IMPORT") ||
             (genOpt.m_inputType == "DEMO") ||
             (genOpt.m_inputType == "MPEG" &&
              genOpt.m_videoDev.startsWith("file:", Qt::CaseInsensitive)))
    {
        channel = new DummyChannel(tvrec);
        rbFileExt = "mpg";
    }
#if defined(USING_IPTV) || defined(USING_VBOX)
    else if ((genOpt.m_inputType == "FREEBOX") || // IPTV
             (genOpt.m_inputType == "VBOX"))
    {
        channel = new IPTVChannel(tvrec, genOpt.m_videoDev);
    }
#endif
#ifdef USING_ASI
    else if (genOpt.m_inputType == "ASI")
    {
        channel = new ASIChannel(tvrec, genOpt.m_videoDev);
    }
#endif
#ifdef USING_CETON
    else if (genOpt.m_inputType == "CETON")
    {
        channel = new CetonChannel(tvrec, genOpt.m_videoDev);
    }
#endif
    else if (genOpt.m_inputType == "V4L2ENC")
    {
#ifdef USING_V4L2
        channel = new V4LChannel(tvrec, genOpt.m_videoDev);
#endif
        if (genOpt.m_inputType == "MPEG")
            rbFileExt = "mpg";
    }
    else if (CardUtil::IsV4L(genOpt.m_inputType))
    {
#ifdef USING_V4L2
        channel = new V4LChannel(tvrec, genOpt.m_videoDev);
#endif
        if (genOpt.m_inputType != "HDPVR")
        {
            if (genOpt.m_inputType != "MPEG")
                rbFileExt = "nuv";
            else
                rbFileExt = "mpg";
        }
    }
    else if (genOpt.m_inputType == "EXTERNAL")
    {
        channel = new ExternalChannel(tvrec, genOpt.m_videoDev);
    }

    if (!channel)
    {
        QString msg = QString(
            "%1 card configured on video device %2, \n"
            "but MythTV was not compiled with %3 support. \n"
            "\n"
            "Recompile MythTV with %4 support or remove the card \n"
            "from the configuration and restart MythTV.")
            .arg(genOpt.m_inputType, genOpt.m_videoDev,
                 genOpt.m_inputType, genOpt.m_inputType);
        LOG(VB_GENERAL, LOG_ERR, "ChannelBase: CreateChannel() Error: \n" +
            msg + "\n");
        return nullptr;
    }

    if (!channel->Open())
    {
        LOG(VB_GENERAL, LOG_ERR, "ChannelBase: CreateChannel() Error: " +
            QString("Failed to open device %1").arg(genOpt.m_videoDev));
        delete channel;
        return nullptr;
    }

    QString channum = startchannel;
    channel->Init(channum, setchan);

    if (enter_power_save_mode)
    {
        if (channel &&
            ((genOpt.m_inputType == "DVB" && dvbOpt.m_dvbOnDemand) ||
             genOpt.m_inputType == "HDHOMERUN" ||
             genOpt.m_inputType == "EXTERNAL" ||
             CardUtil::IsV4L(genOpt.m_inputType)))
        {
            channel->Close();
        }
        else if (setchan)
        {
            auto *dtvchannel = dynamic_cast<DTVChannel*>(channel);
            if (dtvchannel)
                dtvchannel->EnterPowerSavingMode();
        }
    }

    return channel;
}

bool ChannelBase::IsExternalChannelChangeInUse(void)
{
    if (!IsExternalChannelChangeSupported())
        return false;

    if (!m_inputId)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("IsExternalChannelChangeInUse: "
                    "non-existant input"));
        return false;
    }

    return !m_externalChanger.isEmpty();
}

int ChannelBase::GetMajorID(void)
{
    return m_pParent ? m_pParent->GetMajorId() : m_inputId;
}
