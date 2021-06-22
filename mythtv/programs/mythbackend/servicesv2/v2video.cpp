#include "v2video.h"
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
