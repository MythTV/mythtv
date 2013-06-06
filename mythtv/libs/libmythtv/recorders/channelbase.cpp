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
#include "sourceutil.h"
#include "exitcodes.h"
#include "cardutil.h"
#include "compat.h"

#define LOC QString("ChannelBase[%1]: ").arg(GetCardID())

ChannelBase::ChannelBase(TVRec *parent) :
    m_pParent(parent), m_curchannelname(""),
    m_currentInputID(-1), m_commfree(false), m_cardid(0),
    m_system(NULL), m_system_status(0)
{
}

ChannelBase::~ChannelBase(void)
{
    ClearInputMap();

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

        ChannelInfoList channels = GetChannels(*it);
        if (channels.size() &&
            IsInputAvailable(GetInputByName(*it), mplexid_restriction))
        {
            uint chanid = ChannelUtil::GetNextChannel(
                channels, channels[0].chanid,
                mplexid_restriction, CHANNEL_DIRECTION_UP);

            ChannelInfoList::const_iterator cit =
                find(channels.begin(), channels.end(), chanid);

            if (chanid && cit != channels.end())
            {
                if (!setchan)
                {
                    ok = IsTunable(*it, (mplexid_restriction) ?
                                   (*cit).channum : startchannel);
                }
                else
                    ok = SwitchToInput(*it, (*cit).channum);

                if (ok)
                {
                    inputname = *it;
                    if (mplexid_restriction)
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

    int inputid = m_currentInputID;
    if (!input.isEmpty())
        inputid = GetInputByName(input);

    InputMap::const_iterator it = m_inputs.find(inputid);
    if (it == m_inputs.end())
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested non-existant input '%1':'%2' ")
            .arg(input).arg(inputid));

        return false;
    }

    uint mplexid_restriction;
    if (!IsInputAvailable(inputid, mplexid_restriction))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Requested channel is on input '%1' "
                    "which is in a busy input group")
            .arg(inputid));

        return false;
    }

    // Fetch tuning data from the database.
    QString tvformat, modulation, freqtable, freqid, dtv_si_std;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, tsid, netid;
    bool commfree;

    if (!ChannelUtil::GetChannelData((*it)->sourceid, channum,
                                     tvformat, modulation, freqtable, freqid,
                                     finetune, frequency, dtv_si_std,
                                     mpeg_prog_num, atsc_major, atsc_minor,
                                     tsid, netid, mplexid, commfree))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Failed to find channel in DB on input '%1' ")
            .arg(inputid));

        return false;
    }

    if (mplexid_restriction && (mplexid != mplexid_restriction))
    {
        LOG(VB_GENERAL, LOG_ERR, loc + " " +
            QString("Channel is valid, but tuner is busy "
                    "on different multiplex (%1 != %2)")
            .arg(mplexid).arg(mplexid_restriction));

        return false;
    }

    return true;
}

uint ChannelBase::GetNextChannel(uint chanid, ChannelChangeDirection direction) const
{
    if (!chanid)
    {
        InputMap::const_iterator it = m_inputs.find(m_currentInputID);
        if (it == m_inputs.end())
            return 0;

        chanid = ChannelUtil::GetChanID((*it)->sourceid, m_curchannelname);
    }

    uint mplexid_restriction = 0;
    IsInputAvailable(m_currentInputID, mplexid_restriction);

    return ChannelUtil::GetNextChannel(
        m_allchannels, chanid, mplexid_restriction, direction);
}

uint ChannelBase::GetNextChannel(const QString &channum, ChannelChangeDirection direction) const
{
    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (it == m_inputs.end())
        return 0;

    uint chanid = ChannelUtil::GetChanID((*it)->sourceid, channum);
    return GetNextChannel(chanid, direction);
}

int ChannelBase::GetNextInputNum(void) const
{
    // Exit early if inputs don't exist..
    if (m_inputs.isEmpty())
        return -1;

    // Find current input
    InputMap::const_iterator it;
    it = m_inputs.find(m_currentInputID);

    // If we can't find the current input, start at
    // the beginning and don't increment initially.
    bool skip_incr = false;
    if (it == m_inputs.end())
    {
        it = m_inputs.begin();
        skip_incr = true;
    }

    // Find the next _connected_ input.
    int i = 0;
    for (; i < 100; i++)
    {
        if (!skip_incr)
        {
            ++it;
            it = (it == m_inputs.end()) ? m_inputs.begin() : it;
        }
        skip_incr = false;

        if ((*it)->sourceid)
            break;
    }

    // if we found anything, including current cap channel return it
    return (i<100) ? (int)it.key() : -1;
}

/** \fn ChannelBase::GetConnectedInputs(void) const
 *  \brief Returns names of connected inputs
 */
QStringList ChannelBase::GetConnectedInputs(void) const
{
    QStringList list;

    InputMap::const_iterator it = m_inputs.begin();
    for (; it != m_inputs.end(); ++it)
        if ((*it)->sourceid)
            list.push_back((*it)->name);

    return list;
}

/** \fn ChannelBase::GetInputByNum(int capchannel) const
 *  \brief Returns name of numbered input, returns null if not found.
 */
QString ChannelBase::GetInputByNum(int capchannel) const
{
    InputMap::const_iterator it = m_inputs.find(capchannel);
    if (it != m_inputs.end())
        return (*it)->name;
    return QString::null;
}

/** \fn ChannelBase::GetInputByName(const QString &input) const
 *  \brief Returns number of named input, returns -1 if not found.
 */
int ChannelBase::GetInputByName(const QString &input) const
{
    InputMap::const_iterator it = m_inputs.begin();
    for (; it != m_inputs.end(); ++it)
    {
        if ((*it)->name == input)
            return (int)it.key();
    }
    return -1;
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

bool ChannelBase::SwitchToInput(int newInputNum, bool setstarting)
{
    InputMap::const_iterator it = m_inputs.find(newInputNum);
    if (it == m_inputs.end() || (*it)->startChanNum.isEmpty())
        return false;

    uint mplexid_restriction;
    if (!IsInputAvailable(newInputNum, mplexid_restriction))
        return false;

    // input switching code would go here

    if (setstarting)
        return SetChannelByString((*it)->startChanNum);

    return true;
}

static bool is_input_group_busy(
    uint                       inputid,
    uint                       groupid,
    const vector<uint>        &excluded_cardids,
    QMap<uint,bool>           &busygrp,
    QMap<uint,bool>           &busyrec,
    QMap<uint,TunedInputInfo> &busyin,
    uint                      &mplexid_restriction)
{
    static QMutex        igrpLock;
    static InputGroupMap igrp;

    // If none are busy, we don't need to check further
    QMap<uint,bool>::const_iterator bit = busygrp.find(groupid);
    if ((bit != busygrp.end()) && !*bit)
        return false;

    vector<TunedInputInfo> conflicts;
    vector<uint> cardids = CardUtil::GetGroupCardIDs(groupid);
    for (uint i = 0; i < cardids.size(); i++)
    {
        if (find(excluded_cardids.begin(),
                 excluded_cardids.end(), cardids[i]) != excluded_cardids.end())
        {
            continue;
        }

        TunedInputInfo info;
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

    // If the source's channels aren't digitally tuned then there is a conflict
    is_busy_input = !SourceUtil::HasDigitalChannel(in.sourceid);
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
        }
    }

    return is_busy_input;
}

static bool is_input_busy(
    uint                       inputid,
    const vector<uint>        &groupids,
    const vector<uint>        &excluded_cardids,
    QMap<uint,bool>           &busygrp,
    QMap<uint,bool>           &busyrec,
    QMap<uint,TunedInputInfo> &busyin,
    uint                      &mplexid_restriction)
{
    bool is_busy = false;
    for (uint i = 0; i < groupids.size() && !is_busy; i++)
    {
        is_busy |= is_input_group_busy(
            inputid, groupids[i], excluded_cardids,
            busygrp, busyrec, busyin, mplexid_restriction);
    }
    return is_busy;
}

bool ChannelBase::IsInputAvailable(
    int inputid, uint &mplexid_restriction) const
{
    if (inputid < 0)
        return false;

    // Check each input to make sure it doesn't belong to an
    // input group which is attached to a busy recorder.
    QMap<uint,bool>           busygrp;
    QMap<uint,bool>           busyrec;
    QMap<uint,TunedInputInfo> busyin;

    // Cache our busy input if applicable
    uint cid = GetCardID();
    TunedInputInfo info;
    busyrec[cid] = m_pParent->IsBusy(&info);
    if (busyrec[cid])
    {
        busyin[cid] = info;
        info.chanid = GetChanID();
    }

    vector<uint> excluded_cardids;
    excluded_cardids.push_back(cid);

    mplexid_restriction = 0;

    vector<uint> groupids = CardUtil::GetInputGroups(inputid);

    return !is_input_busy(inputid, groupids, excluded_cardids,
                          busygrp, busyrec, busyin, mplexid_restriction);
}

/** \fn ChannelBase::GetFreeInputs(const vector<uint>&) const
 *  \brief Returns the recorders available inputs.
 *
 *   This filters out the connected inputs that belong to an input
 *   group which is busy. Recorders in the excluded cardids will
 *   not be considered busy for the sake of determining free inputs.
 *
 */
vector<InputInfo> ChannelBase::GetFreeInputs(
    const vector<uint> &excluded_cardids) const
{
    vector<InputInfo> new_list;

    QStringList list = GetConnectedInputs();
    if (list.empty())
        return new_list;

    // Check each input to make sure it doesn't belong to an
    // input group which is attached to a busy recorder.
    QMap<uint,bool>           busygrp;
    QMap<uint,bool>           busyrec;
    QMap<uint,TunedInputInfo> busyin;

    // Cache our busy input if applicable
    TunedInputInfo info;
    uint cid = GetCardID();
    busyrec[cid] = m_pParent->IsBusy(&info);
    if (busyrec[cid])
    {
        busyin[cid] = info;
        info.chanid = GetChanID();
    }

    // If we're busy and not excluded, all inputs are busy
    if (busyrec[cid] &&
        (find(excluded_cardids.begin(), excluded_cardids.end(),
              cid) == excluded_cardids.end()))
    {
        return new_list;
    }

    QStringList::const_iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        InputInfo info;
        vector<uint> groupids;
        info.inputid = GetInputByName(*it);

        if (!CardUtil::GetInputInfo(info, &groupids))
            continue;

        bool is_busy_grp = is_input_busy(
            info.inputid, groupids, excluded_cardids,
            busygrp, busyrec, busyin, info.mplexid);

        if (!is_busy_grp && info.livetvorder)
            new_list.push_back(info);
    }

    return new_list;
}

uint ChannelBase::GetInputCardID(int inputNum) const
{
    InputMap::const_iterator it = m_inputs.find(inputNum);
    if (it != m_inputs.end())
        return (*it)->cardid;
    return 0;
}

ChannelInfoList ChannelBase::GetChannels(int inputNum) const
{
    int inputid = (inputNum > 0) ? inputNum : m_currentInputID;

    ChannelInfoList ret;
    InputMap::const_iterator it = m_inputs.find(inputid);
    if (it != m_inputs.end())
        ret = (*it)->channels;

    return ret;
}

ChannelInfoList ChannelBase::GetChannels(const QString &inputname) const
{
    int inputid = m_currentInputID;
    if (!inputname.isEmpty())
    {
        int tmp = GetInputByName(inputname);
        inputid = (tmp > 0) ? tmp : inputid;
    }

    return GetChannels(inputid);
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

    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (it == m_inputs.end())
    {
        m_system_status = 2; // failed
        HandleScriptEnd(true);
        return;
    }

    if ((*it)->externalChanger.isEmpty())
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
        if ((*it)->externalChanger.toLower() == "internal")
        {
            ok = ChangeInternalChannel(freqid, (*it)->inputid);
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
            ok = ChangeExternalChannel((*it)->externalChanger, freqid);
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

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("GetScriptStatus() %1")
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

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("GetScriptStatus() %1 -> %2")
            .arg(m_system_status). arg(ret));

    m_system_status = ret;

    if (!holding_lock)
        m_system_lock.unlock();

    return ret;
}

/// \note m_system_lock must be held when this is called
void ChannelBase::HandleScriptEnd(bool ok)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Channel change script %1")
        .arg((ok) ? "succeeded" : "failed"));

    if (ok)
    {
        InputMap::const_iterator it = m_inputs.find(m_currentInputID);
        if (it != m_inputs.end())
        {
            // Set this as the future start channel for this source
            (*it)->startChanNum = m_curchannelname;
        }
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

    return CardUtil::GetFirstCardID(GetDevice());
}

int ChannelBase::GetChanID() const
{
    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (it == m_inputs.end())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT chanid FROM channel "
                  "WHERE channum  = :CHANNUM AND "
                  "      sourceid = :SOURCEID");
    query.bindValue(":CHANNUM", m_curchannelname);
    query.bindValue(":SOURCEID", (*it)->sourceid);

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
    ClearInputMap();

    uint cardid = GetCardID();
    if (!cardid)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "InitializeInputs(): Programmer error, cardid invalid.");
        return false;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid, "
        "       inputname,   startchan, "
        "       tunechan,    externalcommand, "
        "       sourceid,    livetvorder "
        "FROM cardinput "
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

    m_allchannels.clear();
    QString order = gCoreContext->GetSetting("ChannelOrdering", "channum");
    while (query.next())
    {
        uint sourceid = query.value(5).toUInt();
        ChannelInfoList channels = ChannelUtil::GetChannels(sourceid, false);

        ChannelUtil::SortChannels(channels, order);

        m_inputs[query.value(0).toUInt()] = new ChannelInputInfo(
            query.value(1).toString(), query.value(2).toString(),
            query.value(3).toString(), query.value(4).toString(),
            sourceid,                  cardid,
            query.value(0).toUInt(),   query.value(5).toUInt(),
            0,                         channels);

        if (!IsExternalChannelChangeSupported() &&
            !m_inputs[query.value(0).toUInt()]->externalChanger.isEmpty())
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "External Channel changer is "
                "set, but this device does not support it.");
            m_inputs[query.value(0).toUInt()]->externalChanger.clear();
        }

        m_allchannels.insert(m_allchannels.end(),
                           channels.begin(), channels.end());
    }
    ChannelUtil::SortChannels(m_allchannels, order, true);

    m_currentInputID = GetStartInput(cardid);

    // In case that initial input is not set
    if (m_currentInputID == -1)
        m_currentInputID = GetNextInputNum();

    // print em
    InputMap::const_iterator it;
    for (it = m_inputs.begin(); it != m_inputs.end(); ++it)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Input #%1: '%2' schan(%3) sourceid(%4) ccid(%5)")
            .arg(it.key()).arg((*it)->name).arg((*it)->startChanNum)
            .arg((*it)->sourceid).arg((*it)->cardid));
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Current Input #%1: '%2'")
        .arg(GetCurrentInputNum()).arg(GetCurrentInput()));

    return m_inputs.size();
}

/** \fn ChannelBase::Renumber(uint,const QString&,const QString&)
 *  \brief Changes a channum if we have it cached anywhere.
 */
void ChannelBase::Renumber(uint sourceid,
                           const QString &oldChanNum,
                           const QString &newChanNum)
{
    InputMap::iterator it = m_inputs.begin();

    for (; it != m_inputs.end(); ++it)
    {
        bool skip = ((*it)->name.isEmpty()                ||
                     (*it)->startChanNum.isEmpty()        ||
                     (*it)->startChanNum != oldChanNum ||
                     (*it)->sourceid     != sourceid);
        if (!skip)
            (*it)->startChanNum = newChanNum;
    }

    if (GetCurrentSourceID() == sourceid && oldChanNum == m_curchannelname)
        m_curchannelname = newChanNum;

    StoreInputChannels(m_inputs);
}

/** \fn ChannelBase::StoreInputChannels(const InputMap&)
 *  \brief Sets starting channel for the each input in the input map.
 *  \param input Map from cardinputid to input params.
 */
void ChannelBase::StoreInputChannels(const InputMap &inputs)
{
    MSqlQuery query(MSqlQuery::InitCon());
    InputMap::const_iterator it = inputs.begin();
    for (; it != inputs.end(); ++it)
    {
        if ((*it)->name.isEmpty() || (*it)->startChanNum.isEmpty())
            continue;

        query.prepare(
            "UPDATE cardinput "
            "SET startchan = :STARTCHAN "
            "WHERE cardinputid = :CARDINPUTID");
        query.bindValue(":STARTCHAN",   (*it)->startChanNum);
        query.bindValue(":CARDINPUTID", it.key());

        if (!query.exec() || !query.isActive())
            MythDB::DBError("StoreInputChannels", query);
    }
}

/** \fn ChannelBase::GetStartInput(uint)
 *  \brief Gets the default input for the cardid
 *  \param cardid ChannelBase::GetCardID()
 */
int ChannelBase::GetStartInput(uint cardid)
{
    return GetInputByName(CardUtil::GetStartInput(cardid));
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
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum      = :CHANNUM           AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.inputname  = :INPUT             AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID            AND "
        "      capturecard.hostname = :HOSTNAME");
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
        "SELECT channel.chanid, cardinput.inputname "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum      = :CHANNUM           AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID            AND "
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

void ChannelBase::ClearInputMap(void)
{
    InputMap::iterator it = m_inputs.begin();
    for (; it != m_inputs.end(); ++it)
        delete *it;
    m_inputs.clear();
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
    rbFileExt = "mpg";

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
    }
    else if (genOpt.cardtype == "FREEBOX")
    {
#ifdef USING_IPTV
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

    QString input = CardUtil::GetStartInput(tvrec->GetCaptureCardNum());
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

