// -*- Mode: c++ -*-
#ifndef PREMIERE_DESCRIPTORS_H
#define PREMIERE_DESCRIPTORS_H

// C++ headers
#include <cinttypes>
#include <cstdint>
#include <vector>

// Qt headers
#include <QString>
#include <QDateTime>
#include <QTimeZone>

// MythTV headers
#include "mpegdescriptors.h"

class PremiereContentTransmissionDescriptor : public MPEGDescriptor
{
  public:
    explicit PremiereContentTransmissionDescriptor(const unsigned char *data,
                                          int len = 300)
        : MPEGDescriptor(data, len,
                         PrivateDescriptorID::premiere_content_transmission)
    {
        if (m_data && !PremiereContentTransmissionDescriptor::Parse())
            m_data = nullptr;
    }

    // descriptor_tag           8   0.0

    // descriptor_length        8   1.0

    // transport id            16   2.0
    uint TSID() const
        { return (m_data[2] << 8) | m_data[3]; }
    // original network id     16   4.0
    uint OriginalNetworkID() const
        { return (m_data[4] << 8) | m_data[5]; }
    // service id              16   6.0
    uint ServiceID() const
        { return (m_data[6] << 8) | m_data[7]; }

    // start date              16   8.0
    // transmission count       8  10.0
    // for(i=0;i<N;i++)
    //   start_time            24  11.0+x

    uint TransmissionCount(void) const { return m_transmissionCount; }

    QDateTime StartTimeUTC(uint i) const;

  private:
    virtual bool Parse(void);

    uint                           m_transmissionCount { 0 };
    mutable std::vector<const uint8_t*> m_datePtrs;
    mutable std::vector<const uint8_t*> m_timePtrs;
};

#endif // PREMIERE_DESCRIPTORS_H
