//////////////////////////////////////////////////////////////////////////////
// Program Name: video.cpp
// Created     : Apr. 21, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QList>
#include <QFile>

#include <math.h>

#include "video.h"

#include "videometadata.h"
#include "bluraymetadata.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "storagegroup.h"
#include "remotefile.h"
#include "globals.h"
#include "util.h"
#include "serviceUtil.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMetadataInfoList* Video::GetVideos( bool bDescending,
                                              int nStartIndex,
                                              int nCount       )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);

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
        {
            pVideoMetadataInfo->setId(metadata->GetID());
            pVideoMetadataInfo->setTitle(metadata->GetTitle());
            pVideoMetadataInfo->setSubTitle(metadata->GetSubtitle());
            pVideoMetadataInfo->setTagline(metadata->GetTagline());
            pVideoMetadataInfo->setDirector(metadata->GetDirector());
            pVideoMetadataInfo->setStudio(metadata->GetStudio());
            pVideoMetadataInfo->setDescription(metadata->GetPlot());
            pVideoMetadataInfo->setCertification(metadata->GetRating());
            pVideoMetadataInfo->setInetRef(metadata->GetInetRef());
            pVideoMetadataInfo->setHomePage(metadata->GetHomepage());
            pVideoMetadataInfo->setReleaseDate(QDateTime(metadata->GetReleaseDate()));
            pVideoMetadataInfo->setAddDate(QDateTime(metadata->GetInsertdate()));
            pVideoMetadataInfo->setUserRating(metadata->GetUserRating());
            pVideoMetadataInfo->setLength(metadata->GetLength());
            pVideoMetadataInfo->setSeason(metadata->GetSeason());
            pVideoMetadataInfo->setEpisode(metadata->GetEpisode());
            pVideoMetadataInfo->setParentalLevel(metadata->GetShowLevel());
            pVideoMetadataInfo->setVisible(metadata->GetBrowse());
            pVideoMetadataInfo->setWatched(metadata->GetWatched());
            pVideoMetadataInfo->setProcessed(metadata->GetProcessed());
            pVideoMetadataInfo->setFileName(metadata->GetFilename());
            pVideoMetadataInfo->setHash(metadata->GetHash());
            pVideoMetadataInfo->setHost(metadata->GetHost());
            pVideoMetadataInfo->setCoverart(metadata->GetCoverFile());
            pVideoMetadataInfo->setFanart(metadata->GetFanart());
            pVideoMetadataInfo->setBanner(metadata->GetBanner());
            pVideoMetadataInfo->setScreenshot(metadata->GetScreenshot());
            pVideoMetadataInfo->setTrailer(metadata->GetTrailer());
        }
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
    pVideoMetadataInfos->setAsOf          ( QDateTime::currentDateTime() );
    pVideoMetadataInfos->setVersion       ( MYTH_BINARY_VERSION );
    pVideoMetadataInfos->setProtoVer      ( MYTH_PROTO_VERSION  );

    return pVideoMetadataInfos;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMetadataInfo* Video::GetVideoById( int Id )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    VideoMetadataListManager *mlm = new VideoMetadataListManager();
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byID(Id);

    if ( !metadata )
        throw( QString( "No metadata found for selected ID!." ));

    DTC::VideoMetadataInfo *pVideoMetadataInfo = GetInfoFromMetadata(metadata);

    delete mlm;

    return pVideoMetadataInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMetadataInfo* Video::GetVideoByFilename( const QString &Filename )
{
    VideoMetadataListManager::metadata_list videolist;
    VideoMetadataListManager::loadAllFromDatabase(videolist);
    VideoMetadataListManager *mlm = new VideoMetadataListManager();
    mlm->setList(videolist);
    VideoMetadataListManager::VideoMetadataPtr metadata = mlm->byFilename(Filename);

    if ( !metadata )
        throw( QString( "No metadata found for selected filename!." ));

    DTC::VideoMetadataInfo *pVideoMetadataInfo = GetInfoFromMetadata(metadata);

    delete mlm;

    return pVideoMetadataInfo;
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

bool Video::AddVideo( const QString &sFilename,
                      const QString &sHost     )
{
    if ( sHost.isEmpty() )
        throw( QString( "Host not provided! Local storage is deprecated and "
                        "is not supported by the API." ));

    if ( sFilename.isEmpty() ||
        (sFilename.contains("/../")) ||
        (sFilename.startsWith("../")) )
    {
        throw( QString( "Filename not provided, or fails sanity checks!" ));
    }

    StorageGroup sgroup("Videos", sHost);

    QString fullname = sgroup.FindFile(sFilename);

    if ( !QFile::exists(fullname) )
        throw( QString( "Provided filename does not exist!" ));

    QString hash = FileHash(fullname);

    if (hash == "NULL")
    {
        VERBOSE(VB_GENERAL, QString("Video Hash Failed. Unless this is a DVD "
                                    "or Blu-ray, something has probably gone "
                                    "wrong."));
        hash = "";
    }

    VideoMetadata newFile(sFilename, hash,
                          VIDEO_TRAILER_DEFAULT,
                          VIDEO_COVERFILE_DEFAULT,
                          VIDEO_SCREENSHOT_DEFAULT,
                          VIDEO_BANNER_DEFAULT,
                          VIDEO_FANART_DEFAULT,
                          VideoMetadata::FilenameToMeta(sFilename, 1),
                          VideoMetadata::FilenameToMeta(sFilename, 4),
                          QString(), VIDEO_YEAR_DEFAULT,
                          QDate::fromString("0000-00-00","YYYY-MM-DD"),
                          VIDEO_INETREF_DEFAULT, QString(),
                          VIDEO_DIRECTOR_DEFAULT, QString(), VIDEO_PLOT_DEFAULT,
                          0.0, VIDEO_RATING_DEFAULT, 0,
                          VideoMetadata::FilenameToMeta(sFilename, 2).toInt(),
                          VideoMetadata::FilenameToMeta(sFilename, 3).toInt(),
                          QDate::currentDate(), 0, ParentalLevel::plLowest);
    newFile.SetHost(sHost);
    newFile.SaveToDatabase();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::VideoMetadataInfo* Video::GetInfoFromMetadata(
                      VideoMetadataListManager::VideoMetadataPtr metadata)
{
    DTC::VideoMetadataInfo *pVideoMetadataInfo = new DTC::VideoMetadataInfo();

    if (metadata)
    {
        pVideoMetadataInfo->setId(metadata->GetID());
        pVideoMetadataInfo->setTitle(metadata->GetTitle());
        pVideoMetadataInfo->setSubTitle(metadata->GetSubtitle());
        pVideoMetadataInfo->setTagline(metadata->GetTagline());
        pVideoMetadataInfo->setDirector(metadata->GetDirector());
        pVideoMetadataInfo->setStudio(metadata->GetStudio());
        pVideoMetadataInfo->setDescription(metadata->GetPlot());
        pVideoMetadataInfo->setCertification(metadata->GetRating());
        pVideoMetadataInfo->setInetRef(metadata->GetInetRef());
        pVideoMetadataInfo->setHomePage(metadata->GetHomepage());
        pVideoMetadataInfo->setReleaseDate(QDateTime(metadata->GetReleaseDate()));
        pVideoMetadataInfo->setAddDate(QDateTime(metadata->GetInsertdate()));
        pVideoMetadataInfo->setUserRating(metadata->GetUserRating());
        pVideoMetadataInfo->setLength(metadata->GetLength());
        pVideoMetadataInfo->setSeason(metadata->GetSeason());
        pVideoMetadataInfo->setEpisode(metadata->GetEpisode());
        pVideoMetadataInfo->setParentalLevel(metadata->GetShowLevel());
        pVideoMetadataInfo->setVisible(metadata->GetBrowse());
        pVideoMetadataInfo->setWatched(metadata->GetWatched());
        pVideoMetadataInfo->setProcessed(metadata->GetProcessed());
        pVideoMetadataInfo->setFileName(metadata->GetFilename());
        pVideoMetadataInfo->setHash(metadata->GetHash());
        pVideoMetadataInfo->setHost(metadata->GetHost());
        pVideoMetadataInfo->setCoverart(metadata->GetCoverFile());
        pVideoMetadataInfo->setFanart(metadata->GetFanart());
        pVideoMetadataInfo->setBanner(metadata->GetBanner());
        pVideoMetadataInfo->setScreenshot(metadata->GetScreenshot());
        pVideoMetadataInfo->setTrailer(metadata->GetTrailer());
    }

    return pVideoMetadataInfo;
}

DTC::BlurayInfo* Video::GetBluray( const QString &sPath )
{
    QString path = sPath;

    if (sPath.isEmpty())
        path = gCoreContext->GetSetting( "BluRayMountpoint", "/media/cdrom");

    VERBOSE(VB_GENERAL, QString("Parsing Blu-ray at path: %1 ").arg(path));

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
