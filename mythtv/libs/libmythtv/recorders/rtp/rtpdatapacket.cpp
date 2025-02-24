#include "rtpdatapacket.h"

#include "libmythbase/mythlogging.h"

bool RTPDataPacket::IsValid() const
{
    if (m_data.size() < 12)
    {
        return false;
    }
    if (2 != GetVersion())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Version incorrect %1")
            .arg(GetVersion()));
        return false;
    }

    int off = 12 + 4 * GetCSRCCount();
    if (off > m_data.size())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("off %1 > sz %2")
            .arg(off).arg(m_data.size()));
        return false;
    }
    if (HasExtension())
    {
        uint ext_size = m_data[off+2] << 8 | m_data[off+3];
        off += 4 * (1 + ext_size);
    }
    if (off > m_data.size())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("off + ext %1 > sz %2")
            .arg(off).arg(m_data.size()));
        return false;
    }
    m_off = off;

    return true;
}
