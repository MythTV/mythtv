#include <limits.h>
#include <unistd.h>

#include <cstdlib>

#include <QFileInfo>
#include <QDir>

#include "mythcorecontext.h"
#include "httprequest.h"
#include "upnpmedia.h"
#include "mythdb.h"
#include "util.h"
#include "pthread.h"

#define LOC QString("UPnpMedia: ")

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpMedia::UPnpMedia(bool runthread, bool ismaster)
{

    if (gCoreContext->GetNumSetting("UPnP/RebuildDelay",30) > 0)
    {
        VERBOSE(VB_GENERAL,"Enabling Upnpmedia rebuild thread.");
        if ((runthread) && (ismaster))
        {
            pthread_t upnpmediathread;
            pthread_create(&upnpmediathread, NULL, doUPnpMediaThread, this);
        }
    }
    else
    {
        VERBOSE(VB_GENERAL,"Upnpmedia rebuild disabled.");
    }

}

void UPnpMedia::RunRebuildLoop(void)
{

    // Sleep a few seconds to wait for other stuff to settle down.
    sleep(10);

    int irebuildDelay = 1800;

    irebuildDelay = gCoreContext->GetNumSetting("UPnP/RebuildDelay",30) * 60;

    if (irebuildDelay < 60)
        irebuildDelay = 60;

    while (1)
    {
        //VERBOSE(VB_UPNP, "UPnpMedia::RunRebuildLoop Calling BuildMediaMap");
        BuildMediaMap();

        sleep(irebuildDelay + (random()%8));
    }
}

void *UPnpMedia::doUPnpMediaThread(void *param)
{
    UPnpMedia *upnpmedia = static_cast<UPnpMedia*>(param);
    upnpmedia->RunRebuildLoop();

    return NULL;
}

QString UPnpMedia::GetTitleName(QString fPath, QString fName)
{
    if (!m_mapTitleNames[fPath].isNull())
    {
        return m_mapTitleNames[fPath];
    }
    else
        return fName;
}

QString UPnpMedia::GetCoverArt(QString fPath)
{
    if (!m_mapCoverArt[fPath].isNull())
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

    if (query.exec() && query.size() > 0)
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
    //VERBOSE(VB_UPNP, QString("buildFileList = %1, rootID = %2, itemID =
    //%3").arg(directory).arg(rootID).arg(itemID));

    if (rootID > 0)
        parentid = rootID;
    else
        parentid = itemID;

    vidDir.setSorting( QDir::DirsFirst | QDir::Name );
    QFileInfoList List = vidDir.entryInfoList();
    // If we can't read it's contents move on
    if (List.isEmpty())
        return itemID;

    for (QFileInfoList::iterator it = List.begin(); it != List.end(); ++it)
    {
        QFileInfo Info(*it);
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

            if (!query.exec())
                MythDB::DBError("UPnpMedia::buildFileList", query);

            itemID = buildFileList(Info.filePath(), 0, itemID, query);
            continue;

        }
        else
        {
/*
            if (handler->validextensions.count() > 0)
            {
                QRegExp r;

                r.setPattern("^" + Info.suffix() + "$");
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

            if (!query.exec())
                MythDB::DBError("UPnpMedia::buildFileList", query);

        }
    }

    return itemID;
}

void UPnpMedia::BuildMediaMap(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // For now this class only does the video stuff, but eventually other media too
    sMediaType = "VIDEO";

    if (sMediaType == "VIDEO")
    {
        QString RootVidDir = gCoreContext->GetSetting("VideoStartupDir");

        if (!RootVidDir.isEmpty())
        {

            FillMetaMaps();

            query.prepare("DELETE FROM upnpmedia WHERE class = :ITEMCLASS");
            query.bindValue(":ITEMCLASS", sMediaType);
            if (!query.exec())
            {
                MythDB::DBError("BuildMediaMap -- clearing table upnpmedia", query);
                VERBOSE(VB_IMPORTANT, LOC + "BuildMediaMap - aborting");
                return;
            }

            if (!query.exec("LOCK TABLES upnpmedia WRITE"))
                MythDB::DBError("BuildMediaMap -- lock tables", query);

            VERBOSE(VB_UPNP, LOC + QString("VideoStartupDir = %1")
                                            .arg(RootVidDir));

            QStringList parts = RootVidDir.split(':', QString::SkipEmptyParts);

            int nextID = STARTING_VIDEO_OBJECTID;

            for ( QStringList::Iterator it = parts.begin(); it != parts.end();
                                                                        ++it )
            {
                int filecount = nextID;

                VERBOSE(VB_GENERAL, LOC + QString("BuildMediaMap %1 scan "
                                                "starting in :%2:")
                                                    .arg(sMediaType)
                                                    .arg(*it));

                nextID = buildFileList(*it,STARTING_VIDEO_OBJECTID, nextID,
                                                                        query);

                if (!gCoreContext->GetSetting("UPnP/RecordingsUnderVideos").isEmpty())
                {
                    VERBOSE(VB_ALL, "uPnP Unspecified error line 275, "
                                    "upnpmedia.cpp");
                    //   nextID = buildRecordingList(*it,STARTING_VIDEO_OBJECTID,
                    //                                        nextID,query);
                }

                filecount = (filecount - nextID) * -1;

                VERBOSE(VB_GENERAL, LOC + QString("BuildMediaMap Done. Found "
                                                "%1 objects").arg(filecount));

            }

            if (!query.exec("UNLOCK TABLES"))
                MythDB::DBError("BuildMediaMap -- unlock tables", query);

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


