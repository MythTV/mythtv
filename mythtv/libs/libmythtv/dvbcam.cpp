/*
 * Class DVBCam
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      Kenneth Aafloy
 *          - General Implementation
 *
 * Description:
 *      This Class has been developed from bits n' pieces of other
 *      projects.
 *
 *
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

DVBCam::DVBCam(int cardNum): cardnum(cardNum)
{
    exitCiThread = false;
    ciThreadRunning = false;
    noCardSupport = false;
}

DVBCam::~DVBCam()
{
    if (ciThreadRunning)
        Close();

    if (ciHandler)
        delete ciHandler;
}

bool DVBCam::Open()
{
    ciHandler = cCiHandler::CreateCiHandler(dvbdevice(DVB_DEV_CA, cardnum));

    if (ciHandler == NULL)
    {
        ERRNO("CAM Support failed initialization.");
        return false;
    }

    GENERAL("CAM Support initialized successfully.");

    setCamProgramMapTable = false;

    pthread_create(&ciHandlerThread, NULL, CiHandlerThreadHelper, this);

    return true;
}

bool DVBCam::Close()
{    
    exitCiThread = true;
    while(ciThreadRunning)
        usleep(50);

    return true;
}

void DVBCam::ChannelChanged(dvb_channel_t& chan, uint8_t* pmt, int len)
{
    chan_opts = chan;
    pmtbuf = pmt;
    pmtlen = len;

    if (!ciThreadRunning && !noCardSupport)
        if (!Open())
            noCardSupport = true;

    if (chan.serviceID == 0)
    {
        ERROR("CAM: ServiceID is not set.");
        return;
    }

    setCamProgramMapTable = true;
    first = true;
}

void *DVBCam::CiHandlerThreadHelper(void*self)
{
    ((DVBCam*)self)->CiHandlerLoop();
    return NULL;
}

void DVBCam::CiHandlerLoop()
{
    ciThreadRunning = true;

    while (!exitCiThread)
    {
        usleep(250);
        if (!ciHandler->Process())
            continue;

        cCiCaPmt capmt(chan_opts.serviceID);

        capmt.AddCaDescriptor(pmtlen, pmtbuf);

        SetPids(capmt, chan_opts.pids);

        for (int i=0; i<ciHandler->NumSlots(); i++)
            ciHandler->SetCaPmt(capmt,i);
    }
    
    ciThreadRunning = false;
}

void DVBCam::SetPids(cCiCaPmt& capmt, dvb_pids_t& pids)
{
    for (unsigned int i=0; i<pids.audio.size(); i++)
        capmt.AddPid(pids.audio[i]);

    for (unsigned int i=0; i<pids.video.size(); i++)
        capmt.AddPid(pids.video[i]);

    for (unsigned int i=0; i<pids.teletext.size(); i++)
        capmt.AddPid(pids.teletext[i]);

    for (unsigned int i=0; i<pids.subtitle.size(); i++)
        capmt.AddPid(pids.subtitle[i]);

    for (unsigned int i=0; i<pids.pcr.size(); i++)
        capmt.AddPid(pids.pcr[i]);

    for (unsigned int i=0; i<pids.other.size(); i++)
        capmt.AddPid(pids.other[i]);
}

