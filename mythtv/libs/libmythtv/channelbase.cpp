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

ChannelBase::ChannelBase(TVRec *parent)
    : 
    pParent(parent), channelorder("channum + 0"), curchannelname(""),
    capchannels(0), currentcapchannel(-1), commfree(false),
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

void ChannelBase::ToggleInputs(void)
{
    int newcapchannel = currentcapchannel;

    if (capchannels > 0)
    {
        do 
        {
            newcapchannel = (newcapchannel + 1) % capchannels;
        } while (inputTuneTo[newcapchannel].isEmpty());

        SwitchToInput(newcapchannel, true);
    }
}

QString ChannelBase::GetInputByNum(int capchannel) const
{
    if (capchannel > capchannels)
        return "";
    return channelnames[capchannel];
}

int ChannelBase::GetInputByName(const QString &input) const
{
    for (int i = capchannels-1; i >= 0; i--)
        if (channelnames[i] == input)
            return i;
    return -1;
}

void ChannelBase::SwitchToInput(const QString &inputname)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
        SwitchToInput(input, true);
    else
        VERBOSE(VB_IMPORTANT, QString("ChannelBase: Could not find input: "
                                      "%1 on card\n").arg(inputname));
}

void ChannelBase::SwitchToInput(const QString &inputname, const QString &chan)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
    {
        SwitchToInput(input, false);
        SetChannelByString(chan);
    }
    else
        VERBOSE(VB_IMPORTANT,
                QString("ChannelBase: Could not find input: %1 on card when "
                        "setting channel %2\n").arg(inputname).arg(chan));
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
    pParent->StoreInputChannels(inputChannel);
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
    if (progsep>=0)
    {
        currentProgramNum = chan.right(chan.length()-progsep-1).toInt();
        currentATSCMinorChannel = -1;
        currentATSCMajorChannel = chan.left(progsep).toInt();
    }
    else if (chansep>=0)
    {
        currentProgramNum = -1;
        currentATSCMinorChannel = chan.right(chan.length()-chansep-1).toInt();
        currentATSCMajorChannel = chan.left(chansep).toInt();
    }
    else
    {
        bool ok;
        int chanNum = chan.toInt(&ok);
        if (ok && chanNum>=10)
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
    if (currentATSCMinorChannel>=0)
        VERBOSE(VB_CHANNEL, QString("SetCachedATSCInfo(): %1_%2")
                .arg(currentATSCMajorChannel).arg(currentATSCMinorChannel));
    else
        VERBOSE(VB_CHANNEL, QString("SetCachedATSCInfo(): %1-%2")
                .arg(currentATSCMajorChannel).arg(currentProgramNum));
}
