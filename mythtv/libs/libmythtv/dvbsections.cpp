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
#include <cstdlib>
#include <cstdio>
using namespace std;

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

    pollArray = NULL;
    pollLength = 0;

    pthread_mutex_init(&poll_lock, NULL);
    pthread_mutex_init(&pmap_lock, NULL);
}

DVBSections::~DVBSections()
{
    if (sectionThreadRunning)
        Stop();
}

// Need a filtermanager...
void DVBSections::AddPid(int pid)
{
    // FIXME: Check if pid already filtered.
    struct dmx_sct_filter_params params;
    int fd;

    memset(&params, 0, sizeof(struct dmx_sct_filter_params));
    params.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
    params.pid = pid;

    fd = open(dvbdevice(DVB_DEV_DEMUX, cardnum), O_RDWR | O_NONBLOCK);

    if (fd == -1)
    {
        ERRNO(QString("Failed to open section filter (pid %1)").arg(pid));
        return;
    }

    if (ioctl(fd, DMX_SET_FILTER, &params) < 0)
    {
        ERRNO(QString("Failed to set section filter (pid %1)").arg(pid));
        return;
    }

    pthread_mutex_lock(&poll_lock);
    
    pollArray = (pollfd*)realloc((void*)pollArray, sizeof(pollfd) * (pollLength+1));

    pollArray[pollLength].fd = fd;
    pollArray[pollLength].events = POLLIN | POLLPRI;
    pollArray[pollLength].revents = 0;
    pollLength++;

    pthread_mutex_unlock(&poll_lock);
}

void DVBSections::DelPid(int pid)
{
    pthread_mutex_lock(&poll_lock);
    (void)pid;
    pthread_mutex_unlock(&poll_lock);
}

void DVBSections::Start()
{
    AddPid(0);
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
    params.flags = DMX_IMMEDIATE_START;

    curpmtsize = 0;

    programs_t::iterator iter = pmap.programs.find(curprogram);
    if (iter != pmap.programs.end())
    {
        params.pid = iter->second.pmt_pid;
        pmap.programs[iter->first].version = 33;
    }
    else
        pmap.pat_version = 33;

    pollArray[0].events = POLLIN | POLLPRI;
    pollArray[0].revents = 0;

    if (ioctl(pollArray[0].fd, DMX_SET_FILTER, &params) < 0)
        ERRNO("Failed to set DVBSections filter");

    pthread_mutex_unlock(&pmap_lock);
}

void *DVBSections::ThreadHelper(void* cls)
{
    ((DVBSections*)cls)->ThreadLoop();
    return NULL;
}

void DVBSections::ThreadLoop()
{
    uint8_t    buffer[MAX_SECTION_SIZE];
    tablehead_t head;

    sectionThreadRunning = true;

    while (!exitSectionThread)
    {
        usleep(250);
        pthread_mutex_lock(&poll_lock);
        int ret = poll(pollArray, pollLength, 1000);

        if (ret == 0)
        {
            pthread_mutex_unlock(&poll_lock);
            continue;
        }

        if (ret < 0)
        {
            ERRNO("Poll failed while waiting for Section");
            pthread_mutex_unlock(&poll_lock);
            continue;
        }

        for (int i=0; i<pollLength; i++)
        {
            // FIXME: Handle POLLERR
            if (! (pollArray[i].revents & POLLIN || pollArray[i].revents & POLLPRI) )
            // FIXME: does this jump to for or while? it should go to for
                continue;

            int rsz = read(pollArray[i].fd, &buffer, MAX_SECTION_SIZE);

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

            if (rsz == -1 && errno == EAGAIN)
            {
                i--;
                continue;
            }

            if (rsz < 0)
            {
                ERRNO("Reading Section.");
            }

            pollArray[i].events = POLLIN | POLLPRI;
            pollArray[i].revents = 0;
        }
        pthread_mutex_unlock(&poll_lock);
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
            // Ignore other tables
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

                pollArray[0].events = POLLIN | POLLPRI;
                pollArray[0].revents = 0;
                if (ioctl(pollArray[0].fd, DMX_SET_FILTER, &params)<0)
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
        pthread_mutex_unlock(&pmap_lock);

        ChannelChanged(chan_opts, curpmtbuf, curpmtsize);
    }
}

