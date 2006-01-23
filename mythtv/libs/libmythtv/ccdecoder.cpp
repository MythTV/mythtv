#include <iostream>
using namespace std;

#include <qstringlist.h>

#include "format.h"
#include "ccdecoder.h"
#include "mythcontext.h"
#include "vbilut.h"

CCDecoder::CCDecoder(CCReader *ccr)
    : reader(ccr), rbuf(new unsigned char[sizeof(ccsubtitle)+255]),
      vps_l(0),
      wss_flags(0), wss_valid(false)
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

    bzero(vps_pr_label, sizeof(vps_pr_label));
    bzero(vps_label,    sizeof(vps_label));
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
//    printf("%10d:  %02x %02x\n", tc, b1, b2);
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
        int false_dup = 1;
        if ((b1 & 0x70) == 0x10)
        {
            if (tc > (lastcodetc[field] + 67 + dup_ctrl_fudge))
                false_dup = 0;
        }
        else if (b1)
        {
            // text, XDS
            if (tc > (lastcodetc[field] + 33 + dup_text_fudge))
                false_dup = 0;
        }

        if (false_dup)
            goto skip;
    }

    if ((field == 1) &&
        (xds[field] || b1 && ((b1 & 0x70) == 0x00)))
        // 0x01 <= b1 <= 0x0F
        // start XDS
        // or inside XDS packet
    {
        int xds_packet = 1;

        // TODO: process XDS packets
        if (b1 == 0x0F)
        {
            // end XDS
            xds[field] = 0;
            xds_packet = 1;
        }
        else if ((b1 & 0x70) == 0x10)
        {
            // ctrl code -- interrupt XDS
            xds[field] = 0;
            xds_packet = 0;
        }
        else
        {
            xds[field] = 1;
            xds_packet = 1;
        }

        if (xds_packet)
            goto skip;
    }

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
                            if ((tc - lastclr[mode]) > 5000 ||
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
                            else if ((tc - lastclr[mode]) > 5000 ||
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
        if (((tc - timecode[mode]) > 100) &&
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

void CCDecoder::DecodeVPS(const uint8_t *buf)
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

void CCDecoder::DecodeWSS(const uint8_t *buf)
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
