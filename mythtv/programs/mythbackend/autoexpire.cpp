#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qregexp.h>
#include <qstring.h>
#include <qdatetime.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include <sys/stat.h>
#include <sys/vfs.h>

#include "autoexpire.h"

#include "programinfo.h"
#include "libmyth/mythcontext.h"

AutoExpire::AutoExpire(bool runthread, QSqlDatabase *ldb)
{
    db = ldb;

    pthread_mutex_init(&expirerLock, NULL);

    threadrunning = runthread;

    if (runthread)
    {
        pthread_t expthread;
        pthread_create(&expthread, NULL, ExpirerThread, this);
        gContext->addListener(this);
    }
}

AutoExpire::~AutoExpire()
{
    if (threadrunning)
        gContext->removeListener(this);
}

void AutoExpire::RunExpirer(void)
{
    QString recordfileprefix = gContext->GetSetting("RecordFilePrefix");

    while (1)
    {
        pthread_mutex_lock(&expirerLock);

        struct statfs statbuf;
        int freespace = -1;
        if (statfs(recordfileprefix.ascii(), &statbuf) == 0) {
            freespace = statbuf.f_bfree / (1024*1024*1024/statbuf.f_bsize);
        }

        int minFree = gContext->GetNumSetting("AutoExpireDiskThreshold", 0);
        if ((minFree) && (freespace < minFree))
        {
            QString msg = QString("Running AutoExpire: Want %1 Gigs free but "
                                  "only have %2.")
                                  .arg(minFree).arg(freespace);
            VERBOSE(msg);

            FillExpireList();

            while ((freespace < minFree) &&
                   (expireList.size() > 0))
            {
                ProgramInfo *pginfo = expireList.back();

                QString msg = QString("AutoExpiring: %1 %2 %3 MBytes")
                                      .arg(pginfo->title)
                                      .arg(pginfo->startts.toString())
                                      .arg((int)(pginfo->filesize/1024/1024));
                VERBOSE(msg);

                QString message =
                           QString("AUTO_EXPIRE %1 %2")
                                   .arg(pginfo->chanid)
                                   .arg(pginfo->startts.toString(Qt::ISODate));
                MythEvent me(message);
                gContext->dispatchNow(me);

                delete pginfo;
                expireList.erase(expireList.end() - 1);

                if (statfs(recordfileprefix.ascii(), &statbuf) == 0)
                {
                    freespace =
                        statbuf.f_bfree / (1024*1024*1024/statbuf.f_bsize);
                }
            }

            if (freespace < minFree)
            {
                msg = QString("WARNING: Unable to free enough space, only %1 "
                              "Gigs free but need %2.")
                              .arg(freespace).arg(minFree);
            }
            else
            {
                msg = QString("Done AutoExpire, %1 Gigs now free.")
                              .arg(freespace);
            }

            VERBOSE(msg);
        }

        pthread_mutex_unlock(&expirerLock);

        sleep(gContext->GetNumSetting("AutoExpireFrequency", 1) * 60);
    }
} 

void *AutoExpire::ExpirerThread(void *param)
{
    AutoExpire *expirer = (AutoExpire *)param;
    expirer->RunExpirer();
 
    return NULL;
}

void AutoExpire::FillExpireList(void)
{
    int expMethod = gContext->GetNumSetting("AutoExpireMethod", 0);

    ClearExpireList();

    switch(expMethod)
    {
        case 1: FillOldestFirst(); break;

		// default falls through so list is empty so no AutoExpire
    }
}

void AutoExpire::PrintExpireList(void)
{
    cout << "MythTV AutoExpire List (programs listed last will expire first)\n";

    vector<ProgramInfo *>::iterator i = expireList.begin();
    for(; i != expireList.end(); i++)
    {
        ProgramInfo *first = (*i);
        QString title = first->title;

        if (first->subtitle != "")
            title += ": \"" + first->subtitle + "\"";

        cout << title.leftJustify(40, ' ', true) << " "
             << first->startts.toString().leftJustify(24, ' ', true) << "  "
             << first->filesize / 1024 / 1024 << " MBytes"
             << endl;
    }
}


void AutoExpire::ClearExpireList(void)
{
    while (expireList.size() > 0)
    {
        ProgramInfo *pginfo = expireList.back();
        delete pginfo;
        expireList.erase(expireList.end() - 1);
    }
}

void AutoExpire::FillOldestFirst(void)
{
    QString fileprefix = gContext->GetFilePrefix();
    QString querystr = QString(
               "SELECT recorded.chanid, starttime, endtime, title, subtitle, "
               "description, hostname, channum, name, callsign FROM recorded "
               "LEFT JOIN channel ON recorded.chanid = channel.chanid "
               "WHERE recorded.hostname = '%1' "
               "AND autoexpire > 0 "
               "ORDER BY autoexpire ASC, starttime DESC")
               .arg(gContext->GetHostName());

    QSqlQuery query = db->exec(querystr);

    if (query.isActive() && query.numRowsAffected() > 0)
        while (query.next()) {
            ProgramInfo *proginfo = new ProgramInfo;

            proginfo->chanid = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->title = QString::fromUtf8(query.value(3).toString());
            proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
            proginfo->description = QString::fromUtf8(query.value(5).toString());
            proginfo->hostname = query.value(6).toString();

            if (proginfo->hostname.isEmpty() || proginfo->hostname.isNull())
                proginfo->hostname = gContext->GetHostName();

            proginfo->conflicting = false;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";

            if (!query.value(7).toString().isNull())
            {
                proginfo->chanstr = query.value(7).toString();
                proginfo->channame = query.value(8).toString();
                proginfo->chansign = query.value(9).toString();
            }
            else
            {
                proginfo->chanstr = "#" + proginfo->chanid;
                proginfo->channame = "#" + proginfo->chanid;
                proginfo->chansign = "#" + proginfo->chanid;
            }

            proginfo->pathname = proginfo->GetRecordFilename(fileprefix);

            struct stat st;
            long long size = 0;
            if (stat(proginfo->pathname.ascii(), &st) == 0)
                size = st.st_size;

            proginfo->filesize = size;

            expireList.push_back(proginfo);
        }
}

