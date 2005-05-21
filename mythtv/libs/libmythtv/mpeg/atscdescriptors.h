// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _ATSC_DESCRIPTORS_H_
#define _ATSC_DESCRIPTORS_H_

#include <cassert>
#include <vector>
#include <iostream>
#include <qstring.h>
#include <qmap.h>
#include "mythcontext.h"

using namespace std;

typedef QMap<int, const unsigned char*> IntToBuf;

class ISO639LanguageCode {
  public:
    ISO639LanguageCode(const unsigned char* data) : _data(data) { ; }
    QString toString() const;

    // 3 first bytes are code
    const unsigned char* CodeRaw() const { return _data; }

    // code in QString format
    QString Code() const {
        const char code[] = { _data[0], _data[1], _data[2], 0 };
        return QString((const char*)code);
    }
  private:
    const unsigned char* _data;
};

class MultipleStringStructure {
  public:
    MultipleStringStructure(const unsigned char* data) : _data(data) {
        Parse();
    }

    int StringCount() const { return _data[0]; }
    //uimsbf for (i= 0;i< number_strings;i++) {
    //  ISO_639_language_code 24;
    const ISO639LanguageCode LanguageCode(int i) const {
        return ISO639LanguageCode(Offset(i,-1));
    }

    //  uimsbf number_segments 8;
    uint SegmentCount(int i) const {
        return *(Offset(i,-1)+3);
    }

    //  uimsbf for (j=0;j<number_segments;j++) {
    //    compression_type 8;
    int CompressionType(int i, int j) const { return *Offset(i,j); }
    QString CompressionTypeString(int i, int j) const;
    //    uimsbf mode 8;
    int Mode(int i, int j) const { return *(Offset(i,j)+1); }
    //    uimsbf number_bytes 8;
    int Bytes(int i, int j) const { return *(Offset(i,j)+2); }
    //    uimsbf for (k= 0;k<number_bytes;k++)
    //      compressed_string_byte [k] 8 bslbf;
    //  }
    //}
  private:
    static QString Uncompressed(const unsigned char* buf, int len, int mode);
    static QString Huffman1(const unsigned char* buf, int len);
    static QString Huffman2(const unsigned char* buf, int len);
    static uint Index(int i, int j) { return (i<<8)|(j&0xff); }
    const unsigned char* Offset(int i, int j) const { return _ptrs[Index(i,j)]; }

  public:
    QString CompressedString(int i, int j) const;

    void Parse() const;
    QString toString() const;
  private:
    const unsigned char* _data;
    mutable IntToBuf _ptrs;
};

class ATSCDescriptor {
  public:
    ATSCDescriptor(const unsigned char* data) : _data(data) { ; }
    uint DescriptorTag() const { return _data[0]; }
    QString DescriptorTagString() const;
    uint DescriptorLength() const { return _data[1]; }
    static vector<const unsigned char*>
        Parse(const unsigned char* data, uint len);
    QString toString() const;
  protected:
    const unsigned char* _data;
};

class CaptionServiceDescriptor : public ATSCDescriptor {
    mutable IntToBuf _ptrs;
  public:
    CaptionServiceDescriptor(const unsigned char* data) :
        ATSCDescriptor(data) {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x86
        assert(0x86==DescriptorTag());
    // descriptor_length        8   1.0
    }
    // reserved                 3   2.0       0x07
    // number_of_services       5   2.3
    uint ServicesCount() const { return _data[2]&0x1f; }
    //uimsbf for (i=0;i<number_of_services;i++) {
    //   language             8*3  0.0
    const ISO639LanguageCode Language(int i) const {
        return ISO639LanguageCode(Offset(i,-1));
    }
    //   uimsbf cc_type         1  3.0
    bool Type(int i) const {
        return ((Offset(i,-1)[3])>>7)&1;
    }
    //   bslbf reserved         1  3.1           1
    //   if (cc_type==line21) {
    //      reserved            5  3.2        0x1f
    //      line21_field        1  3.7
    bool Line21Field(int i) const {
        return bool(((Offset(i,-1)[3]))&1);
    }
    //   } else
    //      cap_service_number  6  3.2
    int CaptionServiceNumber(int i) const {
        return ((Offset(i,-1)[3]))&0x3f;
    }
    //   easy_reader            1  4.0
    bool EasyReader(int i) const {
        return bool(((Offset(i,-1)[4])>>7)&1);
    }
    //   wide_aspect_ratio      1  4.1
    bool WideAspectRatio(int i) const {
        return bool(((Offset(i,-1)[4])>>6)&1);
    }
    //   reserved              14  4.2      0x3fff
    //}                            6.0
    void Parse() const;
    QString toString() const {
        Parse();
        QString str("   Caption Service Descriptor  ");
        str.append(QString("services(%2)").arg(ServicesCount()));
        for (uint i=0; i<ServicesCount(); i++) {
            str.append(QString("\n     lang(%1) type(%2) ").
                       arg(Language(i).Code()).arg(Type(i)));
            str.append(QString("easy_reader(%1) wide(%2) ").
                       arg(EasyReader(i)).arg(WideAspectRatio(i)));
            if (Type(i))
                str.append(QString("service_num(%1)").arg(CaptionServiceNumber(i)));
            else
                str.append(QString("line_21_field(%1)").arg(Line21Field(i)));
        }
        return str;
    }
  protected:
    int Index(int i, int j) const { return (i<<8)|(j&0xff); }
    const unsigned char* Offset(int i, int j) const {
        return _ptrs[Index(i,j)];
    }    
};

class ComponentNameDescriptor : public ATSCDescriptor {
  public:
    ComponentNameDescriptor(const unsigned char* data) :
        ATSCDescriptor(data) {
        assert(0xA3==DescriptorTag());
    }
    const MultipleStringStructure ComponentNameStrings() const {
        return MultipleStringStructure(_data+2);
    }
    QString toString() const {
        return QString("           Component Name Descriptor  %1").
            arg(ComponentNameStrings().toString());
    }
};

// a_52a.pdf p119, Table A1
class RegistrationDescriptor : public ATSCDescriptor {
  public:
    RegistrationDescriptor(const unsigned char* data) :
        ATSCDescriptor(data) {
        assert(0x05==DescriptorTag());
        if (0x04!=DescriptorLength()) {
            cerr<<"Registration Descriptor length != 4 !!!!"<<endl;
        }
        //assert(0x41432d33==formatIdentifier());
    }
    uint FormatIdentifier() const {
        return (_data[2]<<24) | (_data[3]<<16) |
            (_data[4]<<8) | (_data[5]);
    }
    QString toString() const {
        if (0x41432d33==FormatIdentifier())
            return QString("           Registration Descriptor   OK");
        return QString("           Registration Descriptor   not OK \n"
                       " format Identifier is 0x%1 but should be 0x41432d33").
            arg(FormatIdentifier(), 0, 16);
    }
};

// a_52a.pdf p120, Table A2
class AudioStreamDescriptor : public ATSCDescriptor {
  public:
    AudioStreamDescriptor(const unsigned char* data) :
        ATSCDescriptor(data) {
// descriptor_tag   The value for the AC-3 descriptor tag is 0x81.
        assert(0x81==DescriptorTag());
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

/*
// a_52a.pdf p125 Table A7 (for DVB)
AC3Descriptor {
 descriptor_tag 8 uimsbf
// descriptor_tag   The descriptor tag is an 8-bit field which
// identifies each descriptor. The AC-3 descriptor_tag shall
// have a value of 0x6A.
 descriptor_length 8 uimsbf
 AC-3_type_flag 1 bslbf
 bsid_flag 1 bslbf
 mainid_flag 1 bslbf
 asvc_flag 1 bslbf
 reserved 1 bslbf
 reserved 1 bslbf
 reserved 1 bslbf
 reserved 1 bslbf
 If (AC-3_type_flag)==1 { AC-3_type } 8 uimsbf
 If (bsid_flag)==1 { bsid { 8 uimsbf
   If (mainid_flag)==1{ mainid } 8 uimsbf
   If (asvc_flag)==1{ asvc } 8 bslbf
   For(I=0;I<N;I++){ additional_info[I] } N x 8 uimsbf
};
*/

/** \class ContentIdentifierDescriptor
 *   This is something like an ISBN for %TV shows.
 *   See also: a_57a.pdf page6 Tables 6.1, 6.2, and 7.1
 */
class ContentIdentifierDescriptor : public ATSCDescriptor {
    ContentIdentifierDescriptor(const unsigned char* data) :
        ATSCDescriptor(data) {
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
};

#endif
