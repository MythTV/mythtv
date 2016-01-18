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
using namespace std;

// Qt headers
#include <QCoreApplication>

// MythTV headers
#ifdef USING_OSX_FIREWIRE
#include "darwinfirewiredevice.h"
#endif
#ifdef USING_LINUX_FIREWIRE
#include "linuxfirewiredevice.h"
#endif
#include "firewirechannel.h"
#include "mythcorecontext.h"
#include "cetonchannel.h"
#include "dummychannel.h"
#include "tvremoteutil.h"
#include "channelbase.h"
#include "channelutil.h"
#include "frequencies.h"
#include "hdhrchannel.h"
#include "iptvchannel.h"
#include "mythlogging.h"
#include "asichannel.h"
#include "dtvchannel.h"
#include "dvbchannel.h"
#include "v4lchannel.h"
#include "ExternalChannel.h"
#include "sourceutil.h"
#include "exitcodes.h"
#include "cardutil.h"
#include "compat.h"
#include "inputinfo.h"

#define LOC QString("ChannelBase[%1]: ").arg(m_inputid)

ChannelBase::ChannelBase(TVRec *parent) :
    m_pParent(parent), m_curchannelname(""),
    m_commfree(false), m_inputid(0), m_sourceid(0), m_name(""),
    m_startChanNum(""), m_externalChanger(""), m_tuneToChannel(""),
    m_system(NULL), m_system_status(0)
{
}

ChannelBase::~ChannelBase(void)
{
    QMutexLocker locker(&m_system_lock);
    if (m_system)
        KillScript();
}

bool ChannelBase::Init(QString &startchannel, bool setchan)
{
    bool ok;

    if (!setchan)
        ok = IsTunable(startchannel);
    else
        ok = SetChannelByString(startchannel);

    if (ok)
        return true;

    // try to find a valid channel if given start channel fails.
    QString msg1 = QString("Setting start channel '%1' failed, ")
        .arg(startchannel);
    QString msg2 = "and we failed to find any suitible channels on any input.";
    bool msg_error = true;

    // Attempt to find the requested startchannel
    ChannelInfoList::const_iterator cit = m_channels.begin();
    for (; cit != m_channels.end(); ++cit)
    {
        if ((*cit).channum == startchannel &&
            IsTunable(startchannel))
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString("Found startchannel '%1'").arg(startchannel));
            return true;
        }
    }

    uint mplexid_restriction = 0;
    uint chanid_restriction = 0;

    if (m_channels.size() &&
        IsInputAvailable(mplexid_restriction, chanid_restriction))
    {
        uint chanid = ChannelUtil::GetNextChannel(
            m_channels, m_channels[0].chanid,
            mplexid_restriction, chanid_restriction, CHANNEL_DIRECTION_UP);

        ChannelInfoList::const_iterator cit =
            find(m_channels.begin(), m_channels.end(), chanid);

        if (chanid && cit != m_channels.end())
        {
            if (!setchan)
            {
                ok = IsTunable((mplexid_restriction || chanid_restriction)
                               ? (*cit).channum : startchannel);
            }
            else
                ok = SetChannelByString((*cit).channum);

            if (ok)
            {
                if (mplexid_restriction || chanid_restriction)
                {
                    startchannel = (*cit).channum;
                    startchannel.detach();
                }
                msg2 = QString("selected to '%1' instead.")
                    .arg(startchannel);
                msg_error = false;
            }
        }
    }

    LOG(VB_GENERAL, ((msg_error) ? LOG_ERR : LOG_WARNING), LOC +
        msg1 + "\n\t\t\t" + msg2);

    return ok;
}

bool ChannelBase::IsTunable(const QString &channum) const
{
    QString loc = LOC + QString("IsTunable(%1)").arg(channum);

    if (!m_inputid)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested non-existant input"));

        return false;
    }

    uint mplexid_restriction;
    uint chanid_restriction;
    if (!IsInputAvailable(mplexid_restriction, chanid_restriction))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested channel is on input '%1' "
                    "which is in a busy input group")
            .arg(m_inputid));

        return false;
    }

    // Fetch tuning data from the database.
    QString tvformat, modulation, freqtable, freqid, dtv_si_std;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, chanid, tsid, netid;
    bool commfree;

    if (!ChannelUtil::GetChannelData(m_sourceid, chanid, channum,
                                     tvformat, modulation, freqtable, freqid,
                                     finetune, frequency, dtv_si_std,
                                     mpeg_prog_num, atsc_major, atsc_minor,
                                     tsid, netid, mplexid, commfree))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Failed to find channel in DB on input '%1' ")
            .arg(m_inputid));

        return false;
    }

    if ((mplexid_restriction && (mplexid != mplexid_restriction)) ||
        (chanid_restriction && (chanid != chanid_restriction)))
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
        if (!m_inputid)
            return 0;

        chanid = ChannelUtil::GetChanID(m_sourceid, m_curchannelname);
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
    if (!m_inputid)
        return 0;

    uint chanid = ChannelUtil::GetChanID(m_sourceid, channum);
    return GetNextChannel(chanid, direction);
}

static bool is_input_group_busy(
    uint                       inputid,
    uint                       groupid,
    QMap<uint,bool>           &busygrp,
    QMap<uint,bool>           &busyrec,
    QMap<uint,InputInfo>      &busyin,
    uint                      &mplexid_restriction,
    uint                      &chanid_restriction)
{
    static QMutex        igrpLock;
    static InputGroupMap igrp;

    // If none are busy, we don't need to check further
    QMap<uint,bool>::const_iterator bit = busygrp.find(groupid);
    if ((bit != busygrp.end()) && !*bit)
        return false;

    vector<InputInfo> conflicts;
    vector<uint> cardids = CardUtil::GetGroupInputIDs(groupid);
    for (uint i = 0; i < cardids.size(); i++)
    {
        if (inputid == cardids[i])
            continue;

        InputInfo info;
        QMap<uint,bool>::const_iterator it = busyrec.find(cardids[i]);
        if (it == busyrec.end())
        {
            busyrec[cardids[i]] = RemoteIsBusy(cardids[i], info);
            it = busyrec.find(cardids[i]);
            if (*it)
                busyin[cardids[i]] = info;
        }

        if (*it)
        {
            QMutexLocker locker(&igrpLock);
            if (igrp.GetSharedInputGroup(busyin[cardids[i]].inputid, inputid))
                conflicts.push_back(busyin[cardids[i]]);
        }
    }

    // If none are busy, we don't need to check further
    busygrp[groupid] = !conflicts.empty();
    if (conflicts.empty())
        return false;

    InputInfo in;
    in.inputid = inputid;
    if (!CardUtil::GetInputInfo(in))
        return true;

    // If they aren't using the same source they are definately busy
    bool is_busy_input = false;

    for (uint i = 0; i < conflicts.size() && !is_busy_input; i++)
        is_busy_input = (in.sourceid != conflicts[i].sourceid);

    if (is_busy_input)
        return true;

    if (!is_busy_input && conflicts[0].chanid)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT mplexid "
            "FROM channel "
            "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", conflicts[0].chanid);
        if (!query.exec())
            MythDB::DBError("is_input_group_busy", query);
        else if (query.next())
        {
            mplexid_restriction = query.value(0).toUInt();
            mplexid_restriction = (32767 == mplexid_restriction) ?
                0 : mplexid_restriction;
            if (mplexid_restriction)
                chanid_restriction = 0;
            else
                chanid_restriction = conflicts[0].chanid;
        }
    }

    return is_busy_input;
}

static bool is_input_busy(
    uint                       inputid,
    const vector<uint>        &groupids,
    QMap<uint,bool>           &busygrp,
    QMap<uint,bool>           &busyrec,
    QMap<uint,InputInfo>      &busyin,
    uint                      &mplexid_restriction,
    uint                      &chanid_restriction)
{
    bool is_busy = false;
    for (uint i = 0; i < groupids.size() && !is_busy; i++)
    {
        is_busy |= is_input_group_busy(
            inputid, groupids[i],
            busygrp, busyrec, busyin, mplexid_restriction, chanid_restriction);
    }
    return is_busy;
}

bool ChannelBase::IsInputAvailable(
    uint &mplexid_restriction, uint &chanid_restriction) const
{
    if (!m_inputid)
        return false;

    // Check each input to make sure it doesn't belong to an
    // input group which is attached to a busy recorder.
    QMap<uint,bool>           busygrp;
    QMap<uint,bool>           busyrec;
    QMap<uint,InputInfo>      busyin;

    // Cache our busy input if applicable
    if (m_pParent)
    {
        InputInfo info;
        busyrec[m_inputid] = m_pParent->IsBusy(&info);
        if (busyrec[m_inputid])
        {
            busyin[m_inputid] = info;
            info.chanid = GetChanID();
        }
    }

    mplexid_restriction = 0;
    chanid_restriction = 0;

    vector<uint> groupids = CardUtil::GetInputGroups(m_inputid);

    bool res = !is_input_busy(m_inputid, groupids,
                          busygrp, busyrec, busyin, mplexid_restriction,
                          chanid_restriction);
    return res;
}

/// \note m_system_lock must be held when this is called
bool ChannelBase::KillScript(void)
{
    if (!m_system)
        return true;

    m_system->Term(true);

    delete m_system;
    m_system = NULL;
    return true;
}

/// \note m_system_lock must NOT be held when this is called
void ChannelBase::HandleScript(const QString &freqid)
{
    QMutexLocker locker(&m_system_lock);

    bool ok = true;
    m_system_status = 0; // unknown

    if (!m_inputid)
    {
        m_system_status = 2; // failed
        HandleScriptEnd(true);
        return;
    }

    if (m_externalChanger.isEmpty())
    {
        m_system_status = 3; // success
        HandleScriptEnd(true);
        return;
    }

    if (freqid.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "A channel changer is set, but the freqid field is empty."
            "\n\t\t\tWe will return success to ease setup pains, "
            "but no script is will actually run.");
        m_system_status = 3; // success
        HandleScriptEnd(true);
        return;
    }

    // It's possible we simply never reaped the process, check status first.
    if (m_system)
        GetScriptStatus(true);

    // If it's still running, try killing it
    if (m_system)
        ok = KillScript();

    // The GetScriptStatus() call above can reset m_system_status with
    // the exit status of the last channel change script invocation, so
    // we must set it to pending here.
    m_system_status = 1; // pending

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Can not execute channel changer, previous call to script "
            "is still running.");
        m_system_status = 2; // failed
        HandleScriptEnd(ok);
    }
    else
    {
        if (m_externalChanger.toLower() == "internal")
        {
            ok = ChangeInternalChannel(freqid, m_inputid);
            if (!ok)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Can not execute internal channel "
                    "changer.");
                m_system_status = 2; // failed
            }
            else
                m_system_status = 3; // success

            HandleScriptEnd(ok);
        }
        else
        {
            ok = ChangeExternalChannel(m_externalChanger, freqid);
            if (!ok)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Can not execute channel changer.");
                m_system_status = 2; // failed
                HandleScriptEnd(ok);
            }
        }
    }
}

bool ChannelBase::ChangeInternalChannel(const QString &freqid,
                                        uint inputid)
{
#ifdef USING_FIREWIRE
    FirewireDevice *device = NULL;
    QString fwnode = CardUtil::GetFirewireChangerNode(inputid);
    uint64_t guid = string_to_guid(fwnode);
    QString fwmodel = CardUtil::GetFirewireChangerModel(inputid);

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Internal channel change to %1 "
            "on inputid %2, GUID %3 (%4)").arg(freqid).arg(inputid)
            .arg(fwnode).arg(fwmodel));

#ifdef USING_LINUX_FIREWIRE
    device = new LinuxFirewireDevice(
        guid, 0, 100, 1);
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
        device = NULL;
        return false;
    }

    device->ClosePort();
    delete device;
    device = NULL;
    return true;
#else
    return false;
#endif
}

/// \note m_system_lock must be held when this is called
bool ChannelBase::ChangeExternalChannel(const QString &changer,
                                        const QString &freqid)
{
    if (m_system)
        return false;

    if (changer.isEmpty() || freqid.isEmpty())
        return false;

    QString command = QString("%1 %2").arg(changer).arg(freqid);
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Running command: %1").arg(command));

    m_system = new MythSystemLegacy(command, kMSRunShell | kMSRunBackground);
    m_system->Run();

    return true;
}

uint ChannelBase::GetScriptStatus(bool holding_lock)
{
    if (!m_system)
        return m_system_status;

    if (!holding_lock)
        m_system_lock.lock();

    m_system_status = m_system->Wait();
    if (m_system_status != GENERIC_EXIT_RUNNING &&
        m_system_status != GENERIC_EXIT_START)
    {
        delete m_system;
        m_system = NULL;

        HandleScriptEnd(m_system_status == GENERIC_EXIT_OK);
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("GetScriptStatus() %1")
        .arg(m_system_status));

    uint ret;
    switch(m_system_status)
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
            .arg(m_system_status). arg(ret));

    m_system_status = ret;

    if (!holding_lock)
        m_system_lock.unlock();

    return ret;
}

/// \note m_system_lock must be held when this is called
void ChannelBase::HandleScriptEnd(bool ok)
{
    if (ok)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Channel change script succeeded.");
        if (m_inputid)
        {
            // Set this as the future start channel for this source
            m_startChanNum = m_curchannelname;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Channel change script failed.");
    }
}

int ChannelBase::GetChanID() const
{
    if (!m_inputid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid FROM channel "
                  "WHERE channum  = :CHANNUM AND "
                  "      sourceid = :SOURCEID");
    query.bindValue(":CHANNUM", m_curchannelname);
    query.bindValue(":SOURCEID", m_sourceid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("fetching chanid", query);
        return -1;
    }

    if (!query.next())
        return -1;

    return query.value(0).toInt();
}

/** \fn ChannelBase::InitializeInputs(void)
 *  \brief Fills in input map from DB
 */
bool ChannelBase::InitializeInput(void)
{
    if (!m_inputid)
    {
        if (m_pParent)
            m_inputid = m_pParent->GetInputId();
        else
            m_inputid = CardUtil::GetFirstInputID(GetDevice());
    }

    if (!m_inputid)
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
    query.bindValue(":INPUTID", m_inputid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("InitializeInputs", query);
        return false;
    }
    else if (!query.size())
    {
        LOG(VB_GENERAL, LOG_ERR, "InitializeInputs(): "
            "\n\t\t\tCould not get inputs for the capturecard."
            "\n\t\t\tPerhaps you have forgotten to bind video"
            "\n\t\t\tsources to your card's inputs?");
        return false;
    }

    query.next();

    m_sourceid = query.value(0).toUInt();
    m_name = query.value(1).toString();
    m_startChanNum = query.value(2).toString();
    m_externalChanger = query.value(3).toString();
    m_tuneToChannel = query.value(4).toString();

    m_channels = ChannelUtil::GetChannels(m_sourceid, false);
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
        .arg(m_inputid).arg(m_name).arg(m_startChanNum)
        .arg(m_sourceid));

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
                 m_sourceid     != sourceid);
    if (!skip)
        m_startChanNum = newChanNum;

    if (GetSourceID() == sourceid && oldChanNum == m_curchannelname)
        m_curchannelname = newChanNum;

    StoreInputChannels();
}

/** \fn ChannelBase::StoreInputChannels(void)
 *  \brief Sets starting channel for the each input in the input map.
 *  \param input Map from cardinputid to input params.
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
    query.bindValue(":CARDINPUTID", m_inputid);

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
        "WHERE channel.channum       = :CHANNUM             AND "
        "      channel.sourceid      = capturecard.sourceid AND "
        "      capturecard.cardid    = :INPUTID             AND "
        "      capturecard.hostname  = :HOSTNAME");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":INPUTID",   m_inputid);
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkchannel", query);
    }
    else if (query.size() > 0)
        return true;

    LOG(VB_CHANNEL, LOG_ERR, LOC +
        QString("Failed to find channel(%1) on input (%2).")
        .arg(channum).arg(m_inputid));
    return false;
}

ChannelBase *ChannelBase::CreateChannel(
    TVRec                    *tvrec,
    const GeneralDBOptions   &genOpt,
    const DVBDBOptions       &dvbOpt,
    const FireWireDBOptions  &fwOpt,
    const QString            &startchannel,
    bool                      enter_power_save_mode,
    QString                  &rbFileExt)
{
    rbFileExt = "ts";

    ChannelBase *channel = NULL;
    if (genOpt.inputtype == "DVB")
    {
#ifdef USING_DVB
        channel = new DVBChannel(genOpt.videodev, tvrec);
        static_cast<DVBChannel*>(channel)->SetSlowTuning(
            dvbOpt.dvb_tuning_delay);
#endif
    }
    else if (genOpt.inputtype == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
        channel = new FirewireChannel(tvrec, genOpt.videodev, fwOpt);
#endif
    }
    else if (genOpt.inputtype == "HDHOMERUN")
    {
#ifdef USING_HDHOMERUN
        channel = new HDHRChannel(tvrec, genOpt.videodev);
#endif
    }
    else if ((genOpt.inputtype == "IMPORT") ||
             (genOpt.inputtype == "DEMO") ||
             (genOpt.inputtype == "MPEG" &&
              genOpt.videodev.toLower().startsWith("file:")))
    {
        channel = new DummyChannel(tvrec);
        rbFileExt = "mpg";
    }
    else if (genOpt.inputtype == "FREEBOX") // IPTV
    {
#ifdef USING_IPTV
        channel = new IPTVChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (genOpt.inputtype == "VBOX")
    {
#ifdef USING_VBOX
        channel = new IPTVChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (genOpt.inputtype == "ASI")
    {
#ifdef USING_ASI
        channel = new ASIChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (genOpt.inputtype == "CETON")
    {
#ifdef USING_CETON
        channel = new CetonChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (genOpt.inputtype == "V4L2ENC")
    {
#ifdef USING_V4L2
        channel = new V4LChannel(tvrec, genOpt.videodev);
#endif
        if (genOpt.inputtype == "MPEG")
            rbFileExt = "mpg";
    }
    else if (CardUtil::IsV4L(genOpt.inputtype))
    {
#ifdef USING_V4L2
        channel = new V4LChannel(tvrec, genOpt.videodev);
#endif
        if (genOpt.inputtype != "HDPVR")
        {
            if (genOpt.inputtype != "MPEG")
                rbFileExt = "nuv";
            else
                rbFileExt = "mpg";
        }
    }
    else if (genOpt.inputtype == "EXTERNAL")
    {
        channel = new ExternalChannel(tvrec, genOpt.videodev);
    }

    if (!channel)
    {
        QString msg = QString(
            "%1 card configured on video device %2, \n"
            "but MythTV was not compiled with %3 support. \n"
            "\n"
            "Recompile MythTV with %4 support or remove the card \n"
            "from the configuration and restart MythTV.")
            .arg(genOpt.inputtype).arg(genOpt.videodev)
            .arg(genOpt.inputtype).arg(genOpt.inputtype);
        LOG(VB_GENERAL, LOG_ERR, "ChannelBase: CreateChannel() Error: \n" +
            msg + "\n");
        return NULL;
    }

    if (!channel->Open())
    {
        LOG(VB_GENERAL, LOG_ERR, "ChannelBase: CreateChannel() Error: " +
            QString("Failed to open device %1").arg(genOpt.videodev));
        delete channel;
        return NULL;
    }

    QString input = CardUtil::GetInputName(tvrec->GetInputId());
    QString channum = startchannel;
    channel->Init(channum, true);

    if (enter_power_save_mode)
    {
        if (channel &&
            ((genOpt.inputtype == "DVB" && dvbOpt.dvb_on_demand) ||
             CardUtil::IsV4L(genOpt.inputtype)))
        {
            channel->Close();
        }
        else
        {
            DTVChannel *dtvchannel = dynamic_cast<DTVChannel*>(channel);
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

    if (!m_inputid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("IsExternalChannelChangeInUse: "
                    "non-existant input"));
        return false;
    }

    return !m_externalChanger.isEmpty();
}

;
