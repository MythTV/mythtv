#include <cmath>

#include "premieredescriptors.h"
#include "dvbdescriptors.h"

void PremiereContentTransmissionDescriptor::Parse(void)
{
    _transmission_count = 0;
    _date_ptrs.clear();
    _time_ptrs.clear();
    const uint8_t *dataptr = _data + 8;
    while ((dataptr + 6) <= (_data + 2 + DescriptorLength()))
    {
        uint starttime_no = *(dataptr+2);
        for (uint i=0; i < starttime_no; i+=3)
        {
            _date_ptrs.push_back(dataptr);
            _time_ptrs.push_back(dataptr + 3 + i);
            _transmission_count++;
        }
        dataptr += 3 + starttime_no;
    }
}


QDateTime PremiereContentTransmissionDescriptor::StartTimeUTC(uint index) const
{
    // set buf to the startdate
    const uint8_t *buf = _date_ptrs[index];
    uint mjd = (buf[0] << 8) | buf[1];
    // reset buf two bytes before the startime
    buf = _time_ptrs[index]-2;
    if (mjd >= 40587)
    {
        QDateTime result;
        // Modified Julian date as number of days since 17th November 1858.
        // 1st Jan 1970 was date 40587.
        uint secsSince1970 = (mjd - 40587)   * 86400;
        secsSince1970 += byteBCD2int(buf[2]) * 3600;
        secsSince1970 += byteBCD2int(buf[3]) * 60;
        secsSince1970 += byteBCD2int(buf[4]);
        result.setTime_t(secsSince1970);
        return result;
    }

    // Original function taken from dvbdate.c in linuxtv-apps code
    // Use the routine specified in ETSI EN 300 468 V1.4.1,
    // "Specification for Service Information in Digital Video Broadcasting"
    // to convert from Modified Julian Date to Year, Month, Day.

    const float tmpA = (float)(1.0 / 365.25);
    const float tmpB = (float)(1.0 / 30.6001);

    float mjdf = mjd;
    int year  = (int) truncf((mjdf - 15078.2f) * tmpA);
    int month = (int) truncf(
        (mjdf - 14956.1f - truncf(year * 365.25f)) * tmpB);
    int day   = (int) truncf(
        (mjdf - 14956.0f - truncf(year * 365.25f) - truncf(month * 30.6001f)));
    int i     = (month == 14 || month == 15) ? 1 : 0;

    QDate date(1900 + year + i, month - 1 - i * 12, day);
    QTime time(byteBCD2int(buf[2]), byteBCD2int(buf[3]),
               byteBCD2int(buf[4]));

    return QDateTime(date, time);
}
