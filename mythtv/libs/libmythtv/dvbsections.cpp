/*
 * dvbsections.cpp: Digital Video Broadcast - Section/Table Parser
 *
 * Author:          Kenneth Aafloy
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * The author can be reached at ke-aa@frisurf.no
 *
 * The project's page is at http://www.mythtv.org
 *
 */

#include <qdatetime.h>

#include <iostream>
#include <vector>
#include <map>
using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include "recorderbase.h"

#include "dvbdev.h"

#include "dvbsections.h"
#include "dvbcam.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

DVBSections::DVBSections(int cardNum)
    : cardnum(cardNum)
{
    exitSectionThread = false;
    sectionThreadRunning = false;

    pmap.pat_version = 33;

    curpmtsize  = 0;
    curpmtbuf   = NULL;

    pthread_mutex_init(&pmap_lock, NULL);
}

DVBSections::~DVBSections()
{
    if (sectionThreadRunning)
        Stop();
}

void DVBSections::Start()
{
    pollLength = 2;
    pollArray = (pollfd*)malloc(sizeof(pollfd)*pollLength);

    struct dmx_sct_filter_params params;
    memset(&params, 0, sizeof(struct dmx_sct_filter_params));
    params.flags    = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

    for (int i=0; i<pollLength; i++)
    {
        pollArray[i].fd = open(dvbdevice(DVB_DEV_DEMUX, cardnum),
                                O_RDWR | O_NONBLOCK);
        if (pollArray[i].fd == -1)
            ERRNO("Failed to open DVBSections filter.");

        pollArray[i].events = POLLIN | POLLPRI;
        pollArray[i].revents = 0;

        switch(i)
        {
            case 0: params.pid = 0x0; break;
            case 1: params.pid = 0x0; break;
            case 2: params.pid = 0x11; break;
            case 3: params.pid = 0x12; break;
            case 4: params.pid = 0x13; break;
        }

        if (ioctl(pollArray[i].fd, DMX_SET_FILTER, &params) > 0)
            ERRNO("Failed to set DVBSections filter.");
    }

    pthread_create(&thread, NULL, ThreadHelper, this);
}

void DVBSections::Stop()
{
    exitSectionThread = true;
    while (sectionThreadRunning)
        usleep(50);

    for (int i=0; i<pollLength; i++)
    {
        if (pollArray[i].fd > 0)
            close(pollArray[i].fd);
    }

    free(pollArray);
}

void DVBSections::ChannelChanged(dvb_channel_t& chan)
{
    if (!sectionThreadRunning)
        Start();

    pthread_mutex_lock(&pmap_lock);

    chan_opts = chan;
    curprogram = chan_opts.serviceID;

    struct dmx_sct_filter_params params;
    memset(&params, 0, sizeof(struct dmx_sct_filter_params));
    params.flags    = DMX_IMMEDIATE_START;

    curpmtsize = 0;

    programs_t::iterator iter = pmap.programs.find(curprogram);
    if (iter != pmap.programs.end())
    {
        params.pid = iter->second.pmt_pid;
        pmap.programs[iter->first].version = 33;
    }
    else
        pmap.pat_version = 33;

    pollArray[1].events = POLLIN | POLLPRI;
    pollArray[1].revents = 0;
    if (ioctl(pollArray[1].fd, DMX_SET_FILTER, &params) == -1)
        ERRNO("Failed to set DVBSections filter");

    pthread_mutex_unlock(&pmap_lock);
}

void DVBSections::AllocateAndConvert(uint8_t*& buffer, int& len)
{
    uint8_t* buf = (uint8_t*)malloc(len + 1);

    memcpy(buf, buffer, len);

    uint8_t* bufptr = buf - 1;
    while (++bufptr <= buf + len)
        if (*bufptr < 0x20 || *bufptr > 0x7F)
            *bufptr = '_';

    buf[len] = 0;
    len += 1;

    if (buf[0] > 0x20)
    {
        buffer = buf;
        return;
    }

    // FIXME: This is where we should handle locale conversion.
    static bool warning_printed = false;
    if (!warning_printed)
    {
        warning_printed = true;
        WARNING("Conversion of locale is not yet implemented.");
    }

    buffer = buf;
}

void *DVBSections::ThreadHelper(void* cls)
{
    ((DVBSections*)cls)->ThreadLoop();
    return NULL;
}

void DVBSections::ThreadLoop()
{
    uint8_t    buffer[4096];
    tablehead_t head;

    sectionThreadRunning = true;

    while (!exitSectionThread)
    {
        int ret = poll(pollArray, pollLength, 100);

        if (ret == 0)
            continue;

        if (ret < 0)
        {
            ERRNO("Poll failed while waiting for Section");
            continue;
        }

        for (int i=0; i<pollLength; i++)
        {
            if (! (pollArray[i].revents & POLLIN || pollArray[i].revents & POLLPRI) )
                continue;

            int rsz = read(pollArray[i].fd, &buffer, 4096);

            if (rsz > 0)
            {
                head.table_id       = buffer[0];
                head.section_length = ((buffer[1] & 0x0f)<<8) | buffer[2];
                head.table_id_ext   = (buffer[3] << 8) | buffer[4];
                head.current_next   = (buffer[5] & 0x1);
                head.version        = ((buffer[5] >> 1) & 0x1f);
                head.section_number = buffer[6];
                head.section_last   = buffer[7];

                ParseTable(&head, &buffer[8], rsz - sizeof(tablehead_t));

                continue;
            }

            if (rsz == -1 && (errno == EOVERFLOW || errno == EAGAIN))
            {
                i--;
                usleep(10);
                continue;
            }

            if (rsz < 0)
            {
                ERRNO("Reading Section.");
            }

            pollArray[i].events = POLLIN | POLLPRI;
            pollArray[i].revents = 0;
        }

        usleep(50);
    }

    sectionThreadRunning = false;
}

/*****************************************************************************
                            Table Parsers
 *****************************************************************************/
void DVBSections::ParseTable(tablehead_t* head, uint8_t* buffer, int size)
{
    switch(head->table_id)
    {
        case 0:     ParsePAT(head, buffer, size); break;
        case 2:     ParsePMT(head, buffer, size); break;
        default:
            WARNING("Unknown table with id " << head->table_id);
            break;
    }
}

void DVBSections::ParsePAT(tablehead_t* head, uint8_t* buffer, int size)
{
    if (pmap.pat_version != head->version)
    {
        pthread_mutex_lock(&pmap_lock);
        int pos = -1;
        pmap.programs.clear();

        while (pos < (size - 4))
        {
            if (pos+4 >= (size-4))
            {
                pthread_mutex_unlock(&pmap_lock);
                return;
            }
            uint16_t program = (buffer[pos + 1] << 8) | buffer[pos + 2];
            uint16_t pid = ((buffer[pos + 3] & 0x1f)<<8) | buffer[pos + 4];
            pos += 4;

            pmap.programs[program].version = 33;
            pmap.programs[program].pmt_pid = pid;

            if (curprogram == program)
            {
                struct dmx_sct_filter_params params;
                memset(&params, 0, sizeof(struct dmx_sct_filter_params));

                curpmtsize = 0;

                params.pid = pid;
                params.filter.filter[0] = 0x2;
                params.filter.mask[0] = 0xff;
                params.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

                pollArray[1].events = POLLIN | POLLPRI;
                pollArray[1].revents = 0;
                if (ioctl(pollArray[1].fd, DMX_SET_FILTER, &params)<0)
                    ERRNO("Failed to set PMAP Filter");

                pmap.pat_version = head->version;
            }
        }
        pthread_mutex_unlock(&pmap_lock);
    }
}

void DVBSections::ParsePMT(tablehead_t* head, uint8_t* buffer, int size)
{
    // Store the current pmt, for common interface.
    if (curpmtsize == 0 && head->table_id_ext == curprogram)
    {
        pthread_mutex_lock(&pmap_lock);
        curpmtbuf = (uint8_t*)realloc(curpmtbuf, size - 4);
        memcpy(curpmtbuf, &buffer[4], size - 4);
        curpmtsize = size - 4;
        ERROR(QString("Saved %1 PMT bytes.").arg(size-4));
        pthread_mutex_unlock(&pmap_lock);

        ChannelChanged(chan_opts, curpmtbuf, curpmtsize);
    }
}

