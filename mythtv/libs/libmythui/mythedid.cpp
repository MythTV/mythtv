// QT
#include <QtEndian>

// MythTV
#include "mythlogging.h"
#include "mythedid.h"

#define DESCRIPTOR_ALPHANUMERIC_STRING 0xFE
#define DESCRIPTOR_PRODUCT_NAME 0xFC
#define DESCRIPTOR_SERIAL_NUMBER 0xFF
#define DATA_BLOCK_OFFSET 0x36
#define SERIAL_OFFSET 0x0C

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

int MythEDID::PhysicalAddress(void)
{
    return m_physicalAddress;
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
    // this is adapted from QEdidParser

    const quint8 *data = reinterpret_cast<const quint8 *>(m_data.constData());

    // EDID data should be in 128 byte blocks
    if ((m_data.size() < 128) || (m_data.size() & 0x7f) ||
        data[0] != 0x00 || data[1] != 0xff)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Invalid EDID");
        return;
    }

    // checksum
    qint8 sum = 0;
    for (int i = 0; i < m_data.size(); ++i)
        sum += m_data.at(i);
    if (sum != 0)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Checksum error");
        return;
    }

    // retrieve serial number. This may be subsequently overridden
    qint32 serial = data[SERIAL_OFFSET] +
                    (data[SERIAL_OFFSET + 1] << 8) +
                    (data[SERIAL_OFFSET + 2] << 16) +
                    (data[SERIAL_OFFSET + 3] << 24);
    if (serial > 0)
        m_serialNumbers << QString::number(serial);

    // Parse blocks
    for (int i = 0; i < 5; ++i)
    {
        const int offset = DATA_BLOCK_OFFSET + i * 18;
        if (data[offset] != 0 || data[offset + 1] != 0 || data[offset + 2] != 0)
            continue;
        if (data[offset + 3] == DESCRIPTOR_SERIAL_NUMBER)
        {
            m_serialNumbers << ParseEdidString(&data[offset + 5], false);
            m_serialNumbers << ParseEdidString(&data[offset + 5], true);
        }
    }

    // Set status
    for (auto it = m_serialNumbers.cbegin(); it != m_serialNumbers.cend(); ++it)
        if (!(*it).isEmpty())
            m_valid = true;

    // Look at extension blocks
    if (m_data.size() <= 128)
        return;

    for (int i = 129; i < m_data.size() - 4; ++i)
    {
        // HDMI VSDB
        if (data[i] == 0x03 && data[i + 1] == 0x0C && data[i + 2] == 0x0)
        {
            int length = data[i - 1] & 0xf;
            if (length > 5)
                m_physicalAddress = (data[i + 3] << 8) + data[i + 4];
        }
    }
}
