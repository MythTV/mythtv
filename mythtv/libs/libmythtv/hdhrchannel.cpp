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
#include <iostream>
using namespace std;

// MythTV includes
#include "mythdbcon.h"
#include "mythcontext.h"
#include "hdhrchannel.h"
#include "videosource.h"

#define LOC QString("HDHRChan(%1): ").arg(pParent->GetCaptureCardNum())
#define LOC_ERR QString("HDHRChan(%1), Error: ") \
                    .arg(pParent->GetCaptureCardNum())

HDHRChannel::HDHRChannel(TVRec *parent, const QString &device, uint tuner)
    : QObject(NULL, "HDHRChannel"),
      ChannelBase(parent),      _control_socket(NULL),
      _device_id(0),            _device_ip(0),
      _tuner(tuner)
{
    bool valid;
    _device_id = device.toUInt(&valid, 16);

    if (!valid)
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
    if (_control_socket)
    {
        VERBOSE(VB_RECORD, LOC + QString("Card already open (channel)"));
        return true;
    }

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
    discoverSock = hdhomerun_discover_create();

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

    VERBOSE(VB_RECORD, LOC + "Successfully connected to device");
    return true;
}

QString HDHRChannel::DeviceGet(const QString &name)
{
    QMutexLocker locker(&_lock);
    //VERBOSE(VB_RECORD, LOC + QString("DeviceGet(%1)").arg(name));

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
    unsigned char tag, dlen;
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

    //VERBOSE(VB_RECORD, LOC + QString("DeviceGet(%1) -> '%2' len(%3)")
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

    return QString("dummy");
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

bool HDHRChannel::DeviceSetChannel(int newChannel)
{
    return TunerSet("channel", QString::number(newChannel));
}

bool HDHRChannel::SetChannelByString(const QString &channum)
{
    VERBOSE(VB_RECORD, LOC + QString("Set channel '%1'").arg(channum));
    if (!_control_socket)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Set channel request without active connection");
        return false;
    }

    QString inputName = GetCurrentInput();
    if (!CheckChannel(channum, inputName))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "CheckChannel failed. " +
                QString("Please verify channel '%1'").arg(channum) +
                " in the \"mythtv-setup\" Channel Editor.");
        return false;
    }

    // If CheckChannel filled in the inputName then we need to
    // change inputs and return, since the act of changing
    // inputs will change the channel as well.
    if (!inputName.isEmpty())
        return SwitchToInput(inputName, channum);

    SetCachedATSCInfo("");

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT freqid, atscsrcid, mplexid "
        "FROM channel, videosource "
        "WHERE videosource.sourceid = channel.sourceid AND "
        "      channum              = :CHANNUM         AND "
        "      channel.sourceid     = :SOURCEID");
    query.bindValue(":CHANNUM",  channum);
    query.bindValue(":SOURCEID", GetCurrentSourceID());

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("fetching_tuning_params", query);
        return false;
    }

    if (!query.next())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                    "CheckChannel failed because it could not\n"
                    "\t\t\tfind channel number '%1' in DB for source '%2'.")
                .arg(channum).arg(GetCurrentSourceID()));
        return false;
    }

    QString freqid = query.value(0).toString();
    uint atscsrcid = query.value(1).toInt();
    uint mplexid   = query.value(2).toInt();

    curchannelname = channum;
    inputs[currentInputID]->startChanNum = curchannelname;

    VERBOSE(VB_RECORD, LOC +
            QString("freqid = %1, atscsrcid = %2, mplexid = %3")
            .arg(freqid).arg(atscsrcid).arg(mplexid));

    DeviceSetChannel(freqid.toInt());

    QString min_maj = QString("%1_0").arg(channum);
    if (atscsrcid > 255)
    {
        min_maj = QString("%1_%2").arg(atscsrcid >> 8).arg(atscsrcid & 0xff);
    }
    SetCachedATSCInfo(min_maj);

    return true;
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
