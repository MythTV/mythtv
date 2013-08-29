//////////////////////////////////////////////////////////////////////////////
// Program Name: video.cpp
// Created     : Apr. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QList>
#include <QFile>
#include <QMutex>

#include <math.h>

#include "video.h"

#include "videometadata.h"
#include "metadatafactory.h"
#include "bluraymetadata.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "storagegroup.h"
#include "remotefile.h"
#include "globals.h"
#include "mythdate.h"
#include "serviceUtil.h"
#include "mythmiscutil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMetadataInfoList* Video::GetVideoList( bool bDescending,
                                                 int nStartIndex,
                                                 int nCount       )
{
    VideoMetadataListManager::metadata_list videolist;
    QString sql = "ORDER BY intid";
    if (bDescending)
        sql += " DESC";
    VideoMetadataListManager::loadAllFromDatabase(videolist, sql);

    std::vector<VideoMetadataListManager::VideoMetadataPtr> videos(videolist.begin(), videolist.end());

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::VideoMetadataInfoList *pVideoMetadataInfos = new DTC::VideoMetadataInfoList();

    nStartIndex   = min( nStartIndex, (int)videos.size() );
    nCount        = (nCount > 0) ? min( nCount, (int)videos.size() ) : videos.size();
    int nEndIndex = min((nStartIndex + nCount), (int)videos.size() );

    for( int n = nStartIndex; n < nEndIndex; n++ )
    {
        DTC::VideoMetadataInfo *pVideoMetadataInfo = pVideoMetadataInfos->AddNewVideoMetadataInfo();

        VideoMetadataListManager::VideoMetadataPtr metadata = videos[n];

        if (metadata)
            FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );
    }

    int curPage = 0, totalPages = 0;
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMetadataInfo* Video::GetVideo( int Id )
{
    VideoMetadataListManager::VideoMetadataPtr metadata =
                          VideoMetadataListManager::loadOneFromDatabase(Id);

    if ( !metadata )
        throw( QString( "No metadata found for selected ID!." ));

    DTC::VideoMetadataInfo *pVideoMetadataInfo = new DTC::VideoMetadataInfo();

    FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );

    return pVideoMetadataInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMetadataInfo* Video::GetVideoByFileName( const QString &FileName )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    VideoMetadataListManager *mlm = new VideoMetadataListManager();
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byFilename(FileName);

    if ( !metadata )
        throw( QString( "No metadata found for selected filename!." ));

    DTC::VideoMetadataInfo *pVideoMetadataInfo = new DTC::VideoMetadataInfo();

    FillVideoMetadataInfo ( pVideoMetadataInfo, metadata, true );

    delete mlm;

    return pVideoMetadataInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoLookupList* Video::LookupVideo( const QString    &Title,
                                              const QString    &Subtitle,
                                              const QString    &Inetref,
                                              int              Season,
                                              int              Episode,
                                              const QString    &GrabberType,
                                              bool             AllowGeneric )
{
    DTC::VideoLookupList *pVideoLookups = new DTC::VideoLookupList();

    MetadataLookupList list;

    MetadataFactory *factory = new MetadataFactory(NULL);

    if (factory)
        list = factory->SynchronousLookup(Title, Subtitle,
                                         Inetref, Season, Episode,
                                         GrabberType, AllowGeneric);

    if ( !list.size() )
        return pVideoLookups;

    //MetadataLookupList is a reference counted list.
    //it will delete all its content at its end of life
    for( int n = 0; n < list.size(); n++ )
    {
        DTC::VideoLookup *pVideoLookup = pVideoLookups->AddNewVideoLookup();

        MetadataLookup *lookup = list[n];

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
            pVideoLookup->setReleaseDate(
                QDateTime(lookup->GetReleaseDate(),
                          QTime(0,0),Qt::LocalTime).toUTC());
            pVideoLookup->setUserRating(lookup->GetUserRating());
            pVideoLookup->setLength(lookup->GetRuntime());
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
                DTC::ArtworkItem *art = pVideoLookup->AddNewArtwork();
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
                DTC::ArtworkItem *art = pVideoLookup->AddNewArtwork();
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
                DTC::ArtworkItem *art = pVideoLookup->AddNewArtwork();
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
                DTC::ArtworkItem *art = pVideoLookup->AddNewArtwork();
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Video::RemoveVideoFromDB( int Id )
{
    bool bResult = false;

    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    VideoMetadataListManager *mlm = new VideoMetadataListManager();
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(Id);

    if (metadata)
        bResult = metadata->DeleteFromDatabase();

    delete mlm;

    return bResult;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Video::AddVideo( const QString &sFileName,
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

    VideoMetadata newFile(sFileName, hash,
                          VIDEO_TRAILER_DEFAULT,
                          VIDEO_COVERFILE_DEFAULT,
                          VIDEO_SCREENSHOT_DEFAULT,
                          VIDEO_BANNER_DEFAULT,
                          VIDEO_FANART_DEFAULT,
                          VideoMetadata::FilenameToMeta(sFileName, 1),
                          VideoMetadata::FilenameToMeta(sFileName, 4),
                          QString(), VIDEO_YEAR_DEFAULT,
                          QDate::fromString("0000-00-00","YYYY-MM-DD"),
                          VIDEO_INETREF_DEFAULT, 0, QString(),
                          VIDEO_DIRECTOR_DEFAULT, QString(), VIDEO_PLOT_DEFAULT,
                          0.0, VIDEO_RATING_DEFAULT, 0, 0,
                          VideoMetadata::FilenameToMeta(sFileName, 2).toInt(),
                          VideoMetadata::FilenameToMeta(sFileName, 3).toInt(),
                          MythDate::current().date(), 0,
                          ParentalLevel::plLowest);

    newFile.SetHost(sHostName);
    newFile.SaveToDatabase();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::BlurayInfo* Video::GetBluray( const QString &sPath )
{
    QString path = sPath;

    if (sPath.isEmpty())
        path = gCoreContext->GetSetting( "BluRayMountpoint", "/media/cdrom");

    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Parsing Blu-ray at path: %1 ").arg(path));

    BlurayMetadata *bdmeta = new BlurayMetadata(path);

    if ( !bdmeta )
        throw( QString( "Unable to open Blu-ray Metadata Parser!" ));

    if ( !bdmeta->OpenDisc() )
        throw( QString( "Unable to open Blu-ray Disc/Path!" ));

    if ( !bdmeta->ParseDisc() )
        throw( QString( "Unable to parse metadata from Blu-ray Disc/Path!" ));

    DTC::BlurayInfo *pBlurayInfo = new DTC::BlurayInfo();

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
    pBlurayInfo->setNumHDMVTitles((uint)bdmeta->GetNumHDMVTitles());
    pBlurayInfo->setNumBDJTitles((uint)bdmeta->GetNumBDJTitles());
    pBlurayInfo->setNumUnsupportedTitles((uint)bdmeta->GetNumUnsupportedTitles());
    pBlurayInfo->setAACSDetected(bdmeta->GetAACSDetected());
    pBlurayInfo->setLibAACSDetected(bdmeta->GetLibAACSDetected());
    pBlurayInfo->setAACSHandled(bdmeta->GetAACSHandled());
    pBlurayInfo->setBDPlusDetected(bdmeta->GetBDPlusDetected());
    pBlurayInfo->setLibBDPlusDetected(bdmeta->GetLibBDPlusDetected());
    pBlurayInfo->setBDPlusHandled(bdmeta->GetBDPlusHandled());

    QStringList thumbs = bdmeta->GetThumbnails();
    if (thumbs.size())
        pBlurayInfo->setThumbPath(thumbs.at(0));

    delete bdmeta;

    return pBlurayInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
