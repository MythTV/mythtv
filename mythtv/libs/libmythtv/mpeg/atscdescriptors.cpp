// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include <qmap.h>
#include <iostream>
#include "atscdescriptors.h"
#include "mythcontext.h"
#include "iso639.h"

using namespace std;

QString AudioStreamDescriptor::SampleRateCodeString() const {
    static const char* asd[] = {
        "48kbps", "44.1kbps", "32kbps", "Reserved",
        "48kbps or 44.1kbps", "48kbps or 32kbps",
        "44.1kbps or 32kbps", "48kbps or 44.1kbps or 32kbps" };
    return QString(asd[SampleRateCode()]);
}

QString AudioStreamDescriptor::BitRateCodeString() const {
    static const char* ebr[19] = {
        "=32kbps",  "=40kbps",  "=48kbps",  "=56kbps",  "=64kbps",
        "=80kbps",  "=96kbps",  "=112kbps", "=128kbps", "=160kbps",
        "=192kbps", "=224kbps", "=256kbps", "=320kbps", "=384kbps",
        "=448kbps", "=512kbps", "=576kbps", "=640kbps" };
    static const char* ubr[19] = {
        "<=32kbps",  "<=40kbps", "<=48kbps",  "<=56kbps",  "<=64kbps",
        "<=80kbps",  "<=96kbps", "<=112kbps", "<=128kbps", "<=160kbps",
        "<=192kbps","<=224kbps", "<=256kbps", "<=320kbps", "<=384kbps",
        "<=448kbps","<=512kbps", "<=576kbps", "<=640kbps" };
    if (BitRateCode()<=18)
        return QString(ebr[BitRateCode()]);
    else if ((BitRateCode()>=32) && (BitRateCode()<=50))
        return QString(ubr[BitRateCode()-32]);
    else return QString("Unknown Bit Rate Code");
}

QString AudioStreamDescriptor::SurroundModeString() const {
    static const char* sms[] = {
        "Not indicated", "Not Dolby surround encoded",
        "Dolby surround encoded", "Reserved" };
    return QString(sms[SurroundMode()]);
}

QString AudioStreamDescriptor::ChannelsString() const {
    static const char* cs[] = {
        "1 + 1", "1/0", "2/0", "3/0",
        "2/1", "3/1", "2/2 ", "3/2",
        "1", "<= 2", "<= 3", "<= 4",
        "<= 5", "<= 6", "Reserved", "Reserved"
    };
    return cs[Channels()];
}

QString AudioStreamDescriptor::toString() const {
    QString str;
    str.append(QString("   Audio Stream Descriptor "));
    str.append(QString(" full_srv(%1) sample_rate(%2) bit_rate(%3, %4)\n").
               arg(FullService()).arg(SampleRateCodeString()).
               arg(BitRateCodeString()).arg(BitRateCode()));
    str.append(QString("      bsid(%1) bs_mode(%2) channels(%3) Dolby(%4)\n").arg(bsid()).
               arg(BasicServiceMode()).arg(ChannelsString()).arg(SurroundModeString()));
    /*
    str.append(QString("   language code: %1").arg(languageCode()));
    if (0==channels()) {
        str.append(QString(" language code 2: %1").arg(languageCode2()));
    }
    */
    if (BasicServiceMode()<2)
        str.append(QString("      mainID(%1) ").arg(MainID()));
    else
        str.append(QString("      associated_service(0x%1) ").arg(AServiceFlags(),0,16));
    if (TextLength()) {
        str.append(QString("isLatin-1(%1) ").arg(IsTextLatin1()?"true":"false"));
        str.append(QString("text_length(%1) ").arg(TextLength()));
        str.append(QString("text(%1)").arg(Text()));
    }
    return str;
}

QString ISO639LanguageCode::toString() const {
    return iso639_toName(_data);
}

QString MultipleStringStructure::CompressionTypeString(int i, int j) const {
    int ct = CompressionType(i, j);
    if (0==ct) return QString("no compression");
    if (1==ct) return QString("Huffman Coding using C.4, C.5");
    if (2==ct) return QString("Huffman Coding using C.6, C.7");
    if (ct<0xaf) return QString("reserved");
    return QString("compression not used by ATSC in North America, unknown");
}

void MultipleStringStructure::Parse() const {
    _ptrs.clear();
    _ptrs[Index(0,-1)] = _data+1;
    for (int i=0; i<StringCount(); i++) {
        _ptrs[Index(i,0)] = Offset(i,-1)+4;
        uint j=0;
        for (; j<SegmentCount(i); j++)
            _ptrs[Index(i,j+1)] = Offset(i,j) + Bytes(i,j);
        _ptrs[Index(i+1,-1)] = Offset(i,j);
    }
}

void CaptionServiceDescriptor::Parse() const {
    _ptrs.clear();
    _ptrs[Index(0,-1)] = _data+3;
    for (uint i=0; i<ServicesCount(); i++) {
        _ptrs[Index(i+1,-1)] = Offset(i,-1) + 6;
    }
}


/*
void MultipleStringStructure::print() const {
    cerr<<"MultipleStringStructure"<<endl;
    cerr<<" string count: "<<stringCount()<<endl;
    for (int i=0; i<stringCount(); i++) {
        cerr<<" String #"<<i<<endl;
        cerr<<"       language: "<<LanguageCode(i).toString()<<endl;
        cerr<<"  segment count: "<<SegmentCount(i)<<endl;
        for (uint j=0; j<SegmentCount(i); j++) {
            cerr<<"  Segment #"<<j<<endl;
            cerr<<"   compression type: "<<compressionTypeString(i,j)<<endl;
            cerr<<"               mode: "<<Mode(i,j)<<endl;
            cerr<<"      actual string: "<<CompressedString(i,j)<<endl;
        }
    }
}
*/
/*
void MultipleStringStructure::printShort() const {
    if (1==stringCount() && 1==SegmentCount(0)) {
        if (0==Bytes(0,0))
            cerr<<"Language: "<<LanguageCode(0).toString()<<endl;
        else cerr<<'\"'<<CompressedString(0,0)<<'\"'
                 <<"  lang("<<LanguageCode(0).toString()<<":"
                 <<LanguageCode(0).Code()<<") "<<endl;
        return;
    }
    cerr<<"MultipleStringStructure    count("<<stringCount()<<")"<<endl;
    for (int i=0; i<stringCount(); i++) {
        cerr<<" String #"<<i<<"  lang("<<LanguageCode(i).toString()<<":"
            <<LanguageCode(i).Code()<<")"<<endl;
        if (SegmentCount(i)>1)
            cerr<<"  segment count: "<<SegmentCount(i)<<endl;
        for (uint j=0; j<SegmentCount(i); j++)
            cerr<<"  Segment #"<<j<<" ct("<<CompressionType(i,j)
                <<") \""<<CompressedString(i,j)<<"\""<<endl;
    }
}
*/
QString MultipleStringStructure::toString() const {
    QString str;
    if (1==StringCount() && 1==SegmentCount(0)) {
        str.append(QString("lang(%1) ").arg(LanguageCode(0).toString()));
        if (0!=Bytes(0,0))
            str.append(CompressedString(0,0));
        return str;
    }
    str.append(QString("MultipleStringStructure    count(%1)").arg(StringCount()));
    for (int i=0; i<StringCount(); i++) {
        str.append(QString(" String #%1 lang(%2:%3)").arg(i).
                   arg(LanguageCode(i).toString()).arg(LanguageCode(i).Code()));
        if (SegmentCount(i)>1)
            str.append(QString("  segment count(%1)").arg(SegmentCount(i)));
        for (uint j=0; j<SegmentCount(i); j++)
            str.append(QString("  Segment #%1  ct(%2) str(%3)").arg(j).
                       arg(CompressionType(i,j)).arg(CompressedString(i,j)));
    }
    return str;
}

QString MultipleStringStructure::CompressedString(int i, int j) const {
    const unsigned char* buf = (Offset(i,j)+3);
    int len = Bytes(i,j);
    if (len<=0) return QString("");
    int ct = CompressionType(i,j);
    if (ct<3) {
        if (1==ct) return Huffman1(buf, len);
        else if (2==ct) return Huffman2(buf, len);
        else return Uncompressed(buf, len, Mode(i,j));
    } else return QString("unknown text type");
}

QString MultipleStringStructure::Uncompressed(
    const unsigned char* buf, int len, int mode) {

    QString str=QString("");
    if (mode<=6 ||
        (9<=mode && mode<=0xe) ||
        (0x10==mode) ||
        (0x20<=mode && mode<=0x27) ||
        (0x30<=mode && mode<=0x33)) { // basic runlength encoding
        int hb=mode<<8;
        for (int j=0; j<len; j++) {
            //cerr<<"str.append(0x"<<hex<<mode<<":0x"<<int(buf[j])<<") -> "
            //<<QChar(hb|buf[j])<<endl;
            str.append( QChar( hb|buf[j] ) );
        }
    } else if (mode==0x3e) {
        // Standard Compression Scheme for Unicode (SCSU)
        str=QString("TODO SCSU encoding");
    } else if (mode==0x3f) { //  Unicode, UTF-16 Form
        const unsigned short* ustr =
            reinterpret_cast<const unsigned short*>(buf);
        for (int j=0; j<(len>>1); j++)
            str.append( QChar( (ustr[j]<<8) | (ustr[j]>>8) ) );
    } else if (0x40<=mode && mode<=0x41)
        str = QString("TODO Tawain Characters");
    else if (0x48==mode)
        str = QString("TODO South Korean Characters");
    else
        str = QString("unknown character encoding mode(%0)").arg(mode);
    return str;
}

QString MultipleStringStructure::Huffman1(
    const unsigned char* /*buf*/, int /*len*/) {
    return QString("TODO huffman 1");
}

QString MultipleStringStructure::Huffman2(
    const unsigned char* /*buf*/, int /*len*/) {
    return QString("TODO huffman 2");
}

