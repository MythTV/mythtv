// -*- Mode: c++ -*-
/*******************************************************************
 * CrcIpNetworkRecorder
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

#include <ctime>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "videodev_myth.h"

#include "mythcontext.h"
#include "RingBuffer.h"
#include "programinfo.h"

#include "tspacket.h"
#include "pespacket.h"
#include "mpegtables.h"

#include "crcipnetworkrecorder.h"

#include "../libavformat/avformat.h"

/*

This type of "Capture Card" will record MPEG-2 Transport Stream feeds
over UDP/IP. This recorder supports MPEG1/MPEG2/MPEG4-AVC video.

1. USAGE
--------
To use this recorder in MythTV, start mythtv-setup and:
- Create the a new "CRC IP Network Recorder" capture card.
- Create a new video source with no listing grabber.
- Create manually a new channel in the channel editor and set the video source
  to the one you just created.
- Set the CRC_IP->MPEG2TS input to the new video source and make sure the
  "Starting channel" is set to the the channel number of the newly created
  channel.

The easiest way to try the IP streaming recorder is to use the VLC player
(www.videolan.org) to stream the content of a file (MPEG1 or MPEG2), DVD or VCD
to a UDP address using MPEG-TS transport. You can also use the H264 transcoder
included in VLC if you have installed the mythtv_crc_h264 patch to MythTV. H264
transcoding seems to work correctly only from VLC 0.8.5-test3. A H264 video is
known to work with mp3 audio only, AAC support in MythTV is not working.

Here are some examples of VLC usage to test the Network input. These examples
are for the VLC player for Win32 but it should be the same for the linux version
except for the paths. Any movie DVD can be used in these examples. The IP
destination 192.168.1.100:1234 can be changed for your network configuration:

To stream the content of a DVD:
  vlc dvdsimple://dev/dvd :sout=#duplicate{dst=std{access=udp,mux=ts,dst=192.168.1.100:1234}}

To stream the content of a DVD transcoded in realtime to h264 (fast cpu needed!):
  vlc dvdsimple://dev/dvd :sout=#transcode{vcodec=h264,vb=512,scale=0.5,acodec=mp3,ab=128,channels=2}:duplicate{dst=std{access=udp,mux=ts,dst=192.168.1.100:1234}}

To transcode a DVD to h264 in a file named test.ts and then stream it:
  vlc dvdsimple://dev/dvd :sout=#transcode{vcodec=h264,vb=512,scale=0.5,acodec=mp3,ab=128,channels=2}:duplicate{dst=std{access=file,mux=ts,dst="test.ts"}}
  vlc "test.ts" :sout=#duplicate{dst=std{access=udp,mux=ts,dst=192.168.1.100:1234}}

 */

/** \class CRCIpNetworkRecorder
 *  \brief Records MPEG-2 Transport Stream feeds over UDP/IP,
 *         with H.264 support.
 *
 *  \TODO Support for RTP encapsulation.
 */

CRCIpNetworkRecorder::CRCIpNetworkRecorder(TVRec *rec)
    : DTVRecorder(rec),
      url_context(0),
      network_buffer(0),
      seen_pat_packet(false),
      pmt_id(0),
      video_pid(0),
      video_stream_type(0)
{
    _buffer_size = TSPacket::SIZE * 50 * 1024;
    if ((_buffer = new unsigned char[_buffer_size]))
        bzero(_buffer, _buffer_size);

    QMutexLocker locker(&avcodeclock);
    av_register_all();
}

CRCIpNetworkRecorder::~CRCIpNetworkRecorder()
{
    Close();

    if (_buffer)
    {
        delete [] _buffer;
        _buffer = 0;
    }

    if (network_buffer)
    {
        delete [] network_buffer;
        network_buffer = 0;
    }
}

void CRCIpNetworkRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                                 const QString &videodev,
                                                 const QString &audiodev,
                                                 const QString &vbidev)
{
    (void)audiodev;
    (void)vbidev;
    (void)profile;

    SetOption("videodevice", videodev);
    SetOption("tvformat", gContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gContext->GetSetting("VbiFormat"));
}

/** \fn CRCIpNetworkRecorder::Open(void)
 *  \brief Opens a vloopback pipe output device
 *
 */
bool CRCIpNetworkRecorder::Open(void)
{
    VERBOSE(VB_RECORD, "CRCIpNetworkRecorder: opening \""
            << videodevice << "\"");

    avcodeclock.lock();
    int err = url_open(&url_context, videodevice.ascii(), URL_RDONLY);
    avcodeclock.unlock();

    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder: "
                "failed to open the avformat URLContext" + ENO);
        return false;
    }

    _stream_fd = udp_get_file_handle(url_context);
    if (_stream_fd < 0)
    {
        VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder: "
                "failed to get the URLContext's file descriptor" + ENO);
        return false;
    }

    // initialize MPEG-2 TS state variables
    seen_pat_packet = false;
    pmt_id = 0;
    video_pid = 0;
    video_stream_type = 0;

    return true;
}

void CRCIpNetworkRecorder::Close(void)
{
    if (url_context)
    {
        url_close(url_context);
        url_context = 0;
        _stream_fd = -1;
    }
}

void CRCIpNetworkRecorder::Reset(void)
{
    VERBOSE(VB_RECORD, "CRCIpNetworkRecorder: resetting");

    seen_pat_packet = false;
    pmt_id = 0;
    video_pid = 0;
    video_stream_type = 0;

    DTVRecorder::Reset();
}

void CRCIpNetworkRecorder::StartRecording(void)
{
    VERBOSE(VB_RECORD, "CRCIpNetworkRecorder::StartRecording -- begin");

    if (!Open())
    {
        VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder::StartRecording "
                "-- Open failed");
        _error = true;
        return;
    }

    _request_recording = true;
    _recording = true;

    struct timeval tv;
    fd_set rdset;
    ssize_t bytes_read;
    ssize_t unprocessed_bytes = 0;

    while (_request_recording)
    {
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset);
        FD_SET(_stream_fd, &rdset);

        // wait until we can read something
        switch (select(_stream_fd + 1, &rdset, NULL, NULL, &tv))
        {
            case -1:
                if (errno == EINTR)
                    continue;

                VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder: "
                        "select error" + ENO);

                continue;

            case 0:
                VERBOSE(VB_RECORD, "CRCIpNetworkRecorder: select timeout");
                continue;

            default: break;
        }

        bytes_read = url_read(url_context, _buffer + unprocessed_bytes,
                              _buffer_size - unprocessed_bytes);

        if (bytes_read < 0 && errno != EAGAIN)
        {
            VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder: "
                    "failed to read from the URLContext" + ENO);
            continue;
        }
        else if (bytes_read == 0)
            continue;

        bytes_read += unprocessed_bytes;
        unprocessed_bytes = ProcessData(_buffer, bytes_read);
        if (unprocessed_bytes > 0)
        {
            ssize_t ts_remainder = unprocessed_bytes % TSPacket::SIZE;
            if (ts_remainder > 0)
                unprocessed_bytes += ts_remainder;

            memmove(_buffer, &(_buffer[_buffer_size - unprocessed_bytes]),
                    unprocessed_bytes);
        }
    }

    FinishRecording();
    _recording = false;
    VERBOSE(VB_RECORD, "CRCIpNetworkRecorder::StartRecording -- end");
}

ssize_t CRCIpNetworkRecorder::ProcessData(unsigned char *buffer, ssize_t len)
{
    ssize_t pos = 0;

    // process each packet
    while (pos + TSPacket::SIZE - 1 < len && _request_recording)
    {
        const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[pos]);
        pos += TSPacket::SIZE;

        // if we don't know about the video program yet,
        // we can't do keyframe processing
        if (video_pid == 0)
        {
            // the PAT and PMT will always be on start packets
            if (!pkt->PayloadStart())
            {
                if (seen_pat_packet)
                    ringBuffer->Write(pkt->data(), TSPacket::SIZE);

                continue;
            }

            // find the PAT and the PMT table (once we've seen the PAT)
            if (pkt->PID() == 0x0 && !seen_pat_packet)
            {
                // alright, this packet should have the PAT
                PSIPTable psipt(*pkt);
                ProgramAssociationTable pat(psipt);
                if (!pat.IsGood())
                    continue;

                // make sure we have at least one program
                if (pat.ProgramCount() == 0)
                {
                    VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder: "
                            "MPEG-2 TS stream contains no programs, aborting");
                    StopRecording();
                    _error = true;
                    continue;
                }

                // get the PID for the PMT of the first program
                pmt_id = pat.ProgramPID(0);
                VERBOSE(VB_RECORD, "CRCIpNetworkRecorder: PMT PID is "
                        << pmt_id);

                // enable packet writing (e.g. we start recording at a PAT)
                seen_pat_packet = true;
            }
            else if (pkt->PID() == pmt_id)
            {
                // alright, this packet should have the PMT
                PSIPTable psipt(*pkt);
                ProgramMapTable pmt(psipt);
                if (!pmt.IsGood())
                    continue;

                // find the the first video element's PID
                uint stream_count = pmt.StreamCount();
                uint stream_index = 0;
                for (; stream_index < stream_count; stream_index++)
                {
                    if (pmt.IsVideo(stream_index))
                    {
                        video_pid = pmt.StreamPID(stream_index);
                        video_stream_type = pmt.StreamType(stream_index);

                        ringBuffer->Sync();

                        VERBOSE(VB_RECORD, "CRCIpNetworkRecorder: "
                                "video elementary stream PID is "
                                << video_pid);

                        VERBOSE(VB_RECORD, "CRCIpNetworkRecorder: "
                                "video elementary stream type is "
                                << video_stream_type);

                        break;
                    }
                }

                // we don't support TS streams with no video
                // (or an unknown kind of video)
                if (stream_index == stream_count)
                {
                    VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder: "
                            "MPEG program has no video element, aborting");
                    StopRecording();
                    _error = true;
                    continue;
                }

                // we only support MPEG 1, MPEG 2 or H.264 video
                if (video_stream_type == StreamID::MPEG4Video || 
                    video_stream_type == StreamID::OpenCableVideo)
                {
                    VERBOSE(VB_IMPORTANT, "CRCIpNetworkRecorder: "
                            "video element uses an unsupported type of "
                            "MPEG compression, aborting");
                    StopRecording();
                    _error = true;
                    continue;
                }
            }

            if (seen_pat_packet)
                ringBuffer->Write(pkt->data(), TSPacket::SIZE);

            continue;
        }

        unsigned long long cnt = _frames_seen_count;
        if (pkt->PID() == video_pid)
        {
            if (video_stream_type == StreamID::MPEG1Video || 
                video_stream_type == StreamID::MPEG2Video)
            {
                FindMPEG2Keyframes(pkt);
            }
            else
            {
                FindH264Keyframes(pkt);
            }
        }

        // sync when a new frame has been found (with flooding control)
        if (cnt != _frames_seen_count)
            if (_frames_seen_count < 20 || _frames_seen_count % 20 == 0)
                ringBuffer->Sync();

        // write the packet out
        // FIXME: filter packets based on the selected program?
        // DROP VIDEO PACKETS when the codec is H.264 until we see the SPS
        if (video_stream_type != StreamID::H264Video || _seen_sps)
            ringBuffer->Write(pkt->data(), TSPacket::SIZE);
    }

    return len - pos;
}

void CRCIpNetworkRecorder::FinishRecording(void)
{
    VERBOSE(VB_RECORD, "CRCIpNetworkRecorder::FinishRecording()");
    DTVRecorder::FinishRecording();
}

