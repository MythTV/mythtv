#include <iostream>
using namespace std;

#include <qstringlist.h>

#include "format.h"
#include "ccdecoder.h"
#include "mythcontext.h"
#include "vbilut.h"

#define DEBUG_XDS 0

CCDecoder::CCDecoder(CCReader *ccr)
    : reader(ccr),                  ignore_time_code(false),
      rbuf(new unsigned char[sizeof(ccsubtitle)+255]),
      vps_l(0),
      wss_flags(0),                 wss_valid(false),
      xds_current_packet(0),
      xds_rating_systems(0),        xds_program_name(QString::null),
      xds_net_call(QString::null),  xds_net_call_x(QString::null),
      xds_net_name(QString::null),  xds_net_name_x(QString::null)
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
    }

    bzero(lastrow,  sizeof(lastrow));
    bzero(newrow,   sizeof(newrow));
    bzero(newcol,   sizeof(newcol));
    bzero(timecode, sizeof(timecode));
    bzero(row,      sizeof(row));
    bzero(col,      sizeof(col));
    bzero(rowcount, sizeof(rowcount));
    bzero(style,    sizeof(style));
    bzero(linecont, sizeof(linecont));
    bzero(resumetext, sizeof(resumetext));
    bzero(lastclr,  sizeof(lastclr));

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

    // VPS data
    bzero(vps_pr_label, sizeof(vps_pr_label));
    bzero(vps_label,    sizeof(vps_label));

    // XDS data
    bzero(xds_rating,   sizeof(xds_rating));
}

CCDecoder::~CCDecoder(void)
{
    if (rbuf)
        delete [] rbuf;
}

void CCDecoder::FormatCC(int tc, int code1, int code2)
{
    FormatCCField(tc, 0, code1);
    FormatCCField(tc, 1, code2);
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

void CCDecoder::FormatCCField(int tc, int field, int data)
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

    b1 = data & 0x7f;
    b2 = (data >> 8) & 0x7f;
    VERBOSE(VB_VBI, QString("Format CC @%1/%2 = %3 %4")
                    .arg(tc).arg(field)
                    .arg((data&0xff), 2, 16)
                    .arg((data&0xff00)>>8, 2, 16));
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
        goto skip;

    DecodeXDS(field, b1, b2);

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
                newcol[mode] = (b2 & 0x0E) << 1;
            else
                newcol[mode] = 0;

            // row, indent settings are not final
            // until text code arrives
        }
        else
        {
            switch (b1 & 0x07)
            {
                case 0x00:          //attribute
                    /*
                      printf ("<ATTRIBUTE %d %d>\n", b1, b2);
                      fflush (stdout);
                    */
                    break;
                case 0x01:          //midrow or char
                    if (newrow[mode])
                        len = NewRowCC(mode, len);

                    switch (b2 & 0x70)
                    {
                        case 0x20:      //midrow attribute change
                            // TODO: we _do_ want colors, is that an attribute?
                            ccbuf[mode] += ' ';
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
//                 printf("ccmode %d cmd %02x\n",ccmode,b2);
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
                            if (len ||
                                row[mode] != 0 &&
                                !linecont[mode] &&
                                (!newtxtmode || row[mode] >= 16))
                                BufferCC(mode, len, 0);

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

int CCDecoder::FalseDup(int tc, int field, int data)
{
    int b1, b2;

    b1 = data & 0x7f;
    b2 = (data >> 8) & 0x7f;

    if (ignore_time_code)
    {
        // just suppress duplicate control codes
        if ((data == lastcode[field]) &&
            ((b1 & 0x70) == 0x10))
            return 1;
        else
            return 0;
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

void CCDecoder::ResetCC(int mode)
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

void CCDecoder::BufferCC(int mode, int len, int clr)
{
    QCString tmpbuf;
    if (len)
    {
        // calculate UTF-8 encoding length
        tmpbuf = ccbuf[mode].utf8();
        len = tmpbuf.length();
        if (len > 255)
            len = 255;
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
               tmpbuf,
               len);
        len += sizeof(ccsubtitle);
    }
    else
        len = sizeof(ccsubtitle);

    VERBOSE(VB_VBI, QString("### %1 %2 %3 %4 %5 %6 %7 -")
                           .arg(timecode[mode], 10)
                           .arg(row[mode], 2).arg(rowcount[mode])
                           .arg(style[mode]).arg(f, 2, 16)
                           .arg(clr).arg(len, 3));
    if ((print_verbose_messages & VB_VBI) != 0
        && len)
    {
        QString dispbuf = QString::fromUtf8(tmpbuf, len);
        VERBOSE(VB_VBI, QString("%1 '").arg(timecode[mode], 10));
        QString vbuf = "";
        unsigned int i = 0;
        while (i < dispbuf.length()) {
            QChar cp = dispbuf.at(i);
            switch (cp.unicode())
            {
                case 0x2120 :  vbuf += "(SM)"; break;
                case 0x2122 :  vbuf += "(TM)"; break;
                case 0x2014 :  vbuf += "(--)"; break;
                case 0x201C :  vbuf += "``"; break;
                case 0x201D :  vbuf += "''"; break;
                case 0x250C :  vbuf += "|-"; break;
                case 0x2510 :  vbuf += "-|"; break;
                case 0x2514 :  vbuf += "|_"; break;
                case 0x2518 :  vbuf += "_|"; break;
                case 0x2588 :  vbuf += "[]"; break;
                case 0x266A :  vbuf += "o/~"; break;
                case '\b'   :  vbuf += "\\b"; break;
                default     :  vbuf += cp.latin1();
            }
            i++;
        }
        VERBOSE(VB_VBI, vbuf);
    }

    reader->AddTextData(rbuf, len, timecode[mode], 'C');

    resumetext[mode] = 0;
    if (clr && !len)
        lastclr[mode] = timecode[mode];
    else if (len)
        lastclr[mode] = 0;
}

int CCDecoder::NewRowCC(int mode, int len)
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

    for (int x = 0; x < newcol[mode]; x++)
    {
        ccbuf[mode] += ' ';
        len++;
        col[mode]++;
    }
    newcol[mode] = 0;

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

void DumpPIL(int pil)
{
    int day  = (pil >> 15);
    int mon  = (pil >> 11) & 0xF;
    int hour = (pil >> 6 ) & 0x1F;
    int min  = (pil      ) & 0x3F;

#define _PIL_(day, mon, hour, min) \
  (((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

    if (pil == _PIL_(0, 15, 31, 63))
        VERBOSE(VB_VBI, " PDC: Timer-control (no PDC)");
    else if (pil == _PIL_(0, 15, 30, 63))
        VERBOSE(VB_VBI, " PDC: Recording inhibit/terminate");
    else if (pil == _PIL_(0, 15, 29, 63))
        VERBOSE(VB_VBI, " PDC: Interruption");
    else if (pil == _PIL_(0, 15, 28, 63))
        VERBOSE(VB_VBI, " PDC: Continue");
    else if (pil == _PIL_(31, 15, 31, 63))
        VERBOSE(VB_VBI, " PDC: No time");
    else
        VERBOSE(VB_VBI, QString(" PDC: %1, 200X-%2-%3 %4:%5")
                .arg(pil).arg(mon).arg(day).arg(hour).arg(min));
#undef _PIL_
}

void CCDecoder::DecodeVPS(const unsigned char *buf)
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

    VERBOSE(VB_VBI, QString("VPS: 3-10: %1 %2 %3 %4 %5 %6 %7 %8 (\"%9\")")
            .arg(buf[0]).arg(buf[1]).arg(buf[2]).arg(buf[3]).arg(buf[4])
            .arg(buf[5]).arg(buf[6]).arg(buf[7]).arg(vps_pr_label));

    pcs = buf[2] >> 6;
    cni = + ((buf[10] & 3) << 10)
        + ((buf[11] & 0xC0) << 2)
        + ((buf[8] & 0xC0) << 0)
        + (buf[11] & 0x3F);
    pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);
    pty = buf[12];

    VERBOSE(VB_VBI, QString("CNI: %1 PCS: %2 PTY: %3 ")
            .arg(cni).arg(pcs).arg(pty));

    DumpPIL(pil);

    // c & 0xf0;
}

// // // // // // // // // // // // // // // // // // // // // // // //
// // // // // // // // // // //  WSS  // // // // // // // // // // //
// // // // // // // // // // // // // // // // // // // // // // // //

void CCDecoder::DecodeWSS(const unsigned char *buf)
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

    VERBOSE(VB_VBI,
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

QString CCDecoder::DecodeXDSString(const vector<unsigned char> &buf,
                                   uint start, uint end) const
{
#if DEBUG_XDS
    for (uint i = start; (i < buf.size()) && (i < end); i++)
    {
        VERBOSE(VB_IMPORTANT, QString("%1: 0x%2 -> 0x%3 %4")
                .arg(i,2).arg(buf[i],2,16)
                .arg(CharCC(buf[i]),2,16)
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
    VERBOSE(VB_IMPORTANT, QString("DecodeXDSString: '%1'").arg(tmp));
#endif // DEBUG_XDS

    return tmp.stripWhiteSpace();
}

bool is_better(const QString &newStr, const QString &oldStr)
{
    if (!newStr.isEmpty() && newStr != oldStr &&
        (newStr != oldStr.left(newStr.length())))
    {
        if (oldStr.isEmpty())
            return true;

        // check if the string contains any bogus characters
        for (uint i = 0; i < newStr.length(); i++)
            if ((int)newStr[i] < 0x20)
                return false;

        return true;
    }
    return false;
}

void CCDecoder::DecodeXDSStartTime(int b1, int b2)
{
    if (b1 == 0x0F)
    {
        xds_current_packet = 0;
        VERBOSE(VB_VBI, QString("XDS Packet: Start Time/Program ID: End"));
        return;
    }

    // Process Start Time packets here
    VERBOSE(VB_VBI, QString("XDS Packet: Start Time/Program ID: "
                            "Packet %1 %2").arg(b1).arg(b2));
}

void CCDecoder::DecodeXDSProgramLength(int b1, int b2)
{
    if (b1 == 0x0F)
    {
        xds_current_packet = 0;
        // Payload bytes 1 and 2 are minutes and hours in length
        if (xds_buf.size() > 0)
            VERBOSE(VB_VBI, "XDS Packet: " +
                    QString("Program Length: %1 hours, %1 minutes")
                    .arg(xds_buf[1]-64).arg(xds_buf[0]-64));
        // Payload bytes 3 and 4 are elapsed minutes and hours in length
        if (xds_buf.size() >= 2)
            VERBOSE(VB_VBI, "XDS Packet: " +
                    QString("Program Elapsed: %1 hours, %1 mintues")
                    .arg(xds_buf[3]-64).arg(xds_buf[2]-64));
        // If Payload byte 6 is filler 0x40,
        // then payload byte 5 is elapsed seconds
        if ((xds_buf.size() >= 4) && (xds_buf[5]==0x40))
            VERBOSE(VB_VBI, "XDS Packet: " +
                    QString("Program Elapsed: %1 seconds").arg(xds_buf[4]-64));

        //VERBOSE(VB_VBI, "XDS Packet: Program Length/Time in Show: End");
        return;
    }

    // Process Program Length packets here
    //VERBOSE(VB_VBI, "XDS Packet: " +
    //        QString("Program Length/Time in Show: Packet %1 %2")
    //        .arg(b1).arg(b2));
    xds_buf.push_back(b1);
    xds_buf.push_back(b2);
}

void CCDecoder::DecodeXDSProgramName(int b1, int b2)
{
    if (b1 != 0x0F)
    {
        xds_buf.push_back(b1);
        xds_buf.push_back(b2);
        return;
    }

    xds_current_packet = 0;

    QString tmp = DecodeXDSString(xds_buf, 0, xds_buf.size());
    if (is_better(tmp, xds_program_name))
    {
        VERBOSE(VB_VBI, QString("XDS Packet: Program Name: '%1'").arg(tmp));
        xds_program_name = tmp;
    }
}

void CCDecoder::DecodeXDSProgramType(int b1, int b2)
{
    if (b1 == 0x0F)
    {
        xds_current_packet = 0;
        VERBOSE(VB_VBI, QString("XDS Packet: Program Type: End"));
        return;
    }

    // Process Program Type packets here
    if ((b1 == 0xA8) || (b2 == 0xA8))
        VERBOSE(VB_VBI, "XDS Packet: Program Type: Advertisement");
       
    VERBOSE(VB_VBI, QString("XDS Packet: Program Type: %1 %2")
            .arg(b1).arg(b2));
}

QString CCDecoder::GetRatingString(uint i) const
{
    QString prefix[4] = { "MPAA-", "TV-", "CE-", "CF-" };
    QString mainStr[4][8] =
    {
        { "NR", "G", "PG",  "PG-13", "R",   "NC-17", "X",   "NR" },
        { "NR", "Y", "Y7",  "G",     "PG",  "14",    "MA",  "NR" },
        { "E",  "C", "C8+", "G",     "PG",  "14+",   "18+", "NR" },
        { "E",  "G", "8+",  "13+",   "16+", "18+",   "NR",  "NR" },
    };

    QString main = prefix[i] + mainStr[i][GetRating(i)];

    if (kRatingTPG == i)
    {
        if (!(xds_rating[i]&0xF0))
            return QDeepCopy<QString>(main);

        main += " ";
        // TPG flags
        if (xds_rating[i] & 0x80)
            main += "D"; // Dialog
        if (xds_rating[i] & 0x40)
            main += "V"; // Violence
        if (xds_rating[i] & 0x20)
            main += "S"; // Sex
        if (xds_rating[i] & 0x10)
            main += "L"; // Language
    }

    return QDeepCopy<QString>(main);
}

QString CCDecoder::GetXDS(const QString &key) const
{
    if (key == "ratings")
        return QString::number(GetRatingSystems());
    else if (key.left(11) == "has_rating_")
        return ((1<<key.right(1).toUInt()) & GetRatingSystems()) ? "1" : "0";
    else if (key.left(7) == "rating_")
        return GetRatingString(key.right(1).toUInt());
    else if (key == "callsign")
        return QDeepCopy<QString>(xds_net_call_x);
    else if (key == "channame")
        return QDeepCopy<QString>(xds_net_name_x);

    return QString::null;
}

void CCDecoder::DecodeXDSVChip(int b1, int b2)
{
    if (b1 != 0x0F)
    {
        xds_buf.push_back(b1);
        xds_buf.push_back(b2);
        return;
    }

    xds_current_packet = 0;

    if (xds_buf.size() < 2)
        return; // collected data too small to contain XDS VChip packet

    uint rating_system = ((xds_buf[0]>>3) & 0x7);
    // 0 == us mpaa, 1 == us tpg, 3 == ca english, 7 == ca french

    if (1 == rating_system)
    {
        uint rating = (xds_buf[1] & 0x07);
        if (!(kHasTPG & xds_rating_systems) || rating != GetRating(kRatingTPG))
        {
            uint f = ((xds_buf[0]<<3) & 0x80) | ((xds_buf[1]<<1) & 0x70);
            xds_rating_systems     |= kHasTPG;
            xds_rating[kRatingTPG]  = rating | f;
            VERBOSE(VB_GENERAL, "VChip: "<<GetRatingString(kRatingTPG));
        }
    }
    else if (0 == rating_system)
    {
        uint rating = (xds_buf[0] & 0x07);
        if (!(kHasMPAA & xds_rating_systems) ||
            (rating != GetRating(kRatingMPAA)))
        {
            xds_rating_systems      |= kHasMPAA;
            xds_rating[kRatingMPAA]  = rating;
            VERBOSE(VB_GENERAL, "VChip: "<<GetRatingString(kRatingMPAA));
        }
    }
    else if (3 == rating_system)
    {
        uint rating = (xds_buf[1] & 0x07);
        if (!(kHasCanEnglish & xds_rating_systems) ||
            (rating != GetRating(kRatingCanEnglish)))
        {
            xds_rating_systems            |= kHasCanEnglish;
            xds_rating[kRatingCanEnglish]  = rating;
            VERBOSE(VB_GENERAL, "VChip: "<<GetRatingString(kRatingCanEnglish));
        }
    }
    else if (7 == rating_system)
    {
        uint rating = (xds_buf[1] & 0x07);
        if (!(kHasCanFrench & xds_rating_systems) ||
            (rating != GetRating(kRatingCanFrench)))
        {
            xds_rating_systems            |= kHasCanFrench;
            xds_rating[kRatingCanFrench]  = rating;
            VERBOSE(VB_GENERAL, "VChip: "<<GetRatingString(kRatingCanFrench));
        }
    }
    else
    {
        VERBOSE(VB_VBI, "VChip: Unknown Rating System #" << rating_system);
    }
}

void CCDecoder::DecodeXDSPacket(int b1, int b2)
{
#if DEBUG_XDS
    VERBOSE(VB_VBI, QString("XDSPacket: 0x%1 0x%2 (cp 0x%3)")
            .arg(b1,2,16).arg(b2,2,16).arg(xds_current_packet,0,16));
#endif // DEBUG_XDS

    bool handled = true;
    if (kXDSIgnore == xds_current_packet)
    {
        if (b1 == 0x0f)
            xds_current_packet = 0;
    }
    else if (kXDSNetName == xds_current_packet)
    {
        if (b1 == 0x0f)
            xds_current_packet = 0;
    }
    else if (kXDSNetCall == xds_current_packet)
    {
        if (b1 == 0x0f)
            xds_current_packet = 0;
    }
    else if (kXDSStartTime == xds_current_packet)
        DecodeXDSStartTime(b1, b2);
    else if (kXDSProgLen == xds_current_packet)
        DecodeXDSProgramLength(b1, b2);
    else if (kXDSProgName == xds_current_packet)
        DecodeXDSProgramName(b1, b2);
    else if (kXDSProgType == xds_current_packet)
        DecodeXDSProgramType(b1, b2);
    else if (kXDSVChip == xds_current_packet)
        DecodeXDSVChip(b1, b2);
    else if (kXDSNetCallX == xds_current_packet)
    {
        if (b1 == 0x0f)
        {
            xds_current_packet = 0;

            QString tmp = DecodeXDSString(xds_buf, 0, xds_buf.size());
            if (is_better(tmp, xds_net_call_x))
            {
                VERBOSE(VB_VBI, QString("XDS: Network Call '%1'").arg(tmp));
                xds_net_call_x = tmp;
            }
        }
        else
        {
            xds_buf.push_back(b1);
            xds_buf.push_back(b2);
        }
    }
    else if (kXDSNetNameX == xds_current_packet)
    {
        if (b1 == 0x0f)
        {
            xds_current_packet = 0;

            QString tmp = DecodeXDSString(xds_buf, 0, xds_buf.size());
            if (is_better(tmp, xds_net_name_x))
            {
                VERBOSE(VB_VBI, QString("XDS: Network Name '%1'").arg(tmp));
                xds_net_name_x = tmp;
            }
        }
        else
        {
            xds_buf.push_back(b1);
            xds_buf.push_back(b2);
        }
    }
    else if (b1 == 0x83) // cont code: 0x04
    { // future class
        VERBOSE(VB_VBI, "XDS Packet future start");
        xds_current_packet = kXDSIgnore;
    }
    else if (b1 == 0x01) // cont code: 0x02
    { // current class
        if (b2 == 0x01)
        {
            // Detect start packets 
            VERBOSE(VB_VBI, "XDS Packet: Start Time/Program ID");
            xds_current_packet = kXDSStartTime;
        }
        else if (b2 == 0x02)
        {
            VERBOSE(VB_VBI, "XDS Packet: Program Length/Time in Show");
            xds_current_packet = kXDSProgLen;
            xds_buf.clear();
        }
        else if (b2 == 0x03)
        {
            xds_current_packet = kXDSProgName;
            xds_buf.clear();
        }
        else if (b2 == 0x04)
        {
            VERBOSE(VB_VBI, "XDS Packet: Program Type");
            xds_current_packet = kXDSProgType;
        }
        else if (b2 == 0x05)
        {
            xds_current_packet = kXDSVChip;
            xds_buf.clear();
        }
        else if (b2 == 0x07)
            xds_current_packet = kXDSIgnore; // caption
        else if (b2 == 0x08)
            xds_current_packet = kXDSIgnore; // cgms
        else if (b2 == 0x09)
            xds_current_packet = kXDSIgnore; // unknown
        else if (b2 == 0x0c)
            xds_current_packet = kXDSIgnore; // misc
        else if (b2 == 0x10 || b2 == 0x13 || b2 == 0x15 || b2 == 0x16 ||
                 b2 == 0x91 || b2 == 0x92 || b2 == 0x94 || b2 == 0x97)
            xds_current_packet = kXDSIgnore; // program description
        else if (b2 == 0x86)
            xds_current_packet = kXDSIgnore; // audio
        else if (b2 == 0x89)
            xds_current_packet = kXDSIgnore; // aspect
        else if (b2 == 0x8c)
            xds_current_packet = kXDSIgnore; // program data
        else
            handled = false;
    }
    else if (b1 == 0x85) // cont code: 0x86
    { // channel class
        if (b2 == 0x01)
        {
            VERBOSE(VB_VBI, "XDS Packet: Network Name");
            xds_current_packet = kXDSNetName;
        }
        else if (b2 == 0x02)
        {
            VERBOSE(VB_VBI, "XDS Packet: Network Call Letters");
            xds_current_packet = kXDSNetCall;
        }
        else if (b2 == 0x04)
        {
            VERBOSE(VB_VBI, "XDS Packet: TSID");
            xds_current_packet = kXDSIgnore;
        }
        else if (b2 == 0x83)
            xds_current_packet = kXDSIgnore; // tape delay
        else
            handled = false;
    }
    else if (b1 == 0x07) // cont code: 0x08
    { // misc.
        xds_current_packet = kXDSIgnore;
    }
    else if (b1 == 0x89) // cont code: 0x8a
    { // weather
        xds_current_packet = kXDSIgnore;
    }
    else if (b1 == 0x0b) // cont code: 0x8c
    { // reserved
        xds_current_packet = kXDSIgnore;
    }
    else if (b1 == 0x0d) // cont code: 0x0e
    { // undefined
        xds_current_packet = kXDSIgnore;
    }
    else if (b1 == 0x05)
    { // unknown ?cable?
        if (b2 == 0x01 || b2 == 0x02)
        {
            xds_current_packet = ((uint)b1) << 8 | b2;
            xds_buf.clear();
        }
        else
        {
            VERBOSE(VB_VBI, "XDS Packet: Unknown class 0x05, type "<<b2);
            xds_current_packet = kXDSIgnore;
        }
    }
    else
        handled = false;

    if ((b1 == 0x0f) && xds_current_packet)
        xds_current_packet = 0;

    if (!handled)
    {
#if DEBUG_XDS
        VERBOSE(VB_VBI, QString("XDS: Unknown Packet 0x%1 0x%2 (cp 0x%3)")
                .arg(b1,0,16).arg(b2,0,16).arg(xds_current_packet,0,16));
#endif // DEBUG_XDS
        xds_current_packet = 0;
    }
}

void CCDecoder::DecodeXDS(int field, int b1, int b2)
{
    field = 1;
#if DEBUG_XDS
    VERBOSE(VB_VBI, QString("DecodeXDS: 0x%1 0x%2 (cp 0x%3) '%4%5' "
                            "xds[%6]=%7")
            .arg(b1,2,16).arg(b2,2,16).arg(xds_current_packet,0,16)
            .arg(((int)CharCC(b1)>0x20) ? CharCC(b1) : QChar(' '))
            .arg(((int)CharCC(b2)>0x20) ? CharCC(b2) : QChar(' '))
            .arg(field).arg(xds[field]));
#endif // DEBUG_XDS
    // if (0x01 <= b1 <= 0x0F) -> start XDS or inside XDS packet
    if ((field == 1) && (xds[field] || b1 && ((b1 & 0x70) == 0x00)))
    {
        bool xds_packet = true;

        // VERBOSE(VB_VBI, QString("XDS Packet : %1 %2").arg(b1).arg(b2));
        if (b1 == 0x0F)
        {
            // end XDS
            xds[field] = 0;
            xds_packet = true;
        }
        else if ((b1 & 0x70) == 0x10)
        {
            // ctrl code -- interrupt XDS
            xds[field] = 0;
            xds_packet = false;
        }
        else
        {
            xds[field] = 1;
            xds_packet = true;
        }

        if (xds_packet)
            DecodeXDSPacket(b1, b2);
    }
}
