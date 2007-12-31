//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsvideo.cpp
//                                                                            
// Purpose - uPnp Content Directory Extention for MythVideo Videos
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpcdsvideo.h"
#include "httprequest.h"
#include <qfileinfo.h>
#include <qregexp.h>
#include <qurl.h>
#include <qdir.h>
#include <limits.h>
#include "util.h"

UPnpCDSRootInfo UPnpCDSVideo::g_RootNodes[] = 
{
    {   "VideoRoot", 
        "*",
        "SELECT 0 as key, "
          "title as name, "
          "1 as children "
            "FROM upnpmedia "
            "%1 "
            "ORDER BY title DESC",
        "" }
};

int UPnpCDSVideo::GetBaseCount(void)
{
    int res;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT COUNT(*) FROM upnpmedia WHERE class = 'VIDEO' "
		  "AND parentid = :ROOTID");

    query.bindValue(":ROOTID", STARTING_VIDEO_OBJECTID);
    query.exec();

    res = query.value(0).toInt();

    return res;
}
int UPnpCDSVideo::g_nRootCount = 1;

//int UPnpCDSVideo::g_nRootCount;
//= sizeof( g_RootNodes ) / sizeof( UPnpCDSRootInfo );

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpCDSRootInfo *UPnpCDSVideo::GetRootInfo( int nIdx )
{ 
    if ((nIdx >=0 ) && ( nIdx < g_nRootCount ))
        return &(g_RootNodes[ nIdx ]); 

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int UPnpCDSVideo::GetRootCount()
{
    return g_nRootCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetTableName( QString sColumn )
{
    return "upnpmedia";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSVideo::GetItemListSQL( QString sColumn )
{
    return "SELECT intid, title, filepath, " \
	   "itemtype, itemproperties, parentid "\
           "FROM upnpmedia WHERE class = 'VIDEO'";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams )
{
    int nVideoID = mapParams[ "Id" ].toInt();

    QString sSQL = QString( "%1 WHERE class = 'VIDEO' AND intid=:VIDEOID " ).arg( GetItemListSQL( ) );

    query.prepare( sSQL );

    query.bindValue( ":VIDEOID", (int)nVideoID    );
}

QString UPnpCDSVideo::GetTitleName(QString fPath, QString fName)
{
    if (m_mapTitleNames[fPath])
    {
        return m_mapTitleNames[fPath];
    }
    else
        return fName;
}
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

//  -- TODO -- Going to improve this later, just a rough in right now

void UPnpCDSVideo::FillMetaMaps(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString sSQL = "SELECT filename, title FROM videometadata";

    query.prepare  ( sSQL );
    query.exec();

    if (query.isActive() && query.size() > 0)
    {
        while(query.next())
            m_mapTitleNames[query.value(0).toString()] = query.value(1).toString();
    }

}


int UPnpCDSVideo::buildFileList(QString directory, int itemID, MSqlQuery &query)
{

//    VERBOSE(VB_UPNP, QString("buildFileList(%1)")
//		    .arg(directory));

    int parentid;
    QDir vidDir(directory);
                                // If we can't read it's contents move on
    if (!vidDir.isReadable())
        return itemID;

    parentid = itemID;

    vidDir.setSorting( QDir:: DirsFirst | QDir::Name );
    const QFileInfoList* List = vidDir.entryInfoList();
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

//	    VERBOSE(VB_UPNP, QString("UPnpCDSVideo Video Dir : (%1) (%2)")
//			    .arg(itemID)
//	                    .arg(fName));

	    query.prepare("INSERT INTO upnpmedia "
                          "(intid, class, itemtype, parentid, itemproperties, "
			  "filepath, filename, title) "
			  "VALUES (:ITEMID, 'VIDEO', 'FOLDER', :PARENTID, '', "
			  ":FILEPATH, :FILENAME, :TITLE)");

	    query.bindValue(":ITEMID", itemID);
	    query.bindValue(":PARENTID", parentid);
	    query.bindValue(":FILEPATH", fPath);
	    query.bindValue(":FILENAME", fName);
	    query.bindValue(":TITLE", GetTitleName(fPath,fName));
	    query.exec();
			    
	    itemID = buildFileList(Info.filePath(),itemID, query);
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
//            VERBOSE(VB_UPNP, QString("UPnpCDSVideo Video File : (%1) (%2)")
//			          .arg(itemID)
 //                                 .arg(fName));
            query.prepare("INSERT INTO upnpmedia "
                          "(intid, class, itemtype, parentid, itemproperties, "
                          "filepath, filename, title) "
                          "VALUES (:ITEMID, 'VIDEO', 'FILE', :PARENTID, '', "
                          ":FILEPATH, :FILENAME, :TITLE)");

            query.bindValue(":ITEMID", itemID);
            query.bindValue(":PARENTID", parentid);
            query.bindValue(":FILEPATH", fPath);
            query.bindValue(":FILENAME", fName);
            query.bindValue(":TITLE", GetTitleName(fPath,fName));
	    query.exec();

        }
    }

    return itemID;
}

void UPnpCDSVideo::BuildMediaMap(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString RootVidDir;
    int filecount;

    RootVidDir = gContext->GetSetting("VideoStartupDir");
    filecount = 0;

    query.exec("DELETE FROM upnpmedia WHERE class = 'VIDEO'");

    if ((!RootVidDir.isNull()) && (RootVidDir != "")) 
    {
        VERBOSE(VB_GENERAL, "UPnpCDSVideo::BuildMediaMap - Starting Build of Media Map, this can take a moment.");

        filecount = buildFileList(RootVidDir,STARTING_VIDEO_OBJECTID, query) - STARTING_VIDEO_OBJECTID;


        VERBOSE(VB_GENERAL, QString("UPnpCDSVideo::BuildMediaMap Done. Found %1 objects").arg(filecount));
    }
}
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSVideo::AddItem( const QString           &sObjectId,
                            UPnpCDSExtensionResults *pResults,
                            bool                     bAddRef,
                            MSqlQuery               &query )
{
    int            nVidID       = query.value( 0).toInt();
    QString        sTitle       = query.value( 1).toString();
    QString        sFileName    = query.value( 2).toString();
    QString        sItemType    = query.value( 3).toString();
    QString        sParentID    = query.value( 5).toString();

    // VERBOSE(VB_UPNP,QString("ID = %1, Title = %2, fname = %3 sObjectId = %4").arg(nVidID).arg(sTitle).arg(sFileName).arg(sObjectId));
    
    // ----------------------------------------------------------------------
    // Cache Host ip Address & Port
    // ----------------------------------------------------------------------
    QString sServerIp = gContext->GetSetting( "BackendServerIp"  );
    QString sPort     = gContext->GetSetting("BackendStatusPort" );

    // ----------------------------------------------------------------------
    // Build Support Strings
    // ----------------------------------------------------------------------

    QString sName      = sTitle;

    QString sURIBase   = QString( "http://%1:%2/Myth/" )
                            .arg( sServerIp )
                            .arg( sPort     );

    QString sURIParams = QString( "?Id=%1" )
                            .arg( nVidID );

    QString sId        = QString( "%1/item%2")
                            .arg( sObjectId )
                            .arg( sURIParams );

    CDSObject *pItem;

    if (sItemType == "FILE") 
    {
	sURIParams = QString( "/Id%1" )
		         .arg( nVidID );
	sId        = QString( "%1/item%2")
		         .arg( sObjectId )
			 .arg( sURIParams );

        pItem   = CDSObject::CreateVideoItem( sId, 
                                                         sName, 
                                                        sParentID );
    }
    else if (sItemType == "FOLDER") 
    {
	pItem   = CDSObject::CreateStorageFolder( sId,
	                                                     sName,
							     sParentID);
    }

    pItem->m_bRestricted  = false;
    pItem->m_bSearchable  = true;
    pItem->m_sWriteStatus = "WRITABLE";

    if ( bAddRef )
    {
        QString sRefId = QString( "%1/0/item%2")
                            .arg( m_sExtensionId )
                            .arg( sURIParams     );

        pItem->SetPropValue( "refID", sRefId );
    }

    pResults->Add( pItem );

    // ----------------------------------------------------------------------
    // Add Video Resource Element based on File extension (HTTP)
    // ----------------------------------------------------------------------
    
    QFileInfo fInfo( sFileName );

    QString sMimeType = HTTPRequest::GetMimeType( fInfo.extension( FALSE ));
    QString sProtocol = QString( "http-get:*:%1:DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01500000000000000000000000000000" ).arg( sMimeType  );
    QString sURI      = QString( "%1GetVideo%2").arg( sURIBase   )
                                                    .arg( sURIParams ); 

    Resource *pRes = pItem->AddResource( sProtocol, sURI );

   // Wonder if we should add the filesize to videometadata. would make this cleaner since we 
   // are hitting the db for everything else.

    pRes->AddAttribute( "size"      , QString("%1").arg(fInfo.size()) );

}

