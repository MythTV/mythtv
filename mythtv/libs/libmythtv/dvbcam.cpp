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

DVBCam::DVBCam(int cardNum): cardnum(cardNum)
{
    ciThreadRunning = false;
    ciHandler = NULL;

    pthread_mutex_init(&pmt_lock, NULL);

    int cafd;
    if ((cafd = open(dvbdevice(DVB_DEV_CA, cardnum), O_RDWR)) >= 0)
    {
        ca_caps_t caps;
        ioctl(cafd, CA_GET_CAP, &caps);
        numslots = caps.slot_num;
        close(cafd);
    }
    else
        numslots = 0;
}

DVBCam::~DVBCam()
{
    Stop();

    pthread_mutex_destroy(&pmt_lock);
}

bool DVBCam::Start()
{
    if (numslots == 0)
        return false;

    exitCiThread = false;
    have_pmt = false;
    pmt_sent = false;
    pmt_updated = false;
    pmt_added = false;

    ciHandler = cCiHandler::CreateCiHandler(dvbdevice(DVB_DEV_CA, cardnum));
    if (ciHandler == NULL)
    {
        ERROR("CA: Failed to initialize CI handler");
        return false;
    }

    if (pthread_create(&ciHandlerThread, NULL, CiHandlerThreadHelper, this))
    {
        ERROR("CA: Failed to create CI handler thread");
        return false;
    }

    ciThreadRunning = true;
    GENERAL("CA: CI handler successfully initialized!");

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

bool DVBCam::IsRunning()
{
    return ciThreadRunning;
}

void *DVBCam::CiHandlerThreadHelper(void*self)
{
    ((DVBCam*)self)->CiHandlerLoop();
    return NULL;
}

void DVBCam::CiHandlerLoop()
{
    GENERAL(QString("CA: CI handler thread running"));

    while (!exitCiThread)
    {
        if (ciHandler->Process())
        {
            if (ciHandler->HasUserIO())
            {
                cCiEnquiry* enq = ciHandler->GetEnquiry();
                if (enq != NULL)
                {
                    if (enq->Text() != NULL)
                        GENERAL(QString("CAM: Received message: %1").arg(enq->Text()));
                    delete enq;
                }

                cCiMenu* menu = ciHandler->GetMenu();
                if (menu != NULL)
                {
                    if (menu->TitleText() != NULL)
                        GENERAL(QString("CAM: Menu Title: %1").arg(menu->TitleText()));
                    if (menu->SubTitleText() != NULL)
                        GENERAL(QString("CAM: Menu SubTitle: %1").arg(menu->SubTitleText()));
                    if (menu->BottomText() != NULL)
                        GENERAL(QString("CAM: Menu BottomText: %1").arg(menu->BottomText()));

                    for (int i=0; i<menu->NumEntries(); i++)
                        if (menu->Entry(i) != NULL)
                            GENERAL(QString("CAM: Menu Entry: %1").arg(menu->Entry(i)));

                    if (menu->Selectable())
                    {
                        GENERAL(QString("CAM: Menu is selectable"));
                    }

                    if (menu->NumEntries() > 0)
                    {
                        GENERAL(QString("CAM: Selecting first entry"));
                        menu->Select(0);
                    }
                    else
                    {
                        GENERAL(QString("CAM: Cancelling menu"));
                    }

                    delete menu;
                }
            }

            if ((pmt_sent && (pmt_updated || pmt_added))
                || (have_pmt && ciHandler->NeedCaPmt()))
            {
                GENERAL(QString("CA: CiHandler needs CA_PMT"));
                pthread_mutex_lock(&pmt_lock);

                if (pmt_sent && pmt_added && !pmt_updated)
                {
                    // Send added PMT
                    while (PMTAddList.size() > 0)
                    {
                        PMTObject pmt = PMTAddList.first();
                        SendPMT(pmt, CPLM_ADD);
                        PMTList += pmt;
                        PMTAddList.pop_front();
                    }
                }
                else
                {
                    // Grab any added PMT
                    while (PMTAddList.size() > 0)
                    {
                        PMTList += PMTAddList.first();
                        PMTAddList.pop_front();
                    }

                    int length = PMTList.size();
                    int count = 0;

                    QValueList<PMTObject>::Iterator pmtit;
                    for (pmtit = PMTList.begin(); pmtit != PMTList.end(); ++pmtit)
                    {
                        uint8_t cplm;
                        if (length == 1)
                            cplm = CPLM_ONLY;
                        else if (count == 0)
                            cplm = CPLM_FIRST;
                        else if (count == length - 1)
                            cplm = CPLM_LAST;
                        else
                            cplm = CPLM_MORE;

                        SendPMT(*pmtit, cplm);

                        count++;
                    }

                    pmt_sent = true;
                }

                pmt_updated = false;
                pmt_added = false;
                pthread_mutex_unlock(&pmt_lock);
            }
        }
        usleep(250);
    }
    
    ciThreadRunning = false;
    GENERAL(QString("CA: CiHandler thread stopped"));
}

void DVBCam::SetPMT(const PMTObject *pmt)
{
    GENERAL(QString("CA: SetPMT for ServiceID=%1").arg(pmt->ServiceID));
    pthread_mutex_lock(&pmt_lock);
    PMTList.clear();
    PMTList += *pmt;
    have_pmt = true;
    pmt_updated = true;
    pthread_mutex_unlock(&pmt_lock);
}

void DVBCam::AddPMT(const PMTObject *pmt)
{
    GENERAL(QString("CA: AddPMT for ServiceID=%1").arg(pmt->ServiceID));
    pthread_mutex_lock(&pmt_lock);
    PMTAddList += *pmt;
    pmt_added = true;
    pthread_mutex_unlock(&pmt_lock);
}

/*
 * Send a CA_PMT object to the CAM (see EN50221, section 8.4.3.4)
 */
void DVBCam::SendPMT(PMTObject &pmt, uint8_t cplm)
{
    for (int s = 0; s < ciHandler->NumSlots(); s++)
    {
        const unsigned short *casids = ciHandler->GetCaSystemIds(s);
        if (!casids)
        {
            ERROR(QString("CA: GetCaSystemIds returned NULL! (Slot #%1)").arg(s));
            continue;
        }
        if (!*casids)
        {
            ERROR(QString("CA: CAM supports no CA systems! (Slot #%1)").arg(s));
            continue;
        }

        GENERAL(QString("CA: Creating CA_PMT, ServiceID=%1").arg(pmt.ServiceID));
        cCiCaPmt capmt(pmt.ServiceID, cplm);

        // Add CA descriptors for the service
        CAList::Iterator ca;
        for (ca = pmt.CA.begin(); ca!= pmt.CA.end(); ++ca)
        {
            for (int q = 0; casids[q]; q++)
            {
                if ((*ca).CASystemID == casids[q])
                {
                    GENERAL(QString("CA: Adding CA descriptor: CASID=0x%1, ECM PID=%2").arg((*ca).CASystemID, 0, 16).arg((*ca).PID));
                    capmt.AddCaDescriptor((*ca).CASystemID, (*ca).PID, (*ca).Data_Length, (*ca).Data);
                }
            }
        }

        // Add elementary streams + CA descriptors
        QValueList<ElementaryPIDObject>::Iterator es;
        for (es = pmt.Components.begin(); es != pmt.Components.end(); ++es)
        {
            if ((*es).Record)
            {
                GENERAL(QString("CA: Adding elementary stream: %1, PID=%2")
                        .arg((*es).GetDescription()).arg((*es).PID));
                capmt.AddElementaryStream((*es).Orig_Type, (*es).PID);

                for (ca = (*es).CA.begin(); ca != (*es).CA.end(); ++ca)
                {
                    for (int q = 0; casids[q]; q++)
                    {
                        if ((*ca).CASystemID == casids[q])
                        {
                            GENERAL(QString("CA: Adding elementary CA descriptor: CASID=0x%1, ECM PID=%2").arg((*ca).CASystemID, 0, 16).arg((*ca).PID));
                            capmt.AddCaDescriptor((*ca).CASystemID, (*ca).PID, (*ca).Data_Length, (*ca).Data);
                        }
                    }
                }
            }
        }

        char *cplm_info[] = { "CPLM_MORE", "CPLM_FIRST", "CPLM_LAST", "CPLM_ONLY", "CPLM_ADD", "CPLM_UPDATE" };
        GENERAL(QString("CA: Sending CA_PMT with %1 to CI slot #%2")
                    .arg(cplm_info[cplm])
                    .arg(s));
        if (!ciHandler->SetCaPmt(capmt, s))
        {
            GENERAL(QString("CA: CA_PMT send failed!"));
        }
    }
}

