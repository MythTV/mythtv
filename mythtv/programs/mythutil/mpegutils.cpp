// -*- Mode: c++ -*-
/**
 *   MPEG-TS processing utilities (for debugging.)
 *   Copyright (c) 2003-2004, Daniel Thor Kristjansson
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Author: Daniel Kristjansson
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// MythTV headers
#include "exitcodes.h"
#include "mythlogging.h"
#include "ringbuffer.h"

// Application local headers
#include "mpegutils.h"

#define SYNC_BYTE 0x47

static int resync_stream(
    const char *buffer, int curr_pos, int len, int packet_size)
{
    // Search for two sync bytes 188 bytes apart,
    int pos = curr_pos;
    int nextpos = pos + packet_size;
    if (nextpos >= len)
        return -1; // not enough bytes; caller should try again

    while (buffer[pos] != SYNC_BYTE || buffer[nextpos] != SYNC_BYTE)
    {
        pos++;
        nextpos++;
        if (nextpos == len)
            return -2; // not found
    }

    return pos;
}

static int pid_counter(const MythUtilCommandLineParser &cmdline)
{
    if (cmdline.toString("infile").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --infile option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString src = cmdline.toString("infile");

    RingBuffer *srcRB = RingBuffer::Create(src, false);
    if (!srcRB)
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Couldn't open input URL\n");
        return GENERIC_EXIT_NOT_OK;
    }

    uint packet_size = cmdline.toUInt("packetsize");
    if (packet_size == 0)
    {
        packet_size = 188;
    }
    else if (packet_size != 188 &&
             packet_size != (188+16) &&
             packet_size != (188+20))
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Invalid packet size %1, must be 188, 204, or 208\n")
            .arg(packet_size));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    const int kBufSize = 2 * 1024 * 1024;
    long long pid_count[0x2000];
    memset(pid_count, 0, sizeof(pid_count));
    char *buffer = new char[kBufSize];
    int offset = 0;
    long long total_count = 0;

    while (true)
    {
        int r = srcRB->Read(&buffer[offset], kBufSize - offset);
        if (r <= 0)
            break;
        int pos = 0;
        int len = offset + r;
        while (pos + 187 < len) // while we have a whole packet left
        {
            if (buffer[pos] != SYNC_BYTE)
            {
                pos = resync_stream(buffer, pos+1, len, packet_size);
                if (pos < 0)
                {
                    offset = 0;
                    break;
                }
            }
            int pid = ((buffer[pos+1]<<8) | buffer[pos+2]) & 0x1fff;
            pid_count[pid]++;
            pos += packet_size;
            total_count++;
        }

        if (len - pos > 0)
        {
            memcpy(buffer, buffer + pos, len - pos);
            offset = len - pos;
        }
        else
        {
            offset = 0;
        }
        LOG(VB_STDIO, logLevel,
            QString("\r                                            \r"
                    "Processed %1 packets")
            .arg(total_count));
    }
    LOG(VB_STDIO|VB_FLUSH, logLevel, "\n");

    delete[] buffer;
    delete srcRB;

    for (uint i = 0; i < 0x2000; i++)
    {
        if (pid_count[i])
        {
            LOG(VB_STDIO|VB_FLUSH, LOG_CRIT,
                QString("PID 0x%1 -- %2\n")
                .arg(i,4,16,QChar('0'))
                .arg(pid_count[i],11));
        }
    }

    return GENERIC_EXIT_OK;
}

static int pid_filter(const MythUtilCommandLineParser &cmdline)
{
    if (cmdline.toString("infile").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --infile option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString src = cmdline.toString("infile");

    if (cmdline.toString("outfile").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --outfile option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString dest = cmdline.toString("outfile");

    uint packet_size = cmdline.toUInt("packetsize");
    if (packet_size == 0)
    {
        packet_size = 188;
    }
    else if (packet_size != 188 &&
             packet_size != (188+16) &&
             packet_size != (188+20))
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Invalid packet size %1, must be 188, 204, or 208\n")
            .arg(packet_size));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    bool use_pid[0x2000];
    if (cmdline.toString("pids").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --pids option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    else
    {
        bool use_any_pid = false;
        QString pidsStr = cmdline.toString("pids");
        QStringList pidsList = pidsStr.split(",");
        for (uint i = 0; i < 0x2000 ; i++)
            use_pid[i] = false;
        for (uint i = 0; i < (uint) pidsList.size(); i++)
        {
            bool ok;
            uint tmp = pidsList[i].toUInt(&ok, 0);
            if (ok && (tmp < 0x2000))
                use_pid[tmp] = use_any_pid = true;
        }
        if (!use_any_pid)
        {
            LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
                "At least one pid must be specified\n");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    RingBuffer *srcRB = RingBuffer::Create(src, false);
    if (!srcRB)
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Couldn't open input URL\n");
        return GENERIC_EXIT_NOT_OK;
    }

    RingBuffer *destRB = RingBuffer::Create(dest, true);
    if (!destRB)
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Couldn't open output URL\n");
        delete srcRB;
        return GENERIC_EXIT_NOT_OK;
    }

    const int kBufSize = 2 * 1024 * 1024;
    char *buffer = new char[kBufSize];
    int offset = 0;
    long long total_count = 0;
    long long write_count = 0;

    while (true)
    {
        int r = srcRB->Read(&buffer[offset], kBufSize - offset);
        if (r <= 0)
            break;
        int pos = 0;
        int len = offset + r;
        while (pos + 187 < len) // while we have a whole packet left
        {
            if (buffer[pos] != SYNC_BYTE)
            {
                pos = resync_stream(buffer, pos+1, len, packet_size);
                if (pos < 0)
                {
                    offset = 0;
                    break;
                }
            }
            int pid = ((buffer[pos+1]<<8) | buffer[pos+2]) & 0x1fff;
            if (use_pid[pid])
            {
                destRB->Write(buffer+pos, packet_size);
                write_count++;
            }
            pos += packet_size;
            total_count++;
        }

        if (len - pos > 0)
        {
            memcpy(buffer, buffer + pos, len - pos);
            offset = len - pos;
        }
        else
        {
            offset = 0;
        }
        LOG(VB_STDIO|VB_FLUSH, logLevel,
            QString("\r                                            \r"
                    "Processed %1 packets")
            .arg(total_count));
    }
    LOG(VB_STDIO|VB_FLUSH, logLevel, "\n");

    delete[] buffer;
    delete srcRB;
    delete destRB;

    LOG(VB_STDIO|VB_FLUSH, logLevel, QString("Wrote %1 of %2 packets\n")
        .arg(write_count).arg(total_count));

    return GENERIC_EXIT_OK;
}

void registerMPEGUtils(UtilMap &utilMap)
{
    utilMap["pidcounter"] = &pid_counter;
    utilMap["pidfilter"]  = &pid_filter;
}
