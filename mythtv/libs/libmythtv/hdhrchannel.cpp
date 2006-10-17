/**
 *  DBox2Channel
 *  Copyright (c) 2006 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C includes
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <fcntl.h>

// C++ includes
#include <algorithm>
using namespace std;

// MythTV includes
#include "mythdbcon.h"
#include "mythcontext.h"
#include "hdhrchannel.h"
#include "videosource.h"
#include "channelutil.h"
#include "frequencytables.h"

#define LOC QString("HDHRChan(%1): ").arg(GetDevice())
#define LOC_ERR QString("HDHRChan(%1), Error: ").arg(GetDevice())

HDHRChannel::HDHRChannel(TVRec *parent, const QString &device, uint tuner)
    : ChannelBase(parent),      _control_socket(NULL),
      _device_id(0),            _device_ip(0),
      _tuner(tuner),            _lock(true)
{
    bool valid;
    _device_id = device.toUInt(&valid, 16);

    if (!valid || !hdhomerun_discover_validate_device_id(_device_id))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Invalid DeviceID '%1'")
                .arg(device));

        _device_id = HDHOMERUN_DEVICE_ID_WILDCARD;
    }
}

HDHRChannel::~HDHRChannel(void)
{
    Close();
}

bool HDHRChannel::Open(void)
{
    if (IsOpen())
        return true;

    if (!FindDevice())
        return false;

    if (!InitializeInputs())
        return false;

    return (_device_ip != 0) && Connect();
}

void HDHRChannel::Close(void)
{
    if (_control_socket)
    {
        hdhomerun_control_destroy(_control_socket);
        _control_socket = NULL;
    }
}

bool HDHRChannel::FindDevice(void)
{
    _device_ip = 0;

    /* Create socket. */
    struct hdhomerun_discover_sock_t *discoverSock = NULL;
    discoverSock = hdhomerun_discover_create(500);

    if (!discoverSock)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to create discovery socket");
        return false;
    }

    /* Discover. */
    for (int retry = 0; retry < 6; retry++)
    {
        /* Send discovery request. */
        int ret = hdhomerun_discover_send(discoverSock,
                                          HDHOMERUN_DEVICE_TYPE_TUNER,
                                          _device_id);
        if (ret < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unable to send discovery request" + ENO);
            break;
        }

        /* Wait for response. */
        struct hdhomerun_discover_device_t device;
        ret = hdhomerun_discover_recv(discoverSock, &device, 500);
        if (ret < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unable to listen for discovery response" + ENO);
            break;
        }
        if (ret == 0)
        {
            continue;
        }

        /* Found. */
        _device_ip = device.ip_addr;

        VERBOSE(VB_IMPORTANT, LOC +
                QString("device found at address %1.%2.%3.%4")
                .arg((_device_ip>>24) & 0xFF).arg((_device_ip>>16) & 0xFF)
                .arg((_device_ip>> 8) & 0xFF).arg((_device_ip>> 0) & 0xFF));

        hdhomerun_discover_destroy(discoverSock);
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("device not found"));
    hdhomerun_discover_destroy(discoverSock);

    return false;
}

bool HDHRChannel::Connect(void)
{
    _control_socket = hdhomerun_control_create(_device_ip, 2000);
    if (!_control_socket)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to connect to device");
        return false;
    }

    VERBOSE(VB_CHANNEL, LOC + "Successfully connected to device");
    return true;
}

QString HDHRChannel::DeviceGet(const QString &name)
{
    QMutexLocker locker(&_lock);
    //VERBOSE(VB_CHANNEL, LOC + QString("DeviceGet(%1)").arg(name));

    if (!_control_socket)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Get request failed (not connected)");
        return QString::null;
    }

    /* Send request. */
    if (hdhomerun_control_send_get_request(_control_socket, name) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Get request failed (send)" + ENO);
        return QString::null;
    }

    /* Wait for response. */
    struct hdhomerun_control_data_t response;
    if (hdhomerun_control_recv(_control_socket, &response, 2000) <= 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Get request failed (timeout)");
        return QString::null;
    }
    if (response.type != HDHOMERUN_TYPE_GETSET_RPY)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Get request failed (unexpected response)");
        return QString::null;
    }

    QString ret = QString::null;
    unsigned char *ptr = response.ptr, *data = NULL;
    unsigned char tag;
    int dlen;
    while (hdhomerun_read_tlv(&ptr, response.end, &tag, &dlen, &data) >= 0)
    {
        char buf[1024+64];
        if (HDHOMERUN_TAG_GETSET_VALUE == tag)
        {
            memcpy(buf, (char*)data, dlen);
            buf[dlen - 1] = 0x0;
            ret = QString(buf);
        }
        else if (HDHOMERUN_TAG_ERROR_MESSAGE == tag)
        {
            memcpy(buf, (char*)data, dlen);
            buf[dlen - 1] = 0x0;
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString("DeviceGet(%1): %2")
                    .arg(name).arg(buf));
        }
    }

    //VERBOSE(VB_CHANNEL, LOC + QString("DeviceGet(%1) -> '%2' len(%3)")
    //        .arg(name).arg(ret).arg(len));

    return ret;
}

QString HDHRChannel::DeviceSet(const QString &name, const QString &val)
{
    QMutexLocker locker(&_lock);

    if (!_control_socket)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Set request failed (not connected)");
        return QString::null;
    }

    /* Send request. */
    if (hdhomerun_control_send_set_request(_control_socket, name, val) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Set request failed (send)" + ENO);
        return QString::null;
    }

    /* Wait for response. */
    struct hdhomerun_control_data_t response;
    bzero(&response, sizeof(response));
    if (hdhomerun_control_recv(_control_socket, &response, 2000) <= 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Set request failed (timeout)");
        return QString::null;
    }
    if (response.type != HDHOMERUN_TYPE_GETSET_RPY)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Set request failed (unexpected response)");
        return QString::null;
    }

    QStringList list;
    QString tmp = "";
    unsigned char *ptr = response.ptr;
    for (;ptr < response.end; ptr++)
    {
        if (*ptr)
        {
            tmp += QChar(*((char*)ptr));
            continue;
        }
        list.push_back(tmp);
        tmp = "";
        ptr++;
        ptr++;
    }

    if (list.size() >= 2)
        return list[1];
    return "";
}

QString HDHRChannel::TunerGet(const QString &name)
{
    return DeviceGet(QString("/tuner%1/%2").arg(_tuner).arg(name));
}

QString HDHRChannel::TunerSet(const QString &name, const QString &value)
{
    return DeviceSet(QString("/tuner%1/%2").arg(_tuner).arg(name), value);
}

bool HDHRChannel::DeviceSetTarget(unsigned short localPort)
{
    if (localPort == 0)
    {
        return false;
    }

    unsigned long localIP = hdhomerun_control_get_local_addr(_control_socket);
    if (localIP == 0)
    {
        return false;
    }

    QString configValue = QString("%1.%2.%3.%4:%5")
        .arg((localIP >> 24) & 0xFF).arg((localIP >> 16) & 0xFF)
        .arg((localIP >>  8) & 0xFF).arg((localIP >>  0) & 0xFF)
        .arg(localPort);

    if (!TunerSet("target", configValue))
    {
        return false;
    }

    return true;
}

bool HDHRChannel::DeviceClearTarget()
{
    return TunerSet("target", "0.0.0.0:0");
}

bool HDHRChannel::SetChannelByString(const QString &channum)
{
    QString loc = LOC + QString("SetChannelByString(%1)").arg(channum);
    QString loc_err = loc + ", Error: ";
    VERBOSE(VB_CHANNEL, loc);
    
    if (!Open())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Channel object "
                "will not open, can not change channels.");

        return false;
    }

    QString inputName;
    if (!CheckChannel(channum, inputName))
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                "CheckChannel failed.\n\t\t\tPlease verify the channel "
                "in the 'mythtv-setup' Channel Editor.");

        return false;
    }

    // If CheckChannel filled in the inputName then we need to
    // change inputs and return, since the act of changing
    // inputs will change the channel as well.
    if (!inputName.isEmpty())
        return SwitchToInput(inputName, channum);

    SetCachedATSCInfo("");

    InputMap::const_iterator it = inputs.find(currentInputID);
    if (it == inputs.end())
        return false;

    // Fetch tuning data from the database.
    QString tvformat, modulation, freqtable, freqid;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, tsid, netid;

    if (!ChannelUtil::GetChannelData(
        (*it)->sourceid, channum,
        tvformat, modulation, freqtable, freqid,
        finetune, frequency,
        mpeg_prog_num, atsc_major, atsc_minor, tsid, netid,
        mplexid, commfree))
    {
        return false;
    }

    // If the frequency is zeroed out, don't use it directly.
    bool ok = (frequency > 0);
    if (!ok)
    {
        frequency = (freqid.toInt(&ok) + finetune) * 1000;
        mplexid = 0;
    }
    bool isFrequency = ok && (frequency > 10000000);

    // Tune to proper frequency
    if ((*it)->externalChanger.isEmpty())
    {
        if (isFrequency)
        {
            if (!Tune(frequency, inputName, modulation))
                return false;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "dtv_multiplex data is required for tuning");

            return false;
        }
    }
    else if (!ChangeExternalChannel(freqid))
        return false;

    // Set the current channum to the new channel's channum
    curchannelname = QDeepCopy<QString>(channum);

    // Set the major and minor channel for any additional multiplex tuning
    if (atsc_major || atsc_minor)
        SetCachedATSCInfo(QString("%1_%2").arg(atsc_major).arg(atsc_minor));
    else if (mpeg_prog_num >= 0)
        SetCachedATSCInfo(QString("0-%1").arg(mpeg_prog_num));
    else
        SetCachedATSCInfo(QString("%1_0").arg(channum));

    // Set this as the future start channel for this source
    inputs[currentInputID]->startChanNum = curchannelname;

    return true;
}

bool HDHRChannel::TuneMultiplex(uint mplexid)
{
    VERBOSE(VB_CHANNEL, LOC + QString("TuneMultiplex(%1)").arg(mplexid));

    MSqlQuery query(MSqlQuery::InitCon());

    int cardid = GetCardID();
    if (cardid < 0)
        return false;

    // Query for our tuning params
    QString thequery(
        "SELECT frequency, inputname, modulation "
        "FROM dtv_multiplex, cardinput, capturecard "
        "WHERE dtv_multiplex.sourceid = cardinput.sourceid AND "
        "      cardinput.cardid = capturecard.cardid AND ");

    thequery += QString("mplexid = '%1' AND cardinput.cardid = '%2'")
        .arg(mplexid).arg(cardid);

    query.prepare(thequery);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError(
            LOC + QString("TuneMultiplex(): Error, could not find tuning "
                          "parameters for transport %1.").arg(mplexid),query);
        return false;
    }
    if (!query.next())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "TuneMultiplex(): " +
                QString("Could not find tuning parameters for transport %1.")
                .arg(mplexid));
        return false;
    }

    uint    frequency  = query.value(0).toInt();
    QString inputname  = query.value(1).toString();
    QString modulation = query.value(2).toString();

    if (!Tune(frequency, inputname, modulation))
        return false;

    return true;
}

bool HDHRChannel::Tune(uint frequency, QString /*input*/, QString modulation)
{
    bool ok = false;

    VERBOSE(VB_CHANNEL, LOC + "TuneTo("<<frequency<<","<<modulation<<")");

    if (modulation == "8vsb")
        ok = TunerSet("channel", QString("8vsb:%1").arg(frequency));
    else if (modulation == "qam_64")
        ok = TunerSet("channel", QString("qam64:%1").arg(frequency));
    else if (modulation == "qam_256")
        ok = TunerSet("channel", QString("qam256:%1").arg(frequency));

    return ok;
}

bool HDHRChannel::SwitchToInput(const QString &inputname,
                                const QString &chan)
{
    if (inputname == GetCurrentInput())
    {
        return SetChannelByString(chan);
    }

    int inputnum = GetInputByName(inputname);
    if (inputnum < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to locate input '%1'").arg(inputname));
        return false;
    }

    /* Device only has one input so there is nothing on it to configure. */
    /* Maybe there will be a rev 2 one day? */
    currentInputID = inputnum;

    /* Set channel. */
    return SetChannelByString(chan);
}

bool HDHRChannel::SwitchToInput(int newInputNum, bool setstarting)
{
    (void)setstarting;

    InputMap::const_iterator it = inputs.find(newInputNum);
    if (it == inputs.end() || (*it)->startChanNum.isEmpty())
        return false;

    return SwitchToInput((*it)->name, (*it)->startChanNum);
}

bool HDHRChannel::AddPID(uint pid, bool do_update)
{
    QMutexLocker locker(&_lock);

    vector<uint>::iterator it;
    it = lower_bound(_pids.begin(), _pids.end(), pid);
    if (it != _pids.end() && *it == pid)
    {
        VERBOSE(VB_CHANNEL, "AddPID(0x"<<hex<<pid<<dec<<") NOOP");
        return true;
    }

    _pids.insert(it, pid);

    VERBOSE(VB_CHANNEL, "AddPID(0x"<<hex<<pid<<dec<<")");

    if (do_update)
        return UpdateFilters();
    return true;
}

bool HDHRChannel::DelPID(uint pid, bool do_update)
{
    QMutexLocker locker(&_lock);

    vector<uint>::iterator it;
    it = lower_bound(_pids.begin(), _pids.end(), pid);
    if (it == _pids.end())
    {
        VERBOSE(VB_CHANNEL, "DelPID(0x"<<hex<<pid<<dec<<") NOOP");
        return true;
    }

    if (*it == pid)
    {
        VERBOSE(VB_CHANNEL, "DelPID(0x"<<hex<<pid<<dec<<") -- found");
        _pids.erase(it);
    }
    else
    {
        VERBOSE(VB_CHANNEL, "DelPID(0x"<<hex<<pid<<dec<<") -- failed");
    }

    if (do_update)
        return UpdateFilters();
    return true;
}

bool HDHRChannel::DelAllPIDs(void)
{
    QMutexLocker locker(&_lock);

    VERBOSE(VB_CHANNEL, "DelAllPID()");
    _pids.clear();

    return UpdateFilters();
}

QString filt_str(uint pid)
{
    uint pid0 = (pid / (16*16*16)) % 16;
    uint pid1 = (pid / (16*16))    % 16;
    uint pid2 = (pid / (16))        % 16;
    uint pid3 = pid % 16;
    return QString("0x%1%2%3%4")
        .arg(pid0,0,16).arg(pid1,0,16)
        .arg(pid2,0,16).arg(pid3,0,16);
}

bool HDHRChannel::UpdateFilters(void)
{
    QMutexLocker locker(&_lock);

    QString filter = "";

    vector<uint> range_min;
    vector<uint> range_max;

    for (uint i = 0; i < _pids.size(); i++)
    {
        uint pid_min = _pids[i];
        uint pid_max  = pid_min;
        for (uint j = i + 1; j < _pids.size(); j++)
        {
            if (pid_max + 1 != _pids[j])
                break;
            pid_max++;
            i++;
        }
        range_min.push_back(pid_min);
        range_max.push_back(pid_max);
    }

    if (range_min.size() > 16)
    {
        range_min.resize(16);
        uint pid_max = range_max.back();
        range_max.resize(15);
        range_max.push_back(pid_max);
    }

    for (uint i = 0; i < range_min.size(); i++)
    {
        filter += filt_str(range_min[i]);
        if (range_min[i] != range_max[i])
            filter += QString("-%1").arg(filt_str(range_max[i]));
        filter += " ";
    }

    filter = filter.stripWhiteSpace();

    QString new_filter = TunerSet("filter", filter);

    QString msg = QString("Filter: '%1'").arg(filter);
    if (filter != new_filter)
        msg += QString("\n\t\t\t\t'%2'").arg(new_filter);

    VERBOSE(VB_CHANNEL, msg);

    return filter == new_filter;
}
