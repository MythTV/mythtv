//////////////////////////////////////////////////////////////////////////////
// Program Name: content.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

// C++
#include <cmath>

// Qt
#include <QDir>
#include <QImage>
#include <QImageWriter>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/storagegroup.h"
#include "libmythmetadata/musicmetadata.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythprotoserver/requesthandler/fileserverutil.h"
#include "libmythtv/metadataimagehelper.h"
#include "libmythtv/previewgenerator.h"

// MythBackend
#include "v2content.h"
#include "v2serviceUtil.h"

// Qt6 has made the QFileInfo::QFileInfo(QString) constructor
// explicit, which means that it is no longer possible to use an
// initializer list to construct a QFileInfo. Disable that clang-tidy
// check for this file so it can still be run on the rest of the file
// in the project.
//
// NOLINTBEGIN(modernize-return-braced-init-list)

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (CONTENT_HANDLE, V2Content::staticMetaObject, &V2Content::RegisterCustomTypes))

void V2Content::RegisterCustomTypes()
{
    qRegisterMetaType< QFileInfo >();
    qRegisterMetaType<V2ArtworkInfoList*>("V2ArtworkInfoList");
    qRegisterMetaType<V2ArtworkInfo*>("V2ArtworkInfo");
    // qRegisterMetaType<V2LiveStreamInfo*>("V2LiveStreamInfo");
    // qRegisterMetaType<V2LiveStreamInfoList*>("V2LiveStreamInfoList");
}

V2Content::V2Content() : MythHTTPService(s_service) {}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetFile( const QString &sStorageGroup,
                            const QString &sFileName )
{
    QString sGroup = sStorageGroup;

    if (sGroup.isEmpty())
    {
        LOG(VB_UPNP, LOG_WARNING,
            "GetFile - StorageGroup missing... using 'Default'");
        sGroup = "Default";
    }

    if (sFileName.isEmpty())
    {
        QString sMsg ( "GetFile - FileName missing." );

        //LOG(VB_UPNP, LOG_ERR, sMsg);

        throw QString(sMsg);
    }

    // ------------------------------------------------------------------
    // Search for the filename
    // ------------------------------------------------------------------

    StorageGroup storage( sGroup );
    QString sFullFileName = storage.FindFile( sFileName );

    if (sFullFileName.isEmpty())
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("GetFile - Unable to find %1.").arg(sFileName));

        return {};
    }

    // ----------------------------------------------------------------------
    // check to see if the file (still) exists
    // ----------------------------------------------------------------------

    if (QFile::exists( sFullFileName ))
    {
        return QFileInfo( sFullFileName );
    }

    LOG(VB_UPNP, LOG_ERR,
        QString("GetFile - File Does not exist %1.").arg(sFullFileName));

    return {};
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetImageFile( const QString &sStorageGroup,
                                 const QString &sFileName,
                                 int nWidth,
                                 int nHeight)
{
    QString sGroup = sStorageGroup;

    if (sGroup.isEmpty())
    {
        LOG(VB_UPNP, LOG_WARNING,
            "GetImageFile - StorageGroup missing... using 'Default'");
        sGroup = "Default";
    }

    if (sFileName.isEmpty())
    {
        QString sMsg ( "GetImageFile - FileName missing." );

        //LOG(VB_UPNP, LOG_WARNING, sMsg);

        throw QString(sMsg);
    }

    // ------------------------------------------------------------------
    // Search for the filename
    // ------------------------------------------------------------------

    StorageGroup storage( sGroup );
    QString sFullFileName = storage.FindFile( sFileName );

    if (sFullFileName.isEmpty())
    {
        LOG(VB_UPNP, LOG_WARNING,
            QString("GetImageFile - Unable to find %1.").arg(sFileName));

        return {};
    }

    // ----------------------------------------------------------------------
    // check to see if the file (still) exists
    // ----------------------------------------------------------------------

    if (!QFile::exists( sFullFileName ))
    {
        LOG(VB_UPNP, LOG_WARNING,
            QString("GetImageFile - File Does not exist %1.").arg(sFullFileName));
        return {};
    }

    // ----------------------------------------------------------------------
    // If no scaling is required return the file info
    // ----------------------------------------------------------------------
    if ((nWidth == 0) && (nHeight == 0))
        return QFileInfo( sFullFileName );

    // ----------------------------------------------------------------------
    // Create a filename for the scaled copy
    // ----------------------------------------------------------------------
    QString sNewFileName = QString( "%1.%2x%3.jpg" )
                              .arg( sFullFileName )
                              .arg( nWidth    )
                              .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sNewFileName ))
        return QFileInfo( sNewFileName );

    // ----------------------------------------------------------------------
    // Must generate Generate Image and save.
    // ----------------------------------------------------------------------

    auto *pImage = new QImage( sFullFileName );

    if (!pImage || pImage->isNull())
        return {};

    float fAspect = (float)(pImage->width()) / pImage->height();

    if ( nWidth == 0 )
        nWidth = (int)std::rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)std::rint(nWidth / fAspect);

    QImage img = pImage->scaled( nWidth, nHeight, Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);

    QByteArray fname = sNewFileName.toLatin1();
    img.save( fname.constData(), "JPG", 60 );

    delete pImage;

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Content::GetDirList( const QString &sStorageGroup )
{

    if (sStorageGroup.isEmpty())
    {
        QString sMsg( "GetDirList - StorageGroup missing.");
        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw QString(sMsg);
    }

    StorageGroup sgroup(sStorageGroup);

    return sgroup.GetDirList("", true);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList V2Content::GetFileList( const QString &sStorageGroup )
{

    if (sStorageGroup.isEmpty())
    {
        QString sMsg( "GetFileList - StorageGroup missing.");
        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw QString(sMsg);
    }

    StorageGroup sgroup(sStorageGroup);

    return sgroup.GetFileList("", true);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetRecordingArtwork ( const QString   &sType,
                                         const QString   &sInetref,
                                         int nSeason,
                                         int nWidth,
                                         int nHeight)
{
    ArtworkMap map = GetArtwork(sInetref, nSeason);

    if (map.isEmpty())
        return {};

    VideoArtworkType type = kArtworkCoverart;
    QString sgroup;

    if (sType.toLower() == "coverart")
    {
        sgroup = "Coverart";
        type = kArtworkCoverart;
    }
    else if (sType.toLower() == "fanart")
    {
        sgroup = "Fanart";
        type = kArtworkFanart;
    }
    else if (sType.toLower() == "banner")
    {
        sgroup = "Banners";
        type = kArtworkBanner;
    }

    if (!map.contains(type))
        return {};

    QUrl url(map.value(type).url);
    QString sFileName = url.path();

    if (sFileName.isEmpty())
        return {};

    return GetImageFile( sgroup, sFileName, nWidth, nHeight);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

V2ArtworkInfoList* V2Content::GetRecordingArtworkList( int        RecordedId,
                                                        int        chanid,
                                                        const QDateTime &StartTime)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pginfo;
    if (RecordedId > 0)
        pginfo = ProgramInfo(RecordedId);
    else
        pginfo = ProgramInfo(chanid, StartTime.toUTC());

    return GetProgramArtworkList(pginfo.GetInetRef(), pginfo.GetSeason());
}

V2ArtworkInfoList* V2Content::GetProgramArtworkList( const QString &sInetref,
                                                      int            nSeason  )
{
    auto *pArtwork = new V2ArtworkInfoList();

    V2FillArtworkInfoList (pArtwork, sInetref, nSeason);

    return pArtwork;
}
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// NOTE: If you rename this, you must also update upnpcdsvideo.cpp
QFileInfo V2Content::GetVideoArtwork( const QString &sType,
                                    int nId, int nWidth, int nHeight )
{
    LOG(VB_UPNP, LOG_INFO, QString("GetVideoArtwork ID = %1").arg(nId));

    QString sgroup = "Coverart";
    QString column = "coverfile";

    if (sType.toLower() == "coverart")
    {
        sgroup = "Coverart";
        column = "coverfile";
    }
    else if (sType.toLower() == "fanart")
    {
        sgroup = "Fanart";
        column = "fanart";
    }
    else if (sType.toLower() == "banner")
    {
        sgroup = "Banners";
        column = "banner";
    }
    else if (sType.toLower() == "screenshot")
    {
        sgroup = "Screenshots";
        column = "screenshot";
    }

    // ----------------------------------------------------------------------
    // Read Video artwork file path from database
    // ----------------------------------------------------------------------

    MSqlQuery query(MSqlQuery::InitCon());

    QString querystr = QString("SELECT %1 FROM videometadata WHERE "
                               "intid = :ITEMID").arg(column);

    query.prepare(querystr);
    query.bindValue(":ITEMID", nId);

    if (!query.exec())
        MythDB::DBError("GetVideoArtwork ", query);

    if (!query.next())
        return {};

    QString sFileName = query.value(0).toString();

    if (sFileName.isEmpty())
        return {};

    return GetImageFile( sgroup, sFileName, nWidth, nHeight );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetAlbumArt( int nTrackId, int nWidth, int nHeight )
{
    // ----------------------------------------------------------------------
    // Read AlbumArt file path from database
    // ----------------------------------------------------------------------

    MusicMetadata *metadata = MusicMetadata::createFromID(nTrackId);

    if (!metadata)
        return {};

    QString sFullFileName = metadata->getAlbumArtFile();
    LOG(VB_GENERAL, LOG_DEBUG, QString("GetAlbumArt: %1").arg(sFullFileName));

    delete metadata;

    if (!RemoteFile::Exists(sFullFileName))
        return {};

    QString sNewFileName = QString( "/tmp/%1.%2x%3.jpg" )
                              .arg( QFileInfo(sFullFileName).fileName() )
                              .arg( nWidth    )
                              .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if albumart image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sNewFileName ))
        return QFileInfo( sNewFileName );

    // ----------------------------------------------------------------------
    // Must generate Albumart Image, Generate Image and save.
    // ----------------------------------------------------------------------


    QImage img;
    if (sFullFileName.startsWith("myth://"))
    {
        RemoteFile rf(sFullFileName, false, false, 0s);
        QByteArray data;
        rf.SaveAs(data);

        img.loadFromData(data);
    }
    else
    {
        img.load(sFullFileName);
    }

    if (img.isNull())
        return {};

    // We don't need to scale if no height and width were specified
    // but still need to save as jpg if it's in another format
    if ((nWidth == 0) && (nHeight == 0))
    {
        if (!sFullFileName.startsWith("myth://"))
        {
            QFileInfo fi(sFullFileName);
            if (fi.suffix().toLower() == "jpg")
                return fi;
        }
    }
    else if (nWidth > img.width() && nHeight > img.height())
    {
        // Requested dimensions are larger than the source image, so instead of
        // scaling up which will produce horrible results return the fullsize
        // image and the user can scale further if they want instead
        // NOTE: If this behaviour is changed, for example making it optional,
        //       then upnp code will need changing to compensate
    }
    else
    {
        float fAspect = (float)(img.width()) / img.height();

        if ( nWidth == 0 || nWidth > img.width() )
            nWidth = (int)std::rint(nHeight * fAspect);

        if ( nHeight == 0 || nHeight > img.height() )
            nHeight = (int)std::rint(nWidth / fAspect);

        img = img.scaled( nWidth, nHeight, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
    }

    QString fname = sNewFileName.toLatin1().constData();
    // Use JPG not PNG for compatibility with the most uPnP devices and
    // faster loading (smaller file to send over network)
    if (!img.save( fname, "JPG" ))
        return {};

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetPreviewImage(        int        nRecordedId,
                                           int        nChanId,
                                     const QDateTime &StartTime,
                                           int        nWidth,
                                           int        nHeight,
                                           int        nSecsIn,
                                     const QString   &sFormat )
{
    if ((nRecordedId <= 0) &&
        (nChanId <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    if (!sFormat.isEmpty()
        && !QImageWriter::supportedImageFormats().contains(sFormat.toLower().toLocal8Bit()))
    {
        throw QString("GetPreviewImage: Specified 'Format' is not supported.");
    }

    // ----------------------------------------------------------------------
    // Read Recording From Database
    // ----------------------------------------------------------------------

    // TODO Should use RecordingInfo
    ProgramInfo pginfo;
    if (nRecordedId > 0)
        pginfo = ProgramInfo(nRecordedId);
    else
        pginfo = ProgramInfo(nChanId, StartTime.toUTC());

    if (!pginfo.GetChanID())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetPreviewImage: No recording for '%1'")
            .arg(nRecordedId));
        return {};
    }

    if (pginfo.GetHostname().toLower() != gCoreContext->GetHostName().toLower()
            &&  ! gCoreContext->GetBoolSetting("MasterBackendOverride", false))
    {
        QString sMsg =
            QString("GetPreviewImage: Wrong Host '%1' request from '%2'")
                          .arg( gCoreContext->GetHostName(),
                                pginfo.GetHostname() );

        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw V2HttpRedirectException( pginfo.GetHostname() );
    }

    QString sImageFormat = sFormat;
    if (sImageFormat.isEmpty())
        sImageFormat = "PNG";

    QString sFileName = GetPlaybackURL(&pginfo);

    // ----------------------------------------------------------------------
    // check to see if default preview image is already created.
    // ----------------------------------------------------------------------

    QString sPreviewFileName;

    auto nSecs = std::chrono::seconds(nSecsIn);
    if (nSecs <= 0s)
    {
        nSecs = -1s;
        sPreviewFileName = QString("%1.png").arg(sFileName);
    }
    else
    {
        sPreviewFileName = QString("%1.%2.png").arg(sFileName).arg(nSecsIn);
    }

    if (!QFile::exists( sPreviewFileName ))
    {
        // ------------------------------------------------------------------
        // Must generate Preview Image, Generate Image and save.
        // ------------------------------------------------------------------
        if (!pginfo.IsLocal() && sFileName.startsWith("/"))
            pginfo.SetPathname(sFileName);

        if (!pginfo.IsLocal())
            return {};

        auto *previewgen = new PreviewGenerator( &pginfo, QString(),
                                                 PreviewGenerator::kLocal);
        previewgen->SetPreviewTimeAsSeconds( nSecs            );
        previewgen->SetOutputFilename      ( sPreviewFileName );

        bool ok = previewgen->Run();

        previewgen->deleteLater();

        if (!ok)
            return {};
    }

    bool bDefaultPixmap = (nWidth == 0) && (nHeight == 0);

    QString sNewFileName;

    if (bDefaultPixmap)
        sNewFileName = sPreviewFileName;
    else
    {
        sNewFileName = QString( "%1.%2.%3x%4.%5" )
                          .arg( sFileName )
                          .arg( nSecsIn   )
                          .arg( nWidth == 0 ? -1 : nWidth )
                          .arg( nHeight == 0 ? -1 : nHeight )
                          .arg( sImageFormat.toLower() );

        // ----------------------------------------------------------------------
        // check to see if scaled preview image is already created and isn't
        // out of date
        // ----------------------------------------------------------------------
        if (QFile::exists( sNewFileName ))
        {
            if (QFileInfo(sPreviewFileName).lastModified() <=
                QFileInfo(sNewFileName).lastModified())
                return QFileInfo( sNewFileName );
        }

        QImage image = QImage(sPreviewFileName);

        if (image.isNull())
            return {};

        // We can just re-scale the default (full-size version) to avoid
        // a preview generator run
        if ( nWidth <= 0 )
            image = image.scaledToHeight(nHeight, Qt::SmoothTransformation);
        else if ( nHeight <= 0 )
            image = image.scaledToWidth(nWidth, Qt::SmoothTransformation);
        else
            image = image.scaled(nWidth, nHeight, Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation);

        image.save(sNewFileName, sImageFormat.toUpper().toLocal8Bit());

        // Let anybody update it
        bool ret = makeFileAccessible(sNewFileName.toLocal8Bit().constData());
        if (!ret)
        {
            LOG(VB_GENERAL, LOG_ERR, "Unable to change permissions on "
                                     "preview image. Backends and frontends "
                                     "running under different users will be "
                                     "unable to access it");
        }
    }

    if (QFile::exists( sNewFileName ))
        return QFileInfo( sNewFileName );

    auto *previewgen = new PreviewGenerator( &pginfo, QString(),
                                             PreviewGenerator::kLocal);
    previewgen->SetPreviewTimeAsSeconds( nSecs               );
    previewgen->SetOutputFilename      ( sNewFileName        );
    previewgen->SetOutputSize          (QSize(nWidth,nHeight));

    bool ok = previewgen->Run();

    previewgen->deleteLater();

    if (!ok)
        return {};

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetRecording( int              nRecordedId,
                                 int              nChanId,
                                 const QDateTime &StartTime,
                                 const QString   &Download )
{
    if ((nRecordedId <= 0) &&
        (nChanId <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // ------------------------------------------------------------------
    // Read Recording From Database
    // ------------------------------------------------------------------

    // TODO Should use RecordingInfo
    ProgramInfo pginfo;
    if (nRecordedId > 0)
        pginfo = ProgramInfo(nRecordedId);
    else
        pginfo = ProgramInfo(nChanId, StartTime.toUTC());

    if (!pginfo.GetChanID())
    {
        LOG(VB_UPNP, LOG_ERR, QString("GetRecording - for '%1' failed")
            .arg(nRecordedId));

        return {};
    }

    if (pginfo.GetHostname().toLower() != gCoreContext->GetHostName().toLower()
            &&  ! gCoreContext->GetBoolSetting("MasterBackendOverride", false))
    {
        // We only handle requests for local resources

        QString sMsg =
            QString("GetRecording: Wrong Host '%1' request from '%2'.")
                          .arg( gCoreContext->GetHostName(),
                                pginfo.GetHostname() );

        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw V2HttpRedirectException( pginfo.GetHostname() );
    }

    QString sFileName( GetPlaybackURL(&pginfo) );

    if (HAS_PARAMv2("Download"))
        m_request->m_headers->insert("mythtv-download",Download);

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (QFile::exists( sFileName ))
        return QFileInfo( sFileName );

    return {};
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetMusic( int nId )
{
    QString sFileName;

    // ----------------------------------------------------------------------
    // Load Track's FileName
    // ----------------------------------------------------------------------

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        query.prepare("SELECT CONCAT_WS('/', music_directories.path, "
                       "music_songs.filename) AS filename FROM music_songs "
                       "LEFT JOIN music_directories ON "
                         "music_songs.directory_id="
                         "music_directories.directory_id "
                       "WHERE music_songs.song_id = :KEY");

        query.bindValue(":KEY", nId );

        if (!query.exec())
        {
            MythDB::DBError("GetMusic()", query);
            return {};
        }

        if (query.next())
        {
            sFileName = query.value(0).toString();
        }
    }

    if (sFileName.isEmpty())
        return {};

    return GetFile( "Music", sFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo V2Content::GetVideo( int nId )
{
    QString sFileName;

    // ----------------------------------------------------------------------
    // Load Track's FileName
    // ----------------------------------------------------------------------

    MSqlQuery query(MSqlQuery::InitCon());

    if (query.isConnected())
    {
        query.prepare("SELECT filename FROM videometadata WHERE intid = :KEY" );
        query.bindValue(":KEY", nId );

        if (!query.exec())
        {
            MythDB::DBError("GetVideo()", query);
            return {};
        }

        if (query.next())
            sFileName = query.value(0).toString();
    }

    if (sFileName.isEmpty())
        return {};

    if (!QFile::exists( sFileName ))
        return GetFile( "Videos", sFileName );

    return QFileInfo( sFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString V2Content::GetHash( const QString &sStorageGroup,
                          const QString &sFileName )
{
    if ((sFileName.isEmpty()) ||
        (sFileName.contains("/../")) ||
        (sFileName.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ERROR checking for file, filename '%1' "
                    "fails sanity checks").arg(sFileName));
        return {};
    }

    QString storageGroup = "Default";

    if (!sStorageGroup.isEmpty())
        storageGroup = sStorageGroup;

    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());

    QString fullname = sgroup.FindFile(sFileName);
    QString hash = FileHash(fullname);

    if (hash == "NULL")
        return {};

    return hash;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool V2Content::DownloadFile( const QString &sURL, const QString &sStorageGroup )
{
    QFileInfo finfo(sURL);
    QString filename = finfo.fileName();
    StorageGroup sgroup(sStorageGroup, gCoreContext->GetHostName(), false);
    QString outDir = sgroup.FindNextDirMostFree();
    QString outFile;

    if (outDir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Unable to determine directory "
                    "to write to in %1 write command").arg(sURL));
        return false;
    }

    if ((filename.contains("/../")) ||
        (filename.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ERROR: %1 write filename '%2' does not "
                    "pass sanity checks.").arg(sURL, filename));
        return false;
    }

    outFile = outDir + "/" + filename;

    return GetMythDownloadManager()->download(sURL, outFile);
}


// V2LiveStreamInfo *V2Content::AddLiveStream( const QString   &sStorageGroup,
//                                              const QString   &sFileName,
//                                              const QString   &sHostName,
//                                              int              nMaxSegments,
//                                              int              nWidth,
//                                              int              nHeight,
//                                              int              nBitrate,
//                                              int              nAudioBitrate,
//                                              int              nSampleRate )
// {
//     QString sGroup = sStorageGroup;

//     if (sGroup.isEmpty())
//     {
//         LOG(VB_UPNP, LOG_WARNING,
//             "AddLiveStream - StorageGroup missing... using 'Default'");
//         sGroup = "Default";
//     }

//     if (sFileName.isEmpty())
//     {
//         QString sMsg ( "AddLiveStream - FileName missing." );

//         LOG(VB_UPNP, LOG_ERR, sMsg);

//         throw QString(sMsg);
//     }

//     // ------------------------------------------------------------------
//     // Search for the filename
//     // ------------------------------------------------------------------

//     QString sFullFileName;
//     if (sHostName.isEmpty() || sHostName == gCoreContext->GetHostName())
//     {
//         StorageGroup storage( sGroup );
//         sFullFileName = storage.FindFile( sFileName );

//         if (sFullFileName.isEmpty())
//         {
//             LOG(VB_UPNP, LOG_ERR,
//                 QString("AddLiveStream - Unable to find %1.").arg(sFileName));

//             return nullptr;
//         }
//     }
//     else
//     {
//         sFullFileName =
//             MythCoreContext::GenMythURL(sHostName, 0, sFileName, sStorageGroup);
//     }

//     auto *hls = new HTTPLiveStream(sFullFileName, nWidth, nHeight, nBitrate,
//                                nAudioBitrate, nMaxSegments, 0, 0, nSampleRate);

//     if (!hls)
//     {
//         LOG(VB_UPNP, LOG_ERR,
//             "AddLiveStream - Unable to create HTTPLiveStream.");
//         return nullptr;
//     }

//     V2LiveStreamInfo *lsInfo = hls->StartStream();

//     delete hls;

//     return lsInfo;
// }

// /////////////////////////////////////////////////////////////////////////////
// //
// /////////////////////////////////////////////////////////////////////////////

// bool V2Content::RemoveLiveStream( int nId )
// {
//     return HTTPLiveStream::RemoveStream(nId);
// }

// /////////////////////////////////////////////////////////////////////////////
// //
// /////////////////////////////////////////////////////////////////////////////

// V2LiveStreamInfo *V2Content::StopLiveStream( int nId )
// {
//     return HTTPLiveStream::StopStream(nId);
// }

// /////////////////////////////////////////////////////////////////////////////
// //
// /////////////////////////////////////////////////////////////////////////////

// V2LiveStreamInfo *V2Content::GetLiveStream( int nId )
// {
//     auto *hls = new HTTPLiveStream(nId);

//     if (!hls)
//     {
//         LOG( VB_UPNP, LOG_ERR,
//              QString("GetLiveStream - for stream id %1 failed").arg( nId ));
//         return nullptr;
//     }

//     V2LiveStreamInfo *hlsInfo = hls->GetLiveStreamInfo();
//     if (!hlsInfo)
//     {
//         LOG( VB_UPNP, LOG_ERR,
//              QString("HLS::GetLiveStreamInfo - for stream id %1 failed")
//                      .arg( nId ));
//         return nullptr;
//     }

//     delete hls;
//     return hlsInfo;
// }

// /////////////////////////////////////////////////////////////////////////////
// //
// /////////////////////////////////////////////////////////////////////////////

// V2LiveStreamInfoList *V2Content::GetLiveStreamList( const QString   &FileName )
// {
//     return HTTPLiveStream::GetLiveStreamInfoList(FileName);
// }

// /////////////////////////////////////////////////////////////////////////////
// //
// /////////////////////////////////////////////////////////////////////////////

// V2LiveStreamInfo *V2Content::AddRecordingLiveStream(
//     int              nRecordedId,
//     int              nChanId,
//     const QDateTime &StartTime,
//     int              nMaxSegments,
//     int              nWidth,
//     int              nHeight,
//     int              nBitrate,
//     int              nAudioBitrate,
//     int              nSampleRate )
// {
//     if ((nRecordedId <= 0) &&
//         (nChanId <= 0 || !StartTime.isValid()))
//         throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

//     // ------------------------------------------------------------------
//     // Read Recording From Database
//     // ------------------------------------------------------------------

//     // TODO Should use RecordingInfo
//     ProgramInfo pginfo;
//     if (nRecordedId > 0)
//         pginfo = ProgramInfo(nRecordedId);
//     else
//         pginfo = ProgramInfo(nChanId, StartTime.toUTC());

//     if (!pginfo.GetChanID())
//     {
//         LOG(VB_UPNP, LOG_ERR,
//             QString("AddRecordingLiveStream - for %1, %2 failed")
//             .arg(QString::number(nRecordedId)));
//         return nullptr;
//     }

//     bool masterBackendOverride = gCoreContext->GetBoolSetting("MasterBackendOverride", false);

//     if (pginfo.GetHostname().toLower() != gCoreContext->GetHostName().toLower()
//             &&  ! masterBackendOverride)
//     {
//         // We only handle requests for local resources

//         QString sMsg =
//             QString("GetRecording: Wrong Host '%1' request from '%2'.")
//                           .arg( gCoreContext->GetHostName(),
//                                 pginfo.GetHostname() );

//         LOG(VB_UPNP, LOG_ERR, sMsg);

//         throw V2HttpRedirectException( pginfo.GetHostname() );
//     }

//     QString sFileName( GetPlaybackURL(&pginfo) );

//     // ----------------------------------------------------------------------
//     // check to see if the file exists
//     // ----------------------------------------------------------------------

//     if (!QFile::exists( sFileName ))
//     {
//         LOG( VB_UPNP, LOG_ERR, QString("AddRecordingLiveStream - for %1, %2 failed")
//                                     .arg( nChanId )
//                                     .arg( StartTime.toUTC().toString() ));
//         return nullptr;
//     }

//     QFileInfo fInfo( sFileName );

//     QString hostName;
//     if (masterBackendOverride)
//         hostName = gCoreContext->GetHostName();
//     else
//         hostName = pginfo.GetHostname();

//     return AddLiveStream( pginfo.GetStorageGroup(), fInfo.fileName(),
//                           hostName, nMaxSegments, nWidth,
//                           nHeight, nBitrate, nAudioBitrate, nSampleRate );
// }

// /////////////////////////////////////////////////////////////////////////////
// //
// /////////////////////////////////////////////////////////////////////////////

// V2LiveStreamInfo *V2Content::AddVideoLiveStream( int nId,
//                                                   int nMaxSegments,
//                                                   int nWidth,
//                                                   int nHeight,
//                                                   int nBitrate,
//                                                   int nAudioBitrate,
//                                                   int nSampleRate )
// {
//     if (nId < 0)
//         throw QString( "Id is invalid" );

//     VideoMetadataListManager::VideoMetadataPtr metadata =
//                           VideoMetadataListManager::loadOneFromDatabase(nId);

//     if (!metadata)
//     {
//         LOG( VB_UPNP, LOG_ERR, QString("AddVideoLiveStream - no metadata for %1")
//                                     .arg( nId ));
//         return nullptr;
//     }

//     if ( metadata->GetHost().toLower() != gCoreContext->GetHostName().toLower())
//     {
//         // We only handle requests for local resources

//         QString sMsg =
//             QString("AddVideoLiveStream: Wrong Host '%1' request from '%2'.")
//                           .arg( gCoreContext->GetHostName(),
//                                 metadata->GetHost() );

//         LOG(VB_UPNP, LOG_ERR, sMsg);

//         throw V2HttpRedirectException( metadata->GetHost() );
//     }

//     StorageGroup sg("Videos", metadata->GetHost());
//     QString sFileName = sg.FindFile(metadata->GetFilename());

//     // ----------------------------------------------------------------------
//     // check to see if the file exists
//     // ----------------------------------------------------------------------

//     if (!QFile::exists( sFileName ))
//     {
//         LOG( VB_UPNP, LOG_ERR, QString("AddVideoLiveStream - file does not exist."));
//         return nullptr;
//     }

//     return AddLiveStream( "Videos", metadata->GetFilename(),
//                           metadata->GetHost(), nMaxSegments, nWidth,
//                           nHeight, nBitrate, nAudioBitrate, nSampleRate );
// }

// NOLINTEND(modernize-return-braced-init-list)
