#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "videodev_myth.h"
#include "channel.h"
#include "frequencies.h"
#include "tv.h"
#include "mythcontext.h"
#include "exitcodes.h"
#include "mythdbcon.h"

#include <iostream>
using namespace std;

#define LOC QString("ChannelBase(%1): ").arg(GetCardID())
#define LOC_ERR QString("ChannelBase(%1) Error: ").arg(GetCardID())

ChannelBase::ChannelBase(TVRec *parent)
    : 
    pParent(parent), channelorder("channum + 0"), curchannelname(""),
    currentcapchannel(-1), commfree(false),
    currentATSCMajorChannel(-1), currentATSCMinorChannel(-1),
    currentProgramNum(-1)
{
}

ChannelBase::~ChannelBase(void)
{
}

bool ChannelBase::SetChannelByDirection(ChannelChangeDirection dir)
{
    bool fTune = false;
    QString start, nextchan;
    start = nextchan = pParent->GetNextChannel(this, dir);

    do
    {
       fTune = SetChannelByString(nextchan);
       if (!fTune)
           nextchan = pParent->GetNextChannel(this, dir);
    }
    while (!fTune && nextchan != start);

    return fTune;
}

int ChannelBase::GetNextInput(void) const
{
    QMap<int, QString>::const_iterator it;
    it = channelnames.find(currentcapchannel);

    // if we can't find the current input, start at the beginning.
    if (it == channelnames.end())
        it = channelnames.begin();

    // check that the list isn't empty...
    if (it == channelnames.end())
        return -1;

    // find the next _connected_ input.
    int i = 0;
    for (; i < 100; i++)
    {
        ++it;
        if (it == channelnames.end())
            it = channelnames.begin();
        if (!inputTuneTo[it.key()].isEmpty())
            break;
    }

    // if we found anything, including current cap channel return it
    return (i<100) ? it.key() : -1;
}

bool ChannelBase::ToggleInputs(void)
{
    int nextInput = GetNextInput();
    if (nextInput >= 0 && (nextInput != currentcapchannel))
        return SwitchToInput(nextInput, true);
    return true;
}

QString ChannelBase::GetInputByNum(int capchannel) const
{
    QMap<int, QString>::const_iterator it = channelnames.find(capchannel);
    if (it != channelnames.end())
        return *it;
    return "";
}

int ChannelBase::GetInputByName(const QString &input) const
{
    QMap<int, QString>::const_iterator it = channelnames.begin();
    for (; it != channelnames.end(); ++it)
    {
        if (*it == input)
            return it.key();
    }
    return -1;
}

bool ChannelBase::SwitchToInput(const QString &inputname)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
        return SwitchToInput(input, true);
    else
        VERBOSE(VB_IMPORTANT, QString("ChannelBase: Could not find input: "
                                      "%1 on card\n").arg(inputname));
    return false;
}

bool ChannelBase::SwitchToInput(const QString &inputname, const QString &chan)
{
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
        VERBOSE(VB_IMPORTANT,
                QString("ChannelBase: Could not find input: %1 on card when "
                        "setting channel %2\n").arg(inputname).arg(chan));
    }
    return ok;
}

bool ChannelBase::ChangeExternalChannel(const QString &channum)
{
    if (externalChanger[currentcapchannel].isEmpty())
        return false;

    QString command = QString("%1 %2").arg(externalChanger[currentcapchannel])
                                      .arg(channum);

    VERBOSE(VB_CHANNEL, QString("External channel change: %1").arg(command));
    pid_t child = fork();
    if (child < 0)
    {   // error encountered in creating fork
        QString msg("ChannelBase: fork error -- ");
        msg.append(strerror(errno));
        VERBOSE(VB_IMPORTANT, msg);
        return false;
    }
    else if (child == 0)
    {   // we are the new fork
        for(int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
            close(i);
        int ret = execl("/bin/sh", "sh", "-c", command.ascii(), NULL);
        QString msg("ChannelBase: ");
        if (EACCES == ret) {
            msg.append(QString("Access denied to /bin/sh"
                               " when executing %1\n").arg(command.ascii()));
        }
        msg.append(strerror(errno));
        VERBOSE(VB_IMPORTANT, msg);
        _exit(CHANNEL__EXIT__EXECL_ERROR); // this exit is ok
    }
    else
    {   // child contains the pid of the new process
        int status = 0, pid = 0;
        VERBOSE(VB_CHANNEL, "Waiting for External Tuning program to exit");

        bool timed_out = false;
        uint timeout = 30; // how long to wait in seconds
        time_t start_time = time(0);
        while (-1 != pid && !timed_out)
        {
            sleep(1);
            pid = waitpid(child, &status, WUNTRACED|WNOHANG);
            VERBOSE(VB_IMPORTANT, QString("ret_pid(%1) child(%2) status(0x%3)")
                    .arg(pid).arg(child).arg(status,0,16));
            if (pid==child)
                break;
            else if (time(0) > (time_t)(start_time + timeout))
                timed_out = true;
        }
        if (timed_out)
        {
            VERBOSE(VB_IMPORTANT, "External Tuning program timed out, killing");
            kill(child, SIGTERM);
            usleep(500);
            kill(child, SIGKILL);
            return false;
        }

        VERBOSE(VB_CHANNEL, "External Tuning program no longer running");
        if (WIFEXITED(status))
        {   // program exited normally
            int ret = WEXITSTATUS(status);
            if (CHANNEL__EXIT__EXECL_ERROR == ret)
            {
                VERBOSE(VB_IMPORTANT, QString("ChannelBase: Could not execute "
                                              "external tuning program."));
                return false;
            }
            else if (ret)
            {   // external tuning program returned error value
                VERBOSE(VB_IMPORTANT,
                        QString("ChannelBase: external tuning program "
                                "exited with error %1").arg(ret));
                return false;
            }
            VERBOSE(VB_IMPORTANT, "External Tuning program exited with no error");
        }
        else
        {   // program exited abnormally
            QString msg = QString("ChannelBase: external tuning program "
                                  "encountered error %1 -- ").arg(errno);
            msg.append(strerror(errno));
            VERBOSE(VB_IMPORTANT, msg);
            return false;
        }
    }

    return true;
}

void ChannelBase::StoreInputChannels(void)
{
    StoreInputChannels(inputChannel);
}

/** \fn ChannelBase::GetCachedPids(int, pid_cache_t&)
 *  \brief Returns cached MPEG PIDs when given a Channel ID.
 *
 *  \param chanid   Channel ID to fetch cached pids for.
 *  \param pid_cache List of PIDs with their TableID
 *                   types is returned in pid_cache.
 */
void ChannelBase::GetCachedPids(int chanid, pid_cache_t &pid_cache)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("SELECT pid, tableid FROM pidcache "
                               "WHERE chanid='%1'").arg(chanid);
    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetCachedPids: fetching pids", query);
        return;
    }
    
    while (query.next())
    {
        int pid = query.value(0).toInt(), tid = query.value(1).toInt();
        if ((pid >= 0) && (tid >= 0))
            pid_cache.push_back(pid_cache_item_t(pid, tid));
    }
}

/** \fn ChannelBase::SaveCachedPids(int, const pid_cache_t&)
 *  \brief Saves PIDs for PSIP tables to database.
 *
 *  \param chanid    Channel ID to fetch cached pids for.
 *  \param pid_cache List of PIDs with their TableID types to be saved.
 */
void ChannelBase::SaveCachedPids(int chanid, const pid_cache_t &pid_cache)
{
    MSqlQuery query(MSqlQuery::InitCon());

    /// delete
    QString thequery =
        QString("DELETE FROM pidcache WHERE chanid='%1'").arg(chanid);
    query.prepare(thequery);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetCachedPids -- delete", query);
        return;
    }

    /// insert
    pid_cache_t::const_iterator it = pid_cache.begin();
    for (; it != pid_cache.end(); ++it)
    {
        thequery = QString("INSERT INTO pidcache "
                           "SET chanid='%1', pid='%2', tableid='%3'")
            .arg(chanid).arg(it->first).arg(it->second);

        query.prepare(thequery);

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("GetCachedPids -- insert", query);
            return;
        }
    }
}

void ChannelBase::SetCachedATSCInfo(const QString &chan)
{
    int progsep = chan.find("-");
    int chansep = chan.find("_");
    if (progsep >= 0)
    {
        currentProgramNum = chan.right(chan.length()-progsep-1).toInt();
        currentATSCMinorChannel = -1;
        currentATSCMajorChannel = chan.left(progsep).toInt();
    }
    else if (chansep >= 0)
    {
        currentProgramNum = -1;
        currentATSCMinorChannel = chan.right(chan.length()-chansep-1).toInt();
        currentATSCMajorChannel = chan.left(chansep).toInt();
    }
    else
    {
        bool ok;
        int chanNum = chan.toInt(&ok);
        if (ok && chanNum >= 10)
        {
            currentATSCMinorChannel = chanNum%10;
            currentATSCMajorChannel = chanNum/10;
        }
        else
        {
            currentProgramNum = -1;
            currentATSCMajorChannel = currentATSCMinorChannel = -1;
        }
    }
    if (currentATSCMinorChannel >= 0)
        VERBOSE(VB_CHANNEL,
                QString("ChannelBase(%1)::SetCachedATSCInfo(%2): %3_%4")
                .arg(GetDevice()).arg(chan)
                .arg(currentATSCMajorChannel).arg(currentATSCMinorChannel));
    else if ((-1 == currentATSCMajorChannel) && (-1 == currentProgramNum))
        VERBOSE(VB_CHANNEL,
                QString("ChannelBase(%1)::SetCachedATSCInfo(%2): RESET")
                .arg(GetDevice()).arg(chan));
    else
        VERBOSE(VB_CHANNEL,
                QString("ChannelBase(%1)::SetCachedATSCInfo(%2): %3-%4")
                .arg(GetDevice()).arg(chan)
                .arg(currentATSCMajorChannel).arg(currentProgramNum));
}

int ChannelBase::GetCardID() const
{
    if (pParent)
        return pParent->GetCaptureCardNum();
    return -1;
}

/** \fn  ChannelBase::RetrieveInputChannels(QMap<int,QString>&,QMap<int,QString>&,QMap<int,QString>&,QMap<int,QString>&)
 */
void ChannelBase::RetrieveInputChannels(QMap<int, QString> &startChanNum,
                                        QMap<int, QString> &inputTuneTo,
                                        QMap<int, QString> &externalChanger,
                                        QMap<int, QString> &sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT inputname, "
        "       trim(externalcommand), "
        "       if (tunechan='', 'Undefined', tunechan), "
        "       if (startchan, startchan, ''), "
        "       sourceid "
        "FROM capturecard, cardinput "
        "WHERE capturecard.cardid = :CARDID AND "
        "      capturecard.cardid = cardinput.cardid");
    query.bindValue(":CARDID", GetCardID());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("RetrieveInputChannels", query);
        return;
    }
    else if (!query.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + 
                "\n\t\t\tCould not get inputs for the capturecard."
                "\n\t\t\tPerhaps you have forgotten to bind video"
                "\n\t\t\tsources to your card's inputs?");
        return;
    }

    while (query.next())
    {
        int inputNum = GetInputByName(query.value(0).toString());

        externalChanger[inputNum] = query.value(1).toString();
        // This is initialized to "Undefined" when empty, this is
        // so that GetNextInput() knows this is a connected input.
        inputTuneTo[inputNum]     = query.value(2).toString();
        startChanNum[inputNum]    = query.value(3).toString();
        sourceid[inputNum]        = query.value(4).toString();
    }
}

/** \fn ChannelBase::StoreInputChannels(const QMap<int, QString>&)
 *  \brief Sets starting channel for the each input in the 
 *         "startChanNum" map.
 *
 *  \param startChanNum Map from input number to channel, 
 *                      with an input and default channel 
 *                      for each input to update.
 */
void ChannelBase::StoreInputChannels(
    const QMap<int, QString> &startChanNum)
{
    int cardid = GetCardID();

    MSqlQuery query(MSqlQuery::InitCon());

    QMap<int, QString>::const_iterator it = startChanNum.begin();

    for (; it != startChanNum.end(); ++it)
    {
	int i = it.key();
        QString input = GetInputByNum(i);

        if (input.isEmpty() || startChanNum[i].isEmpty())
            continue;

        query.prepare(
            "UPDATE cardinput "
            "SET startchan = :STARTCHAN "
            "WHERE cardid    = :CARDID AND "
            "      inputname = :INPUTNAME");
        query.bindValue(":STARTCHAN", startChanNum[i]);
        query.bindValue(":CARDID",    cardid);
        query.bindValue(":INPUTNAME", input);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("StoreInputChannels", query);
    }
}
