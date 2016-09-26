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

#include "content.h"

#include <QDir>
#include <QImage>
#include <QImageWriter>

#include <math.h>
#include <compat.h>

#include "mythcorecontext.h"
#include "storagegroup.h"
#include "programinfo.h"
#include "previewgenerator.h"
#include "backendutil.h"
#include "httprequest.h"
#include "serviceUtil.h"
#include "mythdate.h"
#include "mythdownloadmanager.h"
#include "metadataimagehelper.h"
#include "musicmetadata.h"
#include "videometadatalistmanager.h"
#include "HLS/httplivestream.h"
#include "mythmiscutil.h"
#include "remotefile.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetFile( const QString &sStorageGroup,
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

        throw sMsg;
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

        return QFileInfo();
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

    return QFileInfo();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetImageFile( const QString &sStorageGroup,
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

        throw sMsg;
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

        return QFileInfo();
    }

    // ----------------------------------------------------------------------
    // check to see if the file (still) exists
    // ----------------------------------------------------------------------

    if (!QFile::exists( sFullFileName ))
    {
        LOG(VB_UPNP, LOG_WARNING,
            QString("GetImageFile - File Does not exist %1.").arg(sFullFileName));
        return QFileInfo();
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

    float fAspect = 0.0;

    QImage *pImage = new QImage( sFullFileName );

    if (!pImage || pImage->isNull())
        return QFileInfo();

    if (fAspect <= 0)
           fAspect = (float)(pImage->width()) / pImage->height();

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

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

QStringList Content::GetDirList( const QString &sStorageGroup )
{

    if (sStorageGroup.isEmpty())
    {
        QString sMsg( "GetDirList - StorageGroup missing.");
        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw sMsg;
    }

    StorageGroup sgroup(sStorageGroup);

    return sgroup.GetDirList("", true);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList Content::GetFileList( const QString &sStorageGroup )
{

    if (sStorageGroup.isEmpty())
    {
        QString sMsg( "GetFileList - StorageGroup missing.");
        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw sMsg;
    }

    StorageGroup sgroup(sStorageGroup);

    return sgroup.GetFileList("", true);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetRecordingArtwork ( const QString   &sType,
                                         const QString   &sInetref,
                                         int nSeason,
                                         int nWidth,
                                         int nHeight)
{
    ArtworkMap map = GetArtwork(sInetref, nSeason);

    if (map.isEmpty())
        return QFileInfo();

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
        return QFileInfo();

    QUrl url(map.value(type).url);
    QString sFileName = url.path();

    if (sFileName.isEmpty())
        return QFileInfo();

    return GetImageFile( sgroup, sFileName, nWidth, nHeight);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ArtworkInfoList* Content::GetRecordingArtworkList( int        RecordedId,
                                                        int        chanid,
                                                        const QDateTime &recstarttsRaw)
{
    if ((RecordedId <= 0) &&
        (chanid <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // TODO Should use RecordingInfo
    ProgramInfo pginfo;
    if (RecordedId > 0)
        pginfo = ProgramInfo(RecordedId);
    else
        pginfo = ProgramInfo(chanid, recstarttsRaw.toUTC());

    return GetProgramArtworkList(pginfo.GetInetRef(), pginfo.GetSeason());
}

DTC::ArtworkInfoList* Content::GetProgramArtworkList( const QString &sInetref,
                                                      int            nSeason  )
{
    DTC::ArtworkInfoList *pArtwork = new DTC::ArtworkInfoList();

    FillArtworkInfoList (pArtwork, sInetref, nSeason);

    return pArtwork;
}
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

// NOTE: If you rename this, you must also update upnpcdsvideo.cpp
QFileInfo Content::GetVideoArtwork( const QString &sType,
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
        return QFileInfo();

    QString sFileName = query.value(0).toString();

    if (sFileName.isEmpty())
        return QFileInfo();

    return GetImageFile( sgroup, sFileName, nWidth, nHeight );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetAlbumArt( int nTrackId, int nWidth, int nHeight )
{
    // ----------------------------------------------------------------------
    // Read AlbumArt file path from database
    // ----------------------------------------------------------------------

    MusicMetadata *metadata = MusicMetadata::createFromID(nTrackId);

    if (!metadata)
        return QFileInfo();

    QString sFullFileName = metadata->getAlbumArtFile();
    LOG(VB_GENERAL, LOG_DEBUG, QString("GetAlbumArt: %1").arg(sFullFileName));

    delete metadata;

    if (!RemoteFile::Exists(sFullFileName))
        return QFileInfo();

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
        RemoteFile rf(sFullFileName, false, false, 0);
        QByteArray data;
        rf.SaveAs(data);

        img.loadFromData(data);
    }
    else
        img.load(sFullFileName);

    if (img.isNull())
        return QFileInfo();

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
        float fAspect = 0.0;
        if (fAspect <= 0)
            fAspect = (float)(img.width()) / img.height();

        if ( nWidth == 0 || nWidth > img.width() )
            nWidth = (int)rint(nHeight * fAspect);

        if ( nHeight == 0 || nHeight > img.height() )
            nHeight = (int)rint(nWidth / fAspect);

        img = img.scaled( nWidth, nHeight, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
    }

    QString fname = sNewFileName.toLatin1().constData();
    // Use JPG not PNG for compatibility with the most uPnP devices and
    // faster loading (smaller file to send over network)
    if (!img.save( fname, "JPG" ))
        return QFileInfo();

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetPreviewImage(        int        nRecordedId,
                                           int        nChanId,
                                     const QDateTime &recstarttsRaw,
                                           int        nWidth,
                                           int        nHeight,
                                           int        nSecsIn,
                                     const QString   &sFormat )
{
    if ((nRecordedId <= 0) &&
        (nChanId <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    if (!sFormat.isEmpty()
        && !QImageWriter::supportedImageFormats().contains(sFormat.toLower().toLocal8Bit()))
    {
        throw "GetPreviewImage: Specified 'Format' is not supported.";
    }

    // ----------------------------------------------------------------------
    // Read Recording From Database
    // ----------------------------------------------------------------------

    // TODO Should use RecordingInfo
    ProgramInfo pginfo;
    if (nRecordedId > 0)
        pginfo = ProgramInfo(nRecordedId);
    else
        pginfo = ProgramInfo(nChanId, recstarttsRaw.toUTC());

    if (!pginfo.GetChanID())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetPreviewImage: No recording for '%1'")
            .arg(nRecordedId));
        return QFileInfo();
    }

    if (pginfo.GetHostname().toLower() != gCoreContext->GetHostName().toLower())
    {
        QString sMsg =
            QString("GetPreviewImage: Wrong Host '%1' request from '%2'")
                          .arg( gCoreContext->GetHostName())
                          .arg( pginfo.GetHostname() );

        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw HttpRedirectException( pginfo.GetHostname() );
    }

    QString sImageFormat = sFormat;
    if (sImageFormat.isEmpty())
        sImageFormat = "PNG";

    QString sFileName = GetPlaybackURL(&pginfo);

    // ----------------------------------------------------------------------
    // check to see if default preview image is already created.
    // ----------------------------------------------------------------------

    QString sPreviewFileName;

    if (nSecsIn <= 0)
    {
        nSecsIn = -1;
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
            return QFileInfo();

        PreviewGenerator *previewgen = new PreviewGenerator( &pginfo,
                                                             QString(),
                                                             PreviewGenerator::kLocal);
        previewgen->SetPreviewTimeAsSeconds( nSecsIn          );
        previewgen->SetOutputFilename      ( sPreviewFileName );

        bool ok = previewgen->Run();

        previewgen->deleteLater();

        if (!ok)
            return QFileInfo();
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
            return QFileInfo();

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

    PreviewGenerator *previewgen = new PreviewGenerator( &pginfo,
                                                         QString(),
                                                         PreviewGenerator::kLocal);
    previewgen->SetPreviewTimeAsSeconds( nSecsIn             );
    previewgen->SetOutputFilename      ( sNewFileName        );
    previewgen->SetOutputSize          (QSize(nWidth,nHeight));

    bool ok = previewgen->Run();

    previewgen->deleteLater();

    if (!ok)
        return QFileInfo();

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetRecording( int              nRecordedId,
                                 int              nChanId,
                                 const QDateTime &recstarttsRaw )
{
    if ((nRecordedId <= 0) &&
        (nChanId <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // ------------------------------------------------------------------
    // Read Recording From Database
    // ------------------------------------------------------------------

    // TODO Should use RecordingInfo
    ProgramInfo pginfo;
    if (nRecordedId > 0)
        pginfo = ProgramInfo(nRecordedId);
    else
        pginfo = ProgramInfo(nChanId, recstarttsRaw.toUTC());

    if (!pginfo.GetChanID())
    {
        LOG(VB_UPNP, LOG_ERR, QString("GetRecording - for '%1' failed")
            .arg(nRecordedId));

        return QFileInfo();
    }

    if (pginfo.GetHostname().toLower() != gCoreContext->GetHostName().toLower())
    {
        // We only handle requests for local resources

        QString sMsg =
            QString("GetRecording: Wrong Host '%1' request from '%2'.")
                          .arg( gCoreContext->GetHostName())
                          .arg( pginfo.GetHostname() );

        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw HttpRedirectException( pginfo.GetHostname() );
    }

    QString sFileName( GetPlaybackURL(&pginfo) );

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (QFile::exists( sFileName ))
        return QFileInfo( sFileName );

    return QFileInfo();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetMusic( int nId )
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
            return QFileInfo();
        }

        if (query.next())
        {
            sFileName = query.value(0).toString();
        }
    }

    if (sFileName.isEmpty())
        return QFileInfo();

    return GetFile( "Music", sFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetVideo( int nId )
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
            return QFileInfo();
        }

        if (query.next())
            sFileName = query.value(0).toString();
    }

    if (sFileName.isEmpty())
        return QFileInfo();

    if (!QFile::exists( sFileName ))
        return GetFile( "Videos", sFileName );

    return QFileInfo( sFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString Content::GetHash( const QString &sStorageGroup,
                          const QString &sFileName )
{
    if ((sFileName.isEmpty()) ||
        (sFileName.contains("/../")) ||
        (sFileName.startsWith("../")))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("ERROR checking for file, filename '%1' "
                    "fails sanity checks").arg(sFileName));
        return QString();
    }

    QString storageGroup = "Default";

    if (!sStorageGroup.isEmpty())
        storageGroup = sStorageGroup;

    StorageGroup sgroup(storageGroup, gCoreContext->GetHostName());

    QString fullname = sgroup.FindFile(sFileName);
    QString hash = FileHash(fullname);

    if (hash == "NULL")
        return QString();

    return hash;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Content::DownloadFile( const QString &sURL, const QString &sStorageGroup )
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
                    "pass sanity checks.") .arg(sURL).arg(filename));
        return false;
    }

    outFile = outDir + "/" + filename;

    if (GetMythDownloadManager()->download(sURL, outFile))
        return true;

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::LiveStreamInfo *Content::AddLiveStream( const QString   &sStorageGroup,
                                             const QString   &sFileName,
                                             const QString   &sHostName,
                                             int              nMaxSegments,
                                             int              nWidth,
                                             int              nHeight,
                                             int              nBitrate,
                                             int              nAudioBitrate,
                                             int              nSampleRate )
{
    QString sGroup = sStorageGroup;

    if (sGroup.isEmpty())
    {
        LOG(VB_UPNP, LOG_WARNING,
            "AddLiveStream - StorageGroup missing... using 'Default'");
        sGroup = "Default";
    }

    if (sFileName.isEmpty())
    {
        QString sMsg ( "AddLiveStream - FileName missing." );

        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw sMsg;
    }

    // ------------------------------------------------------------------
    // Search for the filename
    // ------------------------------------------------------------------

    QString sFullFileName;
    if (sHostName.isEmpty() || sHostName == gCoreContext->GetHostName())
    {
        StorageGroup storage( sGroup );
        sFullFileName = storage.FindFile( sFileName );

        if (sFullFileName.isEmpty())
        {
            LOG(VB_UPNP, LOG_ERR,
                QString("AddLiveStream - Unable to find %1.").arg(sFileName));

            return NULL;
        }
    }
    else
    {
        sFullFileName =
            gCoreContext->GenMythURL(sHostName, 0, sFileName, sStorageGroup);
    }

    HTTPLiveStream *hls = new
        HTTPLiveStream(sFullFileName, nWidth, nHeight, nBitrate, nAudioBitrate,
                       nMaxSegments, 0, 0, nSampleRate);

    if (!hls)
    {
        LOG(VB_UPNP, LOG_ERR,
            "AddLiveStream - Unable to create HTTPLiveStream.");
        return NULL;
    }

    DTC::LiveStreamInfo *lsInfo = hls->StartStream();

    delete hls;

    return lsInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool Content::RemoveLiveStream( int nId )
{
    return HTTPLiveStream::RemoveStream(nId);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::LiveStreamInfo *Content::StopLiveStream( int nId )
{
    return HTTPLiveStream::StopStream(nId);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::LiveStreamInfo *Content::GetLiveStream( int nId )
{
    HTTPLiveStream *hls = new HTTPLiveStream(nId);

    if (!hls)
    {
        LOG( VB_UPNP, LOG_ERR,
             QString("GetLiveStream - for stream id %1 failed").arg( nId ));
        return NULL;
    }

    DTC::LiveStreamInfo *hlsInfo = hls->GetLiveStreamInfo();
    if (!hlsInfo)
    {
        LOG( VB_UPNP, LOG_ERR,
             QString("HLS::GetLiveStreamInfo - for stream id %1 failed")
                     .arg( nId ));
        return NULL;
    }

    delete hls;
    return hlsInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::LiveStreamInfoList *Content::GetLiveStreamList( const QString   &FileName )
{
    return HTTPLiveStream::GetLiveStreamInfoList(FileName);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::LiveStreamInfo *Content::AddRecordingLiveStream(
    int              nRecordedId,
    int              nChanId,
    const QDateTime &recstarttsRaw,
    int              nMaxSegments,
    int              nWidth,
    int              nHeight,
    int              nBitrate,
    int              nAudioBitrate,
    int              nSampleRate )
{
    if ((nRecordedId <= 0) &&
        (nChanId <= 0 || !recstarttsRaw.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    // ------------------------------------------------------------------
    // Read Recording From Database
    // ------------------------------------------------------------------

    // TODO Should use RecordingInfo
    ProgramInfo pginfo;
    if (nRecordedId > 0)
        pginfo = ProgramInfo(nRecordedId);
    else
        pginfo = ProgramInfo(nChanId, recstarttsRaw.toUTC());

    if (!pginfo.GetChanID())
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("AddRecordingLiveStream - for %1, %2 failed")
            .arg(QString::number(nRecordedId)));
        return NULL;
    }

    if (pginfo.GetHostname().toLower() != gCoreContext->GetHostName().toLower())
    {
        // We only handle requests for local resources

        QString sMsg =
            QString("GetRecording: Wrong Host '%1' request from '%2'.")
                          .arg( gCoreContext->GetHostName())
                          .arg( pginfo.GetHostname() );

        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw HttpRedirectException( pginfo.GetHostname() );
    }

    QString sFileName( GetPlaybackURL(&pginfo) );

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (!QFile::exists( sFileName ))
    {
        LOG( VB_UPNP, LOG_ERR, QString("AddRecordingLiveStream - for %1, %2 failed")
                                    .arg( nChanId )
                                    .arg( recstarttsRaw.toUTC().toString() ));
        return NULL;
    }

    QFileInfo fInfo( sFileName );

    return AddLiveStream( pginfo.GetStorageGroup(), fInfo.fileName(),
                          pginfo.GetHostname(), nMaxSegments, nWidth,
                          nHeight, nBitrate, nAudioBitrate, nSampleRate );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::LiveStreamInfo *Content::AddVideoLiveStream( int nId,
                                                  int nMaxSegments,
                                                  int nWidth,
                                                  int nHeight,
                                                  int nBitrate,
                                                  int nAudioBitrate,
                                                  int nSampleRate )
{
    if (nId < 0)
        throw( "Id is invalid" );

    VideoMetadataListManager::VideoMetadataPtr metadata =
                          VideoMetadataListManager::loadOneFromDatabase(nId);

    if (!metadata)
    {
        LOG( VB_UPNP, LOG_ERR, QString("AddVideoLiveStream - no metadata for %1")
                                    .arg( nId ));
        return NULL;
    }

    if ( metadata->GetHost().toLower() != gCoreContext->GetHostName().toLower())
    {
        // We only handle requests for local resources

        QString sMsg =
            QString("AddVideoLiveStream: Wrong Host '%1' request from '%2'.")
                          .arg( gCoreContext->GetHostName())
                          .arg( metadata->GetHost() );

        LOG(VB_UPNP, LOG_ERR, sMsg);

        throw HttpRedirectException( metadata->GetHost() );
    }

    StorageGroup sg("Videos", metadata->GetHost());
    QString sFileName = sg.FindFile(metadata->GetFilename());

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (!QFile::exists( sFileName ))
    {
        LOG( VB_UPNP, LOG_ERR, QString("AddVideoLiveStream - file does not exist."));
        return NULL;
    }

    return AddLiveStream( "Videos", metadata->GetFilename(),
                          metadata->GetHost(), nMaxSegments, nWidth,
                          nHeight, nBitrate, nAudioBitrate, nSampleRate );
}
