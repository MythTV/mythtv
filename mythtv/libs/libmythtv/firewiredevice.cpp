/**
 *  FirewireDevice
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C++ headers
#include <algorithm>

// Qt headers
#include <QMap>

// MythTV headers
#include "linuxfirewiredevice.h"
#include "darwinfirewiredevice.h"
#include "mythverbose.h"
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
    m_lock()
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

    VERBOSE(VB_RECORD, LOC + QString("AddListener() %1").arg(m_listeners.size()));
}

void FirewireDevice::RemoveListener(TSDataListener *listener)
{
    vector<TSDataListener*>::iterator it = m_listeners.end();

    do
    {
        it = find(m_listeners.begin(), m_listeners.end(), listener);
        if (it != m_listeners.end())
        {
            m_listeners.erase(it);
            it = m_listeners.begin();
        }
    }
    while (it != m_listeners.end());

    VERBOSE(VB_RECORD, LOC + QString("RemoveListener() %1").arg(m_listeners.size()));
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

    if (!SendAVCCommand(cmd, ret, -1))
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
    VERBOSE(VB_CHANNEL, QString("SetChannel(model %1, alt %2, chan %3)")
            .arg(panel_model).arg(alt_method).arg(channel));

    QMutexLocker locker(&m_lock);
    VERBOSE(VB_CHANNEL, "SetChannel() -- locked");

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
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "SetChannel: Extended subunits are not supported.");

        return false;
    }

    vector<uint8_t> cmd;
    vector<uint8_t> ret;

    if ((panel_model.toUpper() == "GENERIC") ||
        (panel_model.toUpper() == "SA4200HD") ||
        (panel_model.toUpper() == "SA4250HDC"))
    {
        if (panel_model.toUpper() == "SA4250HDC")
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "The Scientific Atlanta 4250 HDC is not supported "
                    "\n\t\t\tby any MythTV Firewire channel changer."
                    "At the moment you must use an IR blaster.");
        }

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

    // the PACE is obviously not a Motorola channel changer, but the
    // same commands work for it as the Motorola.
    bool is_mot = ((panel_model.toUpper().left(4) == "DCT-") ||
                   (panel_model.toUpper().left(4) == "DCH-") ||
                   (panel_model.toUpper().left(4) == "DCX-") ||
                   (panel_model.toUpper().left(4) == "QIP-") ||
                   (panel_model.toUpper().left(5) == "PACE-"));

    if (is_mot && !alt_method)
    {
        for (uint i = 0; i < 3 ;i++)
        {
            cmd.clear();
            cmd.push_back(kAVCControlCommand);
            cmd.push_back(kAVCSubunitTypePanel | m_subunitid);
            cmd.push_back(kAVCPanelPassThrough);
            cmd.push_back((kAVCPanelKey0 + digit[i]) | kAVCPanelKeyPress);
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

    if (panel_model.toUpper() == "SA3250HD")
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
    if (!tspacket.TransportError() && !tspacket.Scrambled() &&
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

    ret.detach();
    return ret;
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
    // WARNING: Please update 6200ch.c or sa3250ch.c as well
    //          when updating this function.

    const uint64_t sa_vendor_ids[] =
    {
        0x0a73,    0x0f21,    0x11e6,    0x14f8,    0x1692,    0x1868,
        0x1947,    0x1ac3,    0x1bd7,    0x1cea,    0x1e6b,    0x21be,
        0x223a,    0x22ce,    0x23be,    0x252e,
    };
    const uint sa_vendor_id_cnt =
        sizeof(sa_vendor_ids) / sizeof(uint64_t);

    for (uint i = 0; i < sa_vendor_id_cnt; i++)
    {
        id_to_model[sa_vendor_ids[i] << 32 | 0x0be0] = "SA3250HD";
        id_to_model[sa_vendor_ids[i] << 32 | 0x1072] = "SA4200HD";
        id_to_model[sa_vendor_ids[i] << 32 | 0x10cc] = "SA4250HDC";
        id_to_model[sa_vendor_ids[i] << 32 | 0x22ce] = "SA8300HD";
    }

    const uint64_t motorola_vendor_ids[] =
    {
        /* DCH-3200, DCX-3200 */
        0x1c11,    0x1cfb,    0x1fc4,    0x23a3,    0x23ee,    0x25f1,
        /* DCX-3432 */
        0x24a0,
        /* DCH-3416 */
        0x1e46,
        /* DCT-3416 */
        0x1bdd,
        /* DCT-3412 */
        0x159a,
        /* DCT-6200, DCT-3416 */
        0x0ce5,    0x0e5c,    0x1225,    0x0f9f,    0x1180,
        0x12c9,    0x11ae,    0x152f,    0x14e8,    0x16b5,    0x1371,
        0x19a6,    0x1aad,    0x0b06,    0x195e,    0x10dc,
        /* DCT-6212 */
        0x0f9f,    0x152f,
        /* DCT-6216, 2224 */
        0x17ee,    0x1a66,
        /* QIP 6200 */
        0x211e,
        /* QIP 7100 */
        0x2374,
        /* unknown, see http://standards.ieee.org/regauth/oui/oui.txt */
        0x04db,    0x0406,    0x0ce5,    0x111a,    0x1225,    0x1404,
        0x1626,    0x18c0,    0x1ade,    0x1cfb,    0x2040,    0x2180,
        0x2210,    0x230b,    0x2375,    0x2395,    0x23a2,    0x23ed,
        0x23ee,    0x23a0,    0x23a1,

    };
    const uint motorola_vendor_id_cnt =
        sizeof(motorola_vendor_ids) / sizeof(uint64_t);

    for (uint i = 0; i < motorola_vendor_id_cnt; i++)
    {
        id_to_model[motorola_vendor_ids[i] << 32 | 0xf740] = "DCX-3200";
        id_to_model[motorola_vendor_ids[i] << 32 | 0xfa07] = "DCX-3200";
        id_to_model[motorola_vendor_ids[i] << 32 | 0xea05] = "DCX-3432";
        id_to_model[motorola_vendor_ids[i] << 32 | 0xd330] = "DCH-3200";
        id_to_model[motorola_vendor_ids[i] << 32 | 0xb630] = "DCH-3416";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x34cb] = "DCT-3412";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x346b] = "DCT-3416";
        id_to_model[motorola_vendor_ids[i] << 32 | 0xb630] = "DCT-3416";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x6200] = "DCT-6200";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x620a] = "DCT-6200";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x64ca] = "DCT-6212";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x64cb] = "DCT-6212";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x646b] = "DCT-6216";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x8100] = "QIP-7100";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x7100] = "QIP-6200";
        id_to_model[motorola_vendor_ids[i] << 32 | 0x0001] = "QIP-7100";
    }

    const uint64_t pace_vendor_ids[] =
    {
        /* PACE 550-HD & 779 */
        0x1cc3, 0x5094,
    };

    const uint pace_vendor_id_cnt =
        sizeof(pace_vendor_ids) / sizeof(uint64_t);

    for (uint i = 0; i < pace_vendor_id_cnt; i++)
    {
        id_to_model[pace_vendor_ids[i] << 32 | 0x10551] = "PACE-550";
        id_to_model[pace_vendor_ids[i] << 32 | 0x10755] = "PACE-779";
    }
}

bool FirewireDevice::IsSTBSupported(const QString &panel_model)
{
    QString model = panel_model.toUpper();
    return ((model == "DCH-3200") ||
            (model == "DCH-3416") ||
            (model == "DCT-3412") ||
            (model == "DCT-3416") ||
            (model == "DCT-6200") ||
            (model == "DCT-6212") ||
            (model == "DCT-6216") ||
            (model == "SA3250HD") ||
            (model == "SA4200HD") ||
            (model == "SA4250HDC") ||
            (model == "SA8300HD") ||
            (model == "PACE-550") ||
            (model == "PACE-779") ||
            (model == "QIP-6200") ||
            (model == "QIP-7100") ||
            (model == "GENERIC"));
}

