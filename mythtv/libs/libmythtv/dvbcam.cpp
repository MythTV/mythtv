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
#include <qsqldatabase.h>
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
    ciHandler = NULL;
    pmtbuf = cachedpmtbuf = NULL;
    pmtlen = 0;
    first_send = false;
    pthread_mutex_init(&pmt_lock, NULL);
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
        return false;

    GENERAL("CAM - Initialized successfully.");

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&ciHandlerThread, &attr, CiHandlerThreadHelper, this);

    return true;
}

bool DVBCam::Close()
{    
    exitCiThread = true;
    while(ciThreadRunning)
        usleep(50);

    return true;
}

void DVBCam::ChannelChanged(dvb_channel_t& chan)
{
    pthread_mutex_lock(db_lock);

    MythContext::KickDatabase(db);
    QSqlQuery query = db->exec(QString("SELECT pmtcache FROM dvb_channel"
                               " WHERE serviceid=%1").arg(chan.serviceID));

    if (!query.isActive())
    {
        MythContext::DBError("DVBCam PMT Query", query);
        return;
    }

    if (query.numRowsAffected() > 0)
    {
        query.next();
        if (!query.value(0).isNull())
        {
            pthread_mutex_lock(&pmt_lock);
            GENERAL("CAM - Cached PMT found.");
            QByteArray ba = query.value(0).toByteArray();
            if (cachedpmtbuf)
                delete cachedpmtbuf;
            chan_opts = chan;
            pmtbuf = cachedpmtbuf = new uint8_t [ba.size()];
            memcpy(pmtbuf, ba.data(), ba.size());
            pmtlen = ba.size();
            first_send = true;
            pthread_mutex_unlock(&pmt_lock);
        }
    }

    pthread_mutex_unlock(db_lock);
}

void DVBCam::ChannelChanged(dvb_channel_t& chan, uint8_t* pmt, int len)
{
    if (chan.serviceID == 0)
    {
        ERROR("CAM - ServiceID is not set.");
        return;
    }

    pthread_mutex_lock(&pmt_lock);

    bool setpmt = true;
    if (len == pmtlen && pmtbuf != NULL &&
        chan.serviceID == chan_opts.serviceID)
    {
        if (!memcmp(pmt, pmtbuf, len))
            setpmt = false;
    }

    if (setpmt)
    {
        chan_opts = chan;
        pmtbuf = pmt;
        pmtlen = len;
        first_send = true;
    }

    pthread_mutex_unlock(&pmt_lock);

    QByteArray ba;
    ba.resize(len);
    memcpy(ba.data(), pmt, len);

    pthread_mutex_lock(db_lock);
    QSqlQuery query(QString::null, db);
    query.prepare(QString("UPDATE dvb_channel SET pmtcache=?"
                  " WHERE serviceid=%1").arg(chan.serviceID));
    query.bindValue(0, QVariant(ba));

    MythContext::KickDatabase(db);
    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("DVBCam PMT Update", query);
        pthread_mutex_unlock(db_lock);
        return;
    }

    if (query.numRowsAffected() == 1)
        GENERAL("CAM - PMT cache updated.");

    pthread_mutex_unlock(db_lock);
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

        if (ciHandler->NeedCaPmt())
            first_send = true;

        if (ciHandler->HasUserIO())
        {
            cCiEnquiry* enq = ciHandler->GetEnquiry();
            if (enq != NULL)
            {
                if (enq->Text() != NULL)
                    cerr << "Message from CAM: " << enq->Text() << endl;
                delete enq;
            }

            cCiMenu* menu = ciHandler->GetMenu();
            if (menu != NULL)
            {
                if (menu->TitleText() != NULL)
                    cerr << "CAM Menu Title: " << menu->TitleText() << endl;
                if (menu->SubTitleText() != NULL)
                    cerr << "CAM Menu SubTitle: " << menu->SubTitleText() << endl;
                if (menu->BottomText() != NULL)
                    cerr << "CAM Menu BottomText: " << menu->BottomText() << endl;

                for (int i=0; i<menu->NumEntries(); i++)
                    if (menu->Entry(i) != NULL)
                        cerr << "CAM Menu Entry(" << i << "): " << menu->Entry(i) << endl;

                if (menu->Selectable())
                {
                    cerr << "Menu is selectable" << endl;
                }

                if (menu->NumEntries() > 0)
                {
                    cerr << "Selecting first entry" << endl;
                    menu->Select(0);
                }
                else
                {
                    cerr << "Canceling menu" << endl;
                    menu->Cancel();
                }

                delete menu;
            }

            first_send = true;
        }

        for (int s=0; s<ciHandler->NumSlots(); s++)
        {
            const unsigned short *caids = ciHandler->GetCaSystemIds(s);

            pthread_mutex_lock(&pmt_lock);
            if (caids != NULL && pmtbuf != NULL)
            {
                if (!first_send)
                {
                    pthread_mutex_unlock(&pmt_lock);
                    continue;
                }

                cCiCaPmt capmt(chan_opts.serviceID, CPLM_FIRST);

                /* FIXME:
                 * We have to make some kind of CAM association map like VDR has,
                 * for now we just clear the CAM that has no provider support.
                 */
                if (FindCaDescriptors(capmt, caids, s))
                {
                    SetPids(capmt, chan_opts.pids);
                    GENERAL(QString("CAM - Sending PMT to slot %1.").arg(s));
                    first_send = false;
                }

                ciHandler->SetCaPmt(capmt,s);
            }
            pthread_mutex_unlock(&pmt_lock);
        }
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

bool DVBCam::FindCaDescriptors(cCiCaPmt &capmt, const unsigned short *caids, int slot)
{
    bool found = false;
    uint16_t prg_len = ((*(pmtbuf+2)&0x0f)<<8) | *(pmtbuf+3);
    uint8_t *b = pmtbuf + 3;
    uint8_t *e = pmtbuf + 4 + prg_len;

    for ( ;; )
    {
        if (e+2 > pmtbuf+pmtlen)
            break;

        while ( prg_len > 0 && b+2 <= e )
        {
            uint8_t tag = *(b+1);
            uint8_t len = *(b+2);

            if (b+2+len > e)
                break;

            prg_len -= len - 2;

            if (tag == 0x09)
            {
                uint16_t ca_system_id = ((*(b+3))<<8) | *(b+4);
                uint16_t ca_pid = ((*(b+5))<<8) | *(b+6);
                fprintf(stderr,"Found CA Descriptor (CASID=%0.4X,"
                        " CAPID=%0.4X)\n", ca_system_id, ca_pid);

                for (int i=0; caids[i] != 0; i++)
                {
                    if (ca_system_id == caids[i] && ca_pid != 0)
                    {
                        if (slot == 1 && ca_system_id == 0x0B00)
                            continue;

                        if (first_send)
                            fprintf(stderr,"Adding CA Descriptor (CASID=%0.4X, "
                                    "CAPID=%0.4X)\n", ca_system_id, ca_pid);

                        capmt.AddCaDescriptor(len + 2, b+1);
                        found = true;
                    }
                }
            }
            b += len + 2;
        }

        if (e+5 > pmtbuf+pmtlen)
            break;

        prg_len = ((*(e+3)&0x0f)<<8) | *(e+4);
        b = e + 4;
        e = b + 1 + prg_len;
    }

    return found;
}

