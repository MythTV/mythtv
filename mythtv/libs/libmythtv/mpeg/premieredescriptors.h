// -*- Mode: c++ -*-
#ifndef _PRIVATE_DESCRIPTORS_H_
#define _PRIVATE_DESCRIPTORS_H_

#include <stdint.h>
#include <inttypes.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>
#include <QDateTime>

// MythTV headers
#include "mpegdescriptors.h"

class PremiereContentTransmissionDescriptor : public MPEGDescriptor
{
  public:
    PremiereContentTransmissionDescriptor(const unsigned char *data,
                                          int len = 300)
        : MPEGDescriptor(data, len,
                         PrivateDescriptorID::premiere_content_transmission),
          _transmission_count(0)
    {
        if (_data && !Parse())
            _data = NULL;
    }

    // descriptor_tag           8   0.0

    // descriptor_length        8   1.0

    // transport id            16   2.0
    uint TSID() const
        { return (_data[2] << 8) | _data[3]; }
    // original network id     16   4.0
    uint OriginalNetworkID() const
        { return (_data[4] << 8) | _data[5]; }
    // service id              16   6.0
    uint ServiceID() const
        { return (_data[6] << 8) | _data[7]; }

    // start date              16   8.0
    // transmission count       8  10.0
    // for(i=0;i<N;i++)
    //   start_time            24  11.0+x

    uint TransmissionCount(void) const { return _transmission_count; }

    QDateTime StartTimeUTC(uint i) const;

  private:
    virtual bool Parse(void);

    uint                                 _transmission_count;
    mutable vector<const uint8_t*> _date_ptrs;
    mutable vector<const uint8_t*> _time_ptrs;
};

#endif // _PRIVATE_DESCRIPTORS_H_
