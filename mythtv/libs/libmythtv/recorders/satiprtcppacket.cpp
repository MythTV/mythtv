#include "satiprtcppacket.h"

SatIPRTCPPacket::SatIPRTCPPacket(QByteArray& data)
    : m_data(data)
{
   parse();
}

void SatIPRTCPPacket::parse()
{
    m_satipData = QString();

    uint pkt_length = m_data.length();
    uint offset = 0;

    while (offset < pkt_length - 3)
    {
        auto type = (uint8_t)m_data[offset + 1];
        uint16_t length = ((m_data[offset + 2] & 0xFF) << 8) | (m_data[offset + 3] & 0xFF);
        length++;

        if (type == RTCP_TYPE_APP && offset + 15 < pkt_length)
        {
            if ((m_data[offset + 8] == 'S') && (m_data[offset + 9] == 'E') &&
                (m_data[offset + 10] == 'S') && (m_data[offset + 11] == '1'))
            {
                uint16_t str_length = ((m_data[offset + 14] & 0xFF) << 8) | (m_data[offset + 15] & 0xFF);

                if (offset + 16 + str_length <= pkt_length)
                {
                    QString str = QString::fromUtf8(m_data.data() + offset + 16, str_length);
                    m_satipData = str;
                    return;
                }
            }
        }

        offset += length * 4;
    }
}
