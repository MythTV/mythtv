// -*- Mode: c++ -*-

// Some of the XDS was inspired by code in TVTime. -- dtk 03/30/2006

#include <algorithm>
using namespace std;

// Qt headers
#include <QStringList>
#include <QCoreApplication>

// MythTV headers
#include "format.h"
#include "cc608decoder.h"
#include "mythcontext.h"
#include "mythlogging.h"
#include "vbilut.h"

#define DEBUG_XDS 0

static void init_xds_program_type(QString xds_program_type[96]);

CC608Decoder::CC608Decoder(CC608Input *ccr)
    : reader(ccr),                  ignore_time_code(false),
      rbuf(new unsigned char[sizeof(ccsubtitle)+255]),
      vps_l(0),
      wss_flags(0),                 wss_valid(false),
      xds_cur_service(-1),
      xds_crc_passed(0),            xds_crc_failed(0),
      xds_lock(QMutex::Recursive),
      xds_net_call(QString::null),  xds_net_name(QString::null),
      xds_tsid(0)
{
    for (uint i = 0; i < 2; i++)
    {
        badvbi[i]      =  0;
        lasttc[i]      =  0;
        lastcode[i]    = -1;
        lastcodetc[i]  =  0;
        ccmode[i]      = -1;
        xds[i]         =  0;
        txtmode[i*2+0] =  0;
        txtmode[i*2+1] =  0;
        last_format_tc[i]   = 0;
        last_format_data[i] = 0;
    }

    // The following are not bzero() because MS Windows doesn't like it.
    memset(lastrow,    0, sizeof(lastrow));
    memset(newrow,     0, sizeof(newrow));
    memset(newcol,     0, sizeof(newcol));
    memset(newattr,    0, sizeof(newattr));
    memset(timecode,   0, sizeof(timecode));
    memset(row,        0, sizeof(row));
    memset(col,        0, sizeof(col));
    memset(rowcount,   0, sizeof(rowcount));
    memset(style,      0, sizeof(style));
    memset(linecont,   0, sizeof(linecont));
    memset(resumetext, 0, sizeof(resumetext));
    memset(lastclr,    0, sizeof(lastclr));

    for (uint i = 0; i < 8; i++)
        ccbuf[i] = "";

    // fill translation table
    for (uint i = 0; i < 128; i++)
        stdchar[i] = QChar(i);
    stdchar[42]  = 'á';
    stdchar[92]  = 'é';
    stdchar[94]  = 'í';
    stdchar[95]  = 'ó';
    stdchar[96]  = 'ú';
    stdchar[123] = 'ç';
    stdchar[124] = '÷';
    stdchar[125] = 'Ñ';
    stdchar[126] = 'ñ';
    stdchar[127] = 0x2588; /* full block */

    // VPS data (MS Windows doesn't like bzero())
    memset(vps_pr_label, 0, sizeof(vps_pr_label));
    memset(vps_label,    0, sizeof(vps_label));

    // XDS data
    memset(xds_rating, 0, sizeof(uint) * 2 * 4);
    for (uint i = 0; i < 2; i++)
    {
        xds_rating_systems[i] = 0;
        xds_program_name[i]   = QString::null;
    }

    init_xds_program_type(xds_program_type_string);
}

CC608Decoder::~CC608Decoder(void)
{
    if (rbuf)
        delete [] rbuf;
}

void CC608Decoder::FormatCC(int tc, int code1, int code2)
{
    FormatCCField(tc, 0, code1);
    FormatCCField(tc, 1, code2);
}

void CC608Decoder::GetServices(uint seconds, bool seen[4]) const
{
    time_t now = time(NULL);
    time_t then = now - seconds;
    for (uint i = 0; i < 4; i++)
        seen[i] = (last_seen[i] >= then);
}

static const int rowdata[] =
{
    11, -1, 1, 2, 3, 4, 12, 13,
    14, 15, 5, 6, 7, 8,  9, 10
};

static const QChar specialchar[] =
{
    '®', '°', '½', '¿', 0x2122 /* TM */, '¢', '£', 0x266A /* 1/8 note */,
    'à', ' ', 'è', 'â', 'ê', 'î', 'ô', 'û'
};

static const QChar extendedchar2[] =
{
    'Á', 'É', 'Ó', 'Ú', 'Ü', 'ü', '`', '¡',
    '*', '\'', 0x2014 /* dash */, '©',
    0x2120 /* SM */, '·', 0x201C, 0x201D /* double quotes */,
    'À', 'Â', 'Ç', 'È', 'Ê', 'Ë', 'ë', 'Î',
    'Ï', 'ï', 'Ô', 'Ù', 'ù', 'Û', '«', '»'
};

static const QChar extendedchar3[] =
{
    'Ã', 'ã', 'Í', 'Ì', 'ì', 'Ò', 'ò', 'Õ',
    'õ', '{', '}', '\\', '^', '_', '¦', '~',
    'Ä', 'ä', 'Ö', 'ö', 'ß', '¥', '¤', '|',
    'Å', 'å', 'Ø', 'ø', 0x250C, 0x2510, 0x2514, 0x2518 /* box drawing */
};

void CC608Decoder::FormatCCField(int tc, int field, int data)
{
    int b1, b2, len, x;
    int mode;

    if (data == -1)              // invalid data. flush buffers to be safe.
    {
        // TODO:  flush reader buffer
        if (ccmode[field] != -1)
        {
            for (mode = field*4; mode < (field*4 + 4); mode++)
                ResetCC(mode);
            xds[field] = 0;
            badvbi[field] = 0;
            ccmode[field] = -1;
            txtmode[field*2] = 0;
            txtmode[field*2 + 1] = 0;
        }
        return;
    }

    if ((last_format_data[field&1] == data) &&
        (last_format_tc[field&1] == tc))
    {
        LOG(VB_VBI, LOG_DEBUG, "Format CC -- Duplicate");
        return;
    }

    last_format_tc[field&1] = tc;
    last_format_data[field&1] = data;

    b1 = data & 0x7f;
    b2 = (data >> 8) & 0x7f;
#if 1
    LOG(VB_VBI, LOG_DEBUG, QString("Format CC @%1/%2 = %3 %4")
                    .arg(tc).arg(field)
                    .arg((data&0xff), 2, 16)
                    .arg((data&0xff00)>>8, 2, 16));
#endif
    if (ccmode[field] >= 0)
    {
        mode = field << 2 |
            (txtmode[field*2 + ccmode[field]] << 1) |
            ccmode[field];
        len = ccbuf[mode].length();
    }
    else
    {
        mode = -1;
        len = 0;
    }

    if (FalseDup(tc, field, data))
    {
        if (ignore_time_code)
            return;
        else
           goto skip;
    }

    if (XDSDecode(field, b1, b2))
        return;

    if (b1 & 0x60)
        // 0x20 <= b1 <= 0x7F
        // text codes
    {
        if (mode >= 0)
        {
            lastcodetc[field] += 33;
            timecode[mode] = tc;

            // commit row number only when first text code
            // comes in
            if (newrow[mode])
                len = NewRowCC(mode, len);

            ccbuf[mode] += CharCC(b1);
            len++;
            col[mode]++;
            if (b2 & 0x60)
            {
                ccbuf[mode] += CharCC(b2);
                len++;
                col[mode]++;
            }
        }
    }

    else if ((b1 & 0x10) && (b2 > 0x1F))
        // 0x10 <= b1 <= 0x1F
        // control codes
    {
        lastcodetc[field] += 67;

        int newccmode = (b1 >> 3) & 1;
        int newtxtmode = txtmode[field*2 + newccmode];
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
        ccmode[field] = newccmode;
        txtmode[field*2 + newccmode] = newtxtmode;
        mode = (field << 2) | (newtxtmode << 1) | ccmode[field];

        timecode[mode] = tc;
        len = ccbuf[mode].length();

        if (b2 & 0x40)           //preamble address code (row & indent)
        {
            if (newtxtmode)
                // no address codes in TXT mode?
                goto skip;

            newrow[mode] = rowdata[((b1 << 1) & 14) | ((b2 >> 5) & 1)];
            if (newrow[mode] == -1)
                // bogus code?
                newrow[mode] = lastrow[mode] + 1;

            if (b2 & 0x10)        //row contains indent flag
            {
                newcol[mode] = (b2 & 0x0E) << 1;
                // Encode as 0x7020 or 0x7021 depending on the
                // underline flag.
                newattr[mode] = (b2 & 0x1) + 0x20;
                LOG(VB_VBI, LOG_INFO,
                        QString("cc608 preamble indent, b2=%1")
                        .arg(b2, 2, 16));
            }
            else
            {
                newcol[mode] = 0;
                newattr[mode] = (b2 & 0xf) + 0x10;
                // Encode as 0x7010 through 0x702f for the 16 possible
                // values of b2.
                LOG(VB_VBI, LOG_INFO,
                        QString("cc608 preamble color change, b2=%1")
                        .arg(b2, 2, 16));
            }

            // row, indent, attribute settings are not final
            // until text code arrives
        }
        else
        {
            switch (b1 & 0x07)
            {
                case 0x00:          //attribute
#if 0
                    LOG(VB_VBI, LOG_DEBUG, 
                        QString("<ATTRIBUTE %1 %2>").arg(b1).arg(b2));
#endif
                    break;
                case 0x01:          //midrow or char
                    if (newrow[mode])
                        len = NewRowCC(mode, len);

                    switch (b2 & 0x70)
                    {
                        case 0x20:      //midrow attribute change
                            LOG(VB_VBI, LOG_INFO,
                                    QString("cc608 mid-row color change, b2=%1")
                                    .arg(b2, 2, 16));
                            // Encode as 0x7000 through 0x700f for the
                            // 16 possible values of b2.
                            ccbuf[mode] += QChar(0x7000 + (b2 & 0xf));
                            len = ccbuf[mode].length();
                            col[mode]++;
                            break;
                        case 0x30:      //special character..
                            ccbuf[mode] += specialchar[b2 & 0x0f];
                            len++;
                            col[mode]++;
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
                        ccbuf[mode].remove(len - 1, 1);
                        ccbuf[mode] += extendedchar2[b2 - 0x20];
                        len = ccbuf[mode].length();
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
                        ccbuf[mode].remove(len - 1, 1);
                        ccbuf[mode] += extendedchar3[b2 - 0x20];
                        len = ccbuf[mode].length();
                        break;
                    }
                    break;
                case 0x04:          //misc
                case 0x05:          //misc + F
#if 0
                    LOG(VB_VBI, LOG_DEBUG,
                        QString("ccmode %1 cmd %2").arg(ccmode)
                            .arg(b2, 2, 16, '0'));
#endif
                    switch (b2)
                    {
                        case 0x21:      //backspace
                            // add backspace if line has been encoded already
                            if (newrow[mode])
                                len = NewRowCC(mode, len);

                            if (len == 0 ||
                                ccbuf[mode].left(1) == "\b")
                            {
                                ccbuf[mode] += (char)'\b';
                                len++;
                                col[mode]--;
                            }
                            else
                            {
                                ccbuf[mode].remove(len - 1, 1);
                                len = ccbuf[mode].length();
                                col[mode]--;
                            }
                            break;
                        case 0x25:      //2 row caption
                        case 0x26:      //3 row caption
                        case 0x27:      //4 row caption
                            if (style[mode] == CC_STYLE_PAINT && len)
                            {
                                // flush
                                BufferCC(mode, len, 0);
                                ccbuf[mode] = "";
                                row[mode] = 0;
                                col[mode] = 0;
                            }
                            else if (style[mode] == CC_STYLE_POPUP)
                                ResetCC(mode);

                            rowcount[mode] = b2 - 0x25 + 2;
                            style[mode] = CC_STYLE_ROLLUP;
                            break;
                        case 0x2D:      //carriage return
                            if (style[mode] != CC_STYLE_ROLLUP)
                                break;

                            if (newrow[mode])
                                row[mode] = newrow[mode];

                            // flush if there is text or need to scroll
                            // TODO:  decode ITV (WebTV) link in TXT2
                            if (len || (row[mode] != 0 && !linecont[mode] &&
                                        (!newtxtmode || row[mode] >= 16)))
                            {
                                BufferCC(mode, len, 0);
                            }

                            if (newtxtmode)
                            {
                                if (row[mode] < 16)
                                    newrow[mode] = row[mode] + 1;
                                else
                                    // scroll up previous lines
                                    newrow[mode] = 16;
                            }

                            ccbuf[mode] = "";
                            col[mode] = 0;
                            linecont[mode] = 0;
                            break;

                        case 0x29:
                            // resume direct caption (paint-on style)
                            if (style[mode] == CC_STYLE_ROLLUP && len)
                            {
                                // flush
                                BufferCC(mode, len, 0);
                                ccbuf[mode] = "";
                                row[mode] = 0;
                                col[mode] = 0;
                            }
                            else if (style[mode] == CC_STYLE_POPUP)
                                ResetCC(mode);

                            style[mode] = CC_STYLE_PAINT;
                            rowcount[mode] = 0;
                            linecont[mode] = 0;
                            break;

                        case 0x2B:      //resume text display
                            resumetext[mode] = 1;
                            if (row[mode] == 0)
                            {
                                newrow[mode] = 1;
                                newcol[mode] = 0;
                                newattr[mode] = 0;
                            }
                            style[mode] = CC_STYLE_ROLLUP;
                            break;
                        case 0x2C:      //erase displayed memory
                            if (ignore_time_code ||
                                (tc - lastclr[mode]) > 5000 ||
                                lastclr[mode] == 0)
                                // don't overflow the frontend with
                                // too many redundant erase codes
                                BufferCC(mode, 0, 1);
                            if (style[mode] != CC_STYLE_POPUP)
                            {
                                row[mode] = 0;
                                col[mode] = 0;
                            }
                            linecont[mode] = 0;
                            break;

                        case 0x20:      //resume caption (pop-up style)
                            if (style[mode] != CC_STYLE_POPUP)
                            {
                                if (len)
                                    // flush
                                    BufferCC(mode, len, 0);
                                ccbuf[mode] = "";
                                row[mode] = 0;
                                col[mode] = 0;
                            }
                            style[mode] = CC_STYLE_POPUP;
                            rowcount[mode] = 0;
                            linecont[mode] = 0;
                            break;
                        case 0x2F:      //end caption + swap memory
                            if (style[mode] != CC_STYLE_POPUP)
                            {
                                if (len)
                                    // flush
                                    BufferCC(mode, len, 0);
                            }
                            else if (ignore_time_code ||
                                     (tc - lastclr[mode]) > 5000 ||
                                     lastclr[mode] == 0)
                                // clear and flush
                                BufferCC(mode, len, 1);
                            else if (len)
                                // flush
                                BufferCC(mode, len, 0);
                            ccbuf[mode] = "";
                            row[mode] = 0;
                            col[mode] = 0;
                            style[mode] = CC_STYLE_POPUP;
                            rowcount[mode] = 0;
                            linecont[mode] = 0;
                            break;

                        case 0x2A:      //text restart
                            // clear display
                            BufferCC(mode, 0, 1);
                            ResetCC(mode);
                            // TXT starts at row 1
                            newrow[mode] = 1;
                            newcol[mode] = 0;
                            newattr[mode] = 0;
                            style[mode] = CC_STYLE_ROLLUP;
                            break;

                        case 0x2E:      //erase non-displayed memory
                            ResetCC(mode);
                            break;
                    }
                    break;
                case 0x07:          //misc (TAB)
                    if (newrow[mode])
                    {
                        newcol[mode] += (b2 & 0x03);
                        len = NewRowCC(mode, len);
                    }
                    else
                        // illegal?
                        for (x = 0; x < (b2 & 0x03); x++)
                        {
                            ccbuf[mode] += ' ';
                            len++;
                            col[mode]++;
                        }
                    break;
            }
        }
    }

  skip:
    for (mode = field*4; mode < (field*4 + 4); mode++)
    {
        len = ccbuf[mode].length();
        if ((ignore_time_code || ((tc - timecode[mode]) > 100)) &&
             (style[mode] != CC_STYLE_POPUP) && len)
        {
            // flush unfinished line if waiting too long
            // in paint-on or scroll-up mode
            timecode[mode] = tc;
            BufferCC(mode, len, 0);
            ccbuf[mode] = "";
            row[mode] = lastrow[mode];
            linecont[mode] = 1;
        }
    }

    if (data != lastcode[field])
    {
        lastcode[field] = data;
        lastcodetc[field] = tc;
    }
    lasttc[field] = tc;
}

int CC608Decoder::FalseDup(int tc, int field, int data)
{
    int b1, b2;

    b1 = data & 0x7f;
    b2 = (data >> 8) & 0x7f;

    if (ignore_time_code)
    {
        // most digital streams with encoded VBI
        // have duplicate control codes;
        // suppress every other repeated control code
        if ((data == lastcode[field]) &&
            ((b1 & 0x70) == 0x10))
        {
            lastcode[field] = -1;
            return 1;
        }
        else
        {
            return 0;
        }
    }

    // bttv-0.9 VBI reads are pretty reliable (1 read/33367us).
    // bttv-0.7 reads don't seem to work as well so if read intervals
    // vary from this, be more conservative in detecting duplicate
    // CC codes.
    int dup_text_fudge, dup_ctrl_fudge;
    if (badvbi[field] < 100 && b1 != 0 && b2 != 0)
    {
        int d = tc - lasttc[field];
        if (d < 25 || d > 42)
            badvbi[field]++;
        else if (badvbi[field] > 0)
            badvbi[field]--;
    }
    if (badvbi[field] < 4)
    {
        // this should pick up all codes
        dup_text_fudge = -2;
        // this should pick up 1st, 4th, 6th, 8th, ... codes
        dup_ctrl_fudge = 33 - 4;
    }
    else
    {
        dup_text_fudge = 4;
        dup_ctrl_fudge = 33 - 4;
    }

    if (data == lastcode[field])
    {
        if ((b1 & 0x70) == 0x10)
        {
            if (tc > (lastcodetc[field] + 67 + dup_ctrl_fudge))
                return 0;
        }
        else if (b1)
        {
            // text, XDS
            if (tc > (lastcodetc[field] + 33 + dup_text_fudge))
                return 0;
        }

        return 1;
    }

    return 0;
}

void CC608Decoder::ResetCC(int mode)
{
//    lastrow[mode] = 0;
//    newrow[mode] = 0;
//    newcol[mode] = 0;
//    timecode[mode] = 0;
    row[mode] = 0;
    col[mode] = 0;
    rowcount[mode] = 0;
//    style[mode] = CC_STYLE_POPUP;
    linecont[mode] = 0;
    resumetext[mode] = 0;
    lastclr[mode] = 0;
    ccbuf[mode] = "";
}

QString CC608Decoder::ToASCII(const QString &cc608str, bool suppress_unknown)
{
    QString ret = "";

    for (int i = 0; i < cc608str.length(); i++)
    {
        QChar cp = cc608str[i];
        int cpu = cp.unicode();
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
                        ret += QString("[%1]").arg(cpu - 0x7000, 2, 16);
                }
                else if (cpu >= 0x20 && cpu <= 0x80)
                    ret += QString(cp.toLatin1());
                if (!suppress_unknown)
                    ret += QString("[%1]").arg(cpu - 0x7000, 2, 16);
        }
    }

    return ret;
}

void CC608Decoder::BufferCC(int mode, int len, int clr)
{
    QByteArray tmpbuf;
    if (len)
    {
        // calculate UTF-8 encoding length
        tmpbuf = ccbuf[mode].toUtf8();
        len = min(tmpbuf.length(), 255);
    }

    unsigned char f;
    unsigned char *bp = rbuf;
    *(bp++) = row[mode];
    *(bp++) = rowcount[mode];
    *(bp++) = style[mode];
    // overload resumetext field
    f = resumetext[mode];
    f |= mode << 4;
    if (linecont[mode])
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
        len = sizeof(ccsubtitle);

    if (len && VERBOSE_LEVEL_CHECK(VB_VBI, LOG_INFO))
    {
        LOG(VB_VBI, LOG_INFO, QString("### %1 %2 %3 %4 %5 %6 %7 - '%8'")
            .arg(timecode[mode], 10)
            .arg(row[mode], 2).arg(rowcount[mode])
            .arg(style[mode]).arg(f, 2, 16)
            .arg(clr).arg(len, 3)
            .arg(ToASCII(QString::fromUtf8(tmpbuf.constData(), len), false)));
    }

    reader->AddTextData(rbuf, len, timecode[mode], 'C');
    int ccmode = rbuf[3] & CC_MODE_MASK;
    int stream = -1;
    switch (ccmode)
    {
        case CC_CC1: stream = 0; break;
        case CC_CC2: stream = 1; break;
        case CC_CC3: stream = 2; break;
        case CC_CC4: stream = 3; break;
    }
    if (stream >= 0)
        last_seen[stream] = time(NULL);

    resumetext[mode] = 0;
    if (clr && !len)
        lastclr[mode] = timecode[mode];
    else if (len)
        lastclr[mode] = 0;
}

int CC608Decoder::NewRowCC(int mode, int len)
{
    if (style[mode] == CC_STYLE_ROLLUP)
    {
        // previous line was likely missing a carriage return
        row[mode] = newrow[mode];
        if (len)
        {
            BufferCC(mode, len, 0);
            ccbuf[mode] = "";
            len = 0;
        }
        col[mode] = 0;
        linecont[mode] = 0;
    }
    else
    {
        // popup/paint style

        if (row[mode] == 0)
        {
            if (len == 0)
                row[mode] = newrow[mode];
            else
            {
                // previous line was missing a row address
                // - assume it was one row up
                ccbuf[mode] += (char)'\n';
                len++;
                if (row[mode] == 0)
                    row[mode] = newrow[mode] - 1;
                else
                    row[mode]--;
            }
        }
        else if (newrow[mode] > lastrow[mode])
        {
            // next line can be more than one row away
            for (int i = 0; i < (newrow[mode] - lastrow[mode]); i++)
            {
                ccbuf[mode] += (char)'\n';
                len++;
            }
            col[mode] = 0;
        }
        else if (newrow[mode] == lastrow[mode])
        {
            // same row
            if (newcol[mode] >= col[mode])
                // new line appends to current line
                newcol[mode] -= col[mode];
            else
            {
                // new line overwrites current line;
                // could be legal (overwrite spaces?) but
                // more likely we have bad address codes
                // - just move to next line; may exceed row 15
                // but frontend will adjust
                ccbuf[mode] += (char)'\n';
                len++;
                col[mode] = 0;
            }
        }
        else
        {
            // next line goes upwards (not legal?)
            // - flush
            BufferCC(mode, len, 0);
            ccbuf[mode] = "";
            row[mode] = newrow[mode];
            col[mode] = 0;
            linecont[mode] = 0;
            len = 0;
        }
    }

    lastrow[mode] = newrow[mode];
    newrow[mode] = 0;

    int limit = (newattr[mode] ? newcol[mode] - 1 : newcol[mode]);
    for (int x = 0; x < limit; x++)
    {
        ccbuf[mode] += ' ';
        len++;
        col[mode]++;
    }

    if (newattr[mode])
    {
        ccbuf[mode] += QChar(newattr[mode] + 0x7000);
        len++;
        col[mode]++;
    }

    newcol[mode] = 0;
    newattr[mode] = 0;

    return len;
}


static bool IsPrintable(char c)
{
    return !(((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E);
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

static void DumpPIL(int pil)
{
    int day  = (pil >> 15);
    int mon  = (pil >> 11) & 0xF;
    int hour = (pil >> 6 ) & 0x1F;
    int min  = (pil      ) & 0x3F;

#define _PIL_(day, mon, hour, min) \
  (((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

    if (pil == _PIL_(0, 15, 31, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Timer-control (no PDC)");
    else if (pil == _PIL_(0, 15, 30, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Recording inhibit/terminate");
    else if (pil == _PIL_(0, 15, 29, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Interruption");
    else if (pil == _PIL_(0, 15, 28, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: Continue");
    else if (pil == _PIL_(31, 15, 31, 63))
        LOG(VB_VBI, LOG_INFO, " PDC: No time");
    else
        LOG(VB_VBI, LOG_INFO, QString(" PDC: %1, 200X-%2-%3 %4:%5")
                .arg(pil).arg(mon).arg(day).arg(hour).arg(min));
#undef _PIL_
}

void CC608Decoder::DecodeVPS(const unsigned char *buf)
{
    int cni, pcs, pty, pil;

    int c = vbi_bit_reverse[buf[1]];

    if ((int8_t) c < 0)
    {
        vps_label[vps_l] = 0;
        memcpy(vps_pr_label, vps_label, sizeof(vps_pr_label));
        vps_l = 0;
    }
    c &= 0x7F;
    vps_label[vps_l] = Printable(c);
    vps_l = (vps_l + 1) % 16;

    LOG(VB_VBI, LOG_INFO, QString("VPS: 3-10: %1 %2 %3 %4 %5 %6 %7 %8 (\"%9\")")
            .arg(buf[0]).arg(buf[1]).arg(buf[2]).arg(buf[3]).arg(buf[4])
            .arg(buf[5]).arg(buf[6]).arg(buf[7]).arg(vps_pr_label));

    pcs = buf[2] >> 6;
    cni = + ((buf[10] & 3) << 10)
        + ((buf[11] & 0xC0) << 2)
        + ((buf[8] & 0xC0) << 0)
        + (buf[11] & 0x3F);
    pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);
    pty = buf[12];

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
    static const int wss_bits[8] = { 0, 0, 0, 1, 0, 1, 1, 1 };
    uint wss = 0;

    for (uint i = 0; i < 16; i++)
    {
        uint b1 = wss_bits[buf[i] & 7];
        uint b2 = wss_bits[(buf[i] >> 3) & 7];

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
            .arg(formats[wss & 7])
            .arg((wss & 0x0010) ? "film"                 : "camera")
            .arg((wss & 0x0020) ? "MA/CP"                : "standard")
            .arg((wss & 0x0040) ? "modulated"            : "no")
            .arg(!!(wss & 0x0080))
            .arg((wss & 0x0100) ? "have TTX subtitles; " : "")
            .arg(subtitles[(wss >> 9) & 3])
            .arg((wss & 0x0800) ? "surround sound; "     : "")
            .arg((wss & 0x1000) ? "asserted"             : "unknown")
            .arg((wss & 0x2000) ? "restricted"           : "not restricted"));

    if (parity & 1)
    {
        wss_flags = wss;
        wss_valid = true;
    }
}

QString CC608Decoder::XDSDecodeString(const vector<unsigned char> &buf,
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
        for (int i = 0; i < newStr.length(); i++)
            if (newStr[i].toAscii() < 0x20)
                return false;

        return true;
    }
    return false;
}

uint CC608Decoder::GetRatingSystems(bool future) const
{
    QMutexLocker locker(&xds_lock);
    return xds_rating_systems[(future) ? 1 : 0];
}

uint CC608Decoder::GetRating(uint i, bool future) const
{
    QMutexLocker locker(&xds_lock);
    return xds_rating[(future) ? 1 : 0][i & 0x3] & 0x7;
}

QString CC608Decoder::GetRatingString(uint i, bool future) const
{
    QMutexLocker locker(&xds_lock);

    QString prefix[4] = { "MPAA-", "TV-", "CE-", "CF-" };
    QString mainStr[4][8] =
    {
        { "NR", "G", "PG",  "PG-13", "R",   "NC-17", "X",   "NR" },
        { "NR", "Y", "Y7",  "G",     "PG",  "14",    "MA",  "NR" },
        { "E",  "C", "C8+", "G",     "PG",  "14+",   "18+", "NR" },
        { "E",  "G", "8+",  "13+",   "16+", "18+",   "NR",  "NR" },
    };

    QString main = prefix[i] + mainStr[i][GetRating(i, future)];

    if (kRatingTPG == i)
    {
        uint cf = (future) ? 1 : 0;
        if (!(xds_rating[cf][i]&0xF0))
        {
            main.detach();
            return main;
        }

        main += " ";
        // TPG flags
        if (xds_rating[cf][i] & 0x80)
            main += "D"; // Dialog
        if (xds_rating[cf][i] & 0x40)
            main += "V"; // Violence
        if (xds_rating[cf][i] & 0x20)
            main += "S"; // Sex
        if (xds_rating[cf][i] & 0x10)
            main += "L"; // Language
    }

    main.detach();
    return main;
}

QString CC608Decoder::GetProgramName(bool future) const
{
    QMutexLocker locker(&xds_lock);
    QString ret = xds_program_name[(future) ? 1 : 0];
    ret.detach();
    return ret;
}

QString CC608Decoder::GetProgramType(bool future) const
{
    QMutexLocker locker(&xds_lock);
    const vector<uint> &program_type = xds_program_type[(future) ? 1 : 0];
    QString tmp = "";

    for (uint i = 0; i < program_type.size(); i++)
    {
        if (i != 0)
            tmp += ", ";
        tmp += xds_program_type_string[program_type[i]];
    }

    tmp.detach();
    return tmp;
}

QString CC608Decoder::GetXDS(const QString &key) const
{
    QMutexLocker locker(&xds_lock);

    if (key == "ratings")
        return QString::number(GetRatingSystems(false));
    else if (key.left(11) == "has_rating_")
        return ((1<<key.right(1).toUInt()) & GetRatingSystems(false))?"1":"0";
    else if (key.left(7) == "rating_")
        return GetRatingString(key.right(1).toUInt(), false);

    else if (key == "future_ratings")
        return QString::number(GetRatingSystems(true));
    else if (key.left(11) == "has_future_rating_")
        return ((1<<key.right(1).toUInt()) & GetRatingSystems(true))?"1":"0";
    else if (key.left(14) == "future_rating_")
        return GetRatingString(key.right(1).toUInt(), true);

    else if (key == "programname")
        return GetProgramName(false);
    else if (key == "future_programname")
        return GetProgramName(true);

    else if (key == "programtype")
        return GetProgramType(false);
    else if (key == "future_programtype")
        return GetProgramType(true);

    else if (key == "callsign")
    {
        QString ret = xds_net_call;
        ret.detach();
        return ret;
    }
    else if (key == "channame")
    {
        QString ret = xds_net_name;
        ret.detach();
        return ret;
    }
    else if (key == "tsid")
        return QString::number(xds_tsid);

    return QString::null;
}

static int b1_to_service[16] =
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

bool CC608Decoder::XDSDecode(int field, int b1, int b2)
{
    if (field == 0)
        return false; // XDS is only on second field

#if DEBUG_XDS
    LOG(VB_VBI, LOG_INFO,
        QString("XDSDecode: 0x%1 0x%2 '%3%4' xds[%5]=%6 in XDS %7")
        .arg(b1,2,16,QChar('0')).arg(b2,2,16,QChar('0'))
        .arg((CharCC(b1).unicode()>0x20) ? CharCC(b1) : QChar(' '))
        .arg((CharCC(b2).unicode()>0x20) ? CharCC(b2) : QChar(' '))
        .arg(field).arg(xds[field])
        .arg(xds_cur_service));
#else
    (void) field;
#endif // DEBUG_XDS

    if (xds_cur_service < 0)
    {
        if (b1 > 0x0f)
            return false;

        xds_cur_service = b1_to_service[b1];

        if (xds_cur_service < 0)
            return false;

        if (b1 & 1)
        {
            xds_buf[xds_cur_service].clear(); // if start of service clear buffer
#if DEBUG_XDS
            LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Starting XDS %1").arg(xds_cur_service));
#endif // DEBUG_XDS
        }
    }
    else if ((0x0 < b1) && (b1 < 0x0f))
    { // switch to different service
        xds_cur_service = b1_to_service[b1];
#if DEBUG_XDS
        LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Resuming XDS %1").arg(xds_cur_service));
#endif // DEBUG_XDS
    }

    if (xds_cur_service < 0)
        return false;

    xds_buf[xds_cur_service].push_back(b1);
    xds_buf[xds_cur_service].push_back(b2);

    if (b1 == 0x0f) // end of packet
    {
#if DEBUG_XDS
        LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Ending XDS %1").arg(xds_cur_service));
#endif // DEBUG_XDS
        if (XDSPacketCRC(xds_buf[xds_cur_service]))
            XDSPacketParse(xds_buf[xds_cur_service]);
        xds_buf[xds_cur_service].clear();
        xds_cur_service = -1;
    }
    else if ((0x10 <= b1) && (b1 <= 0x1f)) // suspension of XDS packet
    {
#if DEBUG_XDS
        LOG(VB_VBI, LOG_INFO, QString("XDSDecode: Suspending XDS %1 on 0x%2")
            .arg(xds_cur_service).arg(b1,2,16,QChar('0')));
#endif // DEBUG_XDS
        xds_cur_service = -1;
    }

    return true;
}

void CC608Decoder::XDSPacketParse(const vector<unsigned char> &xds_buf)
{
    QMutexLocker locker(&xds_lock);

    bool handled   = false;
    int  xds_class = xds_buf[0];

    if (!xds_class)
        return;

    if ((xds_class == 0x01) || (xds_class == 0x03)) // cont code 2 and 4, resp.
        handled = XDSPacketParseProgram(xds_buf, (xds_class == 0x03));
    else if (xds_class == 0x05) // cont code: 0x06
        handled = XDSPacketParseChannel(xds_buf);
    else if (xds_class == 0x07) // cont code: 0x08
        ; // misc.
    else if (xds_class == 0x09) // cont code: 0x0a
        ; // public (aka weather)
    else if (xds_class == 0x0b) // cont code: 0x0c
        ; // reserved
    else if (xds_class == 0x0d) // cont code: 0x0e
        handled = true; // undefined

    if (DEBUG_XDS && !handled)
    {
        LOG(VB_VBI, LOG_INFO, QString("XDS: ") +
            QString("Unhandled packet (0x%1 0x%2) sz(%3) '%4'")
            .arg(xds_buf[0],0,16).arg(xds_buf[1],0,16)
            .arg(xds_buf.size())
            .arg(XDSDecodeString(xds_buf, 2, xds_buf.size() - 2)));
    }
}

bool CC608Decoder::XDSPacketCRC(const vector<unsigned char> &xds_buf)
{
    /* Check the checksum for validity of the packet. */
    int sum = 0;
    for (uint i = 0; i < xds_buf.size() - 1; i++)
        sum += xds_buf[i];

    if ((((~sum) & 0x7f) + 1) != xds_buf[xds_buf.size() - 1])
    {
        xds_crc_failed++;

        LOG(VB_VBI, LOG_ERR, QString("XDS: failed CRC %1 of %2")
                .arg(xds_crc_failed).arg(xds_crc_failed + xds_crc_passed));

        return false;
    }

    xds_crc_passed++;
    return true;
}

bool CC608Decoder::XDSPacketParseProgram(
    const vector<unsigned char> &xds_buf, bool future)
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
        if (is_better(tmp, xds_program_name[cf]))
        {
            xds_program_name[cf] = tmp;
            LOG(VB_VBI, LOG_INFO, loc + QString("Program Name: '%1'")
                    .arg(GetProgramName(future)));
        }
    }
    else if ((b2 == 0x04) && (xds_buf.size() >= 6))
    {
        vector<uint> program_type;
        for (uint i = 2; i < xds_buf.size() - 2; i++)
        {
            int cur = xds_buf[i] - 0x20;
            if (cur >= 0 && cur < 96)
                program_type.push_back(cur);
        }

        bool unchanged = xds_program_type[cf].size() == program_type.size();
        for (uint i = 0; (i < program_type.size()) && unchanged; i++)
            unchanged = xds_program_type[cf][i] == program_type[i];

        if (!unchanged)
        {
            xds_program_type[cf] = program_type;
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
            if (!(kHasCanEnglish & xds_rating_systems[cf]) ||
                (tv_rating != GetRating(kRatingCanEnglish, future)))
            {
                xds_rating_systems[cf]            |= kHasCanEnglish;
                xds_rating[cf][kRatingCanEnglish]  = tv_rating;
                LOG(VB_VBI, LOG_INFO, loc + QString("VChip %1")
                        .arg(GetRatingString(kRatingCanEnglish, future)));
            }
        }
        else if (sel == 7)
        {
            if (!(kHasCanFrench & xds_rating_systems[cf]) ||
                (tv_rating != GetRating(kRatingCanFrench, future)))
            {
                xds_rating_systems[cf]           |= kHasCanFrench;
                xds_rating[cf][kRatingCanFrench]  = tv_rating;
                LOG(VB_VBI, LOG_INFO, loc + QString("VChip %1")
                        .arg(GetRatingString(kRatingCanFrench, future)));
            }
        }
        else if (sel == 0x13 || sel == 0x1f)
            ; // Reserved according to TVTime code
        else if ((rating_system & 0x3) == 1)
        {
            if (!(kHasTPG & xds_rating_systems[cf]) ||
                (tv_rating != GetRating(kRatingTPG, future)))
            {
                uint f = ((xds_buf[0]<<3) & 0x80) | ((xds_buf[1]<<1) & 0x70);
                xds_rating_systems[cf]     |= kHasTPG;
                xds_rating[cf][kRatingTPG]  = tv_rating | f;
                LOG(VB_VBI, LOG_INFO, loc + QString("VChip %1")
                        .arg(GetRatingString(kRatingTPG, future)));
            }
        }
        else if (rating_system == 0)
        {
            if (!(kHasMPAA & xds_rating_systems[cf]) ||
                (movie_rating != GetRating(kRatingMPAA, future)))
            {
                xds_rating_systems[cf]      |= kHasMPAA;
                xds_rating[cf][kRatingMPAA]  = movie_rating;
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
        handled = false;

    return handled;
}

bool CC608Decoder::XDSPacketParseChannel(const vector<unsigned char> &xds_buf)
{
    bool handled = true;

    int b2 = xds_buf[1];
    if ((b2 == 0x01) && (xds_buf.size() >= 6))
    {
        QString tmp = XDSDecodeString(xds_buf, 2, xds_buf.size() - 2);
        if (is_better(tmp, xds_net_name))
        {
            LOG(VB_VBI, LOG_INFO, QString("XDS: Network Name '%1'").arg(tmp));
            xds_net_name = tmp;
        }
    }
    else if ((b2 == 0x02) && (xds_buf.size() >= 6))
    {
        QString tmp = XDSDecodeString(xds_buf, 2, xds_buf.size() - 2);
        if (is_better(tmp, xds_net_call) && (tmp.indexOf(" ") < 0))
        {
            LOG(VB_VBI, LOG_INFO, QString("XDS: Network Call '%1'").arg(tmp));
            xds_net_call = tmp;
        }
    }
    else if ((b2 == 0x04) && (xds_buf.size() >= 6))
    {
        uint tsid = (xds_buf[2] << 24 | xds_buf[3] << 16 |
                     xds_buf[4] <<  8 | xds_buf[5]);
        if (tsid != xds_tsid)
        {
            LOG(VB_VBI, LOG_INFO, QString("XDS: TSID 0x%1").arg(tsid,0,16));
            xds_tsid = tsid;
        }
    }
    else
        handled = false;

    return handled;
}

static void init_xds_program_type(QString xds_program_type[96])
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
