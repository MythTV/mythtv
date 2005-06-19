/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *
 * Description:
 *     Collection of classes to provide dvb channel scanning
 *     functionallity
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

#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include <qmutex.h>
#include "mythcontext.h"
#include "videosource.h"
#include "frequencies.h"
#include "videodev_myth.h"

extern "C" {
#include "vbitext/vbi.h"
};

#include "analogscan.h"

AnalogScan::AnalogScan(unsigned _sourceid,unsigned _cardid) :
    fRunning(false),
    fStop(false),
    sourceid(_sourceid),
    cardid(_cardid)
{
    pthread_mutex_init(&lock, NULL);
    vbi=NULL;
}

AnalogScan::~AnalogScan()
{
    pthread_mutex_destroy(&lock);
}

bool AnalogScan::start()
{
    if (!CardUtil::videoDeviceFromCardID(cardid,device,vbidevice))
        return false;

    fd = ::open(device.ascii(),O_RDWR); 
    if (fd < 0)
        return false;

    pthread_mutex_lock(&lock);
    fRunning = true;
    fStop = false;
    pthread_mutex_unlock(&lock);

    pthread_create(&thread, NULL, spawn, this);
#ifdef DOVBI
    pthread_create(&threadVBI, NULL, spawnVBI, this);
#endif

    return true;
}

void AnalogScan::stop()
{
    bool _stop = false;
    pthread_mutex_lock(&lock);
    if (fRunning && !fStop)
        _stop = true; 
    fStop = true;
    pthread_mutex_unlock(&lock);

    if (_stop)
    {
        pthread_join(thread,NULL);
#ifdef DO_VBI
        pthread_join(threadVBI,NULL);
#endif
        if (fd >=0)
           ::close(fd);
   }
}

void AnalogScan::vbi_event(AnalogScan* _this, struct vt_event *ev)
{
    (void)_this;
    //cerr << "AnalogScan::vbi_event " << ev->type << "\n";
    switch (ev->type)
    {
    case EV_HEADER:
        break;
    case EV_XPACKET:
        break;
//    default:
//        cerr << "AnalogScan unkown " << ev->type << "\n";
    }
}

void *AnalogScan::spawn(void *param)
{
    AnalogScan *_this = (AnalogScan*)param;
    _this->doScan();
    return NULL;
}

void *AnalogScan::spawnVBI(void *param)
{
    AnalogScan *_this = (AnalogScan*)param;
    _this->doVBI();
    return NULL;
}

void AnalogScan::doVBI()
{
    vbi = ::vbi_open(vbidevice.ascii(), NULL, 99, -1);
    if (!vbi)
        return;
    VERBOSE(VB_IMPORTANT, QString("AnalogScan opened vbi device: %1").arg(vbidevice));
    vbi_add_handler(vbi, (void*)vbi_event,this);

    while(!fStop)
    {
        struct timeval tv;
        fd_set rdset;

        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        FD_ZERO(&rdset);
        FD_SET(vbi->fd, &rdset);

        switch (::select(vbi->fd + 1, &rdset, 0, 0, &tv))
        {
            case -1:
                  perror("vbi select");
                  continue;
            case 0:
                  //printf("vbi select timeout\n");
                  continue;
        }
        vbi_handler(vbi, vbi->fd);
    }
    vbi_del_handler(vbi, (void*)vbi_event,this);
    ::vbi_close(vbi);
}

void AnalogScan::doScan()
{
    struct CHANLIST *l = chanlists[nTable].list;
    int count = chanlists[nTable].count;
    for (int i = 0; i < count && !fStop; i++,l++)
    {
        //cerr << i << " " << l->name << " " << l->freq << endl;
        struct v4l2_frequency vf;
        memset(&vf, 0, sizeof(vf));
        vf.frequency = l->freq * 16 / 1000;
        vf.type = V4L2_TUNER_ANALOG_TV;

        ::ioctl(fd, VIDIOC_S_FREQUENCY, &vf);
        usleep(200000); /* 0.2 sec */
        if (isTuned())
        {
#ifdef DOVBI
             usleep(2000000); /* 0.2 sec */
#endif
             addChannel(l->name,l->freq);
             emit serviceScanUpdateText(QObject::tr("Channel %1").arg(l->name));
        }
        emit serviceScanPCTComplete((i*100)/count);
    }
    emit serviceScanComplete();
    pthread_mutex_lock(&lock);
    fRunning = false;
    pthread_mutex_unlock(&lock);
}

bool AnalogScan::scan(const QString& freqtable)
{
    int i = 0;
    char *listname = (char *)chanlists[i].name;
    QString table;

    if (freqtable == "default" || freqtable.isNull() || freqtable.isEmpty())
        table = gContext->GetSetting("FreqTable");
    else
        table = freqtable;

    while (listname != NULL)
    {
        if (table == listname)
            return scan(i);
        i++;
        listname = (char *)chanlists[i].name;
    }

    return scan(0);
}

bool AnalogScan::isTuned()
{
    struct v4l2_tuner tuner;

    memset(&tuner,0,sizeof(tuner));
    if (-1 == ::ioctl(fd,VIDIOC_G_TUNER,&tuner,0))
        return 0;
    return tuner.signal ? true : false;
}

bool AnalogScan::scan(unsigned table)
{
    nTable = table;
    return start();
}

int AnalogScan::generateNewChanID(int sourceID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString theQuery =
        QString("SELECT max(chanid) as maxchan "
                "FROM channel WHERE sourceid=%1").arg(sourceID);
    query.prepare(theQuery);

    if(!query.exec())
        MythContext::DBError("Calculating new ChanID", query);

    if (!query.isActive())
        MythContext::DBError("Calculating new ChanID for Analog Channel",query);

    query.next();

    // If transport not present add it, and move on to the next
    if (query.size() <= 0)
        return sourceID * 1000;

    int MaxChanID = query.value(0).toInt();

    if (MaxChanID == 0)
        return sourceID * 1000;
    else
        return MaxChanID + 1;
}

void AnalogScan::addChannel(const QString& name, int frequency)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM channel WHERE channum=:CHANNUM AND "
                  "sourceid=:SOURCEID");
    query.bindValue(":CHANNUM",name);
    query.bindValue(":SOURCEID",sourceid);
    query.exec();

    query.prepare("INSERT INTO channel (chanid, channum, "
                   "sourceid, callsign, name, freqid )"
                   "VALUES (:CHANID,:CHANNUM,:SOURCEID,:CALLSIGN,"
                   ":NAME,:FREQID);");

    query.bindValue(":CHANID",generateNewChanID(sourceid));
    query.bindValue(":CHANNUM",name);
    query.bindValue(":SOURCEID",sourceid);
    query.bindValue(":CALLSIGN",name);
    query.bindValue(":NAME",name);
    query.bindValue(":FREQID",frequency);

    if(!query.exec())
        MythContext::DBError("Adding new DVB Channel", query);

    if (!query.isActive())
        MythContext::DBError("Adding new DVB Channel", query);
}
