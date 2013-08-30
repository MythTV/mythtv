// -*- Mode: c++ -*-
/**
 *   ANSI/SCTE 35 splice descriptor implementation
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdint.h>

#include <QByteArray>

#include "mpegdescriptors.h"

// These descriptors are not registered descriptors, but private
// descriptors that should only be seen on an SpliceInformationTable

class SpliceDescriptorID
{
    // ANSI SCTE 35 2007
  public:
    enum
    {
        avail        = 0x00,
        dtmf         = 0x01,
        segmentation = 0x02,
    };
};

class SpliceDescriptor
{
  public:
    operator const unsigned char*(void) const { return _data; }

    SpliceDescriptor(const unsigned char *data, int len) : _data(data)
    {
        if ((len < 2) || (int(DescriptorLength()) + 2) > len)
            _data = NULL;
    }
    SpliceDescriptor(const unsigned char *data,
                     int len, uint tag) : _data(data)
    {
        if ((len < 2) || (int(DescriptorLength()) + 2) > len)
            _data = NULL;
        else if (DescriptorTag() != tag)
            _data = NULL;
    }
    virtual ~SpliceDescriptor(void) {}

    bool IsValid(void) const { return _data; }
    uint size(void) const { return DescriptorLength() + 2; }

    //       Name             bits  loc  expected value
    // splice_descriptor_tag    8   0.0  0x01
    uint DescriptorTag(void) const { return _data[0]; }
    QString DescriptorTagString(void) const;
    // descriptor_length        8   1.0
    uint DescriptorLength(void) const { return _data[1]; }
    // identifier              32   2.0  0x43554549 "CUEI"
    uint Identifier(void) const
    {
        return (_data[2]<<24) | (_data[3]<<16) | (_data[4]<<8) | _data[5];
    }
    QString IdentifierString(void) const
    {
        return QString(QChar(_data[2])) + QChar(_data[3]) +
            QChar(_data[4]) + QChar(_data[5]);
    }

    virtual QString toString(void) const;
    virtual QString toStringXML(uint indent_level) const;

    static desc_list_t Parse(const unsigned char *data, uint len);
    static desc_list_t ParseAndExclude(const unsigned char *data, uint len,
                                       int descriptorid);
    static desc_list_t ParseOnlyInclude(const unsigned char *data, uint len,
                                        int descriptorid);

    static const unsigned char *Find(const desc_list_t &parsed, uint desc_tag);
    static desc_list_t FindAll(const desc_list_t &parsed, uint desc_tag);

  protected:
    virtual bool Parse(void) { return true; }

    const unsigned char *_data;
};

class AvailDescriptor : SpliceDescriptor
{
  public:
    AvailDescriptor(const unsigned char *data, int len = 300) :
        SpliceDescriptor(data, len, SpliceDescriptorID::segmentation) { }
    //       Name             bits  loc  expected value
    // splice_descriptor_tag    8   0.0  0x00
    // descriptor_length        8   1.0  0x08
    // identifier              32   2.0  0x43554549 "CUEI"
    // provider_avail_id       32   6.0  
    uint ProviderAvailId(void) const
    {
        return (_data[2]<<24) | (_data[3]<<16) | (_data[4]<<8) | _data[5];
    }
    QString ProviderAvailIdString(void) const
    {
        return QString(QChar(_data[6])) + QChar(_data[7]) +
            QChar(_data[8]) + QChar(_data[9]);
    }

    virtual QString toString(void) const
    {
        return QString("Splice Avail: id(%1)").arg(ProviderAvailId());
    }
};

class DTMFDescriptor : SpliceDescriptor
{
  public:
    DTMFDescriptor(const unsigned char *data, int len = 300) :
        SpliceDescriptor(data, len, SpliceDescriptorID::dtmf) { }

    //       Name             bits  loc  expected value
    // splice_descriptor_tag    8   0.0  0x01
    // descriptor_length        8   1.0
    // identifier              32   2.0  0x43554549 "CUEI"
    // preroll                  8   6.0
    uint Preroll(void) const { return _data[6]; }
    // dtmf_count               3   7.0
    uint DTMFCount(void) const { return _data[7]>>5; }
    // reserved                 5   7.3
    // for (i = 0; i < dtmf_count; i++)
    //   DTMF_char              8   8.0+i
    char DTMFChar(uint i) const { return _data[8+i]; }
    QString DTMFString(void) const
    {
        QByteArray ba(reinterpret_cast<const char*>(_data+8), DTMFCount());
        return QString(ba);
    }

    static bool IsParsible(const unsigned char *data, uint safe_bytes);

    virtual QString toString(void) const
    {
        return QString("Splice DTMF: %1").arg(DTMFString());
    }
};

class SegmentationDescriptor : SpliceDescriptor
{
  public:
    SegmentationDescriptor(const unsigned char *data, int len = 300) :
        SpliceDescriptor(data, len, SpliceDescriptorID::segmentation)
    {
        _ptrs[2] = _ptrs[1] = _ptrs[0] = NULL;
        if (_data && !Parse())
            _data = NULL;
    }

    //       Name             bits  loc  expected value
    // splice_descriptor_tag    8   0.0  0x01
    // descriptor_length        8   1.0
    // identifier              32   2.0  0x43554549 "CUEI"
    // segmentation_event_id   32   6.0
    uint SegmentationEventId(void) const
    {
        return (_data[2]<<24) | (_data[3]<<16) | (_data[4]<<8) | _data[5];
    }
    QString SegmentationEventIdString(void) const
    {
        return QString(QChar(_data[6])) + QChar(_data[7]) +
            QChar(_data[8]) + QChar(_data[9]);
    }
    // segmentation_event_cancel_indicator 1 10.0
    bool IsSegmentationEventCancel(void) const { return _data[10] & 0x80; }
    // reserved                 7  10.1
    // if (segmentation_event_cancel_indicator == ‘0’) {
    //   program_seg_flag       1  11.0
    bool IsProgramSegmentation(void) const { return _data[11] & 0x80; }
    //   seg_duration_flag      1  11.1
    bool HasSegmentationDuration(void) const { return _data[11] & 0x40; }
    //   reserved               6  11.2
    //   if (program_segmentation_flag == ‘0’) {
    //     component_count      8  12
    uint ComponentCount(void) const { return _data[12]; }
    //     for (i = 0; i < component_count; i++) {
    //       component_tag      8  13 + i * 6
    uint ComponentTag(uint i) const { return _data[13 + i * 6]; }
    //       reserved           7  14.1 + i * 6
    //       pts_offset        33  14.7 + i * 6
    uint64_t PTSOffset(uint i)
    {
        return ((uint64_t(_data[14+(i*6)] & 0x1) << 32) |
                (uint64_t(_data[15+(i*6)])       << 24) |
                (uint64_t(_data[16+(i*6)])       << 16) |
                (uint64_t(_data[17+(i*6)])       <<  8) |
                (uint64_t(_data[18+(i*6)])));
    }
    //     }
    //   }
    //   if(segmentation_duration_flag == ‘1’)
    //      segmentation_duration 40 _ptrs[0]
    uint64_t SegmentationDuration(void) const
    {
        return ((uint64_t(_ptrs[0][0]) << 32) |
                (uint64_t(_ptrs[0][1]) << 24) |
                (uint64_t(_ptrs[0][2]) << 16) |
                (uint64_t(_ptrs[0][3]) <<  8) |
                (uint64_t(_ptrs[0][4])));
    }
    //   segmentation_upid_type   8 _ptrs[1]
    enum
    {
        kNotUsed  = 0x0, ///< upid is not present in the descriptor
        kVariable = 0x1, ///< user defined
        kISCI     = 0x2, ///< 4 alpha + 4 numeric
        kAdID     = 0x3, ///< 4 alpha + 4 alphanumeric
        kUMID     = 0x4, ///< UMID           See SMPTE 330M
        kISAN     = 0x5, ///< ISAN           See ISO 15706
        kVISAN    = 0x6, ///< versioned ISAN See ISO 15706-2
        kTID      = 0x7, ///< TMS ProgramID 2 alpha followed by 10 numeric
        kTI       = 0x8, ///< Turner Identifier
        kADI      = 0x9, ///< ADI See MD-SP-ADI2.0-AS-I03-070105
    };
    uint SegmentationUPIDType(void) const { return _ptrs[1][0]; }
    //   segmentation_upid_length 8 _ptrs[1]+1
    uint SegmentationUPIDLength(void) const
    { return _ptrs[1][1]; }
    //   segmentation_upid()      ? _ptrs[1]+2
    const unsigned char *SegmentationUPID(void) const
    {
        return _ptrs[1]+2;
    }
    QString SegmentationUPIDString(void) const
    {
        QByteArray ba(reinterpret_cast<const char*>(_ptrs[1]+2),
                      SegmentationUPIDLength());
        return QString(ba);
    }

    enum
    {
        kNotIndicated                  = 0x00,
        kContentIdentification         = 0x01,
        kProgramStart                  = 0x10,
        kProgramEnd                    = 0x11,
        kProgramEarlyTermination       = 0x12,
        kProgramBreakaway              = 0x13,
        kProgramResumption             = 0x14,
        kProgramRunoverPlanned         = 0x15,
        kProgramRunoverUnplanned       = 0x16,
        kChapterStart                  = 0x20,
        kChapterEnd                    = 0x21,
        kProviderAdvertisementStart    = 0x30,
        kProviderAdvertisementEnd      = 0x31,
        kDistributorAdvertisementStart = 0x32,
        kDistributorAdvertisementEnd   = 0x33,
        kUnscheduledEventStart         = 0x40,
        kUnscheduledEventEnd           = 0x41,
    };
    //   segmentation_type_id     8 _ptrs[2]
    uint SegmentationTypeID(void) const { return _ptrs[2][0]; }
    //   segment_num              8 _ptrs[2]+1
    uint SegmentNum(void) const { return _ptrs[2][1]; }
    //   segments_expected        8 _ptrs[2]+2
    uint SegmentsExpected(void) const { return _ptrs[2][2]; }
    // }

    virtual bool Parse(void);
    QString toString(void) const;

    // _ptrs[0] = program_segmentation_flag ? 12 : 13 + component_count * 6
    // _ptrs[1] = _ptrs[0] + HasSegmentationDuration() ? 5 : 0
    // _ptrs[2] = _ptrs[1] + 2 + SegmentationUPIDLength()
    unsigned char const * _ptrs[3];
};
