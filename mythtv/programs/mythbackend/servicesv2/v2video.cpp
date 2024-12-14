// MythTV
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/storagegroup.h"
#include "libmythmetadata/bluraymetadata.h"
#include "libmythmetadata/globals.h"
#include "libmythmetadata/metadatafactory.h"
#include "libmythmetadata/videometadata.h"
#include "libmythtv/mythavutil.h"

// MythBackend
#include "v2artworkInfoList.h"
#include "v2castMemberList.h"
#include "v2genreList.h"
#include "v2serviceUtil.h"
#include "v2video.h"


// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (VIDEO_HANDLE, V2Video::staticMetaObject, &V2Video::RegisterCustomTypes))

void V2Video::RegisterCustomTypes()
{
    qRegisterMetaType<V2VideoMetadataInfo*>("V2VideoMetadataInfo");
    qRegisterMetaType<V2VideoMetadataInfoList*>("V2VideoMetadataInfoList");
    qRegisterMetaType<V2VideoLookupList*>("V2VideoLookupList");
    qRegisterMetaType<V2BlurayInfo*>("V2BlurayInfo");
    qRegisterMetaType<V2VideoStreamInfoList*>("V2VideoStreamInfoList");
    qRegisterMetaType<V2VideoStreamInfo*>("V2VideoStreamInfo");
    qRegisterMetaType<V2ArtworkInfoList*>("V2ArtworkInfoList");
    qRegisterMetaType<V2ArtworkInfo*>("V2ArtworkInfo");
    qRegisterMetaType<V2CastMemberList*>("V2CastMemberList");
    qRegisterMetaType<V2CastMember*>("V2CastMember");
    qRegisterMetaType<V2GenreList*>("V2GenreList");
    qRegisterMetaType<V2Genre*>("V2Genre");
    qRegisterMetaType<V2VideoLookupList*>("V2VideoLookupList");
    qRegisterMetaType<V2VideoLookup*>("V2VideoLookup");
    qRegisterMetaType<V2ArtworkItem*>("V2ArtworkItem");
    qRegisterMetaType<V2CutList*>("V2CutList");
    qRegisterMetaType<V2Cutting*>("V2Cutting");
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

/////////////////////////////////////////////////////////////////////////////
// Get last play pos of a video as a frame number.
/////////////////////////////////////////////////////////////////////////////

long V2Video::GetLastPlayPos( int  Id )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("V2Video::GetLastPlayPos", query);
        return 0;
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("V2Video/GetLastPlayPos Video id %1 Not found.").arg(Id));
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

    long ret = pi.QueryLastPlayPos();
    return ret;
}


// If CollapseSubDirs is true, then files in subdirectories are not returned.
// Instead, one row is returned for each subdirectory, with the full
// directory name in FileName and the lowest part of the directory name
// in Title. These directories are returned at the beginning of the list.
//
// Example: If the database has these files:
// one.mkv
// dir1/two.mkv
// dir1/dir2/two.mkv
//
// With no Folder name and CollapseSubDirs=true, you get
// one.mkv
// dir1          Title=dir1
//
// With Folder=dir1 and CollapseSubDirs=true, you get
// dir1/two.mkv
// dir1/dir2     Title=dir2

V2VideoMetadataInfoList* V2Video::GetVideoList( const QString &Folder,
                                                 const QString &Sort,
                                                 bool bDescending,
                                                 int nStartIndex,
                                                 int nCount,
                                                 bool CollapseSubDirs )
{
    QString fields = "title,director,studio,plot,rating,year,releasedate,"
                     "userrating,length,playcount,filename,hash,showlevel,"
                     "coverfile,inetref,collectionref,homepage,childid,browse,watched,"
                     "playcommand,category,intid,trailer,screenshot,banner,fanart,"
                     "subtitle,tagline,season,episode,host,insertdate,processed,contenttype";

    QStringList sortFields = fields.split(',');

    VideoMetadataListManager::metadata_list videolist;

    QString sql = "";
    QString folder;
    QString bindValue;
    if (!Folder.isEmpty())
    {
        if (Folder.endsWith('/'))
            folder = Folder;
        else
            folder = Folder + "/";
        bindValue = folder + "%";
        sql.append(" WHERE filename LIKE :BINDVALUE ");
    }
    sql.append(" ORDER BY ");
    QString defSeq = " ASC";
    if (bDescending)
        defSeq = " DESC";

    QStringList sortList = Sort.toLower().split(',',Qt::SkipEmptyParts);
    bool next = false;
    for (const auto & item : std::as_const(sortList))
    {
        QStringList partList = item.split(' ',Qt::SkipEmptyParts);
        if (partList.empty())
            continue;
        QString sort = partList[0];
        if (sort == "added")
            sort = "insertdate";
        else if (sort == "released")
            sort = "releasedate";
        if (sortFields.contains(sort))
        {
            if (next)
                sql.append(",");
            sql.append(sort);
            if (partList.length() > 1 && partList[1].compare("DESC",Qt::CaseInsensitive) == 0)
                sql.append(" DESC");
            else if (partList.length() > 1 && partList[1].compare("ASC",Qt::CaseInsensitive) == 0)
                sql.append(" ASC");
            else
                sql.append(defSeq);
            next = true;
        }
    }
    if (!next)
    {
        sql.append("intid");
        sql.append(defSeq);
    }

    VideoMetadataListManager::loadAllFromDatabase(videolist, sql, bindValue);
    std::vector<VideoMetadataListManager::VideoMetadataPtr> videos(videolist.begin(), videolist.end());

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    auto *pVideoMetadataInfos = new V2VideoMetadataInfoList();
    QMap<QString, QString> map;
    int folderlen = folder.length();

    nStartIndex   = (nStartIndex > 0) ? std::min( nStartIndex, (int)videos.size() ) : 0;
    int selectedCount = 0;
    int totalCount = 0;
    QString dir;

    // Make directory list
    if (CollapseSubDirs)
    {
        for( const auto& metadata : videos )
        {
            if (!metadata)
                break;
            QString fnPart = metadata->GetFilename().mid(folderlen);
            int slashPos = fnPart.indexOf('/',1);
            if (slashPos >= 0)
            {
                dir = fnPart.mid(0, slashPos);
                if (!map.contains(dir))
                {
                    // use toLower here so that lower case are sorted in with
                    // upper case rather than separately at the end.
                    map.insert(dir.toLower(), dir);
                }
            }
        }
        // Insert directory entries at the front of the list, ordered ascending
        // or descending, depending on the value of bDescending
        QMapIterator<QString, QString> it(map);
        if (bDescending)
            it.toBack();

        while (bDescending? it.hasPrevious() : it.hasNext())
        {
            if (bDescending)
                it.previous();
            else
                it.next();
            if (totalCount >= nStartIndex && (nCount == 0 || selectedCount < nCount)) {
                V2VideoMetadataInfo *pVideoMetadataInfo =
                    pVideoMetadataInfos->AddNewVideoMetadataInfo();
                pVideoMetadataInfo->setContentType("DIRECTORY");
                pVideoMetadataInfo->setFileName(folder + it.value());
                pVideoMetadataInfo->setTitle(it.value());
                selectedCount++;
            }
            totalCount++;
        }
    }

    for( const auto& metadata : videos )
    {
        if (!metadata)
            break;

        if (CollapseSubDirs)
        {
            QString fnPart = metadata->GetFilename().mid(folderlen);
            int slashPos = fnPart.indexOf('/',1);
            if (slashPos >= 0)
                continue;
        }

        if (totalCount >= nStartIndex && (nCount == 0 || selectedCount < nCount)) {
            V2VideoMetadataInfo *pVideoMetadataInfo = pVideoMetadataInfos->AddNewVideoMetadataInfo();
            V2FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );
            selectedCount++;
        }
        totalCount++;
    }

    int curPage = 0;
    int totalPages = 0;
    if (nCount == 0)
        totalPages = 1;
    else
        totalPages = (int)ceil((float)totalCount / nCount);

    if (totalPages == 1)
        curPage = 1;
    else
    {
        curPage = (int)ceil((float)nStartIndex / nCount) + 1;
    }

    pVideoMetadataInfos->setStartIndex    ( nStartIndex     );
    pVideoMetadataInfos->setCount         ( selectedCount   );
    pVideoMetadataInfos->setCurrentPage   ( curPage         );
    pVideoMetadataInfos->setTotalPages    ( totalPages      );
    pVideoMetadataInfos->setTotalAvailable( totalCount      );
    pVideoMetadataInfos->setAsOf          ( MythDate::current() );
    pVideoMetadataInfos->setVersion       ( MYTH_BINARY_VERSION );
    pVideoMetadataInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pVideoMetadataInfos;
}


V2VideoLookupList* V2Video::LookupVideo( const QString    &Title,
                                              const QString    &Subtitle,
                                              const QString    &Inetref,
                                              int              Season,
                                              int              Episode,
                                              const QString    &GrabberType,
                                              bool             AllowGeneric )
{
    auto *pVideoLookups = new V2VideoLookupList();

    MetadataLookupList list;

    auto *factory = new MetadataFactory(nullptr);

    if (factory)
    {
        list = factory->SynchronousLookup(Title, Subtitle,
                                         Inetref, Season, Episode,
                                         GrabberType, AllowGeneric);
    }

    if ( list.empty() )
        return pVideoLookups;

    //MetadataLookupList is a reference counted list.
    //it will delete all its content at its end of life
    for(const auto & lookup : std::as_const(list))
    {
        V2VideoLookup *pVideoLookup = pVideoLookups->AddNewVideoLookup();

        if (lookup)
        {
            pVideoLookup->setTitle(lookup->GetTitle());
            pVideoLookup->setSubTitle(lookup->GetSubtitle());
            pVideoLookup->setSeason(lookup->GetSeason());
            pVideoLookup->setEpisode(lookup->GetEpisode());
            pVideoLookup->setYear(lookup->GetYear());
            pVideoLookup->setTagline(lookup->GetTagline());
            pVideoLookup->setDescription(lookup->GetDescription());
            pVideoLookup->setCertification(lookup->GetCertification());
            pVideoLookup->setInetref(lookup->GetInetref());
            pVideoLookup->setCollectionref(lookup->GetCollectionref());
            pVideoLookup->setHomePage(lookup->GetHomepage());
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
            pVideoLookup->setReleaseDate(
                QDateTime(lookup->GetReleaseDate(),
                          QTime(0,0),Qt::LocalTime).toUTC());
#else
            pVideoLookup->setReleaseDate(
                QDateTime(lookup->GetReleaseDate(),
                          QTime(0,0),
                          QTimeZone(QTimeZone::LocalTime)).toUTC());
#endif
            pVideoLookup->setUserRating(lookup->GetUserRating());
            pVideoLookup->setLength(lookup->GetRuntime().count());
            pVideoLookup->setLanguage(lookup->GetLanguage());
            pVideoLookup->setCountries(lookup->GetCountries());
            pVideoLookup->setPopularity(lookup->GetPopularity());
            pVideoLookup->setBudget(lookup->GetBudget());
            pVideoLookup->setRevenue(lookup->GetRevenue());
            pVideoLookup->setIMDB(lookup->GetIMDB());
            pVideoLookup->setTMSRef(lookup->GetTMSref());

            ArtworkList coverartlist = lookup->GetArtwork(kArtworkCoverart);
            ArtworkList::iterator c;
            for (c = coverartlist.begin(); c != coverartlist.end(); ++c)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("coverart");
                art->setUrl((*c).url);
                art->setThumbnail((*c).thumbnail);
                art->setWidth((*c).width);
                art->setHeight((*c).height);
            }
            ArtworkList fanartlist = lookup->GetArtwork(kArtworkFanart);
            ArtworkList::iterator f;
            for (f = fanartlist.begin(); f != fanartlist.end(); ++f)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("fanart");
                art->setUrl((*f).url);
                art->setThumbnail((*f).thumbnail);
                art->setWidth((*f).width);
                art->setHeight((*f).height);
            }
            ArtworkList bannerlist = lookup->GetArtwork(kArtworkBanner);
            ArtworkList::iterator b;
            for (b = bannerlist.begin(); b != bannerlist.end(); ++b)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("banner");
                art->setUrl((*b).url);
                art->setThumbnail((*b).thumbnail);
                art->setWidth((*b).width);
                art->setHeight((*b).height);
            }
            ArtworkList screenshotlist = lookup->GetArtwork(kArtworkScreenshot);
            ArtworkList::iterator s;
            for (s = screenshotlist.begin(); s != screenshotlist.end(); ++s)
            {
                V2ArtworkItem *art = pVideoLookup->AddNewArtwork();
                art->setType("screenshot");
                art->setUrl((*s).url);
                art->setThumbnail((*s).thumbnail);
                art->setWidth((*s).width);
                art->setHeight((*s).height);
            }
        }
    }

    pVideoLookups->setCount         ( list.count()                 );
    pVideoLookups->setAsOf          ( MythDate::current() );
    pVideoLookups->setVersion       ( MYTH_BINARY_VERSION          );
    pVideoLookups->setProtoVer      ( MYTH_PROTO_VERSION           );

    delete factory;

    return pVideoLookups;
}


bool V2Video::RemoveVideoFromDB( int Id )
{
    bool bResult = false;

    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(Id);

    if (metadata)
        bResult = metadata->DeleteFromDatabase();

    return bResult;
}

bool V2Video::AddVideo( const QString &sFileName,
                      const QString &sHostName )
{
    if ( sHostName.isEmpty() )
        throw( QString( "Host not provided! Local storage is deprecated and "
                        "is not supported by the API." ));

    if ( sFileName.isEmpty() ||
        (sFileName.contains("/../")) ||
        (sFileName.startsWith("../")) )
    {
        throw( QString( "Filename not provided, or fails sanity checks!" ));
    }

    StorageGroup sgroup("Videos", sHostName);

    QString fullname = sgroup.FindFile(sFileName);

    if ( !QFile::exists(fullname) )
        throw( QString( "Provided filename does not exist!" ));

    QString hash = FileHash(fullname);

    if (hash == "NULL")
    {
        LOG(VB_GENERAL, LOG_ERR, "Video Hash Failed. Unless this is a DVD or "
                                 "Blu-ray, something has probably gone wrong.");
        hash = "";
    }

    VideoMetadata newFile(sFileName, QString(), hash,
                          VIDEO_TRAILER_DEFAULT,
                          VIDEO_COVERFILE_DEFAULT,
                          VIDEO_SCREENSHOT_DEFAULT,
                          VIDEO_BANNER_DEFAULT,
                          VIDEO_FANART_DEFAULT,
                          QString(), QString(), QString(), QString(),
                          QString(), VIDEO_YEAR_DEFAULT,
                          QDate::fromString("0000-00-00","YYYY-MM-DD"),
                          VIDEO_INETREF_DEFAULT, 0, QString(),
                          VIDEO_DIRECTOR_DEFAULT, QString(), VIDEO_PLOT_DEFAULT,
                          0.0, VIDEO_RATING_DEFAULT, 0, 0,
                          0, 0,
                          MythDate::current().date(), 0,
                          ParentalLevel::plLowest);

    newFile.SetHost(sHostName);
    newFile.SaveToDatabase();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Video::UpdateVideoWatchedStatus ( int  nId,
                                       bool bWatched )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(nId);

    if ( !metadata )
        return false;

    metadata->SetWatched(bWatched);
    metadata->UpdateDatabase();

    return true;
}

bool V2Video::UpdateVideoMetadata ( int           nId,
                                  const QString &sTitle,
                                  const QString &sSubTitle,
                                  const QString &sTagLine,
                                  const QString &sDirector,
                                  const QString &sStudio,
                                  const QString &sPlot,
                                  const QString &sRating,
                                  const QString &sInetref,
                                  int           nCollectionRef,
                                  const QString &sHomePage,
                                  int           nYear,
                                  QDate         sReleasedate,
                                  float         fUserRating,
                                  int           nLength,
                                  int           nPlayCount,
                                  int           nSeason,
                                  int           nEpisode,
                                  int           nShowLevel,
                                  const QString &sFileName,
                                  const QString &sHash,
                                  const QString &sCoverFile,
                                  int           nChildID,
                                  bool          bBrowse,
                                  bool          bWatched,
                                  bool          bProcessed,
                                  const QString &sPlayCommand,
                                  int           nCategory,
                                  const QString &sTrailer,
                                  const QString &sHost,
                                  const QString &sScreenshot,
                                  const QString &sBanner,
                                  const QString &sFanart,
                                  QDate         sInsertDate,
                                  const QString &sContentType,
                                  const QString &sGenres,
                                  const QString &sCast,
                                  const QString &sCountries)
{
    bool update_required = false;
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    QScopedPointer<VideoMetadataListManager> mlm(new VideoMetadataListManager());
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(nId);

    if (!metadata)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("UpdateVideoMetadata: Id=%1 not found")
            .arg(nId));
        return false;
    }

    if (HAS_PARAMv2("Title"))
    {
        metadata->SetTitle(sTitle);
        update_required = true;
    }

    if (HAS_PARAMv2("SubTitle"))
    {
        metadata->SetSubtitle(sSubTitle);
        update_required = true;
    }

    if (HAS_PARAMv2("TagLine"))
    {
        metadata->SetTagline(sTagLine);
        update_required = true;
    }

    if (HAS_PARAMv2("Director"))
    {
        metadata->SetDirector(sDirector);
        update_required = true;
    }

    if (HAS_PARAMv2("Studio"))
    {
        metadata->SetStudio(sStudio);
        update_required = true;
    }

    if (HAS_PARAMv2("Plot"))
    {
        metadata->SetPlot(sPlot);
        update_required = true;
    }

    if (HAS_PARAMv2("UserRating"))
    {
        metadata->SetUserRating(fUserRating);
        update_required = true;
    }

    if (HAS_PARAMv2("Inetref"))
    {
        metadata->SetInetRef(sInetref);
        update_required = true;
    }

    if (HAS_PARAMv2("CollectionRef"))
    {
        metadata->SetCollectionRef(nCollectionRef);
        update_required = true;
    }

    if (HAS_PARAMv2("HomePage"))
    {
        metadata->SetHomepage(sHomePage);
        update_required = true;
    }

    if (HAS_PARAMv2("Year"))
    {
        metadata->SetYear(nYear);
        update_required = true;
    }

    if (HAS_PARAMv2("ReleaseDate"))
    {
        metadata->SetReleaseDate(sReleasedate);
        update_required = true;
    }

    if (HAS_PARAMv2("Rating"))
    {
        metadata->SetRating(sRating);
        update_required = true;
    }

    if (HAS_PARAMv2("Length"))
    {
        metadata->SetLength(std::chrono::minutes(nLength));
        update_required = true;
    }

    if (HAS_PARAMv2("PlayCount"))
    {
        metadata->SetPlayCount(nPlayCount);
        update_required = true;
    }

    if (HAS_PARAMv2("Season"))
    {
        metadata->SetSeason(nSeason);
        update_required = true;
    }

    if (HAS_PARAMv2("Episode"))
    {
        metadata->SetEpisode(nEpisode);
        update_required = true;
    }

    if (HAS_PARAMv2("ShowLevel"))
    {
        metadata->SetShowLevel(ParentalLevel::Level(nShowLevel));
        update_required = true;
    }

    if (HAS_PARAMv2("FileName"))
    {
        metadata->SetFilename(sFileName);
        update_required = true;
    }

    if (HAS_PARAMv2("Hash"))
    {
        metadata->SetHash(sHash);
        update_required = true;
    }

    if (HAS_PARAMv2("CoverFile"))
    {
        metadata->SetCoverFile(sCoverFile);
        update_required = true;
    }

    if (HAS_PARAMv2("ChildID"))
    {
        metadata->SetChildID(nChildID);
        update_required = true;
    }

    if (HAS_PARAMv2("Browse"))
    {
        metadata->SetBrowse(bBrowse);
        update_required = true;
    }

    if (HAS_PARAMv2("Watched"))
    {
        metadata->SetWatched(bWatched);
        update_required = true;
    }

    if (HAS_PARAMv2("Processed"))
    {
        metadata->SetProcessed(bProcessed);
        update_required = true;
    }

    if (HAS_PARAMv2("PlayCommand"))
    {
        metadata->SetPlayCommand(sPlayCommand);
        update_required = true;
    }

    if (HAS_PARAMv2("Category"))
    {
        metadata->SetCategoryID(nCategory);
        update_required = true;
    }

    if (HAS_PARAMv2("Trailer"))
    {
        metadata->SetTrailer(sTrailer);
        update_required = true;
    }

    if (HAS_PARAMv2("Host"))
    {
        metadata->SetHost(sHost);
        update_required = true;
    }

    if (HAS_PARAMv2("Screenshot"))
    {
        metadata->SetScreenshot(sScreenshot);
        update_required = true;
    }

    if (HAS_PARAMv2("Banner"))
    {
        metadata->SetBanner(sBanner);
        update_required = true;
    }

    if (HAS_PARAMv2("Fanart"))
    {
        metadata->SetFanart(sFanart);
        update_required = true;
    }

    if (HAS_PARAMv2("InsertDate"))
    {
        metadata->SetInsertdate(sInsertDate);
        update_required = true;
    }

    if (HAS_PARAMv2("ContentType"))
    {
        // valid values for ContentType are 'MOVIE','TELEVISION','ADULT','MUSICVIDEO','HOMEVIDEO'
        VideoContentType contentType = kContentUnknown;
        if (sContentType == "MOVIE")
            contentType = kContentMovie;

        if (sContentType == "TELEVISION")
            contentType = kContentTelevision;

        if (sContentType == "ADULT")
            contentType = kContentAdult;

        if (sContentType == "MUSICVIDEO")
            contentType = kContentMusicVideo;

        if (sContentType == "HOMEVIDEO")
            contentType = kContentHomeMovie;

        if (contentType != kContentUnknown)
        {
            metadata->SetContentType(contentType);
            update_required = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("UpdateVideoMetadata: Ignoring unknown ContentType: %1").arg(sContentType));
        }
    }

    if (HAS_PARAMv2("Genres"))
    {
        VideoMetadata::genre_list genres;
        QStringList genresList = sGenres.split(',', Qt::SkipEmptyParts);
        std::transform(genresList.cbegin(), genresList.cend(), std::back_inserter(genres),
                       [](const QString& name)
                           {return VideoMetadata::genre_list::value_type(-1, name.simplified());} );

        metadata->SetGenres(genres);
        update_required = true;
    }

    if (HAS_PARAMv2("Cast"))
    {
        VideoMetadata::cast_list cast;
        QStringList castList = sCast.split(',', Qt::SkipEmptyParts);
        std::transform(castList.cbegin(), castList.cend(), std::back_inserter(cast),
                       [](const QString& name)
                           {return VideoMetadata::cast_list::value_type(-1, name.simplified());} );

        metadata->SetCast(cast);
        update_required = true;
    }

    if (HAS_PARAMv2("Countries"))
    {
        VideoMetadata::country_list countries;
        QStringList countryList = sCountries.split(',', Qt::SkipEmptyParts);
        std::transform(countryList.cbegin(), countryList.cend(), std::back_inserter(countries),
                       [](const QString& name)
                           {return VideoMetadata::country_list::value_type(-1, name.simplified());} );

        metadata->SetCountries(countries);
        update_required = true;
    }

    if (update_required)
        metadata->UpdateDatabase();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Set bookmark of a video as a frame number.
/////////////////////////////////////////////////////////////////////////////

bool V2Video::SetSavedBookmark( int  Id, long Offset )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("Video::SetSavedBookmark", query);
        return false;
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Video/SetSavedBookmark Video id %1 Not found.").arg(Id));
        return false;
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

    pi.SaveBookmark(Offset);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Set bookmark of a video as a frame number.
/////////////////////////////////////////////////////////////////////////////

bool V2Video::SetLastPlayPos( int  Id, long Offset )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("Video::SetLastPlayPos", query);
        return false;
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Video/SetLastPlayPos Video id %1 Not found.").arg(Id));
        return false;
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

    pi.SaveLastPlayPos(Offset);
    return true;
}


V2BlurayInfo* V2Video::GetBluray( const QString &sPath )
{
    QString path = sPath;

    if (sPath.isEmpty())
        path = gCoreContext->GetSetting( "BluRayMountpoint", "/media/cdrom");

    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Parsing Blu-ray at path: %1 ").arg(path));

    auto *bdmeta = new BlurayMetadata(path);

    if ( !bdmeta )
        throw( QString( "Unable to open Blu-ray Metadata Parser!" ));

    if ( !bdmeta->OpenDisc() )
        throw( QString( "Unable to open Blu-ray Disc/Path!" ));

    if ( !bdmeta->ParseDisc() )
        throw( QString( "Unable to parse metadata from Blu-ray Disc/Path!" ));

    auto *pBlurayInfo = new V2BlurayInfo();

    pBlurayInfo->setPath(path);
    pBlurayInfo->setTitle(bdmeta->GetTitle());
    pBlurayInfo->setAltTitle(bdmeta->GetAlternateTitle());
    pBlurayInfo->setDiscLang(bdmeta->GetDiscLanguage());
    pBlurayInfo->setDiscNum(bdmeta->GetCurrentDiscNumber());
    pBlurayInfo->setTotalDiscNum(bdmeta->GetTotalDiscNumber());
    pBlurayInfo->setTitleCount(bdmeta->GetTitleCount());
    pBlurayInfo->setThumbCount(bdmeta->GetThumbnailCount());
    pBlurayInfo->setTopMenuSupported(bdmeta->GetTopMenuSupported());
    pBlurayInfo->setFirstPlaySupported(bdmeta->GetFirstPlaySupported());
    pBlurayInfo->setNumHDMVTitles(bdmeta->GetNumHDMVTitles());
    pBlurayInfo->setNumBDJTitles(bdmeta->GetNumBDJTitles());
    pBlurayInfo->setNumUnsupportedTitles(bdmeta->GetNumUnsupportedTitles());
    pBlurayInfo->setAACSDetected(bdmeta->GetAACSDetected());
    pBlurayInfo->setLibAACSDetected(bdmeta->GetLibAACSDetected());
    pBlurayInfo->setAACSHandled(bdmeta->GetAACSHandled());
    pBlurayInfo->setBDPlusDetected(bdmeta->GetBDPlusDetected());
    pBlurayInfo->setLibBDPlusDetected(bdmeta->GetLibBDPlusDetected());
    pBlurayInfo->setBDPlusHandled(bdmeta->GetBDPlusHandled());

    QStringList thumbs = bdmeta->GetThumbnails();
    if (!thumbs.empty())
        pBlurayInfo->setThumbPath(thumbs.at(0));

    delete bdmeta;

    return pBlurayInfo;
}


/////////////////////////////////////////////////////////////////////////////
// Jun 3, 2020
// Service to get stream info for all streams in a media file.
// This gets some basic info. If anything more is needed it can be added,
// depending on whether it is available from ffmpeg avformat apis.
// See the MythStreamInfoList class for the code that uses avformat to
// extract the information.
/////////////////////////////////////////////////////////////////////////////

V2VideoStreamInfoList* V2Video::GetStreamInfo
           ( const QString &storageGroup,
             const QString &FileName  )
{

    // Search for the filename

    StorageGroup storage( storageGroup );
    QString sFullFileName = storage.FindFile( FileName );
    MythStreamInfoList infos(sFullFileName);

    // The constructor of this class reads the file and gets the needed
    // information.
    auto *pVideoStreamInfos = new V2VideoStreamInfoList();

    pVideoStreamInfos->setCount         ( infos.m_streamInfoList.size() );
    pVideoStreamInfos->setAsOf          ( MythDate::current() );
    pVideoStreamInfos->setVersion       ( MYTH_BINARY_VERSION );
    pVideoStreamInfos->setProtoVer      ( MYTH_PROTO_VERSION  );
    pVideoStreamInfos->setErrorCode     ( infos.m_errorCode   );
    pVideoStreamInfos->setErrorMsg      ( infos.m_errorMsg    );

    for (const auto & info : std::as_const(infos.m_streamInfoList))
    {
        V2VideoStreamInfo *pVideoStreamInfo = pVideoStreamInfos->AddNewVideoStreamInfo();
        pVideoStreamInfo->setCodecType       ( QString(QChar(info.m_codecType)) );
        pVideoStreamInfo->setCodecName       ( info.m_codecName   );
        pVideoStreamInfo->setWidth           ( info.m_width 			   );
        pVideoStreamInfo->setHeight          ( info.m_height 			   );
        pVideoStreamInfo->setAspectRatio     ( info.m_SampleAspectRatio    );
        pVideoStreamInfo->setFieldOrder      ( info.m_fieldOrder           );
        pVideoStreamInfo->setFrameRate       ( info.m_frameRate            );
        pVideoStreamInfo->setAvgFrameRate    ( info.m_avgFrameRate 		   );
        pVideoStreamInfo->setChannels        ( info.m_channels   );
        pVideoStreamInfo->setDuration        ( info.m_duration   );

    }
    return pVideoStreamInfos;
}

/////////////////////////////////////////////////////////////////////////////
// October 26,2022
// Support for Cut List for Videos
/////////////////////////////////////////////////////////////////////////////

V2CutList* V2Video::GetVideoCutList ( int Id,
                                          const QString &offsettype,
                                          bool includeFps )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("V2Video::GetVideoCommBreak", query);
        throw QString("Database Error.");
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("V2Video/GetVideoCommBreak Video id %1 Not found.").arg(Id));
        throw QString("Video Not Found.");
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


    int marktype = 0;

    auto* pCutList = new V2CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
        marktype = 2;
    else
        marktype = 0;

    V2FillCutList(pCutList, &pi, marktype, includeFps);

    return pCutList;
}

/////////////////////////////////////////////////////////////////////////////
// October 26,2022
// Support for Commercial Break List for Videos
/////////////////////////////////////////////////////////////////////////////

V2CutList* V2Video::GetVideoCommBreak ( int Id,
                                          const QString &offsettype,
                                          bool includeFps )
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT filename "
                  "FROM videometadata "
                  "WHERE intid = :ID ");
    query.bindValue(":ID", Id);

    if (!query.exec())
    {
        MythDB::DBError("V2Video::GetVideoCommBreak", query);
        throw QString("Database Error.");
    }

    QString fileName;

    if (query.next())
        fileName = query.value(0).toString();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("V2Video/GetVideoCommBreak Video id %1 Not found.").arg(Id));
        throw QString("Video Not Found.");
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


    int marktype = 0;

    auto* pCutList = new V2CutList();
    if (offsettype.toLower() == "position")
        marktype = 1;
    else if (offsettype.toLower() == "duration")
        marktype = 2;
    else
        marktype = 0;

    V2FillCommBreak(pCutList, &pi, marktype, includeFps);

    return pCutList;
}
