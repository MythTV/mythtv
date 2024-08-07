// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson

#include <algorithm>

#include "libmythbase/iso639.h"
#include "libmythbase/mythlogging.h"

#include "atscdescriptors.h"
#include "atsc_huffman.h"

QString MultipleStringStructure::CompressionTypeString(uint i, uint j) const
{
    uint ct = CompressionType(i, j);
    if (0 == ct)
        return {"no compression"};
    if (1 == ct)
        return {"Huffman Coding using C.4, C.5"};
    if (2 == ct)
        return {"Huffman Coding using C.6, C.7"};
    if (ct < 0xaf)
        return {"reserved"};
    return {"compression not used by ATSC in North America, unknown"};
}

QString MultipleStringStructure::toString() const
{
    QString str;
    if (1 == StringCount() && 1 == SegmentCount(0))
    {
        str.append(QString("lang(%1) ").arg(LanguageString(0)));
        if (0 != Bytes(0, 0))
            str.append(GetSegment(0, 0));
        return str;
    }

    str.append(QString("MultipleStringStructure    count(%1)")
               .arg(StringCount()));

    for (uint i = 0; i < StringCount(); i++)
    {
        str.append(QString(" String #%1 lang(%2:%3)")
                   .arg(i).arg(LanguageString(i))
                   .arg(LanguageKey(i)));

        if (SegmentCount(i) > 1)
            str.append(QString("  segment count(%1)").arg(SegmentCount(i)));

        for (uint j=0; j<SegmentCount(i); j++)
            str.append(QString("  Segment #%1  ct(%2) str(%3)").arg(j)
                       .arg(CompressionType(i, j)).arg(GetSegment(i, j)));
    }

    return str;
}

static uint maxPriority(const QMap<uint,uint> &langPrefs)
{
    uint max_pri = 0;
    for (uint pref : std::as_const(langPrefs))
        max_pri = std::max(max_pri, pref);
    return max_pri;
}

uint MultipleStringStructure::GetIndexOfBestMatch(
    QMap<uint,uint> &langPrefs) const
{
    uint match_idx = 0;
    uint match_pri = 0;

    for (uint i = 0; i < StringCount(); i++)
    {
        QMap<uint,uint>::const_iterator it =
            langPrefs.constFind(CanonicalLanguageKey(i));
        if ((it != langPrefs.constEnd()) && (*it > match_pri))
        {
            match_idx = i;
            match_pri = *it;
        }
    }

    if (match_pri)
        return match_idx;

    if (StringCount())
        langPrefs[CanonicalLanguageKey(0)] = maxPriority(langPrefs) + 1;

    return 0;
}

QString MultipleStringStructure::GetBestMatch(QMap<uint,uint> &langPrefs) const
{
    if (StringCount())
        return GetFullString(GetIndexOfBestMatch(langPrefs));
    return {};
}

QString MultipleStringStructure::GetSegment(uint i, uint j) const
{
    const unsigned char* buf = (Offset(i, j)+3);
    int len = Bytes(i, j);

    if (len <= 0)
        return "";

    int ct = CompressionType(i, j);

    if (ct == 0)
        return Uncompressed(buf, len, Mode(i, j));

    if (ct < 3)
        return atsc_huffman1_to_string(buf, len, ct);

    return QString("MSS unknown text compression %1").arg(ct);
}

QString MultipleStringStructure::GetFullString(uint i) const
{
    QString tmp = "";
    for (uint j = 0; j < SegmentCount(i); j++)
        tmp += GetSegment(i, j);
    return tmp.simplified();
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
        for (int j=0; j<len; j++)
        {
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, QString("str.append(0x%1:0x%2) -> %3")
                .arg(mode, 0, 16) .arg(buf[j], 0, 16) .arg(QChar(hb|buf[j])));
#endif
            if (hb|buf[j])
                str.append( QChar( hb|buf[j] ) );
        }
    } else if (mode==0x3e) {
        // Standard Compression Scheme for Unicode (SCSU)
        str=QString("TODO SCSU encoding");
    } else if (mode==0x3f) { //  Unicode, UTF-16 Form
        const auto* ustr = reinterpret_cast<const unsigned short*>(buf);
        for (int j=0; j<(len>>1); j++)
            str.append( QChar( (ustr[j]<<8) | (ustr[j]>>8) ) );
    } else if (0x40<=mode && mode<=0x41) {
        str = QString("TODO Tawain Characters");
    } else if (0x48==mode) {
        str = QString("TODO South Korean Characters");
    } else {
        str = QString("unknown character encoding mode(%0)").arg(mode);
    }
    return str;
}

void MultipleStringStructure::Parse(void) const
{
    m_ptrs.clear();
    m_ptrs[Index(0,-1)] = m_data + 1;
    for (uint i = 0; i < StringCount(); i++)
    {
        m_ptrs[Index(i,0)] = Offset(i,-1) + 4;
        uint j = 0;
        for (; j < SegmentCount(i); j++)
            m_ptrs[Index(i,j+1)] = Offset(i,j) + Bytes(i,j) + 3;
        m_ptrs[Index(i+1,-1)] = Offset(i,j);
    }
}

bool CaptionServiceDescriptor::Parse(void)
{
    m_ptrs.clear();
    m_ptrs[Index(0,-1)] = m_data+3;

    for (uint i = 0; i < ServicesCount(); i++)
        m_ptrs[Index(i+1,-1)] = Offset(i,-1) + 6;

    return true;
}

QString CaptionServiceDescriptor::toString(void) const
{
    QString str("Caption Service Descriptor  ");
    str.append(QString("services(%2)").arg(ServicesCount()));

    for (uint i = 0; i < ServicesCount(); i++)
    {
        str.append(QString("\n     lang(%1) type(%2) ")
                   .arg(LanguageString(i)).arg(static_cast<int>(Type(i))));
        str.append(QString("easy_reader(%1) wide(%2) ")
                   .arg(static_cast<int>(EasyReader(i))).arg(static_cast<int>(WideAspectRatio(i))));
        if (Type(i))
        {
            str.append(QString("service_num(%1)")
                       .arg(CaptionServiceNumber(i)));
        }
        else
        {
            str.append(QString("line_21_field(%1)").arg(static_cast<int>(Line21Field(i))));
        }
    }

    return str;
}

bool ContentAdvisoryDescriptor::Parse(void)
{
    m_ptrs.clear();
    m_ptrs[Index(0,-1)] = m_data + 2;

    for (uint i = 0; i < RatingRegionCount(); i++)
    {
        m_ptrs[Index(i,0)] = Offset(i,-1)+2;
        uint j = 0;
        for (; j < RatedDimensions(i); j++)
            m_ptrs[Index(i,j+1)] = Offset(i,j) + 2;
        const unsigned char *tmp = Offset(i,-1) + 3 + (RatedDimensions(i)<<1);
        uint len = RatingDescriptionLength(i);
        m_ptrs[Index(i+1,-1)] = tmp + len;
    }

    return true;
}

QString ContentAdvisoryDescriptor::toString() const
{
    return "ContentAdvisoryDescriptor::toString(): Not implemented";
}

QString AC3AudioStreamDescriptor::SampleRateCodeString(void) const
{
    static const std::array<const std::string,8> s_asd
    {
        "48kbps", "44.1kbps", "32kbps", "Reserved",
        "48kbps or 44.1kbps", "48kbps or 32kbps",
        "44.1kbps or 32kbps", "48kbps or 44.1kbps or 32kbps"
    };
    return QString::fromStdString(s_asd[SampleRateCode()]);
}

QString AC3AudioStreamDescriptor::BitRateCodeString(void) const
{
    static const std::array<const std::string,19> s_ebr
    {
        "=32kbps",  "=40kbps",  "=48kbps",  "=56kbps",  "=64kbps",
        "=80kbps",  "=96kbps",  "=112kbps", "=128kbps", "=160kbps",
        "=192kbps", "=224kbps", "=256kbps", "=320kbps", "=384kbps",
        "=448kbps", "=512kbps", "=576kbps", "=640kbps"
    };
    static const std::array<const std::string,19> s_ubr
    {
        "<=32kbps",  "<=40kbps", "<=48kbps",  "<=56kbps",  "<=64kbps",
        "<=80kbps",  "<=96kbps", "<=112kbps", "<=128kbps", "<=160kbps",
        "<=192kbps","<=224kbps", "<=256kbps", "<=320kbps", "<=384kbps",
        "<=448kbps","<=512kbps", "<=576kbps", "<=640kbps"
    };

    if (BitRateCode() <= 18)
        return QString::fromStdString(s_ebr[BitRateCode()]);
    if ((BitRateCode() >= 32) && (BitRateCode() <= 50))
        return QString::fromStdString(s_ubr[BitRateCode()-32]);
    return {"Unknown Bit Rate Code"};
}

QString AC3AudioStreamDescriptor::SurroundModeString(void) const
{
    static const std::array<const std::string,4> s_sms
    {
        "Not indicated",
        "Not Dolby surround encoded",
        "Dolby surround encoded",
        "Reserved",
    };
    return QString::fromStdString(s_sms[SurroundMode()]);
}

QString AC3AudioStreamDescriptor::ChannelsString(void) const
{
    static const std::array<const std::string,16> s_cs
    {
        "1 + 1",    "1/0",      "2/0",      "3/0",
        "2/1",      "3/1",      "2/2 ",     "3/2",
        "1",        "<= 2",     "<= 3",     "<= 4",
        "<= 5",     "<= 6",     "Reserved", "Reserved"
    };
    return QString::fromStdString(s_cs[Channels()]);
}

QString AC3AudioStreamDescriptor::toString() const
{
    QString str;
    str.append(QString("AC-3 Audio Stream Descriptor "));
    str.append(QString(" full_srv(%1) sample_rate(%2) bit_rate(%3, %4)\n")
               .arg(static_cast<int>(FullService()))
               .arg(SampleRateCodeString(),
                    BitRateCodeString())
               .arg(BitRateCode()));
    str.append(QString("      bsid(%1) bs_mode(%2) channels(%3) Dolby(%4)\n")
               .arg(bsid()).arg(BasicServiceMode())
               .arg(ChannelsString(), SurroundModeString()));

    /*
    str.append(QString("   language code: %1").arg(languageCode()));
    if (0==channels()) {
        str.append(QString(" language code 2: %1").arg(languageCode2()));
    }
    */

    if (BasicServiceMode() < 2)
        str.append(QString("      mainID(%1) ").arg(MainID()));
    else
        str.append(QString("      associated_service(0x%1) ")
                   .arg(AServiceFlags(),0,16));

    if (TextLength())
    {
        str.append(QString("isLatin-1(%1) ")
                   .arg(IsTextLatin1() ? "true" : "false"));
        str.append(QString("text_length(%1) ").arg(TextLength()));
        str.append(QString("text(%1)").arg(Text()));
    }
    return str;
}

/** \fn ExtendedChannelNameDescriptor::LongChannelName(void) const
 *  \brief Returns a MultipleStringStructure representing the
 *          long name of the associated channel.
 */
MultipleStringStructure ExtendedChannelNameDescriptor::LongChannelName(
    void) const
{
    return MultipleStringStructure(m_data + 2);
}

/** \fn ExtendedChannelNameDescriptor::LongChannelNameString(void) const
 *  \brief Convenience function that returns a QString comprising a
 *         concatenation of all the segments in the LongChannelName() value.
 */
QString ExtendedChannelNameDescriptor::LongChannelNameString(void) const
{
    QString str = "";
    MultipleStringStructure mstr = LongChannelName();

    for (uint i = 0; i < mstr.StringCount(); i++)
        str += mstr.GetFullString(i);

    return str;
}

QString ExtendedChannelNameDescriptor::toString() const
{
    return QString("ExtendedChannelNameDescriptor: '%1'")
        .arg(LongChannelNameString());
}
