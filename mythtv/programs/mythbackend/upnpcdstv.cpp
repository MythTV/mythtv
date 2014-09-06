//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdstv.cpp
//
// Purpose - uPnp Content Directory Extension for Recorded TV
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

// POSIX headers
#include <limits.h>
#include <stdint.h>
#include <QSize>

// MythTV headers
#include "upnpcdstv.h"
#include "httprequest.h"
#include "storagegroup.h"
#include "mythdate.h"
#include "mythcorecontext.h"
#include "upnphelpers.h"
#include "recordinginfo.h"

/*
   Recordings                              RecTv
    - All Programs                         RecTv/All
      + <recording 1>                      RecTv/All/item?ChanId=1004&StartTime=2006-04-06T20:00:00
      + <recording 2>
      + <recording 3>
    - By Title                             RecTv/title
      - <title 1>                          RecTv/title/key=Stargate SG-1
        + <recording 1>                    RecTv/title/key=Stargate SG-1/item?ChanId=1004&StartTime=2006-04-06T20:00:00
        + <recording 2>
    - By Genre
    - By Date
    - By Channel
    - By Group
*/


// UPnpCDSRootInfo UPnpCDSTv::g_RootNodes[] =
// {
//     {   "All Recordings",
//         "*",
//         "SELECT 0 as key, "
//           "CONCAT( title, ': ', subtitle) as name, "
//           "1 as children "
//             "FROM recorded r "
//             "%1 "
//             "ORDER BY r.starttime DESC",
//         "",
//         "r.starttime DESC",
//         "object.container",
//         "object.item.videoItem" },
//
//     {   "By Title",
//         "r.title",
//         "SELECT r.title as id, "
//           "r.title as name, "
//           "count( r.title ) as children "
//             "FROM recorded r "
//             "%1 "
//             "GROUP BY r.title "
//             "ORDER BY r.title",
//         "WHERE r.title=:KEY",
//         "r.title",
//         "object.container",
//         "object.container" },
//
//     {   "By Genre",
//         "r.category",
//         "SELECT r.category as id, "
//           "r.category as name, "
//           "count( r.category ) as children "
//             "FROM recorded r "
//             "%1 "
//             "GROUP BY r.category "
//             "ORDER BY r.category",
//         "WHERE r.category=:KEY",
//         "r.category",
//         "object.container",
//         "object.container.genre.movieGenre" },
//
//     {   "By Date",
//         "DATE_FORMAT(r.starttime, '%Y-%m-%d')",
//         "SELECT  DATE_FORMAT(r.starttime, '%Y-%m-%d') as id, "
//           "DATE_FORMAT(r.starttime, '%Y-%m-%d %W') as name, "
//           "count( DATE_FORMAT(r.starttime, '%Y-%m-%d %W') ) as children "
//             "FROM recorded r "
//             "%1 "
//             "GROUP BY name "
//             "ORDER BY r.starttime DESC",
//         "WHERE DATE_FORMAT(r.starttime, '%Y-%m-%d') =:KEY",
//         "r.starttime DESC",
//         "object.container",
//         "object.container"
//     },
//
//     {   "By Channel",
//         "r.chanid",
//         "SELECT channel.chanid as id, "
//           "CONCAT(channel.channum, ' ', channel.callsign) as name, "
//           "count( channum ) as children "
//             "FROM channel "
//                 "INNER JOIN recorded r ON channel.chanid = r.chanid "
//             "%1 "
//             "GROUP BY name "
//             "ORDER BY channel.chanid",
//         "WHERE channel.chanid=:KEY",
//         "",
//         "object.container",
//         "object.container"}, // Cannot be .channelGroup because children of channelGroup must be videoBroadcast items
//
//     {   "By Group",
//         "recgroup",
//         "SELECT recgroup as id, "
//           "recgroup as name, count( recgroup ) as children "
//             "FROM recorded "
//             "%1 "
//             "GROUP BY recgroup "
//             "ORDER BY recgroup",
//         "WHERE recgroup=:KEY",
//         "recgroup",
//         "object.container",
//         "object.container.album" }
// };

UPnpCDSTv::UPnpCDSTv()
          : UPnpCDSExtension( "Recordings", "Recordings",
                              "object.item.videoItem" )
{
    QString sServerIp   = gCoreContext->GetBackendServerIP4();
    int sPort           = gCoreContext->GetBackendStatusPort();
    m_URIBase.setScheme("http");
    m_URIBase.setHost(sServerIp);
    m_URIBase.setPort(sPort);

    // ShortCuts
    m_shortcuts.insert(UPnPCDSShortcuts::VIDEOS_RECORDINGS, "Recordings");
}

void UPnpCDSTv::CreateRoot()
{
    if (m_pRoot)
        return;

    m_pRoot = CDSObject::CreateContainer(m_sExtensionId,
                                         m_sName,
                                         "0");

    CDSObject* pContainer;
    QString containerId = m_sExtensionId + "/%1";

    // HACK: I'm not entirely happy with this solution, but it's at least
    // tidier than passing through half a dozen extra args to Load[Foo]
    // or having yet more methods just to load the counts
    UPnpCDSRequest *pRequest = new UPnpCDSRequest();
    pRequest->m_nRequestedCount = 0; // We don't want to load any results, we just want the TotalCount
    UPnpCDSExtensionResults *pResult = new UPnpCDSExtensionResults();
    IDTokenMap tokens;
    // END HACK

    // -----------------------------------------------------------------------
    // All Recordings
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Recording"),
                                              QObject::tr("All Recordings"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadRecordings(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(0);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Film
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Movie"),
                                              QObject::tr("Movies"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadMovies(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(0);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Title
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Title"),
                                              QObject::tr("Title"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadTitles(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    // Tricky to calculate ChildContainerCount without loading the full
    // result set
    //pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Date
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Date"),
                                              QObject::tr("Date"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadDates(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Genre
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Genre"),
                                              QObject::tr("Genre"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadGenres(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Channel
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Channel"),
                                              QObject::tr("Channel"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadChannels(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------
    // By Recording Group
    // -----------------------------------------------------------------------
    pContainer = CDSObject::CreateContainer ( containerId.arg("Recgroup"),
                                              QObject::tr("Recording Group"),
                                              m_sExtensionId, // Parent Id
                                              NULL );
    // HACK
    LoadRecGroups(pRequest, pResult, tokens);
    pContainer->SetChildCount(pResult->m_nTotalMatches);
    pContainer->SetChildContainerCount(pResult->m_nTotalMatches);
    // END HACK
    m_pRoot->AddChild(pContainer);

    // -----------------------------------------------------------------------

    // HACK
    delete pRequest;
    delete pResult;
    // END HACK
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadMetadata(const UPnpCDSRequest* pRequest,
                              UPnpCDSExtensionResults* pResults,
                              IDTokenMap tokens, QString currentToken)
{
    if (currentToken.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("UPnpCDSTV::LoadMetadata: Final "
                                         "token missing from id: %1")
                                        .arg(pRequest->m_sObjectId));
        return false;
    }

    // Root + 1
    if (tokens[currentToken].isEmpty())
    {
        CDSObject *container = m_pRoot->GetChild(pRequest->m_sObjectId);

        if (container)
        {
            pResults->Add(container);
            pResults->m_nTotalMatches = 1;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, QString("UPnpCDSTV::LoadMetadata: Requested "
                                             "object cannot be found: %1")
                                               .arg(pRequest->m_sObjectId));
    }
    else if (currentToken == "recording")
    {
        return LoadRecordings(pRequest, pResults, tokens);
    }
    else if (currentToken == "title")
    {
        return LoadTitles(pRequest, pResults, tokens);
    }
    else if (currentToken == "date")
    {
        return LoadDates(pRequest, pResults, tokens);
    }
    else if (currentToken == "genre")
    {
        return LoadGenres(pRequest, pResults, tokens);
    }
    else if (currentToken == "recgroup")
    {
        return LoadRecGroups(pRequest, pResults, tokens);
    }
    else if (currentToken == "channel")
    {
        return LoadChannels(pRequest, pResults, tokens);
    }
    else if (currentToken == "movie")
    {
        return LoadMovies(pRequest, pResults, tokens);
    }
    else
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpCDSTV::LoadMetadata(): "
                    "Unhandled metadata request for '%1'.").arg(currentToken));

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadChildren(const UPnpCDSRequest* pRequest,
                             UPnpCDSExtensionResults* pResults,
                             IDTokenMap tokens, QString currentToken)
{
    if (currentToken.isEmpty() || currentToken == m_sExtensionId.toLower())
    {
        // Root
        pResults->Add(GetRoot()->GetChildren());
        pResults->m_nTotalMatches = GetRoot()->GetChildCount();
        return true;
    }
    else if (currentToken == "title")
    {
        if (!tokens["title"].isEmpty())
            return LoadRecordings(pRequest, pResults, tokens);
        else
            return LoadTitles(pRequest, pResults, tokens);
    }
    else if (currentToken == "date")
    {
        if (!tokens["date"].isEmpty())
            return LoadRecordings(pRequest, pResults, tokens);
        else
            return LoadDates(pRequest, pResults, tokens);
    }
    else if (currentToken == "genre")
    {
        if (!tokens["genre"].isEmpty())
            return LoadRecordings(pRequest, pResults, tokens);
        else
            return LoadGenres(pRequest, pResults, tokens);
    }
    else if (currentToken == "recgroup")
    {
        if (!tokens["recgroup"].isEmpty())
            return LoadRecordings(pRequest, pResults, tokens);
        else
            return LoadRecGroups(pRequest, pResults, tokens);
    }
    else if (currentToken == "channel")
    {
        if (tokens["channel"].toInt() > 0)
            return LoadRecordings(pRequest, pResults, tokens);
        else
            return LoadChannels(pRequest, pResults, tokens);
    }
    else if (currentToken == "movie")
    {
        return LoadMovies(pRequest, pResults, tokens);
    }
    else if (currentToken == "recording")
    {
        return LoadRecordings(pRequest, pResults, tokens);
    }
    else
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpCDSTV::LoadChildren(): "
                    "Unhandled metadata request for '%1'.").arg(currentToken));

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::IsBrowseRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // ----------------------------------------------------------------------
    // Xbox360 compatibility code.
    // ----------------------------------------------------------------------

//     if (pRequest->m_eClient == CDS_ClientXBox &&
//         pRequest->m_sContainerID == "15" &&
//         gCoreContext->GetSetting("UPnP/WMPSource") != "1")
//     {
//         pRequest->m_sObjectId = "Videos/0";
//
//         LOG(VB_UPNP, LOG_INFO,
//             "UPnpCDSTv::IsBrowseRequestForUs - Yes ContainerID == 15");
//         return true;
//     }

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    // ----------------------------------------------------------------------
//     if (pRequest->m_eClient == CDS_ClientWMP &&
//         pRequest->m_nClientVersion < 12.0 &&
//         pRequest->m_sContainerID == "13" &&
//         gCoreContext->GetSetting("UPnP/WMPSource") != "1")
//     {
//         pRequest->m_sObjectId = "RecTv/0";
//
//         LOG(VB_UPNP, LOG_INFO,
//             "UPnpCDSTv::IsBrowseRequestForUs - Yes, ObjectId == 13");
//         return true;
//     }

    LOG(VB_UPNP, LOG_INFO,
        "UPnpCDSTv::IsBrowseRequestForUs - Not sure... Calling base class.");

    return UPnpCDSExtension::IsBrowseRequestForUs( pRequest );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::IsSearchRequestForUs( UPnpCDSRequest *pRequest )
{
    // ----------------------------------------------------------------------
    // See if we need to modify the request for compatibility
    // ----------------------------------------------------------------------

    // ----------------------------------------------------------------------
    // XBox 360 compatibility code
    // ----------------------------------------------------------------------

//     if (pRequest->m_eClient == CDS_ClientXBox &&
//         pRequest->m_sContainerID == "15" &&
//         gCoreContext->GetSetting("UPnP/WMPSource") !=  "1")
//     {
//         pRequest->m_sObjectId = "Videos/0";
//
//         LOG(VB_UPNP, LOG_INFO, "UPnpCDSTv::IsSearchRequestForUs... Yes.");
//
//         return true;
//     }
//
//
//     if ((pRequest->m_sObjectId.isEmpty()) &&
//         (!pRequest->m_sContainerID.isEmpty()))
//         pRequest->m_sObjectId = pRequest->m_sContainerID;

    // ----------------------------------------------------------------------

    bool bOurs = UPnpCDSExtension::IsSearchRequestForUs( pRequest );

    // ----------------------------------------------------------------------
    // WMP11 compatibility code
    //
    // In this mode browsing for "Videos" is forced to either RecordedTV (us)
    // or Videos (handled by upnpcdsvideo)
    //
    // ----------------------------------------------------------------------

//     if ( bOurs && pRequest->m_eClient == CDS_ClientWMP &&
//          pRequest->m_nClientVersion < 12.0)
//     {
//         // GetBoolSetting()?
//         if ( gCoreContext->GetSetting("UPnP/WMPSource") != "1")
//         {
//             pRequest->m_sObjectId = "RecTv/0";
//             // -=>TODO: Not sure why this was added
//             pRequest->m_sParentId = '8';
//         }
//         else
//             bOurs = false;
//     }

    return bOurs;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

 // TODO Load titles where there is more than one, otherwise the recording, but
 //      somehow do so with the minimum number of queries and code duplication
bool UPnpCDSTv::LoadTitles(const UPnpCDSRequest* pRequest,
                           UPnpCDSExtensionResults* pResults,
                           IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "r.title, r.inetref, r.recordedid, COUNT(*) "
                  "FROM recorded r "
                  "LEFT JOIN recgroups g ON r.recgroup=g.recgroup "
                  "%1 " // WHERE clauses
                  "GROUP BY r.title "
                  "ORDER BY r.title "
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);
    query.prepare(sql.arg(whereString));
    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        QString sTitle = query.value(0).toString();
        QString sInetRef = query.value(1).toString();
        int nRecordingID = query.value(2).toInt();
        int nTitleCount = query.value(3).toInt();

         if (nTitleCount > 1)
         {
            // TODO Album or plain old container?
            CDSObject* pContainer = CDSObject::CreateAlbum( sId.arg(sTitle),
                                                            sTitle,
                                                            pRequest->m_sParentId,
                                                            NULL );


            pContainer->SetPropValue("description", QObject::tr("%n Episode(s)", "", nTitleCount));
            pContainer->SetPropValue("longdescription", QObject::tr("%n Episode(s)", "", nTitleCount));

            pContainer->SetChildCount(nTitleCount);
            pContainer->SetChildContainerCount(0); // Recordings, no containers
            pContainer->SetPropValue("storageMedium", "HDD");

            // Artwork
            PopulateArtworkURIS(pContainer, sInetRef, 0, m_URIBase); // No particular season

            pResults->Add(pContainer);
            pContainer->DecrRef();
        }
        else
        {
            IDTokenMap newTokens(tokens);
            newTokens.insert("recording", QString::number(nRecordingID));
            LoadRecordings(pRequest, pResults, newTokens);
        }
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadDates(const UPnpCDSRequest* pRequest,
                              UPnpCDSExtensionResults* pResults,
                              IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "r.starttime, COUNT(r.recordedid) "
                  "FROM recorded r "
                  "LEFT JOIN recgroups g ON g.recgroup=r.recgroup "
                  "%1 " // WHERE clauses
                  "GROUP BY DATE(CONVERT_TZ(r.starttime, 'UTC', 'SYSTEM')) "
                  "ORDER BY r.starttime DESC "
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);
    query.prepare(sql.arg(whereString));
    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        QDate dtDate = query.value(0).toDate();
        int nRecCount = query.value(1).toInt();

        // TODO Album or plain old container?
        CDSObject* pContainer = CDSObject::CreateContainer( sId.arg(dtDate.toString(Qt::ISODate)),
                                                            MythDate::toString(dtDate, MythDate::kDateFull | MythDate::kAutoYear | MythDate::kSimplify),
                                                            pRequest->m_sParentId,
                                                            NULL );
        pContainer->SetChildCount(nRecCount);
        pContainer->SetChildContainerCount(nRecCount);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadGenres( const UPnpCDSRequest* pRequest,
                            UPnpCDSExtensionResults* pResults,
                            IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "r.category, COUNT(r.recordedid) "
                  "FROM recorded r "
                  "LEFT JOIN recgroups g ON g.recgroup=r.recgroup "
                  "%1 " // WHERE clauses
                  "GROUP BY r.category "
                  "ORDER BY r.category "
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);
    query.prepare(sql.arg(whereString));
    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        QString sGenre = query.value(0).toString();
        int nRecCount = query.value(1).toInt();

        // Handle empty genre strings
        QString sDisplayGenre = sGenre.isEmpty() ? QObject::tr("No Genre") : sGenre;
        sGenre = sGenre.isEmpty() ? "MYTH_NO_GENRE" : sGenre;

        // TODO Album or plain old container?
        CDSObject* pContainer = CDSObject::CreateMovieGenre( sId.arg(sGenre),
                                                             sDisplayGenre,
                                                             pRequest->m_sParentId,
                                                             NULL );
        pContainer->SetChildCount(nRecCount);
        pContainer->SetChildContainerCount(nRecCount);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadRecGroups(const UPnpCDSRequest* pRequest,
                              UPnpCDSExtensionResults* pResults,
                              IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "r.recgroupid, g.displayname, g.recgroup, COUNT(r.recordedid) "
                  "FROM recorded r "
                  "LEFT JOIN recgroups g ON g.recgroup=r.recgroup " // Use recgroupid in future
                  "%1 " // WHERE clauses
                  "GROUP BY r.recgroup "
                  "ORDER BY g.displayname "
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        // Use the string for now until recgroupid support is complete
//        int nRecGroupID = query.value(0).toInt();
        QString sDisplayName = query.value(1).toString();
        QString sName = query.value(2).toString();
        int nRecCount = query.value(3).toInt();

        // TODO Album or plain old container?
        CDSObject* pContainer = CDSObject::CreateContainer( sId.arg(sName),
                                                        sDisplayName.isEmpty() ? sName : sDisplayName,
                                                        pRequest->m_sParentId,
                                                        NULL );
        pContainer->SetChildCount(nRecCount);
        pContainer->SetChildContainerCount(nRecCount);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadChannels(const UPnpCDSRequest* pRequest,
                             UPnpCDSExtensionResults* pResults,
                             IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    sId.append("=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "r.chanid, c.channum, c.name, COUNT(r.recordedid) "
                  "FROM recorded r "
                  "JOIN channel c ON c.chanid=r.chanid "
                  "LEFT JOIN recgroups g ON g.recgroup=r.recgroup " // Use recgroupid in future
                  "%1 " // WHERE clauses
                  "GROUP BY c.channum "
                  "ORDER BY LPAD(CAST(c.channum AS UNSIGNED), 10, 0), " // Natural sorting including subchannels e.g. 2_4, 1.3
                  "         LPAD(c.channum,  10, 0)"
                  "LIMIT :OFFSET,:COUNT";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        int nChanID = query.value(0).toInt();
        QString sChanNum = query.value(1).toString();
        QString sName = query.value(2).toString();
        int nRecCount = query.value(3).toInt();

        QString sFullName = QString("%1 %2").arg(sChanNum).arg(sName);

        // TODO Album or plain old container?
        CDSObject* pContainer = CDSObject::CreateContainer( sId.arg(nChanID),
                                                        sFullName,
                                                        pRequest->m_sParentId,
                                                        NULL );
        pContainer->SetChildCount(nRecCount);
        pContainer->SetChildContainerCount(nRecCount);

        pResults->Add(pContainer);
        pContainer->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadMovies(const UPnpCDSRequest* pRequest,
                           UPnpCDSExtensionResults* pResults,
                           IDTokenMap tokens)
{
    tokens["category_type"] = "movie";
    return LoadRecordings(pRequest, pResults, tokens);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// TODO Implement this
// bool UPnpCDSTv::LoadSeasons(const UPnpCDSRequest* pRequest,
//                             UPnpCDSExtensionResults* pResults,
//                             IDTokenMap tokens)
// {
//
//     return false;
// }

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// TODO Implement this
// bool UPnpCDSTv::LoadEpisodes(const UPnpCDSRequest* pRequest,
//                             UPnpCDSExtensionResults* pResults,
//                             IDTokenMap tokens)
// {
//     return false;
// }

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpCDSTv::LoadRecordings(const UPnpCDSRequest* pRequest,
                               UPnpCDSExtensionResults* pResults,
                               IDTokenMap tokens)
{
    QString sId = pRequest->m_sParentId;
    if (GetCurrentToken(pRequest->m_sParentId).first != "recording")
        sId.append("/recording");
    sId.append("=%1");

    uint16_t nCount = pRequest->m_nRequestedCount;
    uint16_t nOffset = pRequest->m_nStartingIndex;

    // HACK this is a bit of a hack for loading Recordings in the Title view
    //      where the count/start index from the request aren't applicable
    if (tokens["recording"].toInt() > 0)
    {
        nCount = 1;
        nOffset = 0;
    }

    // We must use a dedicated connection to get an acccurate value from
    // FOUND_ROWS()
    MSqlQuery query(MSqlQuery::InitCon(MSqlQuery::kDedicatedConnection));

    QString sql = "SELECT SQL_CALC_FOUND_ROWS "
                  "r.chanid, r.starttime, r.endtime, r.title, "
                  "r.subtitle, r.description, r.category, "
                  "r.hostname, r.recgroup, r.filesize, "
                  "r.basename, r.progstart, r.progend, "
                  "r.storagegroup, r.inetref, "
                  "p.category_type, c.callsign, c.channum, "
                  "p.episode, p.totalepisodes, p.season, "
                  "r.programid, r.seriesid, r.recordid, "
                  "c.default_authority, c.name, "
                  "r.recordedid, r.transcoded, p.videoprop+0, p.audioprop+0 "
                  "FROM recorded r "
                  "LEFT JOIN channel c ON r.chanid=c.chanid "
                  "LEFT JOIN recordedprogram p ON p.chanid=r.chanid "
                  "                            AND p.starttime=r.progstart "
                  "LEFT JOIN recgroups g ON r.recgroup=g.recgroup "
                  "%1 " // WHERE clauses
                  "%2 " // ORDER BY
                  "LIMIT :OFFSET,:COUNT";


    QString orderByString = "ORDER BY r.starttime DESC, r.title";

    QStringList clauses;
    QString whereString = BuildWhereClause(clauses, tokens);

    query.prepare(sql.arg(whereString).arg(orderByString));

    BindValues(query, tokens);

    query.bindValue(":OFFSET", nOffset);
    query.bindValue(":COUNT", nCount);

    if (!query.exec())
        return false;

    while (query.next())
    {
        int            nChanid      = query.value( 0).toInt();
        QDateTime      dtStartTime  = MythDate::as_utc(query.value(1).toDateTime());
        QDateTime      dtEndTime    = MythDate::as_utc(query.value(2).toDateTime());
        QString        sTitle       = query.value( 3).toString();
        QString        sSubtitle    = query.value( 4).toString();
        QString        sDescription = query.value( 5).toString();
        QString        sCategory    = query.value( 6).toString();
        QString        sHostName    = query.value( 7).toString();
        QString        sRecGroup    = query.value( 8).toString();
        uint64_t       nFileSize    = query.value( 9).toULongLong();
        QString        sBaseName    = query.value(10).toString();

        QDateTime      dtProgStart  =
            MythDate::as_utc(query.value(11).toDateTime());
        QDateTime      dtProgEnd    =
            MythDate::as_utc(query.value(12).toDateTime());
        QString        sStorageGrp  = query.value(13).toString();

        QString        sInetRef     = query.value(14).toString();
        QString        sCatType     = query.value(15).toString();
        QString        sCallsign    = query.value(16).toString();
        QString        sChanNum     = query.value(17).toString();

        int            nEpisode      = query.value(18).toInt();
        int            nEpisodeTotal = query.value(19).toInt();
        int            nSeason       = query.value(20).toInt();

        QString        sProgramId   = query.value(21).toString();
        QString        sSeriesId    = query.value(22).toString();
        int            nRecordId    = query.value(23).toInt();

        QString        sDefaultAuthority = query.value(24).toString();
        QString        sChanName    = query.value(25).toString();

        int            nRecordedId  = query.value(26).toInt();

        bool           bTranscoded  = query.value(27).toBool();
        int            nVideoProps  = query.value(28).toInt();
        //int            nAudioProps  = query.value(29).toInt();

        // ----------------------------------------------------------------------
        // Cache Host ip Address & Port
        // ----------------------------------------------------------------------

        if (!m_mapBackendIp.contains( sHostName ))
            m_mapBackendIp[ sHostName ] = gCoreContext->GetBackendServerIP4(sHostName);

        if (!m_mapBackendPort.contains( sHostName ))
            m_mapBackendPort[ sHostName ] = gCoreContext->GetBackendStatusPort(sHostName);

        // ----------------------------------------------------------------------
        // Build Support Strings
        // ----------------------------------------------------------------------

        QUrl URIBase;
        URIBase.setScheme("http");
        URIBase.setHost(m_mapBackendIp[sHostName]);
        URIBase.setPort(m_mapBackendPort[sHostName]);

        CDSObject *pItem = CDSObject::CreateVideoItem( sId.arg(nRecordedId),
                                                       sTitle,
                                                       pRequest->m_sParentId );

        // Only add the reference ID for items which are not in the
        // 'All Recordings' container
        QString sRefIDBase = QString("%1/Recording").arg(m_sExtensionId);
        if ( pRequest->m_sParentId != sRefIDBase )
        {
            QString sRefId = QString( "%1=%2")
                                .arg( sRefIDBase )
                                .arg( nRecordedId );

            pItem->SetPropValue( "refID", sRefId );
        }

        pItem->SetPropValue( "genre", sCategory );

        // NOTE There is no max-length on description, no requirement in either UPnP
        // or DLNA that the description be a certain size, only that it's 'brief'
        //
        // The specs only say that the optional longDescription is for longer
        // descriptions. Given that clients could easily truncate the description
        // themselves this is all very vague.
        //
        // It's not really correct to stick the subtitle in the description
        // field given the existence of the programTitle field. Yet that's what
        // we've and what some people have come to expect. There's no easy answer
        // but there are wrong answers and whatever we decide, we shouldn't pander
        // to devices which don't follow the specs.

        if (!sSubtitle.isEmpty())
            pItem->SetPropValue( "description"    , sSubtitle    );
        else
            pItem->SetPropValue( "description", sDescription.left(128).append(" ..."));
        pItem->SetPropValue( "longDescription", sDescription );

        pItem->SetPropValue( "channelName"    , sChanName   );
        // TODO Need to detect/switch between DIGITAL/ANALOG
        pItem->SetPropValue( "channelID"      , sChanNum, "DIGITAL");
        pItem->SetPropValue( "callSign"       , sCallsign    );
        // NOTE channelNr must only be used when a DIGITAL or ANALOG channelID is
        // given and it MUST be an integer i.e. 2_1 or 2.1 are illegal
        int nChanNum = sChanNum.toInt();
        if (nChanNum > 0)
            pItem->SetPropValue( "channelNr" , QString::number(nChanNum) );

        if (sCatType != "movie")
        {
            pItem->SetPropValue( "seriesTitle"  , sTitle);
            pItem->SetPropValue( "programTitle"  , sSubtitle);
        }
        else
            pItem->SetPropValue( "programTitle"  , sTitle);

        if (   nEpisode > 0 || nSeason > 0) // There has got to be a better way
        {
            pItem->SetPropValue( "episodeNumber" , QString::number(nEpisode));
            pItem->SetPropValue( "episodeCount"  , QString::number(nEpisodeTotal));
        }

        pItem->SetPropValue( "scheduledStartTime"   , UPnPDateTime::DateTimeFormat(dtProgStart));
        pItem->SetPropValue( "scheduledEndTime"     , UPnPDateTime::DateTimeFormat(dtProgEnd));
        int msecs = dtProgEnd.toMSecsSinceEpoch() - dtProgStart.toMSecsSinceEpoch();
        pItem->SetPropValue( "scheduledDuration"    , UPnPDateTime::DurationFormat(msecs));
        pItem->SetPropValue( "recordedStartDateTime", UPnPDateTime::DateTimeFormat(dtStartTime));
        pItem->SetPropValue( "recordedDayOfWeek"    , UPnPDateTime::NamedDayFormat(dtStartTime));
        pItem->SetPropValue( "srsRecordScheduleID"  , QString::number(nRecordId));

        if (!sSeriesId.isEmpty())
        {
            // FIXME: This should be set correctly for EIT data to SI_SERIESID and
            //        for known sources such as TMS to the correct identifier
            QString sIdType = "mythtv.org_XMLTV";
            if (sSeriesId.contains(sDefaultAuthority))
                sIdType = "mythtv.org_EIT";

            pItem->SetPropValue( "seriesID", sSeriesId, sIdType );
        }

        if (!sProgramId.isEmpty())
        {
            // FIXME: This should be set correctly for EIT data to SI_PROGRAMID and
            //        for known sources such as TMS to the correct identifier
            QString sIdType = "mythtv.org_XMLTV";
            if (sProgramId.contains(sDefaultAuthority))
                sIdType = "mythtv.org_EIT";

            pItem->SetPropValue( "programID", sProgramId, sIdType );
        }

        pItem->SetPropValue( "date"          , UPnPDateTime::DateTimeFormat(dtStartTime));
        pItem->SetPropValue( "creator"       , "MythTV" );

        // Bookmark support
        //pItem->SetPropValue( "lastPlaybackPosition", QString::number());

        //pItem->SetPropValue( "producer"       , );
        //pItem->SetPropValue( "rating"         , );
        //pItem->SetPropValue( "actor"          , );
        //pItem->SetPropValue( "director"       , );

        // ----------------------------------------------------------------------
        // Add Video Resource Element based on File contents/extension (HTTP)
        // ----------------------------------------------------------------------

        StorageGroup sg(sStorageGrp, sHostName);
        QString sFilePath = sg.FindFile(sBaseName);
        QString sMimeType;

        if ( QFile::exists(sFilePath) )
            sMimeType = HTTPRequest::TestMimeType( sFilePath );
        else
            sMimeType = HTTPRequest::TestMimeType( sBaseName );


        // If we are dealing with Window Media Player 12 (i.e. Windows 7)
        // then fake the Mime type to place the recorded TV in the
        // recorded TV section.
//         if (pRequest->m_eClient == CDS_ClientWMP &&
//             pRequest->m_nClientVersion >= 12.0)
//         {
//             sMimeType = "video/x-ms-dvr";
//         }

        // HACK: If we are dealing with a Sony Blu-ray player then we fake the
        // MIME type to force the video to appear
//         if ( pRequest->m_eClient == CDS_ClientSonyDB )
//             sMimeType = "video/avi";

        int nVideoHeight = 0;
        int nVideoWidth = 0;
        uint32_t nDurationMS = 0;

        // NOTE We intentionally don't use the chanid, recstarttime constructor
        // to avoid an unnecessary db query. At least until the time that we're
        // creating a PI object throughout
        ProgramInfo pgInfo = ProgramInfo();
        pgInfo.SetChanID(nChanid);
        pgInfo.SetRecordingStartTime(dtStartTime);

        nVideoHeight = pgInfo.QueryAverageHeight();
        nVideoWidth = pgInfo.QueryAverageWidth();
        nDurationMS = pgInfo.QueryTotalDuration();

        // Older recordings won't have their precise duration stored in
        // recordedmarkup
        if (nDurationMS == 0)
        {
            uint uiStart = dtStartTime.toTime_t();
            uint uiEnd   = dtEndTime.toTime_t();
            nDurationMS  = (uiEnd - uiStart) * 1000; // To milliseconds
            nDurationMS  = (nDurationMS > 0) ? nDurationMS : 0;
        }

        pItem->SetPropValue( "recordedDuration", UPnPDateTime::DurationFormat(nDurationMS));

        QSize resolution = QSize(nVideoWidth, nVideoHeight);
        QString sVideoCodec = (nVideoProps & VID_AVC) ? "H264" : "MPEG2";
        QString sAudioCodec = ""; // TODO Implement

        QUrl    resURI    = URIBase;
        resURI.setPath("Content/GetRecording");
        resURI.addQueryItem("ChanId", QString::number(nChanid));
        resURI.addQueryItem("StartTime", dtStartTime.toString(Qt::ISODate));

        QString sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                                     sMimeType,
                                                     resolution,
                                                     sVideoCodec,
                                                     sAudioCodec,
                                                     bTranscoded);

        Resource *pRes = pItem->AddResource( sProtocol, resURI.toEncoded() );
        // Must be the duration of the entire video not the scheduled programme duration
        // Appendix B.2.1.4 - res@duration
        if (nDurationMS > 0)
            pRes->AddAttribute ( "duration"    , UPnPDateTime::resDurationFormat(nDurationMS) );
        if (nVideoHeight > 0 && nVideoWidth > 0)
            pRes->AddAttribute ( "resolution"  , QString("%1x%2").arg(nVideoWidth).arg(nVideoHeight) );
        pRes->AddAttribute ( "size"            , QString::number( nFileSize) );

        // ----------------------------------------------------------------------
        // Add Preview URI as <res>
        // MUST be _TN and 160px
        // ----------------------------------------------------------------------

        QUrl previewURI = URIBase;
        previewURI.setPath("Content/GetPreviewImage");
        previewURI.addQueryItem("ChanId", QString::number(nChanid));
        previewURI.addQueryItem("StartTime", dtStartTime.toString(Qt::ISODate));
        previewURI.addQueryItem("Width", "160");
        previewURI.addQueryItem("Format", "JPG");
        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP, "image/jpeg",
                                             QSize(160, 160));
        pItem->AddResource( sProtocol, previewURI.toEncoded());

        // ----------------------------------------------------------------------
        // Add Artwork
        // ----------------------------------------------------------------------
        if (!sInetRef.isEmpty())
        {
            PopulateArtworkURIS(pItem, sInetRef, nSeason, URIBase);
        }

        pResults->Add( pItem );
        pItem->DecrRef();
    }

    // Just in case FOUND_ROWS() should fail, ensure m_nTotalMatches contains
    // at least the size of this result set
    if (query.size() > 0)
        pResults->m_nTotalMatches = query.size();

    // Fetch the total number of matches ignoring any LIMITs
    query.prepare("SELECT FOUND_ROWS()");
    if (query.exec() && query.next())
            pResults->m_nTotalMatches = query.value(0).toUInt();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSTv::PopulateArtworkURIS(CDSObject* pItem, const QString &sInetRef,
                                    int nSeason, const QUrl& URIBase)
{
    QUrl artURI = URIBase;
    artURI.setPath("Content/GetRecordingArtwork");
    artURI.addQueryItem("Inetref", sInetRef);
    artURI.addQueryItem("Season", QString::number(nSeason));

    // Prefer JPEG over PNG here, although PNG is allowed JPEG probably
    // has wider device support and crucially the filesizes are smaller
    // which speeds up loading times over the network

    // We MUST include the thumbnail size, but since some clients may use the
    // first image they see and the thumbnail is tiny, instead return the
    // medium first. The large could be very large, which is no good if the
    // client is pulling images for an entire list at once!

    // Thumbnail
    // At least one albumArtURI must be a ThumbNail (TN) no larger
    // than 160x160, and it must also be a jpeg
    QUrl thumbURI = artURI;
    thumbURI.addQueryItem("Type", "screenshot");
    thumbURI.addQueryItem("Width", "160");
    thumbURI.addQueryItem("Height", "160");

    // Small
    // Must be no more than 640x480
    QUrl smallURI = artURI;
    smallURI.addQueryItem("Type", "coverart");
    smallURI.addQueryItem("Width", "640");
    smallURI.addQueryItem("Height", "480");

    // Medium
    // Must be no more than 1024x768
    QUrl mediumURI = artURI;
    mediumURI.addQueryItem("Type", "coverart");
    mediumURI.addQueryItem("Width", "1024");
    mediumURI.addQueryItem("Height", "768");

    // Large
    // Must be no more than 4096x4096 - for our purposes, just return
    // a fullsize image
    QUrl largeURI = artURI;
    largeURI.addQueryItem("Type", "fanart");

    QList<Property*> propList = pItem->GetProperties("albumArtURI");
    if (propList.size() >= 4)
    {
        Property *pProp = propList.at(0);
        if (pProp)
        {
            pProp->SetValue(mediumURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_MED");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        pProp = propList.at(1);
        if (pProp)
        {
            pProp->SetValue(thumbURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_TN");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        pProp = propList.at(2);
        if (pProp)
        {
            pProp->SetValue(smallURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_SM");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }

        pProp = propList.at(3);
        if (pProp)
        {
            pProp->SetValue(largeURI.toEncoded());
            pProp->AddAttribute("dlna:profileID", "JPEG_LRG");
            pProp->AddAttribute("xmlns:dlna", "urn:schemas-dlna-org:metadata-1-0");
        }
    }

    if (pItem->m_sClass.startsWith("object.item.videoItem"))
    {
        QString sProtocol;

        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                             "image/jpeg", QSize(1024, 768));
        pItem->AddResource( sProtocol, mediumURI.toEncoded());

        // We already include a thumbnail
        //sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
        //                                     "image/jpeg", QSize(160, 160));
        //pItem->AddResource( sProtocol, thumbURI.toEncoded());

        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                             "image/jpeg", QSize(640, 480));
        pItem->AddResource( sProtocol, smallURI.toEncoded());

        sProtocol = DLNA::ProtocolInfoString(UPNPProtocol::kHTTP,
                                             "image/jpeg", QSize(1920, 1080)); // Not the actual res, we don't know that
        pItem->AddResource( sProtocol, largeURI.toEncoded());
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpCDSTv::BuildWhereClause( QStringList clauses,
                                     IDTokenMap tokens)
{
    // We ignore protected recgroups, UPnP offers no mechanism to provide
    // restricted access to containers and there's no point in having
    // password protected groups if that protection can be easily circumvented
    // by children just by pointing a phone, tablet or other computer at the
    // advertised UPnP server.
    //
    // In short, don't use password protected recording groups if you want to
    // be able to access those recordings via upnp
    clauses.append("g.password=''");
    // Ignore recordings in the LiveTV and Deleted recgroups
    // We cannot currently prevent LiveTV recordings from being expired while
    // being streamed to a upnp device, so there's no point in listing them.
    QString liveTVGroup = RecordingInfo::GetRecgroupString(RecordingInfo::kLiveTVRecGroup);
    clauses.append(QString("g.recgroup != '%1'").arg(liveTVGroup));
    QString deletedGroup = RecordingInfo::GetRecgroupString(RecordingInfo::kDeletedRecGroup);
    clauses.append(QString("g.recgroup != '%1'").arg(deletedGroup));

    if (tokens["recording"].toInt() > 0)
        clauses.append("r.recordedid=:RECORDED_ID");
    if (!tokens["date"].isEmpty())
        clauses.append("DATE(CONVERT_TZ(r.starttime, 'UTC', 'SYSTEM'))=:DATE");
    if (!tokens["genre"].isEmpty())
        clauses.append("r.category=:GENRE");
    if (!tokens["recgroup"].isEmpty())
        clauses.append("r.recgroup=:RECGROUP");
    if (!tokens["title"].isEmpty())
        clauses.append("r.title=:TITLE");
    if (!tokens["channel"].isEmpty())
        clauses.append("r.chanid=:CHANNEL");
    // Special token
    if (!tokens["category_type"].isEmpty())
        clauses.append("p.category_type=:CATTYPE");

    QString whereString;
    if (!clauses.isEmpty())
    {
        whereString = " WHERE ";
        whereString.append(clauses.join(" AND "));
    }

    return whereString;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpCDSTv::BindValues( MSqlQuery &query,
                            IDTokenMap tokens)
{
    if (tokens["recording"].toInt() > 0)
        query.bindValue(":RECORDED_ID", tokens["recording"]);
    if (!tokens["date"].isEmpty())
        query.bindValue(":DATE", tokens["date"]);
    if (!tokens["genre"].isEmpty())
        query.bindValue(":GENRE", tokens["genre"] == "MYTH_NO_GENRE" ? "" : tokens["genre"]);
    if (!tokens["recgroup"].isEmpty())
        query.bindValue(":RECGROUP", tokens["recgroup"]);
    if (!tokens["title"].isEmpty())
        query.bindValue(":TITLE", tokens["title"]);
    if (tokens["channel"].toInt() > 0)
        query.bindValue(":CHANNEL", tokens["channel"]);
    if (!tokens["category_type"].isEmpty())
        query.bindValue(":CATTYPE", tokens["category_type"]);
}


// vim:ts=4:sw=4:ai:et:si:sts=4
