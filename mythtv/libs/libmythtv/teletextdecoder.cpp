/* =============================================================
 * File  : vbidecoder.cpp
 * Author: Frank Muenchow <beebof@gmx.de>
 *         Martin Barnasconi
 * Date  : 2005-10-25
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * ============================================================= */
#include <cstring>

#include <stdint.h>
#include <ctype.h>

extern "C" {
#include <inttypes.h>
#include "ivtv_myth.h"
#include "vbitext/vt.h"
}

using namespace std;

#include "osd.h"
#include "teletextdecoder.h"
#include "vbilut.h"
#include "mythplayer.h"
#include "mythverbose.h"

/******************************************************************/
//Decoder section
//

/** \fn TeletextDecoder::Decode(const unsigned char*, int)
 *  \brief Decodes teletext data
 *
 *  \param buf Points to the teletext data
 *  \param vbimode VBI-Mode (as defined in vbilut.h)
 */
void TeletextDecoder::Decode(const unsigned char *buf, int vbimode)
{
    int err = 0, latin1 = -1, zahl1, pagenum, subpagenum, lang, flags;
    uint magazine, packet, header;

    if (!m_player)
        return;

    m_player->LockOSD();

    if (!m_teletextviewer && m_player)
    {
        m_player->UnlockOSD();
        m_player->SetupTeletextViewer();
        return;
    }

    if (!m_teletextviewer)
    {
        VERBOSE(VB_VBI, "TeletextDecoder: No Teletext Viewer defined!");
        m_player->UnlockOSD();
        return;
    }

    m_decodertype = vbimode;

    switch (vbimode)
    {
        case VBI_IVTV:
            header = hamm16(buf, &err);

            if (err & 0xf000)
            {
                m_player->UnlockOSD();
                return; // error in data header
            }

            magazine = header & 7;
            packet = (header >> 3) & 0x1f;

            buf += 2;
            break;

        case VBI_DVB:
        case VBI_DVB_SUBTITLE:
            zahl1 = hamm84(buf,&err) * 16 + hamm84(buf+1,&err);

            magazine = 0;
            if (buf[0] & 0x40)
                magazine += 1;
            if (buf[0] & 0x10)
                magazine += 2;
            if (buf[0] & 0x04)
                magazine += 4;

            packet = 0;
            if (buf[0] & 0x01)
                packet += 1;
            if (buf[1] & 0x40)
                packet += 2;
            if (buf[1] & 0x10)
                packet += 4;
            if (buf[1] & 0x04)
                packet += 8;
            if (buf[1] & 0x01)
                packet += 16;

            if (err == 1)
            {
                m_player->UnlockOSD();
                return;  // error in data header
            }

            buf += 2;
            break;

        default:
            m_player->UnlockOSD();
            return; // error in vbimode
    }

    switch (packet)
    {
        case 0:  // Page Header
            int b1, b2, b3, b4;
            switch (vbimode)
            {
                case VBI_IVTV:
                    b1 = hamm16(buf, &err);// page number
                    b2 = hamm16(buf+2, &err);// subpage number + flags
                    b3 = hamm16(buf+4, &err);// subpage number + flags
                    b4 = hamm16(buf+6, &err);// language code + more flags
                    if (err & 0xf000)
                    {
                        m_player->UnlockOSD();
                        return;
                    }

                    break;

                case VBI_DVB:
                case VBI_DVB_SUBTITLE:
                    b1 = hamm84(buf+1, &err)*16+hamm84(buf, &err);
                    b2 = hamm84(buf+3, &err)*16+hamm84(buf+2, &err);
                    b3 = hamm84(buf+5, &err)*16+hamm84(buf+4, &err);
                    b4 = hamm84(buf+7, &err)*16+hamm84(buf+6, &err);
                    if (err == 1)
                    {
                        m_player->UnlockOSD();
                        return;
                    }

                    break;

                default:
                    m_player->UnlockOSD();
                    return; // error in vbimode
            }

            //VERBOSE(VB_VBI, QString("Page Header found: "
            //                        "Magazine %1, Page Number %2")
            //        .arg(magazine).arg(b1));

            subpagenum= (b2 + b3 * 256) & 0x3f7f;
            pagenum = (magazine?:8)*256 + b1;

            lang = "\0\4\2\6\1\5\3\7"[b4 >> 5] + (latin1 ? 0 : 8);
            flags = b4 & 0x1F;
            flags |= b3 & 0xC0;
            flags |= (b2 & 0x80) >> 2;
            m_teletextviewer->AddPageHeader(pagenum, subpagenum, buf,
                                            vbimode, lang, flags);

            break;

        default: // Page Data
            m_teletextviewer->AddTeletextData((magazine?:8), packet,
                                              buf, vbimode);
            break;
    }
    m_player->UnlockOSD();
}
