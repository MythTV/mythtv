// MythTV
#include "libmythbase/mythlogging.h"
#include "mythedid.h"

// Qt
#include <QObject>

// Std
#include <cmath>

//static constexpr uint8_t DESCRIPTOR_ALPHANUMERIC_STRING { 0xFE };
static constexpr uint8_t DESCRIPTOR_PRODUCT_NAME          { 0xFC };
static constexpr uint8_t DESCRIPTOR_RANGE_LIMITS          { 0xFD };
static constexpr uint8_t DESCRIPTOR_SERIAL_NUMBER         { 0xFF };
static constexpr size_t DATA_BLOCK_OFFSET                 { 0x36 };
static constexpr size_t SERIAL_OFFSET                     { 0x0C };
static constexpr size_t VERSION_OFFSET                    { 0x12 };
//static constexpr size_t DISPLAY_OFFSET                  { 0x14 };
static constexpr size_t WIDTH_OFFSET                      { 0x15 };
static constexpr size_t HEIGHT_OFFSET                     { 0x16 };
static constexpr size_t GAMMA_OFFSET                      { 0x17 };
static constexpr size_t FEATURES_OFFSET                   { 0x18 };
static constexpr size_t EXTENSIONS_OFFSET                 { 0x7E };

#define LOC QString("EDID: ")

MythEDID::MythEDID(QByteArray  Data)
  : m_data(std::move(Data))
{
    Parse();
}

MythEDID::MythEDID(const char* Data, int Length)
  : m_data(Data, Length)
{
    Parse();
}

bool MythEDID::Valid() const
{
    return m_valid;
}

QStringList MythEDID::SerialNumbers() const
{
    return m_serialNumbers;
}

QSize MythEDID::DisplaySize() const
{
    return m_displaySize;
}

double MythEDID::DisplayAspect() const
{
    return m_displayAspect;
}

uint16_t MythEDID::PhysicalAddress() const
{
    return m_physicalAddress;
}

float MythEDID::Gamma() const
{
    return m_gamma;
}

bool MythEDID::IsHDMI() const
{
    return m_isHDMI;
}

bool MythEDID::IsSRGB() const
{
    return m_sRGB;
}

bool MythEDID::IsLikeSRGB() const
{
    return m_likeSRGB;
}

MythColourSpace MythEDID::ColourPrimaries() const
{
    return m_primaries;
}

int MythEDID::AudioLatency(bool Interlaced) const
{
    return m_audioLatency[Interlaced ? 1 : 0];
}

int MythEDID::VideoLatency(bool Interlaced) const
{
    return m_videoLatency[Interlaced ? 1 : 0];
}

MythHDRDesc MythEDID::GetHDRSupport() const
{
    return { m_hdrSupport, m_minLuminance, m_maxAvgLuminance, m_maxLuminance };
}

/*! \brief Return the range of supported refresh rates
 *
 * If the VRR range is explicit in the EDID then the third element of MythVRRRange
 * is set to true, otherwise 'false' indicates that the VRR range is assumed from
 * the vertical refresh range indicated in the EDID. In both cases the behaviour
 * of the display below any minimum is undefined (e.g. there is no current indication
 * whether the display supports 'FreeSync Premium' - in which case low frame rates
 * may produce significant jitter).
*/
MythVRRRange MythEDID::GetVRRRange() const
{
    if (m_valid && m_vrrMin && m_vrrMax)
        return { m_vrrMin, m_vrrMax, true };
    if (m_valid && m_vrangeMin && m_vrangeMax)
        return { m_vrangeMin, m_vrangeMax, false };
    return { 0, 0, false };
}

// from QEdidParser
static QString ParseEdidString(const quint8* data, bool Replace)
{
    QByteArray buffer(reinterpret_cast<const char *>(data), 13);

    // Erase carriage return and line feed
    buffer = buffer.replace('\r', '\0').replace('\n', '\0');

    // Replace non-printable characters with dash
    // Earlier versions of QEdidParser got this wrong - so we potentially get
    // different values for serialNumber from QScreen
    if (Replace)
    {
        for (auto && i : buffer) {
            if (i < '\040' || i > '\176')
                i = '-';
        }
    }
    return QString::fromLatin1(buffer.trimmed());
}

void MythEDID::Parse()
{
    // This is adapted from various sources including QEdidParser, edid-decode and xrandr
    if (!m_data.constData() || m_data.isEmpty())
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
    // NOLINTNEXTLINE(modernize-use-transparent-functors)
    if (auto sum = std::accumulate(m_data.cbegin(), m_data.cend(), 0, std::plus<char>()); (sum % 0xff) != 0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Checksum error");
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

bool MythEDID::ParseBaseBlock(const quint8* Data)
{
    // retrieve version
    if (Data[VERSION_OFFSET] != 1)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Unknown major version number");
        return false;
    }
    m_minorVersion = Data[VERSION_OFFSET + 1];

    // retrieve serial number. This may be subsequently overridden.
    // N.B. 0 is a valid serial number.
    qint32 serial = Data[SERIAL_OFFSET] +
                    (Data[SERIAL_OFFSET + 1] << 8) +
                    (Data[SERIAL_OFFSET + 2] << 16) +
                    (Data[SERIAL_OFFSET + 3] << 24);
    m_serialNumbers << QString::number(serial);

    // digital or analog
    //bool digital = Data[DISPLAY_OFFSET] & 0x80;

    // Display size
    if (Data[WIDTH_OFFSET] && Data[HEIGHT_OFFSET])
    {
        m_displaySize = QSize(Data[WIDTH_OFFSET] * 10, Data[HEIGHT_OFFSET] * 10);
    }
    else if (m_minorVersion >= 4 && (Data[WIDTH_OFFSET] || Data[HEIGHT_OFFSET]))
    {
        if (Data[WIDTH_OFFSET])
            m_displayAspect = 100.0 / (Data[HEIGHT_OFFSET] + 99); // Landscape
        else
            m_displayAspect = 100.0 / (Data[WIDTH_OFFSET] + 99); // Portrait
    }

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
    m_sRGB = ((Data[FEATURES_OFFSET] & 0x04) != 0);
    static constexpr std::array<const uint8_t,10> s_sRGB =
        { 0xEE, 0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54 };
    bool srgb = std::equal(s_sRGB.cbegin(), s_sRGB.cend(), Data + 0x19);

    if (!m_sRGB && srgb)
        m_sRGB = true;
    else if (m_sRGB && !srgb)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Chromaticity mismatch!");

    // Red
    m_primaries.m_primaries[0][0] = ((Data[0x1B] << 2) |  (Data[0x19] >> 6)) / 1024.0F;
    m_primaries.m_primaries[0][1] = ((Data[0x1C] << 2) | ((Data[0x19] >> 4) & 3)) / 1024.0F;
    // Green
    m_primaries.m_primaries[1][0] = ((Data[0x1D] << 2) | ((Data[0x19] >> 2) & 3)) / 1024.0F;
    m_primaries.m_primaries[1][1] = ((Data[0x1E] << 2) |  (Data[0x19] & 3)) / 1024.0F;
    // Blue
    m_primaries.m_primaries[2][0] = ((Data[0x1F] << 2) |  (Data[0x1A] >> 6)) / 1024.0F;
    m_primaries.m_primaries[2][1] = ((Data[0x20] << 2) | ((Data[0x1A] >> 4) & 3)) / 1024.0F;
    // White
    m_primaries.m_whitePoint[0]   = ((Data[0x21] << 2) | ((Data[0x1A] >> 2) & 3)) / 1024.0F;
    m_primaries.m_whitePoint[1]   = ((Data[0x22] << 2) |  (Data[0x1A] & 3)) / 1024.0F;

    // Check whether this is very similar to sRGB and hence if non-exact colourspace
    // handling is preferred, then just use sRGB.
    m_likeSRGB = MythColourSpace::Alike(m_primaries, MythColourSpace::s_sRGB, 0.025F) &&
                 qFuzzyCompare(m_gamma + 1.0F, 2.20F + 1.0F);

    // Parse blocks
    for (size_t i = 0; i < 4; ++i)
    {
        size_t offset = DATA_BLOCK_OFFSET + (i * 18);
        if (Data[offset] == 0 || Data[offset + 1] == 0 || Data[offset + 2] == 0)
            ParseDisplayDescriptor(Data, offset);
        else
            ParseDetailedTimingDescriptor(Data, offset);
    }

    // Set status
    m_valid = std::any_of(m_serialNumbers.cbegin(), m_serialNumbers.cend(),
                          [](const QString& Serial) { return !Serial.isEmpty(); });
    if (!m_valid)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No serial number(s) in EDID");
    return m_valid;
}

void MythEDID::ParseDisplayDescriptor(const quint8* Data, uint Offset)
{
    auto type = Data[Offset + 3];
    auto offset = Offset + 5;
    if (DESCRIPTOR_SERIAL_NUMBER == type)
    {
        m_serialNumbers << ParseEdidString(&Data[offset], false);
        m_serialNumbers << ParseEdidString(&Data[offset], true);
    }
    else if (DESCRIPTOR_PRODUCT_NAME == type)
    {
        m_name = ParseEdidString(&Data[offset], true);
    }
    else if (DESCRIPTOR_RANGE_LIMITS == type)
    {
        auto vminoffset = 0;
        auto vmaxoffset = 0;
        if (m_minorVersion > 3)
        {
            if (Data[Offset + 4] & 0x02)
            {
                vmaxoffset = 255;
                if (Data[Offset + 4] & 0x01)
                    vminoffset = 255;
            }
        }
        m_vrangeMin = Data[Offset + 5] + vminoffset;
        m_vrangeMax = Data[Offset + 6] + vmaxoffset;
    }
}

void MythEDID::ParseDetailedTimingDescriptor(const quint8* Data, size_t Offset)
{
    // We're only really interested in a more accurate display size
    auto width = Data[Offset + 12] + ((Data[Offset + 14] & 0xF0) << 4);
    auto height = Data[Offset + 13] + ((Data[Offset + 14] & 0x0F) << 8);

    // 1.4 may have set an aspect ratio instead of size, otherwise use if this
    // looks like a more accurate version of our current size
    if (m_displaySize.isEmpty() ||
       ((abs(m_displaySize.width() - width) < 10) && (abs(m_displaySize.height() - height) < 10)))
    {
        m_displaySize = { width, height };
    }
}

bool MythEDID::ParseCTA861(const quint8* Data, size_t Offset)
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

bool MythEDID::ParseCTABlock(const quint8* Data, size_t Offset)
{
    uint length = Data[Offset] & 0x1F;
    uint type  = (Data[Offset] & 0xE0) >> 5;
    switch (type)
    {
        case 0x01: break; // Audio data block // NOLINT(bugprone-branch-clone)
        case 0x02: break; // Video data block
        case 0x03: ParseVSDB(Data, Offset + 1, length); break; // Vendor Specific Data Block
        case 0x04: break; // Speaker Allocation data block // NOLINT(bugprone-branch-clone)
        case 0x05: break; // VESA DTC data block
        case 0x07: ParseExtended(Data, Offset + 1, length); break; // Extended tag. HDR metadata here
        default: break;
    }
    return true;
}

bool MythEDID::ParseVSDB(const quint8* Data, size_t Offset, size_t Length)
{
    if (Offset + 3 >= m_size)
        return false;

    // N.B. Little endian
    int registration = Data[Offset] + (Data[Offset + 1] << 8) + (Data[Offset + 2] << 16);

    // "HDMI Licensing, LLC"
    while (registration == 0x000C03)
    {
        m_isHDMI = true;

        if (Length < 5 || (Offset + 5 >= m_size))
            break;

        // CEC physical address
        m_physicalAddress = static_cast<uint16_t>((Data[Offset + 3] << 8) + Data[Offset + 4]);

        if (Length < 6 || (Offset + 6 >= m_size))
            break;

        // Deep color
        m_deepColor = Data[Offset + 5] & 0x78;

        if (Length < 8 || (Offset + 8 >= m_size))
            break;

        // Audio and video latencies
        m_latencies  = ((Data[Offset + 7] & 0x80) != 0);
        m_interLatencies = ((Data[Offset + 7] & 0x40) != 0) && m_latencies;

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

    // "HDMI Forum"
    while (registration == 0xC45DD8)
    {
        if (Length < 5 || (Offset + 5 >= m_size))
            break;

        // Deep Color 4:2:0
        m_deepYUV = Data[Offset + 4] & 0x7;

        if (Length < 11 || (Offset + 11 >= m_size))
            break;

        // Variable Refresh Rate
        m_vrrMin = Data[Offset + 9] & 0x3f;
        m_vrrMax = ((Data[Offset + 9] & 0xc0) << 2) | Data[Offset + 10];
        break;
    }

    return true;
}

bool MythEDID::ParseExtended(const quint8* Data, size_t Offset, size_t Length)
{
    if (Offset + 1 >= m_size)
        return false;

    // HDR Static Metadata Data Block
    if (Data[Offset] == 0x06 && Length)
    {
        if (Length >= 3 && (Offset + 3 < m_size))
        {
            int hdrsupport = Data[Offset + 1] & 0x3f;
            if (hdrsupport & HDR10)
                m_hdrSupport |= MythHDR::HDR10;
            if (hdrsupport & HLG)
                m_hdrSupport |= MythHDR::HLG;
            m_hdrMetaTypes = Data[Offset + 2] & 0xff;
        }

        if (Length >= 4)
            m_maxLuminance = 50.0 * pow(2, Data[Offset + 3] / 32.0);

        if (Length >= 5)
            m_maxAvgLuminance = 50.0 * pow(2, Data[Offset + 4] / 32.0);

        if (Length >= 6)
            m_minLuminance = (50.0 * pow(2, Data[Offset + 3] / 32.0)) * pow(Data[Offset + 5] / 255.0, 2) / 100.0;
    }
    return true;
}

void MythEDID::Debug() const
{
    auto deep = [](uint8_t Deep)
    {
        QStringList res;
        if (Deep & 0x08) res << "Y444";
        if (Deep & 0x10) res << "30bit";
        if (Deep & 0x20) res << "36bit";
        if (Deep & 0x40) res << "48bit";
        return res.join(",");
    };

    auto deepyuv = [](uint8_t Deep)
    {
        QStringList res;
        if (Deep & 0x01) res << "10bit";
        if (Deep & 0x02) res << "12bit";
        if (Deep & 0x04) res << "16bit";
        return res.join(",");
    };

    if (!m_valid)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Invalid EDID");
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Version:1.%1 Size:%2 Exensions:%3")
            .arg(m_minorVersion).arg(m_size).arg((m_size / 128) - 1));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Gamma:%1 sRGB:%2")
            .arg(static_cast<double>(m_gamma)).arg(m_sRGB));
    LOG(VB_GENERAL, LOG_INFO, LOC + "Display chromaticity:-");
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Red:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.m_primaries[0][0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.m_primaries[0][1]), 0, 'f', 4));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Green:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.m_primaries[1][0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.m_primaries[1][1]), 0, 'f', 4));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Blue:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.m_primaries[2][0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.m_primaries[2][1]), 0, 'f', 4));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("White:\t%1,\t%2")
            .arg(static_cast<double>(m_primaries.m_whitePoint[0]), 0, 'f', 4)
            .arg(static_cast<double>(m_primaries.m_whitePoint[1]), 0, 'f', 4));
    QString address = !m_physicalAddress ? QString("N/A") :
            QString("%1.%2.%3.%4").arg((m_physicalAddress >> 12) & 0xF)
                                  .arg((m_physicalAddress >> 8) & 0xF)
                                  .arg((m_physicalAddress >> 4) & 0xF)
                                  .arg(m_physicalAddress & 0xF);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Physical address: %1").arg(address));
    if (m_deepColor)
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Deep color: %1").arg(deep(m_deepColor)));
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
    if (m_deepYUV)
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Deep YUV 4:2:0 %1").arg(deepyuv(m_deepYUV)));
    if (m_vrrMin || m_vrrMax)
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("VRR: %1<->%2").arg(m_vrrMin).arg(m_vrrMax));
    if (m_hdrSupport)
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("HDR types: %1").arg(MythHDR::TypesToString(m_hdrSupport).join(",")));
    if (m_maxLuminance > 0.0 || m_maxAvgLuminance > 0.0)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Desired luminance: Min: %1 Max: %2 Avg: %3")
            .arg(m_minLuminance, 3, 'f', 3).arg(m_maxLuminance, 3, 'f', 3).arg(m_maxAvgLuminance, 3, 'f', 3));
    }
}
