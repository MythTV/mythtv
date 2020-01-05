// MythTV
#include "mythlogging.h"
#include "mythedid.h"

#define DESCRIPTOR_ALPHANUMERIC_STRING 0xFE
#define DESCRIPTOR_PRODUCT_NAME 0xFC
#define DESCRIPTOR_SERIAL_NUMBER 0xFF
#define DATA_BLOCK_OFFSET 0x36
#define SERIAL_OFFSET     0x0C
#define VERSION_OFFSET    0x12
#define DISPLAY_OFFSET    0x14
#define GAMMA_OFFSET      0x17
#define FEATURES_OFFSET   0x18
#define EXTENSIONS_OFFSET 0x7E

#define LOC QString("EDID: ")

MythEDID::MythEDID(QByteArray &Data)
  : m_data(Data)
{
    Parse();
}

MythEDID::MythEDID(const char* Data, int Length)
  : m_data(Data, Length)
{
    Parse();
}

bool MythEDID::Valid(void)
{
    return m_valid;
}

QStringList MythEDID::SerialNumbers(void)
{
    return m_serialNumbers;
}

uint16_t MythEDID::PhysicalAddress(void)
{
    return m_physicalAddress;
}

float MythEDID::Gamma(void)
{
    return m_gamma;
}

bool MythEDID::IsSRGB(void)
{
    return m_sRGB;
}

MythEDID::Primaries MythEDID::ColourPrimaries(void)
{
    return m_primaries;
}

int MythEDID::AudioLatency(bool Interlaced)
{
    return m_audioLatency[Interlaced ? 1 : 0];
}

int MythEDID::VideoLatency(bool Interlaced)
{
    return m_videoLatency[Interlaced ? 1 : 0];
}

// from QEdidParser
static QString ParseEdidString(const quint8 *data, bool Replace)
{
    QByteArray buffer(reinterpret_cast<const char *>(data), 13);

    // Erase carriage return and line feed
    buffer = buffer.replace('\r', '\0').replace('\n', '\0');

    // Replace non-printable characters with dash
    // Earlier versions of QEdidParser got this wrong - so we potentially get
    // different values for serialNumber from QScreen
    if (Replace)
    {
        for (int i = 0; i < buffer.count(); ++i) {
            if (buffer[i] < '\040' || buffer[i] > '\176')
                buffer[i] = '-';
        }
    }
    return QString::fromLatin1(buffer.trimmed());
}

void MythEDID::Parse(void)
{
    // This is adapted from various sources including QEdidParser, edid-decode and xrandr
    if (!m_data.constData() || !m_data.size())
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Invalid EDID");
        return;
    }

    m_size = static_cast<uint>(m_data.size());
    const auto *data = reinterpret_cast<const quint8 *>(m_data.constData());

    // EDID data should be in 128 byte blocks
    if ((m_size % 0x80) || data[0] != 0x00 || data[1] != 0xff)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Invalid EDID");
        return;
    }

    // checksum
    qint8 sum = 0;
    foreach (char i, m_data)
        sum += i;
    if (sum != 0)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Checksum error");
        return;
    }

    if (!ParseBaseBlock(data))
        return;

    // Look at extension blocks
    uint extensions = data[EXTENSIONS_OFFSET];
    uint available  = (m_size / 128) - 1;
    if (extensions != available)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Extension count error");
        return;
    }

    if (extensions < 1)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("EDID has %1 extension blocks").arg(extensions));
    for (uint i = 1; i <= available; ++i)
    {
        uint offset = i * 128;
        switch (data[offset])
        {
            case 0x02: m_valid &= ParseCTA861(data, offset); break;
            default: continue;
        }
    }
}

bool MythEDID::ParseBaseBlock(const quint8 *Data)
{
    // retrieve version
    if (Data[VERSION_OFFSET] != 1)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Unknown major version number");
        return false;
    }
    m_minorVersion = Data[VERSION_OFFSET + 1];

    // retrieve serial number. This may be subsequently overridden
    qint32 serial = Data[SERIAL_OFFSET] +
                    (Data[SERIAL_OFFSET + 1] << 8) +
                    (Data[SERIAL_OFFSET + 2] << 16) +
                    (Data[SERIAL_OFFSET + 3] << 24);
    if (serial > 0)
        m_serialNumbers << QString::number(serial);

    // digital or analog
    //bool digital = Data[DISPLAY_OFFSET] & 0x80;

    // retrieve gamma
    quint8 gamma = Data[GAMMA_OFFSET];
    if (gamma == 0xFF)
    {
        // for 1.4 - this means gamma is defined in an extension block
        if (m_minorVersion < 4)
            m_gamma = 1.0F;
    }
    else
    {
        m_gamma = (gamma + 100.0F) / 100.0F;
    }

    // Chromaticity
    // Note - the EDID format introduces slight rounding errors when converting
    // to sRGB specs. If sRGB is set, the client should use the spec values - not
    // the computed values
    m_sRGB = Data[FEATURES_OFFSET] & 0x04;
    static const unsigned char s_sRGB[10] =
        { 0xEE, 0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54 };
    bool srgb = !memcmp(Data + 0x19, s_sRGB, sizeof(s_sRGB));

    if (!m_sRGB && srgb)
        m_sRGB = true;
    else if (m_sRGB && !srgb)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Chromaticity mismatch!");

    // Red
    m_primaries.primaries[0][0] = ((Data[0x1B] << 2) |  (Data[0x19] >> 6)) / 1024.0F;
    m_primaries.primaries[0][1] = ((Data[0x1C] << 2) | ((Data[0x19] >> 4) & 3)) / 1024.0F;
    // Green
    m_primaries.primaries[1][0] = ((Data[0x1D] << 2) | ((Data[0x19] >> 2) & 3)) / 1024.0F;
    m_primaries.primaries[1][1] = ((Data[0x1E] << 2) |  (Data[0x19] & 3)) / 1024.0F;
    // Blue
    m_primaries.primaries[2][0] = ((Data[0x1F] << 2) |  (Data[0x1A] >> 6)) / 1024.0F;
    m_primaries.primaries[2][1] = ((Data[0x20] << 2) | ((Data[0x1A] >> 4) & 3)) / 1024.0F;
    // White
    m_primaries.whitepoint[0]   = ((Data[0x21] << 2) | ((Data[0x1A] >> 2) & 3)) / 1024.0F;
    m_primaries.whitepoint[1]   = ((Data[0x22] << 2) |  (Data[0x1A] & 3)) / 1024.0F;

    // Parse blocks
    for (int i = 0; i < 5; ++i)
    {
        const int offset = DATA_BLOCK_OFFSET + i * 18;
        if (Data[offset] != 0 || Data[offset + 1] != 0 || Data[offset + 2] != 0)
            continue;
        if (Data[offset + 3] == DESCRIPTOR_SERIAL_NUMBER)
        {
            m_serialNumbers << ParseEdidString(&Data[offset + 5], false);
            m_serialNumbers << ParseEdidString(&Data[offset + 5], true);
        }
    }

    // Set status
    foreach (const auto & sn, m_serialNumbers)
        if (!sn.isEmpty())
            m_valid = true;
    if (!m_valid)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No serial number(s) in EDID");
    return m_valid;
}

bool MythEDID::ParseCTA861(const quint8 *Data, uint Offset)
{
    if (Offset >= m_size)
        return false;

    bool result    = true;
    quint8 version = Data[Offset + 1];
    quint8 offset  = Data[Offset + 2];

    if (offset < 4 || version != 3)
        return result;

    for (uint i = Offset + 4; (i < (Offset + offset)) && (i < m_size); i += (Data[i] & 0x1F) + 1)
        result &= ParseCTABlock(Data, i);
    return result;
}

bool MythEDID::ParseCTABlock(const quint8 *Data, uint Offset)
{
    uint length = Data[Offset] & 0x1F;
    uint type  = (Data[Offset] & 0xE0) >> 5;
    switch (type)
    {
        case 0x01: break; // Audio data block
        case 0x02: break; // Video data block
        case 0x03: ParseVSDB(Data, Offset + 1, length); break; // Vendor Specific Data Block
        case 0x04: break; // Speaker Allocation data block
        case 0x05: break; // VESA DTC data block
        case 0x07: break; // Extended tag. HDR metadata here
        default: break;
    }
    return true;
}

bool MythEDID::ParseVSDB(const quint8 *Data, uint Offset, uint Length)
{
    if (Offset + 3 >= m_size)
        return false;

    int registration = Data[Offset] + (Data[Offset + 1] << 8) + (Data[Offset + 2] << 16);

    // HDMI
    while (registration == 0x000C03)
    {
        if (Length < 5 || (Offset + 5 >= m_size))
            break;

        // CEC physical address
        m_physicalAddress = static_cast<uint16_t>((Data[Offset + 3] << 8) + Data[Offset + 4]);
        if (Length < 8 || (Offset + 8 >= m_size))
            break;

        // Audio and video latencies
        m_latencies  = (Data[Offset + 7] & 0x80);
        m_interLatencies = (Data[Offset + 7] & 0x40) && m_latencies;

        if (Length < 10 || (Offset + 10 >= m_size))
            break;
        if (m_latencies)
        {
            quint8 video = Data[Offset + 8];
            if (video > 0 && video <= 251)
                m_videoLatency[0] = (video - 1) << 1;
            quint8 audio = Data[Offset + 9];
            if (audio > 0 && audio <= 251)
                m_audioLatency[0] = (audio - 1) << 1;
        }

        if (Length < 12 || (Offset + 12 >= m_size))
            break;

        if (m_interLatencies)
        {
            quint8 video = Data[Offset + 10];
            if (video > 0 && video <= 251)
                m_videoLatency[1] = (video - 1) << 1;
            quint8 audio = Data[Offset + 11];
            if (audio > 0 && audio <= 251)
                m_audioLatency[1] = (audio - 1) << 1;
        }

        break;
    }
    return true;
}

void MythEDID::Debug(void)
{
    if (!m_valid)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Invalid EDID");
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Version:1.%1 Size:%2 Exensions:%3")
            .arg(m_minorVersion).arg(m_size).arg(m_size / 128 -1));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Gamma:%1 sRGB:%2")
            .arg(static_cast<double>(m_gamma)).arg(m_sRGB));
    LOG(VB_GENERAL, LOG_INFO, LOC + "Display chromaticity:-");
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Red:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.primaries[0][0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.primaries[0][1]), 0, 'f', 4));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Green:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.primaries[1][0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.primaries[1][1]), 0, 'f', 4));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Blue:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.primaries[2][0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.primaries[2][1]), 0, 'f', 4));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("White:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.whitepoint[0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.whitepoint[1]), 0, 'f', 4));
    QString address = !m_physicalAddress ? QString("N/A") :
            QString("%1.%2.%3.%4").arg((m_physicalAddress >> 12) & 0xF)
                                  .arg((m_physicalAddress >> 8) & 0xF)
                                  .arg((m_physicalAddress >> 4) & 0xF)
                                  .arg(m_physicalAddress & 0xF);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Physical address: %1").arg(address));
    if (m_latencies)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Latencies: Audio:%1 Video:%2")
                .arg(m_audioLatency[0]).arg(m_videoLatency[0]));
        if (m_interLatencies)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Latencies: Audio:%1 Video:%2 (Interlaced)")
                    .arg(m_audioLatency[1]).arg(m_videoLatency[1]));
        }
    }
}
