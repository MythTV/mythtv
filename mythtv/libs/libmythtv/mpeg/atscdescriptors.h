// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _ATSC_DESCRIPTORS_H_
#define _ATSC_DESCRIPTORS_H_

#include <cassert>
#include <vector>
using namespace std;

#include <QString>
#include <QMap>

#include "mpegdescriptors.h"

using namespace std;

typedef QMap<int, const unsigned char*> IntToBuf;

class MultipleStringStructure
{
  public:
    MultipleStringStructure(const unsigned char* data) : _data(data)
    {
        Parse();
    }

    uint StringCount(void) const { return _data[0]; }
    //uimsbf for (i= 0;i< number_strings;i++) {
    //  ISO_639_language_code 24;
    int LanguageKey(uint i) const
        { return iso639_str3_to_key(Offset(i,-1)); }
    QString LanguageString(uint i) const
        { return iso639_key_to_str3(LanguageKey(i)); }
    int CanonicalLanguageKey(uint i) const
        { return iso639_key_to_canonical_key(LanguageKey(i)); }
    QString CanonicalLanguageString(uint i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(i)); }
    //   uimsbf cc_type         1  3.0

    //  uimsbf number_segments 8;
    uint SegmentCount(uint i) const { return *(Offset(i,-1)+3); }

    //  uimsbf for (j=0;j<number_segments;j++) {
    //    compression_type 8;
    uint CompressionType(uint i, uint j) const { return *Offset(i,j); }
    QString CompressionTypeString(uint i, uint j) const;
    //    uimsbf mode 8;
    int Mode(int i, int j) const { return *(Offset(i,j)+1); }
    //    uimsbf number_bytes 8;
    int Bytes(int i, int j) const { return *(Offset(i,j)+2); }
    //    uimsbf for (k= 0;k<number_bytes;k++)
    //      compressed_string_byte [k] 8 bslbf;
    //  }
    //}

    uint GetIndexOfBestMatch(QMap<uint,uint> &langPrefs) const;
    QString GetBestMatch(QMap<uint,uint> &langPrefs) const;

    QString GetSegment(uint i, uint j) const;
    QString GetFullString(uint i) const;

    void Parse(void) const;

    QString toString() const;

  private:
    static QString Uncompressed(const unsigned char* buf, int len, int mode);
    static uint Index(int i, int j) { return (i<<8)|(j&0xff); }
    const unsigned char* Offset(int i, int j) const
        { return _ptrs[Index(i,j)]; }

  private:
    const unsigned char* _data;
    mutable IntToBuf _ptrs;
};

class CaptionServiceDescriptor : public MPEGDescriptor
{
  public:
    CaptionServiceDescriptor(const unsigned char* data) :
        MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x86
        assert(DescriptorID::caption_service == DescriptorTag());
        Parse();
    // descriptor_length        8   1.0
    }

    // reserved                 3   2.0       0x07
    // number_of_services       5   2.3
    uint ServicesCount() const { return _data[2]&0x1f; }
    //uimsbf for (i=0;i<number_of_services;i++) {
    //   language             8*3  0.0
    int LanguageKey(int i) const
        { return iso639_str3_to_key(Offset(i,-1)); }
    QString LanguageString(int i) const
        { return iso639_key_to_str3(LanguageKey(i)); }
    int CanonicalLanguageKey(int i) const
        { return iso639_key_to_canonical_key(LanguageKey(i)); }
    QString CanonicalLanguageString(int i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(i)); }
    //   uimsbf cc_type         1  3.0
    bool Type(int i) const
        { return ((Offset(i,-1)[3])>>7) & 1; }
    //   bslbf reserved         1  3.1           1
    //   if (cc_type==line21) {
    //      reserved            5  3.2        0x1f
    //      line21_field        1  3.7
    bool Line21Field(int i) const
        { return bool(((Offset(i,-1)[3])) & 1); }
    //   } else
    //      cap_service_number  6  3.2
    int CaptionServiceNumber(int i) const
        { return ((Offset(i,-1)[3])) & 0x3f; }
    //   easy_reader            1  4.0
    bool EasyReader(int i) const
        { return bool(((Offset(i,-1)[4])>>7) & 1); }
    //   wide_aspect_ratio      1  4.1
    bool WideAspectRatio(int i) const
        { return bool(((Offset(i,-1)[4])>>6) & 1); }
    //   reserved              14  4.2      0x3fff
    //}                            6.0
    void Parse(void) const;
    QString toString() const;

  private:
    int Index(int i, int j) const { return (i<<8) | (j & 0xff); }
    const unsigned char* Offset(int i, int j) const
        { return _ptrs[Index(i,j)]; }

  private:
    mutable IntToBuf _ptrs;
};

class ContentAdvisoryDescriptor : public MPEGDescriptor
{
    mutable IntToBuf _ptrs;
  public:
    ContentAdvisoryDescriptor(const unsigned char* data) :
        MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x87
        assert(DescriptorID::content_advisory == DescriptorTag());
        Parse();
    // descriptor_length        8   1.0
    }
    // reserved                 2   2.0       0x03
    // rating_region_count      6   2.2
    uint RatingRegionCount(void) const { return _data[2] & 0x3f; }
    // for (i=0; i<rating_region_count; i++) {
    //   rating_region          8 x+0.0
    uint RatingRegion(uint i) const
        { return *Offset(i,-1); }
    //   rated_dimensions       8 x+1.0
    uint RatedDimensions(uint i) const
        { return *(Offset(i,-1) + 1); }
    //   for (j=0;j<rated_dimensions;j++) {
    //     rating_dimension_j   8 y+0.0
    uint RatingDimension(uint i, uint j) const
        { return *Offset(i,j); }
    //     reserved             4 y+1.0       0x0f
    //     rating_value         4 y+1.4
    uint RatingValue(uint i, uint j) const
        { return (*(Offset(i,j) + 1)) & 0xf; }
    //   }
    //   rating_desc_length     8 x+2+(rated_dimensions*2)+0.0
    uint RatingDescriptionLength(uint i) const
        { return (*(Offset(i,-1) + 2 + (RatedDimensions(i)<<1)));  }
    //   rating_desc_text         x+2+(rated_dimensions*2)+1.0
    const MultipleStringStructure RatingDescription(uint i) const
    {
        const unsigned char* data = Offset(i,-1) + 3 + (RatedDimensions(i)<<1);
        return MultipleStringStructure(data);
    }
    // }

    void Parse(void) const;
    QString toString() const;
  protected:
    int Index(int i, int j) const { return (i<<8)|(j&0xff); }
    const unsigned char* Offset(int i, int j) const {
        return _ptrs[Index(i,j)];
    }
};

class ComponentNameDescriptor : public MPEGDescriptor {
  public:
    ComponentNameDescriptor(const unsigned char* data) :
        MPEGDescriptor(data) {
        assert(0xA3==DescriptorTag());
    }
    const MultipleStringStructure ComponentNameStrings() const {
        return MultipleStringStructure(_data+2);
    }
    QString toString() const {
        return QString("Component Name Descriptor  %1").
            arg(ComponentNameStrings().toString());
    }
};


// a_52a.pdf p120, Table A2
class AudioStreamDescriptor : public MPEGDescriptor {
  public:
    AudioStreamDescriptor(const unsigned char* data) :
        MPEGDescriptor(data) {
// descriptor_tag   The value for the AC-3 descriptor tag is 0x81.
        assert(DescriptorID::atsc_audio_stream == DescriptorTag());
    }
    // sample_rate_code                      3   2.0
    uint SampleRateCode() const { return (_data[2]>>5)&7; }
    QString SampleRateCodeString() const;
    // bsid                                  5   2.3
    uint bsid() const { return _data[2]&0x1f; }
    // bit_rate_code                         6   3.0
    uint BitRateCode() const { return (_data[3]>>2)&0x3f; }
    QString BitRateCodeString() const;
    // surround_mode                         2   3.6
    uint SurroundMode() const { return _data[3]&3; }
    QString SurroundModeString() const;
    /*
      000 Major ?
      001 Major ?
      010-111 Minor
      111 Karaoke Mode if acmod >= 0x2. a_52a.pdf p130
    */
    // bsmod                                 3   4.0
    uint BasicServiceMode() const { return (_data[4]>>5)&7; }
    // num_channels                          4   4.3
    uint Channels() const { return (_data[4]>>1)&0xf; }
    QString ChannelsString() const;

    // full service that can be presented alone to listener?
    // full_svc                              1   4.7
    bool FullService() const { return bool((_data[4])&1); }

    // langcod                               8   5.0
    // ignore for language specification
    uint LanguageCode() const { return _data[5]; }
    // if(num_channels==0) /* 1+1 mode */
    //   langcod2                            8   6.0
    // ignore for language specification
    uint LanguageCode2() const { return _data[6]; }

    // if(bsmod<2) {
    //  mainid                               3   7.0/6.0
    uint MainID() const {
        return _data[(Channels()==0)?7:6]>>5;
    }
    //  reserved                             5   7.3/6.3
    //uint reserved() const { return _data[7]&0x1f; }
    // } else asvcflags                      8   7.0/6.0
    uint AServiceFlags() const {
        return _data[(Channels()==0)?7:6];
    }

    // textlen                               7   8.0/7.0
    uint TextLength() const {
        return _data[(Channels()==0)?8:7]>>1;
    }

    /* If this bit is a  1 , the text is encoded as 1-byte characters
       using the ISO Latin-1 alphabet (ISO 8859-1). If this bit is a 0,
       the text is encoded with 2-byte unicode characters. */
    // text_code 1 bslbf
    bool IsTextLatin1() const {
        return bool(_data[(Channels()==0)?8:7]&1);
    }
    // for(i=0; i<M; i++) {
    //        text[i] 8 bslbf
    // }
    QString Text() const { // TODO
#if 0
        char* tmp = new char[TextLength()+2];
        if (IsTextLatin1()) {
            memcpy(tmp, &_data[(Channels()==0)?9:8], TextLength());
            tmp[TextLength()]=0;
            for (uint i=0; i<TextLength(); i++)
                if (!tmp[i]) tmp[i]='H';
            QString str(tmp);
            delete[] tmp;
            return str;
        } else {
            QString str; int len = TextLength();
            const unsigned char* buf = (&_data[(Channels()==0)?9:8]);
            const unsigned short* ustr =
                reinterpret_cast<const unsigned short*>(buf);
            for (int j=0; j<(len>>1); j++)
                str.append( QChar( (ustr[j]<<8) | (ustr[j]>>8) ) );
            return str;
        }
#endif
        return QString("TODO");
    }
    // for(i=0; i<N; i++) {
    //   additional_info[i] N×8 bslbf
    // }

    QString toString() const;
};

/** \class ContentIdentifierDescriptor
 *   This is something like an ISBN for %TV shows.
 *   See also: a_57a.pdf page6 Tables 6.1, 6.2, and 7.1
 */
class ContentIdentifierDescriptor : public MPEGDescriptor {
    ContentIdentifierDescriptor(const unsigned char* data) :
        MPEGDescriptor(data) {
    // descriptor_tag                        8   0.0  0xB6
        assert(0xB6==DescriptorTag());
    }
    // descriptor_length                     8   1.0
    // content_ID_structure
    //   ID_system                           8   2.0
    //        0x00 ISAN (ISO 15706[1])
    //        0x01 V-ISAN (ISO 20925-1[2])
    //        0x02-0xFF ATSC Reserved
    //   ID_length                           8   3.0
    //   content_identifier                  v   4.0

    QString toString() const { return QString("ContentIdentifierDescriptor(stub)"); }
};

/**
 *  \brief Provides the long channel name for the virtual channel containing
 *         this descriptor.
 *
 *   See ATSC A/65B section 6.9.5.
 *   When used, this descriptor must be in the Virtual Channel Table.
 */
class ExtendedChannelNameDescriptor : public MPEGDescriptor
{
  public:
    ExtendedChannelNameDescriptor(const unsigned char *data);
    MultipleStringStructure LongChannelName(void) const;
    QString LongChannelNameString(void) const;

    QString toString() const;
};

#endif
