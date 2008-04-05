#include "httprequest.h"
#include <qfileinfo.h>
#include <qregexp.h>
#include <qurl.h>
#include <qdir.h>
#include <limits.h>
#include <unistd.h>
#include "util.h"

#include <cstdlib>

#include "upnpmedia.h"

#define LOC QString("UPnpMedia: ")

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMedia::UPnpMedia(bool runthread, bool ismaster)
{

    if ((runthread) && (ismaster))
    {
        pthread_t upnpmediathread;
        pthread_create(&upnpmediathread, NULL, doUPnpMediaThread, this);
    }

}

void UPnpMedia::RunRebuildLoop(void)
{

    // Sleep a few seconds to wait for other stuff to settle down.
    sleep(10);

    while (1)
    {
        //VERBOSE(VB_UPNP, "UPnpMedia::RunRebuildLoop Calling BuildMediaMap");
        BuildMediaMap();

        sleep(1800 + (random()%8));
    }
}

void *UPnpMedia::doUPnpMediaThread(void *param)
{
    UPnpMedia *upnpmedia = (UPnpMedia*)param;
    upnpmedia->RunRebuildLoop();

    return NULL;
}

QString UPnpMedia::GetTitleName(QString fPath, QString fName)
{
    if (m_mapTitleNames[fPath])
    {
        return m_mapTitleNames[fPath];
    }
    else
        return fName;
}

QString UPnpMedia::GetCoverArt(QString fPath)
{
    if (m_mapCoverArt[fPath])
    {
        return m_mapCoverArt[fPath];
    }
    else
        return "";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// this should dynamically generate the SQL query and such
void UPnpMedia::FillMetaMaps(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString sSQL = "SELECT filename, title, coverfile FROM videometadata";

    query.prepare  ( sSQL );
    query.exec();

    if (query.isActive() && query.size() > 0)
    {
        while(query.next())
        {
            m_mapTitleNames[query.value(0).toString()] = query.value(1)
                                                                .toString();
            m_mapCoverArt[query.value(0).toString()] = query.value(2)
                                                                .toString();
        }
    }

}


int UPnpMedia::buildFileList(QString directory, int rootID, int itemID, MSqlQuery &query)
{

    int parentid;
    QDir vidDir(directory);
    QString title;
    //VERBOSE(VB_UPNP, QString("buildFileList = %1, rootID = %2, itemID =
    //%3").arg(directory).arg(rootID).arg(itemID));

    if (rootID > 0) 
        parentid = rootID;
    else
        parentid = itemID;

    vidDir.setSorting( QDir:: DirsFirst | QDir::Name );
    const QFileInfoList *List = vidDir.entryInfoList();
    // If we can't read it's contents move on
    if (!List) return itemID;
    for (QFileInfoListIterator it(*List); it; ++it)
    {
        QFileInfo Info(*it.current());
        QString fName = Info.fileName();
        QString fPath = Info.filePath();

        if (fName == "." ||
            fName == "..")
        {
            continue;
        }

        if (Info.isDir())
        {
            itemID++;

            query.prepare("INSERT INTO upnpmedia "
                        "(intid, class, itemtype, parentid, itemproperties, "
            "filepath, filename, title, coverart) "
            "VALUES (:ITEMID, :ITEMCLASS, 'FOLDER', :PARENTID, '', "
            ":FILEPATH, :FILENAME, :TITLE, :COVERART)");

            query.bindValue(":ITEMCLASS", sMediaType);
            query.bindValue(":ITEMID", itemID);
            query.bindValue(":PARENTID", parentid);
            query.bindValue(":FILEPATH", fPath);
            query.bindValue(":FILENAME", fName);

            query.bindValue(":TITLE", GetTitleName(fPath,fName));
            query.bindValue(":COVERART", GetCoverArt(fPath));

            query.exec();

            itemID = buildFileList(Info.filePath(), 0, itemID, query);
            continue;

        }
        else
        {
/*
            if (handler->validextensions.count() > 0)
            {
                QRegExp r;

                r.setPattern("^" + Info.extension( FALSE ) + "$");
                r.setCaseSensitive(false);
                QStringList result = handler->validextensions.grep(r);
                if (result.isEmpty()) {
                    continue;
                }
            }
*/

            itemID++;

//            VERBOSE(VB_UPNP, QString("UPnpMedia Video File : (%1) (%2)")
//                      .arg(itemID)
//                                .arg(fName));

            query.prepare("INSERT INTO upnpmedia "
                        "(intid, class, itemtype, parentid, itemproperties, "
                        "filepath, filename, title, coverart) "
                        "VALUES (:ITEMID, :ITEMCLASS, 'FILE', :PARENTID, '', "
                        ":FILEPATH, :FILENAME, :TITLE, :COVERART)");

            query.bindValue(":ITEMCLASS", sMediaType);
            query.bindValue(":ITEMID", itemID);
            query.bindValue(":PARENTID", parentid);
            query.bindValue(":FILEPATH", fPath);
            query.bindValue(":FILENAME", fName);

            query.bindValue(":TITLE", GetTitleName(fPath,fName));
            query.bindValue(":COVERART", GetCoverArt(fPath));

            query.exec();

        }
    }

    return itemID;
}

void UPnpMedia::BuildMediaMap(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString RootVidDir;
    int filecount;
    int nextID;

    // For now this class only does the video stuff, but eventually other media too
    sMediaType = "VIDEO";

    if (sMediaType == "VIDEO")
    {

        RootVidDir = gContext->GetSetting("VideoStartupDir");

        if ((!RootVidDir.isNull()) && (RootVidDir != ""))  
        {

            FillMetaMaps();

            query.prepare("DELETE FROM upnpmedia WHERE class = :ITEMCLASS");
            query.bindValue(":ITEMCLASS", sMediaType);
            query.exec();

            query.exec("LOCK TABLES upnpmedia WRITE");

            VERBOSE(VB_UPNP, LOC + QString("VideoStartupDir = %1")
                                            .arg(RootVidDir));

            QStringList parts = QStringList::split( ":", RootVidDir );

            nextID = STARTING_VIDEO_OBJECTID;

            for ( QStringList::Iterator it = parts.begin(); it != parts.end();
                                                                        ++it )
            {
                filecount = nextID;

                VERBOSE(VB_GENERAL, LOC + QString("BuildMediaMap %1 scan "
                                                "starting in :%1:")
                                                    .arg(sMediaType)
                                                    .arg(*it));

                nextID = buildFileList(*it,STARTING_VIDEO_OBJECTID, nextID,
                                                                        query);
                filecount = (filecount - nextID) * -1;

                VERBOSE(VB_GENERAL, LOC + QString("BuildMediaMap Done. Found "
                                                "%1 objects").arg(filecount));

            }

            query.exec("UNLOCK TABLES");

        }
        else
        {
            VERBOSE(VB_GENERAL, LOC + "BuildMediaMap - no VideoStartupDir set, "
                                " skipping scan.");
        }

    }
    else
    {
        VERBOSE(VB_GENERAL, LOC + QString("BuildMediaMap UNKNOWN MediaType %1 "
                            ", skipping scan.").arg(sMediaType));
    }

}


