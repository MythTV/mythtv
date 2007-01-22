/**
 *  FirewireDevice
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// Qt headers
#include <qdeepcopy.h>

// MythTV headers
#include "linuxfirewiredevice.h"
#include "darwinfirewiredevice.h"
#include "mythcontext.h"
#include "pespacket.h"

#define LOC      QString("FireDev(%1): ").arg(guid_to_string(m_guid))
#define LOC_WARN QString("FireDev(%1), Warning: ").arg(guid_to_string(m_guid))
#define LOC_ERR  QString("FireDev(%1), Error: ").arg(guid_to_string(m_guid))

static void fw_init(QMap<uint64_t,QString> &id_to_model);

QMap<uint64_t,QString> FirewireDevice::s_id_to_model;
QMutex                 FirewireDevice::s_static_lock;

FirewireDevice::FirewireDevice(uint64_t guid, uint subunitid, uint speed) :
    m_guid(guid),           m_subunitid(subunitid),
    m_speed(speed),
    m_last_channel(0),      m_last_crc(0),
    m_buffer_cleared(true), m_open_port_cnt(0),
    m_lock(false)
{
}

void FirewireDevice::AddListener(TSDataListener *listener)
{
    if (listener)
    {
        vector<TSDataListener*>::iterator it =
            find(m_listeners.begin(), m_listeners.end(), listener);

        if (it == m_listeners.end())
            m_listeners.push_back(listener);
    }

    VERBOSE(VB_RECORD, LOC + "AddListener() "<<m_listeners.size());
}

void FirewireDevice::RemoveListener(TSDataListener *listener)
{
    vector<TSDataListener*>::iterator it = m_listeners.end();

    do
    {
        it = find(m_listeners.begin(), m_listeners.end(), listener);
        if (it != m_listeners.end())
            m_listeners.erase(it);
    }
    while (it != m_listeners.end());

    VERBOSE(VB_RECORD, LOC + "RemoveListener() "<<m_listeners.size());
}

bool FirewireDevice::SetPowerState(bool on)
{
    QMutexLocker locker(&m_lock);

    vector<uint8_t> cmd;
    vector<uint8_t> ret;

    cmd.push_back(kAVCControlCommand);
    cmd.push_back(kAVCSubunitTypeUnit | kAVCSubunitIdIgnore);
    cmd.push_back(kAVCUnitPowerOpcode);
    cmd.push_back((on) ? kAVCPowerStateOn : kAVCPowerStateOff);

    QString cmdStr = (on) ? "on" : "off";
    VERBOSE(VB_RECORD, LOC + QString("Powering %1").arg(cmdStr));

    if (SendAVCCommand(cmd, ret, -1))
    {
        VERBOSE(VB_IMPORTANT, LOC + "Power on cmd failed (no response)");
        return false;
    }

    if (kAVCAcceptedStatus != ret[0])
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Power %1 failed").arg(cmdStr));

        return false;
    }

    VERBOSE(VB_RECORD, LOC +
            QString("Power %1 cmd sent successfully").arg(cmdStr));

    return true;
}

FirewireDevice::PowerState FirewireDevice::GetPowerState(void)
{
    QMutexLocker locker(&m_lock);

    vector<uint8_t> cmd;
    vector<uint8_t> ret;

    cmd.push_back(kAVCStatusInquiryCommand);
    cmd.push_back(kAVCSubunitTypeUnit | kAVCSubunitIdIgnore);
    cmd.push_back(kAVCUnitPowerOpcode);
    cmd.push_back(kAVCPowerStateQuery);

    VERBOSE(VB_CHANNEL, LOC + "Requesting STB Power State");

    if (!SendAVCCommand(cmd, ret, -1))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Power cmd failed (no response)");
        return kAVCPowerQueryFailed;
    }

    QString loc = LOC + "STB Power State: ";

    if (ret[0] != kAVCResponseImplemented)
    {
        VERBOSE(VB_CHANNEL, loc + "Query not implemented");
        return kAVCPowerUnknown;
    }

    // check 1st operand..
    if (ret[3] == kAVCPowerStateOn)
    {
        VERBOSE(VB_CHANNEL, loc + "On");
        return kAVCPowerOn;
    }

    if (ret[3] == kAVCPowerStateOff)
    {
        VERBOSE(VB_CHANNEL, loc + "Off");
        return kAVCPowerOff;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + "STB Power State: Unknown Response");

    return kAVCPowerUnknown;
}

bool FirewireDevice::SetChannel(const QString &panel_model,
                                uint alt_method, uint channel)
{
    QMutexLocker locker(&m_lock);

    if (!IsSTBSupported(panel_model))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Model: '%1' ").arg(panel_model) +
                "is not supported by internal channel changer.");
        return false;
    }

    int digit[3];
    digit[0] = (channel % 1000) / 100;
    digit[1] = (channel % 100)  / 10;
    digit[2] = (channel % 10);

    if (m_subunitid >= kAVCSubunitIdExtended)
        return false;

    vector<uint8_t> cmd;
    vector<uint8_t> ret;

    if ((panel_model.upper() == "GENERIC") ||
        (panel_model.upper() == "SA4200HD"))
    {
        cmd.push_back(kAVCControlCommand);
        cmd.push_back(kAVCSubunitTypePanel | m_subunitid);
        cmd.push_back(kAVCPanelPassThrough);
        cmd.push_back(kAVCPanelKeyTuneFunction | kAVCPanelKeyPress);

        cmd.push_back(4); // operand length
        cmd.push_back((channel>>8) & 0x0f);
        cmd.push_back(channel & 0xff);
        cmd.push_back(0x00);
        cmd.push_back(0x00);

        if (!SendAVCCommand(cmd, ret, -1))
            return false;

        bool press_ok = (kAVCAcceptedStatus == ret[0]);

        cmd[3]= kAVCPanelKeyTuneFunction | kAVCPanelKeyRelease;
        if (!SendAVCCommand(cmd, ret, -1))
            return false;

        bool release_ok = (kAVCAcceptedStatus == ret[0]);

        if (!press_ok && !release_ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Tuning failed");
            return false;
        }

        SetLastChannel(channel);
        return true;
    }

    bool is_mot = ((panel_model.upper() == "DCT-6200") ||
                   (panel_model.upper() == "DCT-6212") ||
                   (panel_model.upper() == "DCT-6216"));

    if (is_mot && !alt_method)
    {
        for (uint i = 0; i < 3 ;i++)
        {
            cmd.clear();
            cmd.push_back(kAVCControlCommand);
            cmd.push_back(kAVCSubunitTypePanel | m_subunitid);
            cmd.push_back(kAVCPanelPassThrough);
            cmd.push_back(kAVCPanelKey0 + digit[i] | kAVCPanelKeyPress);
            cmd.push_back(0x00);
            cmd.push_back(0x00);
            cmd.push_back(0x00);
            cmd.push_back(0x00);

            if (!SendAVCCommand(cmd, ret, -1))
                return false;

            usleep(500000);
        }

        SetLastChannel(channel);
        return true;
    }

    if (is_mot && alt_method)
    {
        cmd.push_back(kAVCControlCommand);
        cmd.push_back(kAVCSubunitTypePanel | m_subunitid);
        cmd.push_back(kAVCPanelPassThrough);
        cmd.push_back(kAVCPanelKeyTuneFunction | kAVCPanelKeyPress);

        cmd.push_back(4); // operand length
        cmd.push_back((channel>>8) & 0x0f);
        cmd.push_back(channel & 0xff);
        cmd.push_back(0x00);
        cmd.push_back(0xff);

        if (!SendAVCCommand(cmd, ret, -1))
            return false;

        SetLastChannel(channel);
        return true;
    }

    if (panel_model.upper() == "SA3250HD")
    {
        cmd.push_back(kAVCControlCommand);
        cmd.push_back(kAVCSubunitTypePanel | m_subunitid);
        cmd.push_back(kAVCPanelPassThrough);
        cmd.push_back(kAVCPanelKeyTuneFunction | kAVCPanelKeyRelease);

        cmd.push_back(4); // operand length
        cmd.push_back(0x30 | digit[2]);
        cmd.push_back(0x30 | digit[1]);
        cmd.push_back(0x30 | digit[0]);
        cmd.push_back(0xff);

        if (!SendAVCCommand(cmd, ret, -1))
            return false;

        cmd[5] = 0x30 | digit[0];
        cmd[6] = 0x30 | digit[1];
        cmd[7] = 0x30 | digit[2];

        if (!SendAVCCommand(cmd, ret, -1))
            return false;

        SetLastChannel(channel);
        return true;
    }

    return false;
}

void FirewireDevice::BroadcastToListeners(
    const unsigned char *data, uint dataSize)
{
    if ((dataSize >= TSPacket::SIZE) && (data[0] == SYNC_BYTE) &&
        ((data[1] & 0x1f) == 0) && (data[2] == 0))
    {
        ProcessPATPacket(*((const TSPacket*)data));
    }

    vector<TSDataListener*>::iterator it = m_listeners.begin();
    for (; it != m_listeners.end(); ++it)
        (*it)->AddData(data, dataSize);
}

void FirewireDevice::SetLastChannel(const uint channel)
{
    m_buffer_cleared = (channel == m_last_channel);
    m_last_channel   = channel;

    VERBOSE(VB_IMPORTANT, QString("SetLastChannel(%1): cleared: %2")
            .arg(channel).arg(m_buffer_cleared ? "yes" : "no"));
}

void FirewireDevice::ProcessPATPacket(const TSPacket &tspacket)
{
    if (!tspacket.TransportError() && !tspacket.ScramplingControl() &&
        tspacket.HasPayload() && tspacket.PayloadStart() && !tspacket.PID())
    {
        PESPacket pes = PESPacket::View(tspacket);
        uint crc = pes.CalcCRC();
        m_buffer_cleared |= (crc != m_last_crc);
        m_last_crc = crc;
#if 0
        VERBOSE(VB_RECORD, LOC +
                QString("ProcessPATPacket: CRC 0x%1 cleared: %2")
                .arg(crc,0,16).arg(m_buffer_cleared ? "yes" : "no"));
#endif
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't handle large PAT's");
    }
}

QString FirewireDevice::GetModelName(uint vendor_id, uint model_id)
{
    QMutexLocker locker(&s_static_lock);
    if (s_id_to_model.empty())
        fw_init(s_id_to_model);

    QString ret = s_id_to_model[(((uint64_t) vendor_id) << 32) | model_id];

    if (ret.isEmpty())
        return "GENERIC";

    return QDeepCopy<QString>(ret);
}

vector<AVCInfo> FirewireDevice::GetSTBList(void)
{
    vector<AVCInfo> list;

#ifdef USING_LINUX_FIREWIRE
    list = LinuxFirewireDevice::GetSTBList();
#elif USING_OSX_FIREWIRE
    list = DarwinFirewireDevice::GetSTBList();
#endif

//#define DEBUG_AVC_INFO
#ifdef DEBUG_AVC_INFO
    AVCInfo info;
    info.guid     = 0x0016928a7b600001ULL;
    info.specid   = 0x0;
    info.vendorid = 0x000014f8;
    info.modelid  = 0x00001072;
    info.firmware_revision = 0x0;
    info.product_name = "Explorer 4200 HD";
    list.push_back(info);

    info.guid     = 0xff2145a850e39810ULL;
    info.specid   = 0x0;
    info.vendorid = 0x000014f8;
    info.modelid  = 0x00000be0;
    info.firmware_revision = 0x0;
    info.product_name = "Explorer 3250 HD";
    list.push_back(info);
#endif // DEBUG_AVC_INFO

    return list;
}

static void fw_init(QMap<uint64_t,QString> &id_to_model)
{
    id_to_model[0x11e6ULL << 32 | 0x0be0] = "SA3250HD";
    id_to_model[0x14f8ULL << 32 | 0x0be0] = "SA3250HD";
    id_to_model[0x1692ULL << 32 | 0x0be0] = "SA3250HD";
    id_to_model[0x1947ULL << 32 | 0x0be0] = "SA3250HD";

    id_to_model[0x11e6ULL << 32 | 0x1072] = "SA4200HD";
    id_to_model[0x14f8ULL << 32 | 0x1072] = "SA4200HD";
    id_to_model[0x1692ULL << 32 | 0x1072] = "SA4200HD";
    id_to_model[0x1947ULL << 32 | 0x1072] = "SA4200HD";

    const uint64_t motorolla_vendor_ids[] =
    {   /* 6200 */
        0x0ce5,    0x0e5c,    0x1225,    0x0f9f,    0x1180,
        0x12c9,    0x11ae,    0x152f,    0x14e8,    0x16b5,    0x1371,
        /* 6212 */
        0x0f9f,    0x152f,
        /* 6216, 2224 */
        0x17ee,
    };
    const uint motorolla_vendor_id_cnt =
        sizeof(motorolla_vendor_ids) / sizeof(uint32_t);

    const uint32_t motorolla_6200model_ids[] = { 0x620a, 0x6200, };
    const uint32_t motorolla_6212model_ids[] = { 0x64ca, 0x64cb, };
    const uint32_t motorolla_6216model_ids[] = { 0x646b, };

    for (uint i = 0; i < motorolla_vendor_id_cnt; i++)
        for (uint j = 0; j < 2; j++)
            id_to_model[motorolla_vendor_ids[i] << 32 |
                        motorolla_6200model_ids[j]] = "DCT-6200";

    for (uint i = 0; i < motorolla_vendor_id_cnt; i++)
        for (uint j = 0; j < 2; j++)
            id_to_model[motorolla_vendor_ids[i] << 32 |
                        motorolla_6212model_ids[j]] = "DCT-6212";

    for (uint i = 0; i < motorolla_vendor_id_cnt; i++)
        for (uint j = 0; j < 2; j++)
            id_to_model[motorolla_vendor_ids[i] << 32 |
                        motorolla_6216model_ids[j]] = "DCT-6216";
}
