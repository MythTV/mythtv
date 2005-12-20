/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4: 
 *
 * DVBSIParser.cpp: Digital Video Broadcast - Section/Table Parser
 *
 * Author(s):  Kenneth Aafloy (ke-aa@frisurf.no)
 *                - Wrote original code
 *             Taylor Jacob (rtjacob@earthlink.net)
 *                - Added NIT/EIT/SDT Scaning Code
 *                - Rewrote major portion of code
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
 * The project's page is at http://www.mythtv.org
 *
 */

#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <qvaluelist.h>
#include <qvaluestack.h>
using namespace std;

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include "recorderbase.h"

#include "dvbdev.h"
#include "dvbsiparser.h"

#include "dvbchannel.h"
#include "dvbrecorder.h"

#define LOC QString("DVBSIParser: ")
#define LOC_ERR QString("DVBSIParser, Error: ")

DVBSIParser::DVBSIParser(int cardNum, bool start_thread)
    : SIParser("DVBSIParser"),  cardnum(cardNum), 
      exitSectionThread(false), sectionThreadRunning(false),
      selfStartedThread(false), pollLength(0), pollArray(NULL),
      filterChange(false)
{
    GENERAL("DVB SI Table Parser Started");

    siparser_thread = PTHREAD_CREATE_JOINABLE;
    if (start_thread)
    {
        pthread_create(&siparser_thread, NULL, SystemInfoThread, this);
    }
}

DVBSIParser::~DVBSIParser()
{
    if (selfStartedThread)
    {
        StopSectionReader();
        pthread_join(siparser_thread, NULL);
        selfStartedThread = false;
    }
}

void DVBSIParser::deleteLater()
{
    disconnect(); // disconnect signals we may be sending...
    if (selfStartedThread)
    {
        StopSectionReader();
        pthread_join(siparser_thread, NULL);
        selfStartedThread = false;
    }
    
    SIParser::deleteLater();
}


/** \fn DVBSIParser::SystemInfoThread(void*)
 *  \brief Thunk that allows siparser_thread pthread to
 *         call DVBSIParser::StartSectionReader().
 */
void *DVBSIParser::SystemInfoThread(void *param)
{
    DVBSIParser *siparser = (DVBSIParser *)param;
    siparser->selfStartedThread = true;
    siparser->StartSectionReader();
    return NULL;
}

void DVBSIParser::AddPid(uint16_t pid, uint8_t mask, uint8_t filter,
                         bool CheckCRC, int bufferFactor)
{

    struct dmx_sct_filter_params params;
    int fd;

    int sect_buf_size = MAX_SECTION_SIZE * bufferFactor;

    SIPARSER(QString("Adding PID 0x%1 Filter 0x%2 Mask 0x%3 Buffer %4")
             .arg(pid, 4, 16).arg(filter, 2, 16)
             .arg(mask, 2, 16).arg(sect_buf_size));

    /* Set flag so other processes can get past pollLock */
    filterChange = true;
    pollLock.lock();

    filterChange = false;

    PIDFDMap::Iterator it;

    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
    {
       if (it.data().pid == pid)
       {
          pollLock.unlock();
          return;
       }
    }

    memset(&params, 0, sizeof(struct dmx_sct_filter_params));
    params.flags = DMX_IMMEDIATE_START;
    if (CheckCRC)
        params.flags |= DMX_CHECK_CRC;
    params.pid = pid;
    params.filter.filter[0] = filter;
    params.filter.mask[0] = mask;

    fd = open(dvbdevice(DVB_DEV_DEMUX, cardnum), O_RDWR | O_NONBLOCK);

    if (fd == -1)
    {
        ERRNO(QString("Failed to open section filter (pid %1)").arg(pid));
        pollLock.unlock();
        return;
    }

    if (ioctl(fd, DMX_SET_BUFFER_SIZE, sect_buf_size) < 0) {
        ERRNO(QString("Failed to set demux buffer size (pid %1)").arg(pid));
        pollLock.unlock();
        return;
    }

    if (ioctl(fd, DMX_SET_FILTER, &params) < 0)
    {
        ERRNO(QString("Failed to set section filter (pid %1)").arg(pid));
        pollLock.unlock();
        return;
    }

    /* Add filter to the Manager Object */
    PIDfilterManager[fd].pid = pid;

    /* Re-allocate memory and add to the pollarray */
    pollArray = (pollfd*)realloc((void*)pollArray,
                                 sizeof(pollfd) * (pollLength+1));
    pollArray[pollLength].fd = fd;
    pollArray[pollLength].events = POLLIN | POLLPRI;
    pollArray[pollLength].revents = 0;
    pollLength++;

    pollLock.unlock();

}

void DVBSIParser::DelPid(int pid)
{
    PIDFDMap::Iterator it;
    int x = 0;

    SIPARSER(QString("Deleting PID %1").arg(pid,4,16));

    filterChange = true;

    pollLock.lock();

    filterChange = false;

    QValueStack<PIDFDMap::Iterator> stack;
    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
    {
       if (it.data().pid == pid)
       {
          stack.push(it);
          pollLength--;
       }
    }

    if (stack.isEmpty())
       return;

    while (!stack.isEmpty())
    {
         it = stack.pop();
         close(it.key());
         PIDfilterManager.remove(it);
    }
    pollArray = (pollfd*)realloc((void*)pollArray,
                                 sizeof(pollfd) * (pollLength));
    /* Re-construct the pollarray */
    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
    {
       pollArray[x].fd = it.key();
       pollArray[x].events = POLLIN | POLLPRI;
       pollArray[x].revents = 0;
       x++;
    }

    pollLock.unlock();
}

void DVBSIParser::DelAllPids()
{
    PIDFDMap::Iterator it;

    filterChange = true;
    pollLock.lock();
    filterChange = false;

    for (it = PIDfilterManager.begin() ; it != PIDfilterManager.end() ; ++it)
        close(it.key());

    PIDfilterManager.clear();
    free(pollArray);
    pollLength = 0;
    pollArray = NULL;

    pollLock.unlock();

}

void DVBSIParser::StopSectionReader()
{

   SIPARSER("Stopping DVB Section Reader");
   exitSectionThread = true;
   DelAllPids();
   filterChange = true;
   pollLock.lock();
   filterChange = false;
   free(pollArray);
   pollLock.unlock();

}

void DVBSIParser::StartSectionReader()
{
    uint8_t    buffer[MAX_SECTION_SIZE];
    bool       processed = false;

    SIPARSER("Starting DVB Section Reader thread");

    sectionThreadRunning = true;

    while (!exitSectionThread)
    {

        CheckTrackers();

        if (!processed || filterChange)
           usleep(250);
        processed = false;

        pollLock.lock();

        int ret = poll(pollArray, pollLength, 1000);

        if (ret < 0)
        {
            if ((errno != EAGAIN) && (errno != EINTR))
                ERRNO("Poll failed while waiting for Section");
        }
        else if (ret > 0)
        {
            for (int i=0; i<pollLength; i++)
            {
                // FIXME: Handle POLLERR
                if (! (pollArray[i].revents & POLLIN || 
                       pollArray[i].revents & POLLPRI) )
                    continue;

                int rsz = read(pollArray[i].fd, &buffer, MAX_SECTION_SIZE);

                if (rsz > 0)
                {
                    ParseTable(buffer, rsz,
                               PIDfilterManager[pollArray[i].fd].pid);
                    processed = true;
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
        }

        pollLock.unlock();
    }

    sectionThreadRunning = false;
    SIPARSER("DVB Section Reader thread stopped");

}

