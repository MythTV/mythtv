#include "v2video.h"
#include "mythversion.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "videometadata.h"
#include "programinfo.h"
#include "v2serviceUtil.h"
#include "mythdb.h"
#include "mythdbcon.h"
#include "mythlogging.h"

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (VIDEO_HANDLE, V2Video::staticMetaObject, std::bind(&V2Video::RegisterCustomTypes)))

void V2Video::RegisterCustomTypes()
{
    qRegisterMetaType<V2VideoMetadataInfo*>("V2VideoMetadataInfo");
    qRegisterMetaType<V2VideoMetadataInfoList*>("V2VideoMetadataInfoList");
}

V2Video::V2Video()
  : MythHTTPService(s_service)
{
}

V2VideoMetadataInfo* V2Video::GetVideo( int Id )
{
    VideoMetadataListManager::VideoMetadataPtr metadata =
                          VideoMetadataListManager::loadOneFromDatabase(Id);

    if ( !metadata )
        throw( QString( "No metadata found for selected ID!." ));

    auto *pVideoMetadataInfo = new V2VideoMetadataInfo();

    V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );

    return pVideoMetadataInfo;
}

V2VideoMetadataInfo* V2Video::GetVideoByFileName( const QString &FileName )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byFilename(FileName);

    if ( !metadata )
        throw( QString( "No metadata found for selected filename!." ));

    auto *pVideoMetadataInfo = new V2VideoMetadataInfo();

    V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );

    return pVideoMetadataInfo;
}


/////////////////////////////////////////////////////////////////////////////
// Get bookmark of a video as a frame number.
/////////////////////////////////////////////////////////////////////////////

long V2Video::GetSavedBookmark( int  Id )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("V2Video::GetSavedBookmark", query);
        return 0;
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("V2Video/GetSavedBookmark Video id %1 Not found.").arg(Id));
        return -1;
    }

    ProgramInfo pi(fileName,
                         nullptr, // _plot,
                         nullptr, // _title,
                         nullptr, // const QString &_sortTitle,
                         nullptr, // const QString &_subtitle,
                         nullptr, // const QString &_sortSubtitle,
                         nullptr, // const QString &_director,
                         0, // int _season,
                         0, // int _episode,
                         nullptr, // const QString &_inetref,
                         0min, // uint _length_in_minutes,
                         0, // uint _year,
                         nullptr); //const QString &_programid);

    long ret = pi.QueryBookmark();
    return ret;
}

V2VideoMetadataInfoList* V2Video::GetVideoList( const QString &Folder,
                                                 const QString &Sort,
                                                 bool bDescending,
                                                 int nStartIndex,
                                                 int nCount       )
{
    QString fields = "title,director,studio,plot,rating,year,releasedate,"
                     "userrating,length,playcount,filename,hash,showlevel,"
                     "coverfile,inetref,collectionref,homepage,childid,browse,watched,"
                     "playcommand,category,intid,trailer,screenshot,banner,fanart,"
                     "subtitle,tagline,season,episode,host,insertdate,processed,contenttype";

    QStringList sortFields = fields.split(',');

    VideoMetadataListManager::metadata_list videolist;

    QString sql = "";
    if (!Folder.isEmpty())
        sql.append(" WHERE filename LIKE '" + Folder + "%'");
    sql.append(" ORDER BY ");
    QString sort = Sort.toLower();
    if (sort == "added")
        sql.append("insertdate");
    else if (sort == "released")
        sql.append("releasedate");
    else if (sortFields.contains(sort))
        sql.append(sort);
    else
        sql.append("intid");

    if (bDescending)
        sql += " DESC";
    VideoMetadataListManager::loadAllFromDatabase(videolist, sql);

    std::vector<VideoMetadataListManager::VideoMetadataPtr> videos(videolist.begin(), videolist.end());

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pVideoMetadataInfos = new V2VideoMetadataInfoList();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)videos.size() ) : 0;
    nCount        = (nCount > 0) ? std::min( nCount, (int)videos.size() ) : videos.size();
    int nEndIndex = std::min((nStartIndex + nCount), (int)videos.size() );

    for( int n = nStartIndex; n < nEndIndex; n++ )
    {
        V2VideoMetadataInfo *pVideoMetadataInfo = pVideoMetadataInfos->AddNewVideoMetadataInfo();

        VideoMetadataListManager::VideoMetadataPtr metadata = videos[n];

        if (metadata)
            V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );
    }

    int curPage = 0;
    int totalPages = 0;
    if (nCount == 0)
        totalPages = 1;
    else
        totalPages = (int)ceil((float)videos.size() / nCount);

    if (totalPages == 1)
        curPage = 1;
    else
    {
        curPage = (int)ceil((float)nStartIndex / nCount) + 1;
    }

    pVideoMetadataInfos->setStartIndex    ( nStartIndex     );
    pVideoMetadataInfos->setCount         ( nCount          );
    pVideoMetadataInfos->setCurrentPage   ( curPage         );
    pVideoMetadataInfos->setTotalPages    ( totalPages      );
    pVideoMetadataInfos->setTotalAvailable( videos.size()   );
    pVideoMetadataInfos->setAsOf          ( MythDate::current() );
    pVideoMetadataInfos->setVersion       ( MYTH_BINARY_VERSION );
    pVideoMetadataInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pVideoMetadataInfos;
}
