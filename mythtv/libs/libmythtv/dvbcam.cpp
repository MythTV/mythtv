/*
 * Class DVBCam
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      Jesper Sorensen
 *          - Changed to work with Taylor Jacob's DVB rewrite
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
#include <qvariant.h>

#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdio>
using namespace std;

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/dvb/ca.h>

#include "recorderbase.h"

#include "dvbdev.h"

#include "dvbcam.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

#define LOC_ERR QString("DVB#%1 CA Error: ").arg(cardnum)
#define LOC QString("DVB#%1 CA: ").arg(cardnum)

DVBCam::DVBCam(int cardNum)
    : cardnum(cardNum),       numslots(0),
      ciHandler(NULL),
      exitCiThread(false),    ciThreadRunning(false),
      have_pmt(false),        pmt_sent(false),
      pmt_updated(false),     pmt_added(false)
{
    int cafd = open(dvbdevice(DVB_DEV_CA, cardnum), O_RDWR);
    if (cafd >= 0)
    {
        ca_caps_t caps;
        ioctl(cafd, CA_GET_CAP, &caps);
        numslots = caps.slot_num;
        close(cafd);
    }
}

DVBCam::~DVBCam()
{
    Stop();
}

bool DVBCam::Start()
{
    if (numslots == 0)
        return false;

    exitCiThread = false;
    have_pmt     = false;
    pmt_sent     = false;
    pmt_updated  = false;
    pmt_added    = false;

    ciHandler = cCiHandler::CreateCiHandler(dvbdevice(DVB_DEV_CA, cardnum));
    if (!ciHandler)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to initialize CI handler");
        return false;
    }

    if (pthread_create(&ciHandlerThread, NULL, CiHandlerThreadHelper, this))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create CI handler thread");
        return false;
    }

    ciThreadRunning = true;

    VERBOSE(VB_CHANNEL, LOC + "CI handler successfully initialized!");

    return true;
}

bool DVBCam::Stop()
{    
    if (ciThreadRunning)
    {
        exitCiThread = true;
        pthread_join(ciHandlerThread, NULL);
    }

    if (ciHandler)
    {
        delete ciHandler;
        ciHandler = NULL;
    }

    return true;
}

void *DVBCam::CiHandlerThreadHelper(void *dvbcam)
{
    ((DVBCam*)dvbcam)->CiHandlerLoop();
    return NULL;
}

void DVBCam::HandleUserIO(void)
{
    cCiEnquiry* enq = ciHandler->GetEnquiry();
    if (enq != NULL)
    {
        if (enq->Text() != NULL)
            VERBOSE(VB_CHANNEL, LOC + "CAM: Received message: " +
                    enq->Text());
        delete enq;
    }

    cCiMenu* menu = ciHandler->GetMenu();
    if (menu != NULL)
    {
        if (menu->TitleText() != NULL)
            VERBOSE(VB_CHANNEL, LOC + "CAM: Menu Title: " +
                    menu->TitleText());
        if (menu->SubTitleText() != NULL)
            VERBOSE(VB_CHANNEL, LOC + "CAM: Menu SubTitle: " +
                    menu->SubTitleText());
        if (menu->BottomText() != NULL)
            VERBOSE(VB_CHANNEL, LOC + "CAM: Menu BottomText: " +
                    menu->BottomText());

        for (int i=0; i<menu->NumEntries(); i++)
            if (menu->Entry(i) != NULL)
                VERBOSE(VB_CHANNEL, LOC + "CAM: Menu Entry: " +
                        menu->Entry(i));

        if (menu->Selectable())
        {
            VERBOSE(VB_CHANNEL, LOC + "CAM: Menu is selectable");
        }

        if (menu->NumEntries() > 0)
        {
            VERBOSE(VB_CHANNEL, LOC +
                    "CAM: Selecting first entry");
            menu->Select(0);
        }
        else
        {
            VERBOSE(VB_CHANNEL, LOC + "CAM: Cancelling menu");
        }

        delete menu;
    }
}

void DVBCam::HandlePMT(void)
{
    VERBOSE(VB_CHANNEL, LOC + "CiHandler needs CA_PMT");
    QMutexLocker locker(&pmt_lock);

    if (pmt_sent && pmt_added && !pmt_updated)
    {
        // Send added PMT
        while (PMTAddList.size() > 0)
        {
            PMTObject pmt = PMTAddList.front();
            PMTAddList.pop_front();

            SendPMT(pmt, CPLM_ADD);
            PMTList.push_back(pmt);
        }

        pmt_updated = false;
        pmt_added   = false;
        return;
    }

    // Grab any added PMT
    while (PMTAddList.size() > 0)
    {
        PMTList.push_back(PMTAddList.front());
        PMTAddList.pop_front();
    }

    uint length = PMTList.size();
    uint count  = 0;

    pmt_list_t::const_iterator pmtit;
    for (pmtit = PMTList.begin(); pmtit != PMTList.end(); ++pmtit)
    {
        uint cplm = (count     == 0)      ? CPLM_FIRST : CPLM_MORE;
        cplm      = (count + 1 == length) ? CPLM_LAST  : cplm;
        cplm      = (length    == 1)      ? CPLM_ONLY  : cplm;

        SendPMT(*pmtit, cplm);

        count++;
    }

    pmt_sent    = true;
    pmt_updated = false;
    pmt_added   = false;
}

void DVBCam::CiHandlerLoop()
{
    VERBOSE(VB_CHANNEL, LOC + "CI handler thread running");

    while (!exitCiThread)
    {
        if (ciHandler->Process())
        {
            if (ciHandler->HasUserIO())
                HandleUserIO();

            bool handle_pmt  = pmt_sent && (pmt_updated || pmt_added);
            handle_pmt      |= have_pmt && ciHandler->NeedCaPmt();

            if (handle_pmt)
                HandlePMT();
        }
        usleep(250);
    }
    
    ciThreadRunning = false;
    VERBOSE(VB_CHANNEL, LOC + "CiHandler thread stopped");
}

void DVBCam::SetPMT(const PMTObject *pmt)
{
    VERBOSE(VB_CHANNEL, LOC + "SetPMT for ServiceID = " << pmt->ServiceID);

    QMutexLocker locker(&pmt_lock);
    PMTList.clear();
    PMTList.push_back(*pmt);

    have_pmt    = true;
    pmt_updated = true;
}

void DVBCam::AddPMT(const PMTObject *pmt)
{
    VERBOSE(VB_CHANNEL, LOC + "AddPMT for ServiceID = " << pmt->ServiceID);

    QMutexLocker locker(&pmt_lock);
    PMTAddList.push_back(*pmt);
    pmt_added = true;
}

static const char *cplm_info[] =
{
    "CPLM_MORE",
    "CPLM_FIRST",
    "CPLM_LAST",
    "CPLM_ONLY",
    "CPLM_ADD",
    "CPLM_UPDATE"
};

cCiCaPmt CreateCAPMT(const PMTObject&, const unsigned short*, uint);

/*
 * Send a CA_PMT object to the CAM (see EN50221, section 8.4.3.4)
 */
void DVBCam::SendPMT(const PMTObject &pmt, uint cplm)
{
    for (uint s = 0; s < (uint)ciHandler->NumSlots(); s++)
    {
        const unsigned short *casids = ciHandler->GetCaSystemIds(s);

        if (!casids)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "GetCaSystemIds returned NULL! " +
                    QString("(Slot #%1)").arg(s));
            continue;
        }

        if (!casids[0])
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "CAM supports no CA systems! " +
                    QString("(Slot #%1)").arg(s));
            continue;
        }

        VERBOSE(VB_CHANNEL, LOC + "Creating CA_PMT, ServiceID = "
                << pmt.ServiceID);

        cCiCaPmt capmt = CreateCAPMT(pmt, casids, cplm);

        VERBOSE(VB_CHANNEL, LOC +
                QString("Sending CA_PMT with %1 to CI slot #%2")
                .arg(cplm_info[cplm]).arg(s));

        if (!ciHandler->SetCaPmt(capmt, s))
            VERBOSE(VB_CHANNEL, LOC + "CA_PMT send failed!");
    }
}

cCiCaPmt CreateCAPMT(const PMTObject &pmt,
                     const unsigned short *casids,
                     uint cplm)
{
    cCiCaPmt capmt(pmt.ServiceID, cplm);

    // Add CA descriptors for the service
    CAList::const_iterator ca;
    for (ca = pmt.CA.begin(); ca != pmt.CA.end(); ++ca)
    {
        for (uint q = 0; casids[q]; q++)
        {
            if ((*ca).CASystemID != casids[q])
                continue;

            VERBOSE(VB_CHANNEL, "Adding CA descriptor: " +
                    QString("CASID=0x%1, ECM PID=%2")
                    .arg((*ca).CASystemID, 0, 16).arg((*ca).PID));
            capmt.AddCaDescriptor((*ca).CASystemID, (*ca).PID,
                                  (*ca).Data_Length, (*ca).Data);
        }
    }

    // Add elementary streams + CA descriptors
    QValueList<ElementaryPIDObject>::const_iterator es;
    for (es = pmt.Components.begin(); es != pmt.Components.end(); ++es)
    {
        if (!(*es).Record)
            continue;

        VERBOSE(VB_CHANNEL, QString("Adding elementary stream: %1, PID=%2")
                .arg((*es).GetDescription()).arg((*es).PID));
            
        capmt.AddElementaryStream((*es).Orig_Type, (*es).PID);

        for (ca = (*es).CA.begin(); ca != (*es).CA.end(); ++ca)
        {
            for (uint q = 0; casids[q]; q++)
            {
                if ((*ca).CASystemID != casids[q])
                    continue;

                VERBOSE(VB_CHANNEL, "Adding elementary CA descriptor: " +
                        QString("CASID = 0x%1, ECM PID = %2")
                        .arg((*ca).CASystemID, 0, 16).arg((*ca).PID));

                capmt.AddCaDescriptor((*ca).CASystemID, (*ca).PID,
                                      (*ca).Data_Length, (*ca).Data);
            }
        }
    }
    return capmt;
}
