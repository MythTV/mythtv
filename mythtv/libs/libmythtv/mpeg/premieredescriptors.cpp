#include <cmath>

#include "premieredescriptors.h"
#include "dvbdescriptors.h"

bool PremiereContentTransmissionDescriptor::Parse(void)
{
    m_transmissionCount = 0;
    m_datePtrs.clear();
    m_timePtrs.clear();
    const uint8_t *dataptr = m_data + 8;
    while ((dataptr + 6) <= (m_data + 2 + DescriptorLength()))
    {
        uint starttime_no = *(dataptr+2);
        for (uint i=0; i < starttime_no; i+=3)
        {
            m_datePtrs.push_back(dataptr);
            m_timePtrs.push_back(dataptr + 3 + i);
            m_transmissionCount++;
        }
        dataptr += 3 + starttime_no;
    }
    return true;
}


QDateTime PremiereContentTransmissionDescriptor::StartTimeUTC(uint index) const
{
    // set buf to the startdate
    const uint8_t *buf = m_datePtrs[index];
    uint mjd = (buf[0] << 8) | buf[1];
    // reset buf two bytes before the startime
    buf = m_timePtrs[index]-2;
    if (mjd >= 40587)
    {
        // Modified Julian date as number of days since 17th November 1858.
        // 1st Jan 1970 was date 40587.
        uint secsSince1970 = (mjd - 40587)   * 86400;
        secsSince1970 += byteBCD2int(buf[2]) * 3600;
        secsSince1970 += byteBCD2int(buf[3]) * 60;
        secsSince1970 += byteBCD2int(buf[4]);
        return MythDate::fromSecsSinceEpoch(secsSince1970);
    }

    // Original function taken from dvbdate.c in linuxtv-apps code
    // Use the routine specified in ETSI EN 300 468 V1.4.1,
    // "Specification for Service Information in Digital Video Broadcasting"
    // to convert from Modified Julian Date to Year, Month, Day.

    const auto tmpA = (float)(1.0 / 365.25);
    const auto tmpB = (float)(1.0 / 30.6001);

    float mjdf = mjd;
    int year  = (int) truncf((mjdf - 15078.2F) * tmpA);
    int month = (int) truncf(
        (mjdf - 14956.1F - truncf(year * 365.25F)) * tmpB);
    int day   = (int) truncf(
        (mjdf - 14956.0F - truncf(year * 365.25F) - truncf(month * 30.6001F)));
    int i     = (month == 14 || month == 15) ? 1 : 0;

    QDate date(1900 + year + i, month - 1 - (i * 12), day);
    QTime time(byteBCD2int(buf[2]), byteBCD2int(buf[3]),
               byteBCD2int(buf[4]));

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    return {date, time, Qt::UTC};
#else
    return {date, time, QTimeZone(QTimeZone::UTC)};
#endif
}
