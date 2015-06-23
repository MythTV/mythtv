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

#define LOC QString("ChannelBase[%1]: ").arg(GetCardID())

ChannelBase::ChannelBase(TVRec *parent) :
    m_pParent(parent), m_curchannelname(""),
    m_commfree(false), m_cardid(0), m_input(),
    m_system(NULL), m_system_status(0)
{
}

ChannelBase::~ChannelBase(void)
{
    QMutexLocker locker(&m_system_lock);
    if (m_system)
        KillScript();
}

bool ChannelBase::Init(QString &inputname, QString &startchannel, bool setchan)
{
    bool ok;

    if (!setchan)
        ok = inputname.isEmpty() ? false : IsTunable(inputname, startchannel);
    else if (inputname.isEmpty())
        ok = SetChannelByString(startchannel);
    else
        ok = SwitchToInput(inputname, startchannel);

    if (ok)
        return true;

    // try to find a valid channel if given start channel fails.
    QString msg1 = QString("Setting start channel '%1' failed, ")
        .arg(startchannel);
    QString msg2 = "and we failed to find any suitible channels on any input.";
    bool msg_error = true;

    QStringList inputs = GetConnectedInputs();
    // Note we use qFind rather than std::find() for ulibc compat (#4507)
    QStringList::const_iterator start =
        qFind(inputs.begin(), inputs.end(), inputname);
    start = (start == inputs.end()) ?  inputs.begin() : start;

    if (start != inputs.end())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Looking for startchannel '%1' on input '%2'")
            .arg(startchannel).arg(*start));
    }

    // Attempt to find an input for the requested startchannel
    QStringList::const_iterator it = start;
    while (it != inputs.end())
    {
        ChannelInfoList channels = GetChannels(*it);

        ChannelInfoList::const_iterator cit = channels.begin();
        for (; cit != channels.end(); ++cit)
        {
            if ((*cit).channum == startchannel &&
                IsTunable(*it, startchannel))
            {
                inputname = *it;
                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("Found startchannel '%1' on input '%2'")
                    .arg(startchannel).arg(inputname));
                return true;
            }
        }

        ++it;
        it = (it == inputs.end()) ? inputs.begin() : it;
        if (it == start)
            break;
    }

    it = start;
    while (it != inputs.end() && !ok)
    {
        uint mplexid_restriction = 0;
        uint chanid_restriction = 0;

        ChannelInfoList channels = GetChannels(*it);
        if (channels.size() &&
            IsInputAvailable(GetInputByName(*it), mplexid_restriction,
                             chanid_restriction))
        {
            uint chanid = ChannelUtil::GetNextChannel(
                channels, channels[0].chanid,
                mplexid_restriction, chanid_restriction, CHANNEL_DIRECTION_UP);

            ChannelInfoList::const_iterator cit =
                find(channels.begin(), channels.end(), chanid);

            if (chanid && cit != channels.end())
            {
                if (!setchan)
                {
                    ok = IsTunable(*it, (mplexid_restriction ||
                                         chanid_restriction) ?
                                   (*cit).channum : startchannel);
                }
                else
                    ok = SwitchToInput(*it, (*cit).channum);

                if (ok)
                {
                    inputname = *it;
                    if (mplexid_restriction || chanid_restriction)
                    {
                        startchannel = (*cit).channum;
                        startchannel.detach();
                    }
                    msg2 = QString("selected to '%1' on input '%2' instead.")
                        .arg(startchannel).arg(inputname);
                    msg_error = false;
                }
            }
        }

        ++it;
        it = (it == inputs.end()) ? inputs.begin() : it;
        if (it == start)
            break;
    }

    LOG(VB_GENERAL, ((msg_error) ? LOG_ERR : LOG_WARNING), LOC +
        msg1 + "\n\t\t\t" + msg2);

    return ok;
}

bool ChannelBase::IsTunable(const QString &input, const QString &channum) const
{
    QString loc = LOC + QString("IsTunable(%1,%2)").arg(input).arg(channum);

    if (!m_input.inputid || (!input.isEmpty() && input != m_input.name))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested non-existant input '%1'").arg(input));

        return false;
    }

    uint mplexid_restriction;
    uint chanid_restriction;
    if (!IsInputAvailable(m_input.inputid, mplexid_restriction, chanid_restriction))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested channel is on input '%1' "
                    "which is in a busy input group")
            .arg(m_input.inputid));

        return false;
    }

    // Fetch tuning data from the database.
    QString tvformat, modulation, freqtable, freqid, dtv_si_std;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, chanid, tsid, netid;
    bool commfree;

    if (!ChannelUtil::GetChannelData(m_input.sourceid, chanid, channum,
                                     tvformat, modulation, freqtable, freqid,
                                     finetune, frequency, dtv_si_std,
                                     mpeg_prog_num, atsc_major, atsc_minor,
                                     tsid, netid, mplexid, commfree))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Failed to find channel in DB on input '%1' ")
            .arg(m_input.inputid));

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
        if (!m_input.inputid)
            return 0;

        chanid = ChannelUtil::GetChanID(m_input.sourceid, m_curchannelname);
    }

    uint mplexid_restriction = 0;
    uint chanid_restriction = 0;
    (void)IsInputAvailable(m_input.inputid, mplexid_restriction,
                           chanid_restriction);

    return ChannelUtil::GetNextChannel(
        m_allchannels, chanid, mplexid_restriction, chanid_restriction,
        direction);
}

uint ChannelBase::GetNextChannel(const QString &channum, ChannelChangeDirection direction) const
{
    if (!m_input.inputid)
        return 0;

    uint chanid = ChannelUtil::GetChanID(m_input.sourceid, channum);
    return GetNextChannel(chanid, direction);
}

/** \fn ChannelBase::GetConnectedInputs(void) const
 *  \brief Returns names of connected inputs
 */
QStringList ChannelBase::GetConnectedInputs(void) const
{
    QStringList list;

    if (m_input.sourceid)
        list.push_back(m_input.name);

    return list;
}

/** \fn ChannelBase::GetInputByNum(int capchannel) const
 *  \brief Returns name of numbered input, returns null if not found.
 */
QString ChannelBase::GetInputByNum(uint inputid) const
{
    if (m_input.inputid == inputid)
        return m_input.name;
    return QString::null;
}

/** \fn ChannelBase::GetInputByName(const QString &input) const
 *  \brief Returns number of named input, returns -1 if not found.
 */
uint ChannelBase::GetInputByName(const QString &input) const
{
    if (m_input.name == input)
        return m_input.inputid;
    return 0;
}

bool ChannelBase::SwitchToInput(const QString &inputname)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
        return SwitchToInput(input, true);
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not find input: %1 on card").arg(inputname));
        return false;
    }
}

bool ChannelBase::SwitchToInput(const QString &inputname, const QString &chan)
{
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("SwitchToInput(%1,%2)")
        .arg(inputname).arg(chan));

    int input = GetInputByName(inputname);

    bool ok = false;
    if (input >= 0)
    {
        ok = SwitchToInput(input, false);
        if (ok)
            ok = SetChannelByString(chan);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not find input: %1 on card when setting channel %2")
            .arg(inputname).arg(chan));
    }
    return ok;
}

bool ChannelBase::SwitchToInput(uint newInputNum, bool setstarting)
{
    if (m_input.inputid != newInputNum || m_input.startChanNum.isEmpty())
        return false;

    uint mplexid_restriction;
    uint chanid_restriction;
    if (!IsInputAvailable(newInputNum, mplexid_restriction,
                          chanid_restriction))
        return false;

    // input switching code would go here

    if (setstarting)
        return SetChannelByString(m_input.startChanNum);

    return true;
}

static bool is_input_group_busy(
    uint                       inputid,
    uint                       groupid,
    uint                      excluded_input,
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
        if (excluded_input == cardids[i])
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
    uint                      excluded_input,
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
            inputid, groupids[i], excluded_input,
            busygrp, busyrec, busyin, mplexid_restriction, chanid_restriction);
    }
    return is_busy;
}

bool ChannelBase::IsInputAvailable(
    int inputid, uint &mplexid_restriction, uint &chanid_restriction) const
{
    if (inputid < 0)
        return false;

    // Check each input to make sure it doesn't belong to an
    // input group which is attached to a busy recorder.
    QMap<uint,bool>           busygrp;
    QMap<uint,bool>           busyrec;
    QMap<uint,InputInfo>      busyin;

    uint cid = GetCardID();
    // Cache our busy input if applicable
    if (m_pParent)
    {
        InputInfo info;
        busyrec[cid] = m_pParent->IsBusy(&info);
        if (busyrec[cid])
        {
            busyin[cid] = info;
            info.chanid = GetChanID();
        }
    }

    mplexid_restriction = 0;
    chanid_restriction = 0;

    vector<uint> groupids = CardUtil::GetInputGroups(inputid);

    bool res = !is_input_busy(inputid, groupids, cid,
                          busygrp, busyrec, busyin, mplexid_restriction,
                          chanid_restriction);
    return res;
}

uint ChannelBase::GetInputCardID(uint inputNum) const
{
    if (m_input.inputid == inputNum)
        return m_input.inputid;
    return 0;
}

ChannelInfoList ChannelBase::GetChannels(int inputNum) const
{
    return m_input.channels;
}

ChannelInfoList ChannelBase::GetChannels(const QString &inputname) const
{
    return m_input.channels;
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

    if (!m_input.inputid)
    {
        m_system_status = 2; // failed
        HandleScriptEnd(true);
        return;
    }

    if (m_input.externalChanger.isEmpty())
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
        if (m_input.externalChanger.toLower() == "internal")
        {
            ok = ChangeInternalChannel(freqid, m_input.inputid);
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
            ok = ChangeExternalChannel(m_input.externalChanger, freqid);
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
        if (m_input.inputid)
        {
            // Set this as the future start channel for this source
            m_input.startChanNum = m_curchannelname;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Channel change script failed.");
    }
}

/** \fn ChannelBase::GetCardID(void) const
 *  \brief Returns card id.
 */
uint ChannelBase::GetCardID(void) const
{
    if (m_cardid > 0)
        return m_cardid;

    if (m_pParent)
        return m_pParent->GetCaptureCardNum();

    if (GetDevice().isEmpty())
        return 0;

    return CardUtil::GetFirstInputID(GetDevice());
}

int ChannelBase::GetChanID() const
{
    if (!m_input.inputid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid FROM channel "
                  "WHERE channum  = :CHANNUM AND "
                  "      sourceid = :SOURCEID");
    query.bindValue(":CHANNUM", m_curchannelname);
    query.bindValue(":SOURCEID", m_input.sourceid);

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
bool ChannelBase::InitializeInputs(void)
{
    m_input.Clear();

    uint cardid = GetCardID();
    if (!cardid)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "InitializeInputs(): Programmer error, cardid invalid.");
        return false;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid, "
        "       inputname,   startchan, "
        "       tunechan,    externalcommand, "
        "       sourceid,    livetvorder "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

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

    m_allchannels.clear();
    QString order = gCoreContext->GetSetting("ChannelOrdering", "channum");

    uint sourceid = query.value(5).toUInt();
    ChannelInfoList channels = ChannelUtil::GetChannels(sourceid, false);

    ChannelUtil::SortChannels(channels, order);

    m_input = ChannelInputInfo(
        query.value(1).toString(), query.value(2).toString(),
        query.value(3).toString(), query.value(4).toString(),
        sourceid,
        query.value(0).toUInt(),   query.value(5).toUInt(),
        0,                         channels);

    if (!IsExternalChannelChangeSupported() &&
        !m_input.externalChanger.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "External Channel changer is "
            "set, but this device does not support it.");
        m_input.externalChanger.clear();
    }

    m_allchannels.insert(m_allchannels.end(),
                         channels.begin(), channels.end());
    ChannelUtil::SortChannels(m_allchannels, order, true);

    // print it
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Input #%1: '%2' schan(%3) sourceid(%4)")
        .arg(m_input.inputid).arg(m_input.name).arg(m_input.startChanNum)
        .arg(m_input.sourceid));
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Current Input #%1: '%2'")
        .arg(GetCurrentInputNum()).arg(GetCurrentInput()));

    return true;
}

/** \fn ChannelBase::Renumber(uint,const QString&,const QString&)
 *  \brief Changes a channum if we have it cached anywhere.
 */
void ChannelBase::Renumber(uint sourceid,
                           const QString &oldChanNum,
                           const QString &newChanNum)
{
    bool skip = (m_input.name.isEmpty()                ||
                 m_input.startChanNum.isEmpty()        ||
                 m_input.startChanNum != oldChanNum ||
                 m_input.sourceid     != sourceid);
    if (!skip)
        m_input.startChanNum = newChanNum;

    if (GetCurrentSourceID() == sourceid && oldChanNum == m_curchannelname)
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

    if (m_input.name.isEmpty() || m_input.startChanNum.isEmpty())
        return;

    query.prepare(
        "UPDATE capturecard "
        "SET startchan = :STARTCHAN "
        "WHERE cardid = :CARDINPUTID");
    query.bindValue(":STARTCHAN", m_input.startChanNum);
    query.bindValue(":CARDINPUTID", m_input.inputid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("StoreInputChannels", query);
}

bool ChannelBase::CheckChannel(const QString &channum,
                               QString& inputName) const
{
    inputName = "";

    bool ret = false;

    QString channelinput = GetCurrentInput();

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return false;

    query.prepare(
        "SELECT channel.chanid "
        "FROM channel, capturecard "
        "WHERE channel.channum       = :CHANNUM             AND "
        "      channel.sourceid      = capturecard.sourceid AND "
        "      capturecard.inputname = :INPUT               AND "
        "      capturecard.cardid    = :CARDID              AND "
        "      capturecard.hostname  = :HOSTNAME");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":INPUT",    channelinput);
    query.bindValue(":CARDID",   GetCardID());
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkchannel", query);
    }
    else if (query.size() > 0)
    {
        return true;
    }

    QString msg = QString(
        "Failed to find channel(%1) on current input (%2) of card (%3).")
        .arg(channum).arg(channelinput).arg(GetCardID());
    LOG(VB_CHANNEL, LOG_ERR, LOC + msg);

    // We didn't find it on the current input let's widen the search
    query.prepare(
        "SELECT channel.chanid, capturecard.inputname "
        "FROM channel, capturecard "
        "WHERE channel.channum      = :CHANNUM             AND "
        "      channel.sourceid     = capturecard.sourceid AND "
        "      capturecard.cardid   = :CARDID              AND "
        "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":CARDID",   GetCardID());
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("checkchannel", query);
    }
    else if (query.next())
    {
        QString test = query.value(1).toString();
        if (test != QString::null)
            inputName = test;

        msg = QString("Found channel(%1) on another input (%2) of card (%3).")
            .arg(channum).arg(inputName).arg(GetCardID());
        LOG(VB_CHANNEL, LOG_INFO, LOC + msg);

        return true;
    }

    msg = QString("Failed to find channel(%1) on any input of card (%2).")
        .arg(channum).arg(GetCardID());
    LOG(VB_CHANNEL, LOG_ERR, LOC + msg);

    query.prepare("SELECT NULL FROM channel");

    if (query.exec() && query.size() == 0)
        ret = true;

    return ret;
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
    if (genOpt.cardtype == "DVB")
    {
#ifdef USING_DVB
        channel = new DVBChannel(genOpt.videodev, tvrec);
        static_cast<DVBChannel*>(channel)->SetSlowTuning(
            dvbOpt.dvb_tuning_delay);
#endif
    }
    else if (genOpt.cardtype == "FIREWIRE")
    {
#ifdef USING_FIREWIRE
        channel = new FirewireChannel(tvrec, genOpt.videodev, fwOpt);
#endif
    }
    else if (genOpt.cardtype == "HDHOMERUN")
    {
#ifdef USING_HDHOMERUN
        channel = new HDHRChannel(tvrec, genOpt.videodev);
#endif
    }
    else if ((genOpt.cardtype == "IMPORT") ||
             (genOpt.cardtype == "DEMO") ||
             (genOpt.cardtype == "MPEG" &&
              genOpt.videodev.toLower().startsWith("file:")))
    {
        channel = new DummyChannel(tvrec);
        rbFileExt = "mpg";
    }
    else if (genOpt.cardtype == "FREEBOX") // IPTV
    {
#ifdef USING_IPTV
        channel = new IPTVChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (genOpt.cardtype == "VBOX")
    {
#ifdef USING_VBOX
        channel = new IPTVChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (genOpt.cardtype == "ASI")
    {
#ifdef USING_ASI
        channel = new ASIChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (genOpt.cardtype == "CETON")
    {
#ifdef USING_CETON
        channel = new CetonChannel(tvrec, genOpt.videodev);
#endif
    }
    else if (CardUtil::IsV4L(genOpt.cardtype))
    {
#ifdef USING_V4L2
        channel = new V4LChannel(tvrec, genOpt.videodev);
#endif
        if ((genOpt.cardtype != "MPEG") && (genOpt.cardtype != "HDPVR"))
            rbFileExt = "nuv";
        else
            rbFileExt = "mpg";
    }
    else if (genOpt.cardtype == "EXTERNAL")
    {
        channel = new ExternalChannel(tvrec, genOpt.videodev);
        rbFileExt = "mpg";
    }

    if (!channel)
    {
        QString msg = QString(
            "%1 card configured on video device %2, \n"
            "but MythTV was not compiled with %3 support. \n"
            "\n"
            "Recompile MythTV with %4 support or remove the card \n"
            "from the configuration and restart MythTV.")
            .arg(genOpt.cardtype).arg(genOpt.videodev)
            .arg(genOpt.cardtype).arg(genOpt.cardtype);
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

    QString input = CardUtil::GetInputName(tvrec->GetCaptureCardNum());
    QString channum = startchannel;
    channel->Init(input, channum, true);

    if (enter_power_save_mode)
    {
        if (channel &&
            ((genOpt.cardtype == "DVB" && dvbOpt.dvb_on_demand) ||
             CardUtil::IsV4L(genOpt.cardtype)))
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

    if (!m_input.inputid)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("IsExternalChannelChangeInUse: "
                    "non-existant input"));
        return false;
    }

    return !m_input.externalChanger.isEmpty();
}

;
