// -*- Mode: c++ -*-

// Some of the XDS was inspired by code in TVTime. -- dtk 03/30/2006
#include "captions/cc608decoder.h"

#include <algorithm>
#include <vector>

// Qt headers
#include <QStringList>
#include <QCoreApplication>

// MythTV headers
#include "libmythbase/mythlogging.h"

#include "vbilut.h"

#define DEBUG_XDS 0

static void init_xds_program_type(CC608ProgramType& xds_program_type);

CC608Decoder::CC608Decoder(CC608Input *ccr)
    : m_reader(ccr),
      m_rbuf(new unsigned char[sizeof(ccsubtitle)+255])
{
    // fill translation table
    for (uint i = 0; i < 128; i++)
        m_stdChar[i] = QChar(i);
    m_stdChar[42]  = QLatin1Char(0xE1); // á
    m_stdChar[92]  = QLatin1Char(0xE9); // é
    m_stdChar[94]  = QLatin1Char(0xED); // í
    m_stdChar[95]  = QLatin1Char(0xF3); // ó
    m_stdChar[96]  = QLatin1Char(0xFA); // ú
    m_stdChar[123] = QLatin1Char(0xE7); // ç
    m_stdChar[124] = QLatin1Char(0xF7); // ÷
    m_stdChar[125] = QLatin1Char(0xD1); // Ñ
    m_stdChar[126] = QLatin1Char(0xF1); // ñ
    m_stdChar[127] = QChar(0x2588);     // full block

    init_xds_program_type(m_xdsProgramTypeString);
}

CC608Decoder::~CC608Decoder(void)
{
    delete [] m_rbuf;
}

void CC608Decoder::FormatCC(std::chrono::milliseconds tc, int code1, int code2)
{
    FormatCCField(tc, 0, code1);
    FormatCCField(tc, 1, code2);
}

void CC608Decoder::GetServices(std::chrono::seconds seconds, CC608Seen& seen) const
{
    auto then = SystemClock::now() - seconds;
    for (uint i = 0; i < 4; i++)
        seen[i] = (m_lastSeen[i] >= then);
}

static const std::array<const int,16> rowdata =
{
    11, -1, 1, 2, 3, 4, 12, 13,
    14, 15, 5, 6, 7, 8,  9, 10
};

static const std::array<const QChar,16> specialchar =
{
    QLatin1Char(0xAE), QLatin1Char(0xB0), QLatin1Char(0xBD), QLatin1Char(0xBF), // ®°½¿
    QChar(0x2122),     QLatin1Char(0xA2), QLatin1Char(0xA3), QChar(0x266A),     // ™¢£♪
    QLatin1Char(0xE0), QLatin1Char(' '),  QLatin1Char(0xE8), QLatin1Char(0xE2), // à èâ
    QLatin1Char(0xEA), QLatin1Char(0xEE), QLatin1Char(0xF4), QLatin1Char(0xFB)  // êîôû
};

static const std::array<const QChar,32> extendedchar2 =
{
    QLatin1Char(0xC1), QLatin1Char(0xC9),  QLatin1Char(0xD3), QLatin1Char(0xDA), // ÁÉÓÚ
    QLatin1Char(0xDC), QLatin1Char(0xFC),  QLatin1Char('`'),  QLatin1Char(0xA1), // Üü`¡
    QLatin1Char('*'),  QLatin1Char('\''),  QChar(0x2014),     QLatin1Char(0xA9), // *'-©
    QChar(0x2120),     QLatin1Char(0xB7),  QChar(0x201C),     QChar(0x201D),     // ℠·“”
    QLatin1Char(0xC0), QLatin1Char(0xC2),  QLatin1Char(0xC7), QLatin1Char(0xC8), // ÀÂÇÈ
    QLatin1Char(0xCA), QLatin1Char(0xCB),  QLatin1Char(0xEB), QLatin1Char(0xCE), // ÊËëÎ
    QLatin1Char(0xCF), QLatin1Char(0xEF),  QLatin1Char(0xD4), QLatin1Char(0xD9), // ÏïÔÙ
    QLatin1Char(0xF9), QLatin1Char(0xDB),  QLatin1Char(0xAB), QLatin1Char(0xBB)  // ùÛ«»
};

static const std::array<const QChar,32> extendedchar3 =
{
    QLatin1Char(0xC3), QLatin1Char(0xE3), QLatin1Char(0xCD), QLatin1Char(0xCC), // ÃãÍÌ
    QLatin1Char(0xEC), QLatin1Char(0xD2), QLatin1Char(0xF2), QLatin1Char(0xD5), // ìÒòÕ
    QLatin1Char(0xF5), QLatin1Char('{'),  QLatin1Char('}'),  QLatin1Char('\\'), // õ{}
    QLatin1Char('^'),  QLatin1Char('_'),  QLatin1Char(0xA6), QLatin1Char('~'),  // ^_¦~
    QLatin1Char(0xC4), QLatin1Char(0xE4), QLatin1Char(0xD6), QLatin1Char(0xF6), // ÄäÖö
    QLatin1Char(0xDF), QLatin1Char(0xA5), QLatin1Char(0xA4), QLatin1Char('|'),  // ß¥¤|
    QLatin1Char(0xC5), QLatin1Char(0xE5), QLatin1Char(0xD8), QLatin1Char(0xF8), // ÅåØø
    QChar(0x250C),     QChar(0x2510),     QChar(0x2514),     QChar(0x2518)      // ┌┐└┘
};

void CC608Decoder::FormatTextCode(std::chrono::milliseconds tc, size_t field, size_t mode, size_t len, int b1, int b2)
{
    if (mode == std::numeric_limits<std::size_t>::max())
        return;

    m_lastCodeTc[field] += 33ms;
    m_timeCode[mode] = tc;

    // commit row number only when first text code
    // comes in
    if (m_newRow[mode])
        NewRowCC(mode, len);

    m_ccBuf[mode] += CharCC(b1);
    m_col[mode]++;
    if (b2 & 0x60)
    {
        m_ccBuf[mode] += CharCC(b2);
        m_col[mode]++;
    }
}

void CC608Decoder::FormatControlCode(std::chrono::milliseconds tc, size_t field, int b1, int b2)
{
    m_lastCodeTc[field] += 67ms;

    int newccmode = (b1 >> 3) & 1;
    int newtxtmode = m_txtMode[(field*2) + newccmode];
    if ((b1 & 0x06) == 0x04)
    {
        switch (b2)
        {
        case 0x29:
        case 0x2C:
        case 0x20:
        case 0x2F:
        case 0x25:
        case 0x26:
        case 0x27:
            // CC1,2
            newtxtmode = 0;
            break;
        case 0x2A:
        case 0x2B:
            // TXT1,2
            newtxtmode = 1;
            break;
        }
    }
    m_ccMode[field] = newccmode;
    m_txtMode[(field*2) + newccmode] = newtxtmode;
    size_t mode = (field << 2) | (newtxtmode << 1) | m_ccMode[field];

    m_timeCode[mode] = tc;
    size_t len = m_ccBuf[mode].length();

    if (b2 & 0x40)           //preamble address code (row & indent)
    {
        if (newtxtmode)
            // no address codes in TXT mode?
            return;

        m_newRow[mode] = rowdata[((b1 << 1) & 14) | ((b2 >> 5) & 1)];
        if (m_newRow[mode] == -1)
            // bogus code?
            m_newRow[mode] = m_lastRow[mode] + 1;

        if (b2 & 0x10)        //row contains indent flag
        {
            m_newCol[mode] = (b2 & 0x0E) << 1;
            // Encode as 0x7020 or 0x7021 depending on the
            // underline flag.
            m_newAttr[mode] = (b2 & 0x1) + 0x20;
            LOG(VB_VBI, LOG_INFO,
                QString("cc608 preamble indent, b2=%1")
                .arg(b2, 2, 16));
        }
        else
        {
            m_newCol[mode] = 0;
            m_newAttr[mode] = (b2 & 0xf) + 0x10;
            // Encode as 0x7010 through 0x702f for the 16 possible
            // values of b2.
            LOG(VB_VBI, LOG_INFO,
                QString("cc608 preamble color change, b2=%1")
                .arg(b2, 2, 16));
        }

        // row, indent, attribute settings are not final
        // until text code arrives
        return;
    }

    switch (b1 & 0x07)
    {
    case 0x00:          //attribute
#if 0
        LOG(VB_VBI, LOG_DEBUG,
            QString("<ATTRIBUTE %1 %2>").arg(b1).arg(b2));
#endif
        break;
    case 0x01:          //midrow or char
        if (m_newRow[mode])
            NewRowCC(mode, len);

        switch (b2 & 0x70)
        {
        case 0x20:      //midrow attribute change
            LOG(VB_VBI, LOG_INFO,
                QString("cc608 mid-row color change, b2=%1")
                .arg(b2, 2, 16));
            // Encode as 0x7000 through 0x700f for the
            // 16 possible values of b2.
            m_ccBuf[mode] += ' ';
            m_ccBuf[mode] += QChar(0x7000 + (b2 & 0xf));
            m_col[mode]++;
            break;
        case 0x30:      //special character..
            m_ccBuf[mode] += specialchar[b2 & 0x0f];
            m_col[mode]++;
            break;
        }
        break;
    case 0x02:          //extended char
        // extended char is preceded by alternate char
        // - if there's no alternate, it could be noise
        if (!len)
            break;

        if (b2 & 0x30)
        {
            m_ccBuf[mode].remove(len - 1, 1);
            m_ccBuf[mode] += extendedchar2[b2 - 0x20];
            break;
        }
        break;
    case 0x03:          //extended char
        // extended char is preceded by alternate char
        // - if there's no alternate, it could be noise
        if (!len)
            break;

        if (b2 & 0x30)
        {
            m_ccBuf[mode].remove(len - 1, 1);
            m_ccBuf[mode] += extendedchar3[b2 - 0x20];
            break;
        }
        break;
    case 0x04:          //misc
    case 0x05:          //misc + F
#if 0
        LOG(VB_VBI, LOG_DEBUG,
            QString("ccmode %1 cmd %2").arg(m_ccMode)
            .arg(b2, 2, 16, '0'));
#endif
        switch (b2)
        {
        case 0x21:      //backspace
            // add backspace if line has been encoded already
            if (m_newRow[mode])
                len = NewRowCC(mode, len);

            if (len == 0 ||
                m_ccBuf[mode].startsWith("\b"))
            {
                m_ccBuf[mode] += '\b';
                m_col[mode]--;
            }
            else
            {
                m_ccBuf[mode].remove(len - 1, 1);
                m_col[mode]--;
            }
            break;
        case 0x25:      //2 row caption
        case 0x26:      //3 row caption
        case 0x27:      //4 row caption
            if (m_style[mode] == CC_STYLE_PAINT && len)
            {
                // flush
                BufferCC(mode, len, 0);
                m_ccBuf[mode] = "";
                m_row[mode] = 0;
                m_col[mode] = 0;
            }
            else if (m_style[mode] == CC_STYLE_POPUP)
            {
                ResetCC(mode);
            }

            m_rowCount[mode] = b2 - 0x25 + 2;
            m_style[mode] = CC_STYLE_ROLLUP;
            break;
        case 0x2D:      //carriage return
            if (m_style[mode] != CC_STYLE_ROLLUP)
                break;

            if (m_newRow[mode])
                m_row[mode] = m_newRow[mode];

            // flush if there is text or need to scroll
            // TODO:  decode ITV (WebTV) link in TXT2
            if (len || (m_row[mode] != 0 && !m_lineCont[mode] &&
                        (!newtxtmode || m_row[mode] >= 16)))
            {
                BufferCC(mode, len, 0);
            }

            if (newtxtmode)
            {
                if (m_row[mode] < 16)
                    m_newRow[mode] = m_row[mode] + 1;
                else
                    // scroll up previous lines
                    m_newRow[mode] = 16;
            }

            m_ccBuf[mode] = "";
            m_col[mode] = 0;
            m_lineCont[mode] = 0;
            break;

        case 0x29:
            // resume direct caption (paint-on style)
            if (m_style[mode] == CC_STYLE_ROLLUP && len)
            {
                // flush
                BufferCC(mode, len, 0);
                m_ccBuf[mode] = "";
                m_row[mode] = 0;
                m_col[mode] = 0;
            }
            else if (m_style[mode] == CC_STYLE_POPUP)
            {
                ResetCC(mode);
            }

            m_style[mode] = CC_STYLE_PAINT;
            m_rowCount[mode] = 0;
            m_lineCont[mode] = 0;
            break;

        case 0x2B:      //resume text display
            m_resumeText[mode] = 1;
            if (m_row[mode] == 0)
            {
                m_newRow[mode] = 1;
                m_newCol[mode] = 0;
                m_newAttr[mode] = 0;
            }
            m_style[mode] = CC_STYLE_ROLLUP;
            break;
        case 0x2C:      //erase displayed memory
            if (m_ignoreTimeCode ||
                (tc - m_lastClr[mode]) > 5s ||
                m_lastClr[mode] == 0ms)
            {
                // don't overflow the frontend with
                // too many redundant erase codes
                BufferCC(mode, 0, 1);
            }
            if (m_style[mode] != CC_STYLE_POPUP)
            {
                m_row[mode] = 0;
                m_col[mode] = 0;
            }
            m_lineCont[mode] = 0;
            break;

        case 0x20:      //resume caption (pop-up style)
            if (m_style[mode] != CC_STYLE_POPUP)
            {
                if (len)
                    // flush
                    BufferCC(mode, len, 0);
                m_ccBuf[mode] = "";
                m_row[mode] = 0;
                m_col[mode] = 0;
            }
            m_style[mode] = CC_STYLE_POPUP;
            m_rowCount[mode] = 0;
            m_lineCont[mode] = 0;
            break;
        case 0x2F:      //end caption + swap memory
            if (m_style[mode] != CC_STYLE_POPUP)
            {
                if (len)
                    // flush
                    BufferCC(mode, len, 0);
            }
            else if (m_ignoreTimeCode ||
                     (tc - m_lastClr[mode]) > 5s ||
                     m_lastClr[mode] == 0ms)
            {
                // clear and flush
                BufferCC(mode, len, 1);
            }
            else if (len)
            {
                // flush
                BufferCC(mode, len, 0);
            }
            m_ccBuf[mode] = "";
            m_row[mode] = 0;
            m_col[mode] = 0;
            m_style[mode] = CC_STYLE_POPUP;
            m_rowCount[mode] = 0;
            m_lineCont[mode] = 0;
            break;

        case 0x2A:      //text restart
            // clear display
            BufferCC(mode, 0, 1);
            ResetCC(mode);
            // TXT starts at row 1
            m_newRow[mode] = 1;
            m_newCol[mode] = 0;
            m_newAttr[mode] = 0;
            m_style[mode] = CC_STYLE_ROLLUP;
            break;

        case 0x2E:      //erase non-displayed memory
            ResetCC(mode);
            break;
        }
        break;
    case 0x07:          //misc (TAB)
        if (m_newRow[mode])
        {
            m_newCol[mode] += (b2 & 0x03);
            NewRowCC(mode, len);
        }
        else
        {
            // illegal?
            for (int x = 0; x < (b2 & 0x03); x++)
            {
                m_ccBuf[mode] += ' ';
                m_col[mode]++;
            }
        }
        break;
    }
}

void CC608Decoder::FormatCCField(std::chrono::milliseconds tc, size_t field, int data)
{
    size_t len = 0;
    size_t mode = 0;

    if (data == -1)              // invalid data. flush buffers to be safe.
    {
        // TODO:  flush reader buffer
        if (m_ccMode[field] != -1)
        {
            for (mode = field*4; mode < ((field*4) + 4); mode++)
                ResetCC(mode);
            m_xds[field] = 0;
            m_badVbi[field] = 0;
            m_ccMode[field] = -1;
            m_txtMode[field*2] = 0;
            m_txtMode[(field*2) + 1] = 0;
        }
        return;
    }

    if ((m_lastFormatData[field&1] == data) &&
        (m_lastFormatTc[field&1] == tc))
    {
        LOG(VB_VBI, LOG_DEBUG, "Format CC -- Duplicate");
        return;
    }

    m_lastFormatTc[field&1] = tc;
    m_lastFormatData[field&1] = data;

    int b1 = data & 0x7f;
    int b2 = (data >> 8) & 0x7f;
#if 1
    LOG(VB_VBI, LOG_DEBUG,
        QString("Format CC @%1/%2 = %3 %4, %5/%6 = '%7' '%8'")
        .arg(tc.count()).arg(field)
        .arg((data&0xff), 2, 16)
        .arg((data&0xff00)>>8, 2, 16)
        .arg(b1, 2, 16, QChar('0'))
        .arg(b2, 2, 16, QChar('0'))
        .arg(QChar((b1 & 0x60) ? b1 : '_'))
        .arg(QChar((b2 & 0x60) ? b2 : '_')));
#endif
    if (m_ccMode[field] >= 0)
    {
        mode = field << 2 |
            (m_txtMode[(field*2) + m_ccMode[field]] << 1) |
            m_ccMode[field];
        if (mode != std::numeric_limits<std::size_t>::max())
            len = m_ccBuf[mode].length();
        else
            len = 0;
    }
    else
    {
        mode = std::numeric_limits<std::size_t>::max();
        len = 0;
    }

    if (FalseDup(tc, field, data))
    {
        if (m_ignoreTimeCode)
            return;
    }
    else if (XDSDecode(field, b1, b2))
    {
        return;
    }
    else if (b1 & 0x60)
    {
        // 0x20 <= b1 <= 0x7F
        // text codes
        FormatTextCode(tc, field, mode, len, b1, b2);
    }
    else if ((b1 & 0x10) && (b2 > 0x1F))
    {
        // 0x10 <= b1 <= 0x1F
        // control codes
        FormatControlCode(tc, field, b1, b2);
    }

    for (size_t mode2 = field*4; mode2 < ((field*4) + 4); mode2++)
    {
        size_t len2 = m_ccBuf[mode2].length();
        if ((m_ignoreTimeCode || ((tc - m_timeCode[mode2]) > 100ms)) &&
             (m_style[mode2] != CC_STYLE_POPUP) && len2)
        {
            // flush unfinished line if waiting too long
            // in paint-on or scroll-up mode
            m_timeCode[mode2] = tc;
            BufferCC(mode2, len2, 0);
            m_ccBuf[mode2] = "";
            m_row[mode2] = m_lastRow[mode2];
            m_lineCont[mode2] = 1;
        }
    }

    if (data != m_lastCode[field])
    {
        m_lastCode[field] = data;
        m_lastCodeTc[field] = tc;
    }
    m_lastTc[field] = tc;
}

bool CC608Decoder::FalseDup(std::chrono::milliseconds tc, int field, int data)
{
    int b1 = data & 0x7f;
    int b2 = (data >> 8) & 0x7f;

    if (m_ignoreTimeCode)
    {
        // most digital streams with encoded VBI
        // have duplicate control codes;
        // suppress every other repeated control code
        if ((data == m_lastCode[field]) &&
            ((b1 & 0x70) == 0x10))
        {
            m_lastCode[field] = -1;
            return true;
        }
        return false;
    }

    // bttv-0.9 VBI reads are pretty reliable (1 read/33367us).
    // bttv-0.7 reads don't seem to work as well so if read intervals
    // vary from this, be more conservative in detecting duplicate
    // CC codes.
    std::chrono::milliseconds dup_text_fudge = 0ms;
    std::chrono::milliseconds dup_ctrl_fudge = 0ms;
    if (m_badVbi[field] < 100 && b1 != 0 && b2 != 0)
    {
        std::chrono::milliseconds d = tc - m_lastTc[field];
        if (d < 25ms || d > 42ms)
            m_badVbi[field]++;
        else if (m_badVbi[field] > 0)
            m_badVbi[field]--;
    }
    if (m_badVbi[field] < 4)
    {
        // this should pick up all codes
        dup_text_fudge = -2ms;
        // this should pick up 1st, 4th, 6th, 8th, ... codes
        dup_ctrl_fudge = 33ms - 4ms;
    }
    else
    {
        dup_text_fudge = 4ms;
        dup_ctrl_fudge = 33ms - 4ms;
    }

    if (data == m_lastCode[field])
    {
        if ((b1 & 0x70) == 0x10)
        {
            if (tc > (m_lastCodeTc[field] + 67ms + dup_ctrl_fudge))
                return false;
        }
        else if (b1)
        {
            // text, XDS
            if (tc > (m_lastCodeTc[field] + 33ms + dup_text_fudge))
                return false;
        }

        return true;
    }

    return false;
}

void CC608Decoder::ResetCC(size_t mode)
{
//    m_lastRow[mode] = 0;
//    m_newRow[mode] = 0;
//    m_newCol[mode] = 0;
//    m_timeCode[mode] = 0;
    m_row[mode] = 0;
    m_col[mode] = 0;
    m_rowCount[mode] = 0;
//    m_style[mode] = CC_STYLE_POPUP;
    m_lineCont[mode] = 0;
    m_resumeText[mode] = 0;
    m_lastClr[mode] = 0ms;
    m_ccBuf[mode] = "";
}

QString CC608Decoder::ToASCII(const QString &cc608str, bool suppress_unknown)
{
    QString ret = "";

    for (const auto& cp : std::as_const(cc608str))
    {
        int cpu = cp.unicode();
        if (cpu == 0)
            break;
        switch (cpu)
        {
            case 0x2120 :  ret += "(SM)"; break;
            case 0x2122 :  ret += "(TM)"; break;
            case 0x2014 :  ret += "(--)"; break;
            case 0x201C :  ret += "``"; break;
            case 0x201D :  ret += "''"; break;
            case 0x250C :  ret += "|-"; break;
            case 0x2510 :  ret += "-|"; break;
            case 0x2514 :  ret += "|_"; break;
            case 0x2518 :  ret += "_|"; break;
            case 0x2588 :  ret += "[]"; break;
            case 0x266A :  ret += "o/~"; break;
            case '\b'   :  ret += "\\b"; break;
            default     :
                if (cpu >= 0x7000 && cpu < 0x7000 + 0x30)
                {
                    if (!suppress_unknown)
                        ret += QString("[%1]").arg(cpu, 2, 16);
                }
                else if (cpu <= 0x80)
                {
                    ret += QString(cp.toLatin1());
                }
                else if (!suppress_unknown)
                {
                    ret += QString("{%1}").arg(cpu, 2, 16);
                }
        }
    }

    return ret;
}

void CC608Decoder::BufferCC(size_t mode, int len, int clr)
{
    QByteArray tmpbuf;
    if (len)
    {
        // calculate UTF-8 encoding length
        tmpbuf = m_ccBuf[mode].toUtf8();
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        len = std::min(tmpbuf.length(), 255);
#else
        len = std::min(tmpbuf.length(), static_cast<qsizetype>(255));
#endif
    }

    unsigned char *bp = m_rbuf;
    *(bp++) = m_row[mode];
    *(bp++) = m_rowCount[mode];
    *(bp++) = m_style[mode];
    // overload resumetext field
    unsigned char f = m_resumeText[mode];
    f |= mode << 4;
    if (m_lineCont[mode])
        f |= CC_LINE_CONT;
    *(bp++) = f;
    *(bp++) = clr;
    *(bp++) = len;
    if (len)
    {
        memcpy(bp,
               tmpbuf.constData(),
               len);
        len += sizeof(ccsubtitle);
    }
    else
    {
        len = sizeof(ccsubtitle);
    }

    if ((len != 0) && VERBOSE_LEVEL_CHECK(VB_VBI, LOG_INFO))
    {
        LOG(VB_VBI, LOG_INFO, QString("### %1 %2 %3 %4 %5 %6 %7 - '%8'")
            .arg(m_timeCode[mode].count(), 10)
            .arg(m_row[mode], 2).arg(m_rowCount[mode])
            .arg(m_style[mode]).arg(f, 2, 16)
            .arg(clr).arg(len, 3)
            .arg(ToASCII(QString::fromUtf8(tmpbuf.constData(), len), false)));
    }

    m_reader->AddTextData(m_rbuf, len, m_timeCode[mode], 'C');
    uint8_t ccmode = m_rbuf[3] & CC_MODE_MASK;
    int stream = -1;
    switch (ccmode)
    {
        case CC_CC1: stream = 0; break;
        case CC_CC2: stream = 1; break;
        case CC_CC3: stream = 2; break;
        case CC_CC4: stream = 3; break;
    }
    if (stream >= 0)
        m_lastSeen[stream] = std::chrono::system_clock::now();

    m_resumeText[mode] = 0;
    if (clr && !len)
        m_lastClr[mode] = m_timeCode[mode];
    else if (len)
        m_lastClr[mode] = 0ms;
}

int CC608Decoder::NewRowCC(size_t mode, int len)
{
    if (m_style[mode] == CC_STYLE_ROLLUP)
    {
        // previous line was likely missing a carriage return
        m_row[mode] = m_newRow[mode];
        if (len)
        {
            BufferCC(mode, len, 0);
            m_ccBuf[mode] = "";
            len = 0;
        }
        m_col[mode] = 0;
        m_lineCont[mode] = 0;
    }
    else
    {
        // popup/paint style

        if (m_row[mode] == 0)
        {
            if (len == 0)
                m_row[mode] = m_newRow[mode];
            else
            {
                // previous line was missing a row address
                // - assume it was one row up
                m_ccBuf[mode] += '\n';
                len++;
                if (m_row[mode] == 0)
                    m_row[mode] = m_newRow[mode] - 1;
                else
                    m_row[mode]--;
            }
        }
        else if (m_newRow[mode] > m_lastRow[mode])
        {
            // next line can be more than one row away
            for (int i = 0; i < (m_newRow[mode] - m_lastRow[mode]); i++)
            {
                m_ccBuf[mode] += '\n';
                len++;
            }
            m_col[mode] = 0;
        }
        else if (m_newRow[mode] == m_lastRow[mode])
        {
            // same row
            if (m_newCol[mode] >= m_col[mode])
            {
                // new line appends to current line
                m_newCol[mode] -= m_col[mode];
            }
            else
            {
                // new line overwrites current line;
                // could be legal (overwrite spaces?) but
                // more likely we have bad address codes
                // - just move to next line; may exceed row 15
                // but frontend will adjust
                m_ccBuf[mode] += '\n';
                len++;
                m_col[mode] = 0;
            }
        }
        else
        {
            // next line goes upwards (not legal?)
            // - flush
            BufferCC(mode, len, 0);
            m_ccBuf[mode] = "";
            m_row[mode] = m_newRow[mode];
            m_col[mode] = 0;
            m_lineCont[mode] = 0;
            len = 0;
        }
    }

    m_lastRow[mode] = m_newRow[mode];
    m_newRow[mode] = 0;

    int limit = m_newCol[mode];
    for (int x = 0; x < limit; x++)
    {
        m_ccBuf[mode] += ' ';
        len++;
        m_col[mode]++;
    }

    if (m_newAttr[mode])
    {
        m_ccBuf[mode] += QChar(m_newAttr[mode] + 0x7000);
        len++;
    }

    m_newCol[mode] = 0;
    m_newAttr[mode] = 0;

    return len;
}


static bool IsPrintable(char c)
{
    return ((c) & 0x7F) >= 0x20 && ((c) & 0x7F) <= 0x7E;
}

static char Printable(char c)
{
    return IsPrintable(c) ? ((c) & 0x7F) : '.';
}

#if 0
static int OddParity(unsigned char c)
{
    c ^= (c >> 4); c ^= (c >> 2); c ^= (c >> 1);
    return c & 1;
}
#endif

// // // // // // // // // // // // // // // // // // // // // // // //
// // // // // // // // // // //  VPS  // // // // // // // // // // //
// // // // // // // // // // // // // // // // // // // // // // // //

static constexpr int PIL_TIME(int day, int mon, int hour, int min)
{ return (day << 15) + (mon << 11) + (hour << 6) + min; }

static void DumpPIL(int pil)
{
    int day  = (pil >> 15);
    int mon  = (pil >> 11) & 0xF;
    int hour = (pil >> 6 ) & 0x1F;
    int min  = (pil      ) & 0x3F;

    if (pil == PIL_TIME(0, 15, 31, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Timer-control (no PDC)");
    else if (pil == PIL_TIME(0, 15, 30, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Recording inhibit/terminate");
    else if (pil == PIL_TIME(0, 15, 29, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Interruption");
    else if (pil == PIL_TIME(0, 15, 28, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Continue");
    else if (pil == PIL_TIME(31, 15, 31, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: No time");
    else
        LOG(VB_VBI, LOG_INFO, QString(" PDC: %1, 200X-%2-%3 %4:%5")
                .arg(pil).arg(mon).arg(day).arg(hour).arg(min));
#undef PIL_TIME
}

void CC608Decoder::DecodeVPS(const unsigned char *buf)
{
    int c = vbi_bit_reverse[buf[1]];

    if ((int8_t) c < 0)
    {
        m_vpsLabel[m_vpsL] = 0;
        m_vpsPrLabel = m_vpsLabel;
        m_vpsL = 0;
    }
    c &= 0x7F;
    m_vpsLabel[m_vpsL] = Printable(c);
    m_vpsL = (m_vpsL + 1) % 16;

    LOG(VB_VBI, LOG_INFO, QString("VPS: 3-10: %1 %2 %3 %4 %5 %6 %7 %8 (\"%9\")")
            .arg(buf[0]).arg(buf[1]).arg(buf[2]).arg(buf[3]).arg(buf[4])
            .arg(buf[5]).arg(buf[6]).arg(buf[7]).arg(m_vpsPrLabel.data()));

    int pcs = buf[2] >> 6;
    int cni = + ((buf[10] & 3) << 10)
        + ((buf[11] & 0xC0) << 2)
        + ((buf[8] & 0xC0) << 0)
        + (buf[11] & 0x3F);
    int pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);
    int pty = buf[12];

    LOG(VB_VBI, LOG_INFO, QString("CNI: %1 PCS: %2 PTY: %3 ")
            .arg(cni).arg(pcs).arg(pty));

    DumpPIL(pil);

    // c & 0xf0;
}

// // // // // // // // // // // // // // // // // // // // // // // //
// // // // // // // // // // //  WSS  // // // // // // // // // // //
// // // // // // // // // // // // // // // // // // // // // // // //

void CC608Decoder::DecodeWSS(const unsigned char *buf)
{
    static const std::array<const int,8> kWssBits { 0, 0, 0, 1, 0, 1, 1, 1 };
    uint wss = 0;

    for (uint i = 0; i < 16; i++)
    {
        uint b1 = kWssBits[buf[i] & 7];
        uint b2 = kWssBits[(buf[i] >> 3) & 7];

        if (b1 == b2)
            return;
        wss |= b2 << i;
    }
    unsigned char parity = wss & 0xf;
    parity ^= parity >> 2;
    parity ^= parity >> 1;

    LOG(VB_VBI, LOG_INFO,
            QString("WSS: %1; %2 mode; %3 color coding;\n\t\t\t"
                    "     %4 helper; reserved b7=%5; %6\n\t\t\t"
                    "      open subtitles: %7; %scopyright %8; copying %9")
            .arg(QString::fromStdString(formats[wss & 7]),
                 (wss & 0x0010) ? "film"                 : "camera",
                 (wss & 0x0020) ? "MA/CP"                : "standard",
                 (wss & 0x0040) ? "modulated"            : "no",
                 (wss & 0x0080) ? "1"                    : "0",
                 (wss & 0x0100) ? "have TTX subtitles; " : "",
                 QString::fromStdString(subtitles[(wss >> 9) & 3]),
                 (wss & 0x0800) ? "surround sound; "     : "",
                 (wss & 0x1000) ? "asserted"             : "unknown")
            // Qt < 5.14 only allows a max of nin string arguments in a single call
            .arg((wss & 0x2000) ? "restricted"           : "not restricted")); // clazy:exclude=qstring-arg

    if (parity & 1)
    {
        m_wssFlags = wss;
        m_wssValid = true;
    }
}

QString CC608Decoder::XDSDecodeString(const std::vector<unsigned char> &buf,
                                      uint start, uint end) const
{
#if DEBUG_XDS
    for (uint i = start; (i < buf.size()) && (i < end); i++)
    {
        LOG(VB_VBI, LOG_INFO, QString("%1: 0x%2 -> 0x%3 %4")
            .arg(i,2)
            .arg(buf[i],2,16,QChar('0'))
            .arg(CharCC(buf[i]).unicode(),2,16,QChar('0'))
            .arg(CharCC(buf[i])));
    }
#endif // DEBUG_XDS

    QString tmp = "";
    for (uint i = start; (i < buf.size()) && (i < end); i++)
    {
        if (buf[i] > 0x0)
            tmp += CharCC(buf[i]);
    }

#if DEBUG_XDS
    LOG(VB_VBI, LOG_INFO, QString("XDSDecodeString: '%1'").arg(tmp));
#endif // DEBUG_XDS

    return tmp.trimmed();
}

static bool is_better(const QString &newStr, const QString &oldStr)
{
    if (!newStr.isEmpty() && newStr != oldStr &&
        (newStr != oldStr.left(newStr.length())))
    {
        if (oldStr.isEmpty())
            return true;

        // check if the string contains any bogus characters
        return std::all_of(newStr.cbegin(), newStr.cend(),
                           [](auto ch){ return ch.toLatin1() >= 0x20; } );
    }
    return false;
}

uint CC608Decoder::GetRatingSystems(bool future) const
{
    QMutexLocker locker(&m_xdsLock);
    return m_xdsRatingSystems[(future) ? 1 : 0];
}

uint CC608Decoder::GetRating(uint i, bool future) const
{
    QMutexLocker locker(&m_xdsLock);
    return m_xdsRating[(future) ? 1 : 0][i & 0x3] & 0x7;
}

QString CC608Decoder::GetRatingString(uint i, bool future) const
{
    QMutexLocker locker(&m_xdsLock);

    const std::array<const QString,4> prefix { "MPAA-", "TV-", "CE-", "CF-" };
    const std::array<const std::array<const QString,8>,4> mainStr
    {{
        { "NR", "G", "PG",  "PG-13", "R",   "NC-17", "X",   "NR" },
        { "NR", "Y", "Y7",  "G",     "PG",  "14",    "MA",  "NR" },
        { "E",  "C", "C8+", "G",     "PG",  "14+",   "18+", "NR" },
        { "E",  "G", "8+",  "13+",   "16+", "18+",   "NR",  "NR" },
    }};

    QString main = prefix[i] + mainStr[i][GetRating(i, future)];

    if (kRatingTPG == i)
    {
        uint cf = (future) ? 1 : 0;
        if (!(m_xdsRating[cf][i]&0xF0))
            return main;

        main += " ";
        // TPG flags
        if (m_xdsRating[cf][i] & 0x80)
            main += "D"; // Dialog
        if (m_xdsRating[cf][i] & 0x40)
            main += "V"; // Violence
        if (m_xdsRating[cf][i] & 0x20)
            main += "S"; // Sex
        if (m_xdsRating[cf][i] & 0x10)
            main += "L"; // Language
    }

    return main;
}

QString CC608Decoder::GetProgramName(bool future) const
{
    QMutexLocker locker(&m_xdsLock);
    return m_xdsProgramName[(future) ? 1 : 0];
}

QString CC608Decoder::GetProgramType(bool future) const
{
    QMutexLocker locker(&m_xdsLock);
    const std::vector<uint> &program_type = m_xdsProgramType[(future) ? 1 : 0];
    QString tmp = "";

    for (size_t i = 0; i < program_type.size(); i++)
    {
        if (i != 0)
            tmp += ", ";
        tmp += m_xdsProgramTypeString[program_type[i]];
    }

    return tmp;
}

QString CC608Decoder::GetXDS(const QString &key) const
{
    QMutexLocker locker(&m_xdsLock);

    if (key == "ratings")
        return QString::number(GetRatingSystems(false));
    if (key.startsWith("has_rating_"))
        return ((1<<key.right(1).toUInt()) & GetRatingSystems(false))?"1":"0";
    if (key.startsWith("rating_"))
        return GetRatingString(key.right(1).toUInt(), false);

    if (key == "future_ratings")
        return QString::number(GetRatingSystems(true));
    if (key.startsWith("has_future_rating_"))
        return ((1<<key.right(1).toUInt()) & GetRatingSystems(true))?"1":"0";
    if (key.startsWith("future_rating_"))
        return GetRatingString(key.right(1).toUInt(), true);

    if (key == "programname")
        return GetProgramName(false);
    if (key == "future_programname")
        return GetProgramName(true);

    if (key == "programtype")
        return GetProgramType(false);
    if (key == "future_programtype")
        return GetProgramType(true);

    if (key == "callsign")
        return m_xdsNetCall;
    if (key == "channame")
        return m_xdsNetName;
    if (key == "tsid")
        return QString::number(m_xdsTsid);

    return {};
}

static std::array<const int,16> b1_to_service
{ -1, // 0x0
  0, 0, //0x1,0x2 -- Current
  1, 1, //0x3,0x4 -- Future
  2, 2, //0x5,0x6 -- Channel
  3, 3, //0x7,0x8 -- Misc
  4, 4, //0x9,0xA -- Public Service
  5, 5, //0xB,0xC -- Reserved
  6, 6, //0xD,0xE -- Private Data
  -1, // 0xF
};

bool CC608Decoder::XDSDecode([[maybe_unused]] int field, int b1, int b2)
{
    if (field == 0)
        return false; // XDS is only on second field

#if DEBUG_XDS
    LOG(VB_VBI, LOG_INFO,
        QString("XDSDecode: 0x%1 0x%2 '%3%4' xds[%5]=%6 in XDS %7")
        .arg(b1,2,16,QChar('0')).arg(b2,2,16,QChar('0'))
        .arg((CharCC(b1).unicode()>0x20) ? CharCC(b1) : QChar(' '))
        .arg((CharCC(b2).unicode()>0x20) ? CharCC(b2) : QChar(' '))
        .arg(field).arg(m_xds[field])
        .arg(m_xdsCurService));
#endif // DEBUG_XDS

    if (m_xdsCurService < 0)
    {
        if (b1 > 0x0f)
            return false;

        m_xdsCurService = b1_to_service[b1];

        if (m_xdsCurService < 0)
            return false;

        if (b1 & 1)
        {
            m_xdsBuf[m_xdsCurService].clear(); // if start of service clear buffer
#if DEBUG_XDS
            LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Starting XDS %1").arg(m_xdsCurService));
#endif // DEBUG_XDS
        }
    }
    else if ((0x0 < b1) && (b1 < 0x0f))
    { // switch to different service
        m_xdsCurService = b1_to_service[b1];
#if DEBUG_XDS
        LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Resuming XDS %1").arg(m_xdsCurService));
#endif // DEBUG_XDS
    }

    if (m_xdsCurService < 0)
        return false;

    m_xdsBuf[m_xdsCurService].push_back(b1);
    m_xdsBuf[m_xdsCurService].push_back(b2);

    if (b1 == 0x0f) // end of packet
    {
#if DEBUG_XDS
        LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Ending XDS %1").arg(m_xdsCurService));
#endif // DEBUG_XDS
        if (XDSPacketCRC(m_xdsBuf[m_xdsCurService]))
            XDSPacketParse(m_xdsBuf[m_xdsCurService]);
        m_xdsBuf[m_xdsCurService].clear();
        m_xdsCurService = -1;
    }
    else if ((0x10 <= b1) && (b1 <= 0x1f)) // suspension of XDS packet
    {
#if DEBUG_XDS
        LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Suspending XDS %1 on 0x%2")
            .arg(m_xdsCurService).arg(b1,2,16,QChar('0')));
#endif // DEBUG_XDS
        m_xdsCurService = -1;
    }

    return true;
}

void CC608Decoder::XDSPacketParse(const std::vector<unsigned char> &xds_buf)
{
    QMutexLocker locker(&m_xdsLock);

    bool handled   = false;
    int  xds_class = xds_buf[0];

    if (!xds_class)
        return;

    if ((xds_class == 0x01) || (xds_class == 0x03)) // cont code 2 and 4, resp.
        handled = XDSPacketParseProgram(xds_buf, (xds_class == 0x03));
    else if (xds_class == 0x05) // cont code: 0x06
        handled = XDSPacketParseChannel(xds_buf);
    else if ((xds_class == 0x07) || // cont code: 0x08 // misc.
             (xds_class == 0x09) || // cont code: 0x0a // public (aka weather)
             (xds_class == 0x0b))   // cont code: 0x0c // reserved
        ;
    else if (xds_class == 0x0d) // cont code: 0x0e
        handled = true; // undefined

    if (!handled)
    {
#if DEBUG_XDS
        LOG(VB_VBI, LOG_INFO, QString("XDS: ") +
            QString("Unhandled packet (0x%1 0x%2) sz(%3) '%4'")
            .arg(xds_buf[0],0,16).arg(xds_buf[1],0,16)
            .arg(xds_buf.size())
            .arg(XDSDecodeString(xds_buf, 2, xds_buf.size() - 2)));
#endif
    }
}

bool CC608Decoder::XDSPacketCRC(const std::vector<unsigned char> &xds_buf)
{
    /* Check the checksum for validity of the packet. */
    int sum = 0;
    for (size_t i = 0; i < xds_buf.size() - 1; i++)
        sum += xds_buf[i];

    if ((((~sum) & 0x7f) + 1) != xds_buf[xds_buf.size() - 1])
    {
        m_xdsCrcFailed++;

        LOG(VB_VBI, LOG_ERR, QString("XDS: failed CRC %1 of %2")
                .arg(m_xdsCrcFailed).arg(m_xdsCrcFailed + m_xdsCrcPassed));

        return false;
    }

    m_xdsCrcPassed++;
    return true;
}

bool CC608Decoder::XDSPacketParseProgram(
    const std::vector<unsigned char> &xds_buf, bool future)
{
    bool handled = true;
    int b2 = xds_buf[1];
    int cf = (future) ? 1 : 0;
    QString loc = (future) ? "XDS: Future " : "XDS: Current ";

    if ((b2 == 0x01) && (xds_buf.size() >= 6))
    {
        uint min   = xds_buf[2] & 0x3f;
        uint hour  = xds_buf[3] & 0x0f;
        uint day   = xds_buf[4] & 0x1f;
        uint month = xds_buf[5] & 0x0f;
        month = (month < 1 || month > 12) ? 0 : month;

        LOG(VB_VBI, LOG_INFO, loc +
                QString("Start Time %1/%2 %3:%4%5")
                .arg(month).arg(day).arg(hour).arg(min / 10).arg(min % 10));
    }
    else if ((b2 == 0x02) && (xds_buf.size() >= 4))
    {
        uint length_min  = xds_buf[2] & 0x3f;
        uint length_hour = xds_buf[3] & 0x3f;
        uint length_elapsed_min  = 0;
        uint length_elapsed_hour = 0;
        uint length_elapsed_secs = 0;
        if (xds_buf.size() > 6)
        {
            length_elapsed_min  = xds_buf[4] & 0x3f;
            length_elapsed_hour = xds_buf[5] & 0x3f;
        }
        if (xds_buf.size() > 8 && xds_buf[7] == 0x40)
            length_elapsed_secs = xds_buf[6] & 0x3f;

        QString msg = QString("Program Length %1:%2%3 "
                              "Time in Show %4:%5%6.%7%8")
            .arg(length_hour).arg(length_min / 10).arg(length_min % 10)
            .arg(length_elapsed_hour)
            .arg(length_elapsed_min / 10).arg(length_elapsed_min % 10)
            .arg(length_elapsed_secs / 10).arg(length_elapsed_secs % 10);

        LOG(VB_VBI, LOG_INFO, loc + msg);
    }
    else if ((b2 == 0x03) && (xds_buf.size() >= 6))
    {
        QString tmp = XDSDecodeString(xds_buf, 2, xds_buf.size() - 2);
        if (is_better(tmp, m_xdsProgramName[cf]))
        {
            m_xdsProgramName[cf] = tmp;
            LOG(VB_VBI, LOG_INFO, loc + QString("Program Name: '%1'")
                    .arg(GetProgramName(future)));
        }
    }
    else if ((b2 == 0x04) && (xds_buf.size() >= 6))
    {
        std::vector<uint> program_type;
        for (size_t i = 2; i < xds_buf.size() - 2; i++)
        {
            int cur = xds_buf[i] - 0x20;
            if (cur >= 0 && cur < 96)
                program_type.push_back(cur);
        }

        bool unchanged = m_xdsProgramType[cf].size() == program_type.size();
        for (uint i = 0; (i < program_type.size()) && unchanged; i++)
            unchanged = m_xdsProgramType[cf][i] == program_type[i];

        if (!unchanged)
        {
            m_xdsProgramType[cf] = program_type;
            LOG(VB_VBI, LOG_INFO, loc + QString("Program Type '%1'")
                    .arg(GetProgramType(future)));
        }
    }
    else if ((b2 == 0x05) && (xds_buf.size() >= 4))
    {
        uint movie_rating  = xds_buf[2] & 0x7;
        uint rating_system = (xds_buf[2] >> 3) & 0x7;
        uint tv_rating     = xds_buf[3] & 0x7;
        uint VSL           = xds_buf[3] & (0x7 << 3);
        uint sel           = VSL | rating_system;
        if (sel == 3)
        {
            if (!(kHasCanEnglish & m_xdsRatingSystems[cf]) ||
                (tv_rating != GetRating(kRatingCanEnglish, future)))
            {
                m_xdsRatingSystems[cf]             |= kHasCanEnglish;
                m_xdsRating[cf][kRatingCanEnglish]  = tv_rating;
                LOG(VB_VBI, LOG_INFO, loc + QString("VChip %1")
                        .arg(GetRatingString(kRatingCanEnglish, future)));
            }
        }
        else if (sel == 7)
        {
            if (!(kHasCanFrench & m_xdsRatingSystems[cf]) ||
                (tv_rating != GetRating(kRatingCanFrench, future)))
            {
                m_xdsRatingSystems[cf]            |= kHasCanFrench;
                m_xdsRating[cf][kRatingCanFrench]  = tv_rating;
                LOG(VB_VBI, LOG_INFO, loc + QString("VChip %1")
                        .arg(GetRatingString(kRatingCanFrench, future)));
            }
        }
        else if (sel == 0x13 || sel == 0x1f)
        {
            ; // Reserved according to TVTime code
        }
        else if ((rating_system & 0x3) == 1)
        {
            if (!(kHasTPG & m_xdsRatingSystems[cf]) ||
                (tv_rating != GetRating(kRatingTPG, future)))
            {
                uint f = ((xds_buf[0]<<3) & 0x80) | ((xds_buf[1]<<1) & 0x70);
                m_xdsRatingSystems[cf]      |= kHasTPG;
                m_xdsRating[cf][kRatingTPG]  = tv_rating | f;
                LOG(VB_VBI, LOG_INFO, loc + QString("VChip %1")
                        .arg(GetRatingString(kRatingTPG, future)));
            }
        }
        else if (rating_system == 0)
        {
            if (!(kHasMPAA & m_xdsRatingSystems[cf]) ||
                (movie_rating != GetRating(kRatingMPAA, future)))
            {
                m_xdsRatingSystems[cf]       |= kHasMPAA;
                m_xdsRating[cf][kRatingMPAA]  = movie_rating;
                LOG(VB_VBI, LOG_INFO, loc + QString("VChip %1")
                        .arg(GetRatingString(kRatingMPAA, future)));
            }
        }
        else
        {
            LOG(VB_VBI, LOG_ERR, loc +
                    QString("VChip Unhandled -- rs(%1) rating(%2:%3)")
                .arg(rating_system).arg(tv_rating).arg(movie_rating));
        }
    }
#if 0
    else if (b2 == 0x07)
        ; // caption
    else if (b2 == 0x08)
        ; // cgms
    else if (b2 == 0x09)
        ; // unknown
    else if (b2 == 0x0c)
        ; // misc
    else if (b2 == 0x10 || b2 == 0x13 || b2 == 0x15 || b2 == 0x16 ||
             b2 == 0x91 || b2 == 0x92 || b2 == 0x94 || b2 == 0x97)
        ; // program description
    else if (b2 == 0x86)
        ; // audio
    else if (b2 == 0x89)
        ; // aspect
    else if (b2 == 0x8c)
        ; // program data
#endif
    else
    {
        handled = false;
    }

    return handled;
}

bool CC608Decoder::XDSPacketParseChannel(const std::vector<unsigned char> &xds_buf)
{
    bool handled = true;

    int b2 = xds_buf[1];
    if ((b2 == 0x01) && (xds_buf.size() >= 6))
    {
        QString tmp = XDSDecodeString(xds_buf, 2, xds_buf.size() - 2);
        if (is_better(tmp, m_xdsNetName))
        {
            LOG(VB_VBI, LOG_INFO, QString("XDS: Network Name '%1'").arg(tmp));
            m_xdsNetName = tmp;
        }
    }
    else if ((b2 == 0x02) && (xds_buf.size() >= 6))
    {
        QString tmp = XDSDecodeString(xds_buf, 2, xds_buf.size() - 2);
        if (is_better(tmp, m_xdsNetCall) && (tmp.indexOf(" ") < 0))
        {
            LOG(VB_VBI, LOG_INFO, QString("XDS: Network Call '%1'").arg(tmp));
            m_xdsNetCall = tmp;
        }
    }
    else if ((b2 == 0x04) && (xds_buf.size() >= 6))
    {
        uint tsid = (xds_buf[2] << 24 | xds_buf[3] << 16 |
                     xds_buf[4] <<  8 | xds_buf[5]);
        if (tsid != m_xdsTsid)
        {
            LOG(VB_VBI, LOG_INFO, QString("XDS: TSID 0x%1").arg(tsid,0,16));
            m_xdsTsid = tsid;
        }
    }
    else
    {
        handled = false;
    }

    return handled;
}

static void init_xds_program_type(CC608ProgramType& xds_program_type)
{
    xds_program_type[0]  = QCoreApplication::translate("(Categories)",
                                                       "Education");
    xds_program_type[1]  = QCoreApplication::translate("(Categories)",
                                                       "Entertainment");
    xds_program_type[2]  = QCoreApplication::translate("(Categories)",
                                                       "Movie");
    xds_program_type[3]  = QCoreApplication::translate("(Categories)",
                                                       "News");
    xds_program_type[4]  = QCoreApplication::translate("(Categories)",
                                                       "Religious");
    xds_program_type[5]  = QCoreApplication::translate("(Categories)",
                                                       "Sports");
    xds_program_type[6]  = QCoreApplication::translate("(Categories)",
                                                       "Other");
    xds_program_type[7]  = QCoreApplication::translate("(Categories)",
                                                       "Action");
    xds_program_type[8]  = QCoreApplication::translate("(Categories)",
                                                       "Advertisement");
    xds_program_type[9]  = QCoreApplication::translate("(Categories)",
                                                       "Animated");
    xds_program_type[10] = QCoreApplication::translate("(Categories)",
                                                       "Anthology");
    xds_program_type[11] = QCoreApplication::translate("(Categories)",
                                                       "Automobile");
    xds_program_type[12] = QCoreApplication::translate("(Categories)",
                                                       "Awards");
    xds_program_type[13] = QCoreApplication::translate("(Categories)",
                                                       "Baseball");
    xds_program_type[14] = QCoreApplication::translate("(Categories)",
                                                       "Basketball");
    xds_program_type[15] = QCoreApplication::translate("(Categories)",
                                                       "Bulletin");
    xds_program_type[16] = QCoreApplication::translate("(Categories)",
                                                       "Business");
    xds_program_type[17] = QCoreApplication::translate("(Categories)",
                                                       "Classical");
    xds_program_type[18] = QCoreApplication::translate("(Categories)",
                                                       "College");
    xds_program_type[19] = QCoreApplication::translate("(Categories)",
                                                       "Combat");
    xds_program_type[20] = QCoreApplication::translate("(Categories)",
                                                       "Comedy");
    xds_program_type[21] = QCoreApplication::translate("(Categories)",
                                                       "Commentary");
    xds_program_type[22] = QCoreApplication::translate("(Categories)",
                                                       "Concert");
    xds_program_type[23] = QCoreApplication::translate("(Categories)",
                                                       "Consumer");
    xds_program_type[24] = QCoreApplication::translate("(Categories)",
                                                       "Contemporary");
    xds_program_type[25] = QCoreApplication::translate("(Categories)",
                                                       "Crime");
    xds_program_type[26] = QCoreApplication::translate("(Categories)",
                                                       "Dance");
    xds_program_type[27] = QCoreApplication::translate("(Categories)",
                                                       "Documentary");
    xds_program_type[28] = QCoreApplication::translate("(Categories)",
                                                       "Drama");
    xds_program_type[29] = QCoreApplication::translate("(Categories)",
                                                       "Elementary");
    xds_program_type[30] = QCoreApplication::translate("(Categories)",
                                                       "Erotica");
    xds_program_type[31] = QCoreApplication::translate("(Categories)",
                                                       "Exercise");
    xds_program_type[32] = QCoreApplication::translate("(Categories)",
                                                       "Fantasy");
    xds_program_type[33] = QCoreApplication::translate("(Categories)",
                                                       "Farm");
    xds_program_type[34] = QCoreApplication::translate("(Categories)",
                                                       "Fashion");
    xds_program_type[35] = QCoreApplication::translate("(Categories)",
                                                       "Fiction");
    xds_program_type[36] = QCoreApplication::translate("(Categories)",
                                                       "Food");
    xds_program_type[37] = QCoreApplication::translate("(Categories)",
                                                       "Football");
    xds_program_type[38] = QCoreApplication::translate("(Categories)",
                                                       "Foreign");
    xds_program_type[39] = QCoreApplication::translate("(Categories)",
                                                       "Fundraiser");
    xds_program_type[40] = QCoreApplication::translate("(Categories)",
                                                       "Game/Quiz");
    xds_program_type[41] = QCoreApplication::translate("(Categories)",
                                                       "Garden");
    xds_program_type[42] = QCoreApplication::translate("(Categories)",
                                                       "Golf");
    xds_program_type[43] = QCoreApplication::translate("(Categories)",
                                                       "Government");
    xds_program_type[44] = QCoreApplication::translate("(Categories)",
                                                       "Health");
    xds_program_type[45] = QCoreApplication::translate("(Categories)",
                                                       "High School");
    xds_program_type[46] = QCoreApplication::translate("(Categories)",
                                                       "History");
    xds_program_type[47] = QCoreApplication::translate("(Categories)",
                                                       "Hobby");
    xds_program_type[48] = QCoreApplication::translate("(Categories)",
                                                       "Hockey");
    xds_program_type[49] = QCoreApplication::translate("(Categories)",
                                                       "Home");
    xds_program_type[50] = QCoreApplication::translate("(Categories)",
                                                       "Horror");
    xds_program_type[51] = QCoreApplication::translate("(Categories)",
                                                       "Information");
    xds_program_type[52] = QCoreApplication::translate("(Categories)",
                                                       "Instruction");
    xds_program_type[53] = QCoreApplication::translate("(Categories)",
                                                       "International");
    xds_program_type[54] = QCoreApplication::translate("(Categories)",
                                                       "Interview");
    xds_program_type[55] = QCoreApplication::translate("(Categories)",
                                                       "Language");
    xds_program_type[56] = QCoreApplication::translate("(Categories)",
                                                       "Legal");
    xds_program_type[57] = QCoreApplication::translate("(Categories)",
                                                       "Live");
    xds_program_type[58] = QCoreApplication::translate("(Categories)",
                                                       "Local");
    xds_program_type[59] = QCoreApplication::translate("(Categories)",
                                                       "Math");
    xds_program_type[60] = QCoreApplication::translate("(Categories)",
                                                       "Medical");
    xds_program_type[61] = QCoreApplication::translate("(Categories)",
                                                       "Meeting");
    xds_program_type[62] = QCoreApplication::translate("(Categories)",
                                                       "Military");
    xds_program_type[63] = QCoreApplication::translate("(Categories)",
                                                       "Miniseries");
    xds_program_type[64] = QCoreApplication::translate("(Categories)",
                                                       "Music");
    xds_program_type[65] = QCoreApplication::translate("(Categories)",
                                                       "Mystery");
    xds_program_type[66] = QCoreApplication::translate("(Categories)",
                                                       "National");
    xds_program_type[67] = QCoreApplication::translate("(Categories)",
                                                       "Nature");
    xds_program_type[68] = QCoreApplication::translate("(Categories)",
                                                       "Police");
    xds_program_type[69] = QCoreApplication::translate("(Categories)",
                                                       "Politics");
    xds_program_type[70] = QCoreApplication::translate("(Categories)",
                                                       "Premiere");
    xds_program_type[71] = QCoreApplication::translate("(Categories)",
                                                       "Prerecorded");
    xds_program_type[72] = QCoreApplication::translate("(Categories)",
                                                       "Product");
    xds_program_type[73] = QCoreApplication::translate("(Categories)",
                                                       "Professional");
    xds_program_type[74] = QCoreApplication::translate("(Categories)",
                                                       "Public");
    xds_program_type[75] = QCoreApplication::translate("(Categories)",
                                                       "Racing");
    xds_program_type[76] = QCoreApplication::translate("(Categories)",
                                                       "Reading");
    xds_program_type[77] = QCoreApplication::translate("(Categories)",
                                                       "Repair");
    xds_program_type[78] = QCoreApplication::translate("(Categories)",
                                                       "Repeat");
    xds_program_type[79] = QCoreApplication::translate("(Categories)",
                                                       "Review");
    xds_program_type[80] = QCoreApplication::translate("(Categories)",
                                                       "Romance");
    xds_program_type[81] = QCoreApplication::translate("(Categories)",
                                                       "Science");
    xds_program_type[82] = QCoreApplication::translate("(Categories)",
                                                       "Series");
    xds_program_type[83] = QCoreApplication::translate("(Categories)",
                                                       "Service");
    xds_program_type[84] = QCoreApplication::translate("(Categories)",
                                                       "Shopping");
    xds_program_type[85] = QCoreApplication::translate("(Categories)",
                                                       "Soap Opera");
    xds_program_type[86] = QCoreApplication::translate("(Categories)",
                                                       "Special");
    xds_program_type[87] = QCoreApplication::translate("(Categories)",
                                                       "Suspense");
    xds_program_type[88] = QCoreApplication::translate("(Categories)",
                                                       "Talk");
    xds_program_type[89] = QCoreApplication::translate("(Categories)",
                                                       "Technical");
    xds_program_type[90] = QCoreApplication::translate("(Categories)",
                                                       "Tennis");
    xds_program_type[91] = QCoreApplication::translate("(Categories)",
                                                       "Travel");
    xds_program_type[92] = QCoreApplication::translate("(Categories)",
                                                       "Variety");
    xds_program_type[93] = QCoreApplication::translate("(Categories)",
                                                       "Video");
    xds_program_type[94] = QCoreApplication::translate("(Categories)",
                                                       "Weather");
    xds_program_type[95] = QCoreApplication::translate("(Categories)",
                                                       "Western");
}
