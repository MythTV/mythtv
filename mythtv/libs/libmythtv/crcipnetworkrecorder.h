// -*- Mode: c++ -*-
/*******************************************************************
 * CRCIpNetworkRecorder
 *
 * Copyright (C) Her Majesty the Queen in Right of Canada, 2006
 * Communications Research Centre (CRC)
 *
 * Distributed as part of MythTV (www.mythtv.org)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Contact:
 * Francois Lefebvre <francois [dot] lefebvre [at] crc [dot] ca>
 * Web: http://www.crc.ca
 * 
 * 2006/04 Jean-Francois Roy for CRC
 *    Initial release
 *
 * 2006/05 Jean-Michel Bouffard for CRC
 *    Move H264 related functions to dtvrecorder
 *
 ********************************************************************/

#ifndef CRCIPNETWORKRECORDER_H
#define CRCIPNETWORKRECORDER_H

#include "dtvrecorder.h"

struct URLContext;

class CRCIpNetworkRecorder : public DTVRecorder
{
  public:
    CRCIpNetworkRecorder(TVRec *rec);
    ~CRCIpNetworkRecorder();

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    bool Open(void);
    void Reset(void);
    void StartRecording(void);

  private:
    void Close(void);

    ssize_t ProcessData(unsigned char *buffer, ssize_t len);

    void FinishRecording(void);

    // UDP support (using libavformat)
    URLContext     *url_context;
    unsigned char  *network_buffer;

    // MPEG-2 Transport Stream support
    bool            seen_pat_packet;
    uint            pmt_id;
    uint            video_pid;
    uint            video_stream_type;
};

#endif // CRCIPNETWORKRECORDER_H

