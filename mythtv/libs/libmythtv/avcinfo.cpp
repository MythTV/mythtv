// -*- Mode: c++ -*-

// MythTV headers
#include "avcinfo.h"
#include "firewiredevice.h"

QString guid_to_string(uint64_t guid)
{
    QString g0 = QString("%1").arg((uint32_t) (guid >> 32), 0, 16);
    QString g1 = QString("%1").arg((uint32_t) guid, 0, 16);

    while (g0.length() < 8)
        g0 = "0" + g0;
    while (g1.length() < 8)
        g1 = "0" + g1;

    return g0.toUpper() + g1.toUpper();
}

uint64_t string_to_guid(const QString &guid)
{
    // QString::toULongLong() is not supported in older Qt's..
    QString guid_l = guid.right(8);
    QString guid_h = guid.left(guid.length() - 8);
    return (((uint64_t)guid_h.toULong(NULL, 16)) << 32 |
            ((uint64_t)guid_l.toULong(NULL, 16)));
}

AVCInfo::AVCInfo() :
    port(-1), node(-1),
    guid(0), specid(0), vendorid(0), modelid(0),
    firmware_revision(0), product_name(QString::null)
{
    memset(unit_table, 0xff, sizeof(unit_table));
}

AVCInfo::AVCInfo(const AVCInfo &o) :
    port(o.port),         node(o.node),
    guid(o.guid),         specid(o.specid),
    vendorid(o.vendorid), modelid(o.modelid),
    firmware_revision(o.firmware_revision),
    product_name(o.product_name)
{
    product_name.detach();
    memcpy(unit_table, o.unit_table, sizeof(unit_table));
}

AVCInfo &AVCInfo::operator=(const AVCInfo &o)
{
    port     = o.port;
    node     = o.node;
    guid     = o.guid;
    specid   = o.specid;
    vendorid = o.vendorid;
    modelid  = o.modelid;
    firmware_revision = o.firmware_revision;
    product_name = o.product_name;
    product_name.detach();
    memcpy(unit_table, o.unit_table, sizeof(unit_table));

    return *this;
}

bool AVCInfo::GetSubunitInfo(void)
{
    memset(unit_table, 0xff, 32 * sizeof(uint8_t));

    for (uint i = 0; i < 8; i++)
    {
        vector<uint8_t> cmd;
        vector<uint8_t> ret;

        cmd.push_back(FirewireDevice::kAVCStatusInquiryCommand);
        cmd.push_back(FirewireDevice::kAVCSubunitTypeUnit |
                      FirewireDevice::kAVCSubunitIdIgnore);
        cmd.push_back(FirewireDevice::kAVCUnitSubunitInfoOpcode);
        cmd.push_back((i<<4) | 0x07);
        cmd.push_back(0xFF);
        cmd.push_back(0xFF);
        cmd.push_back(0xFF);
        cmd.push_back(0xFF);

        if (!SendAVCCommand(cmd, ret, -1))
            return false;

        if (ret.size() >= 8)
        {
            unit_table[(i<<2)+0] = ret[4];
            unit_table[(i<<2)+1] = ret[5];
            unit_table[(i<<2)+2] = ret[6];
            unit_table[(i<<2)+3] = ret[7];
        }
    }

    return true;
}

bool AVCInfo::IsSubunitType(int subunit_type) const
{
    for (uint i = 0; i < 32; i++)
    {
        int subunit = unit_table[i];
        if ((subunit != 0xff) &&
            (subunit & FirewireDevice::kAVCSubunitTypeUnit) == subunit_type)
        {
            return true;
        }
    }

    return false;
}

QString AVCInfo::GetSubunitInfoString(void) const
{
    QString str = "Subunit Types: ";

    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeVideoMonitor))
        str += "Video Monitor, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeAudio))
        str += "Audio, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypePrinter))
        str += "Printer, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeDiscRecorder))
        str += "Disk Recorder, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeTapeRecorder))
        str += "Tape Recorder, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeTuner))
        str += "Tuner, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeCA))
        str += "CA, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeVideoCamera))
        str += "Camera, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypePanel))
        str += "Panel, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeBulletinBoard))
        str += "Bulletin Board, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeCameraStorage))
        str += "Camera Storage, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeMusic))
        str += "Music, ";
    if (IsSubunitType(FirewireDevice::kAVCSubunitTypeVendorUnique))
        str += "Vendor Unique, ";

    return str;
}
