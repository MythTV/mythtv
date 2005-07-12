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

#include "mythcontext.h"
#include "videosource.h"
#include "frequencies.h"
#include "channel.h"
#include "channelutil.h"

#include "analogscan.h"

AnalogScan::AnalogScan(unsigned _sourceid,unsigned _cardid) :
    fRunning(false),
    fStop(false),
    sourceid(_sourceid),
    cardid(_cardid)
{
}

AnalogScan::~AnalogScan()
{
}

void AnalogScan::stop()
{
    if (fRunning)
    {
        fStop = true;
        pthread_join(thread, NULL);
    }
}

void *AnalogScan::spawn(void *param)
{
    AnalogScan *_this = (AnalogScan*)param;
    _this->doScan();
    return NULL;
}

void AnalogScan::doScan()
{
    fRunning = true;
    QString device;
    if (CardUtil::GetVideoDevice(cardid, device))
    {
         Channel channel(NULL,device);
         if (channel.Open())
         {
             QString input = CardUtil::GetDefaultInput(cardid);
             struct CHANLIST *l = chanlists[nTable].list;
             int count = chanlists[nTable].count;
             for (int i = 0; i < count && !fStop; i++,l++)
             {
                 unsigned frequency = l->freq*1000;
                 channel.Tune(frequency,input);
                 usleep(200000); /* 0.2 sec */
                 if (channel.IsTuned())
                 {
                      //cerr << i << " " << l->name << " " << frequency <<  " Tuned " << endl;
                      QString name = QObject::tr("Channel %1").arg(l->name);
                      addChannel(i,l->name,name,l->freq);
                      emit serviceScanUpdateText(name);
                 }
                 emit serviceScanPCTComplete((i*100)/count);
             }
             channel.Close();
        }
    }
    emit serviceScanComplete();
}

bool AnalogScan::scan()
{
    int i = 0;
    char *listname = (char *)chanlists[i].name;
    QString table;

    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = QString("SELECT freqtable FROM videosource WHERE "
                               "sourceid = \"%1\";").arg(sourceid);
    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("analog scan freqtable", query);
    if (query.size() <= 0)
         return false;
    query.next();

    QString freqtable = query.value(0).toString();
//        cerr << "frequency table = " << freqtable.ascii() << endl; 

    if (freqtable == "default" || freqtable.isNull() || freqtable.isEmpty())
        table = gContext->GetSetting("FreqTable");
    else
        table = freqtable;

    nTable = 0;
    while (listname != NULL)
    {
        if (table == listname)
        {
           nTable = i;
           break;
        }
        i++;
        listname = (char *)chanlists[i].name;
    }

    if (!fRunning)
        pthread_create(&thread, NULL, spawn, this);
    while (!fRunning)
        usleep(50);
    return true;
}

void AnalogScan::addChannel(int number,const QString& channumber,
                            const QString& name, int frequency)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM channel WHERE channum=:CHANNUM AND "
                  "sourceid=:SOURCEID");
    query.bindValue(":CHANNUM",channumber);
    query.bindValue(":SOURCEID",sourceid);
    query.exec();

    query.prepare("INSERT INTO channel (chanid, channum, "
                   "sourceid, callsign, name, freqid )"
                   "VALUES (:CHANID,:CHANNUM,:SOURCEID,:CALLSIGN,"
                   ":NAME,:FREQID);");

    int chanid = ChannelUtil::CreateChanID(sourceid,QString::number(number));
    query.bindValue(":CHANID",chanid);
    query.bindValue(":CHANNUM",channumber);
    query.bindValue(":SOURCEID",sourceid);
    query.bindValue(":CALLSIGN",name);
    query.bindValue(":NAME",name);
    query.bindValue(":FREQID",frequency);

    if(!query.exec())
        MythContext::DBError("Adding new Channel", query);

    if (!query.isActive())
        MythContext::DBError("Adding new Channel", query);
}
