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

        LOG(VB_UPNP, LOG_ERR, sMsg);

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
            "GetFile - StorageGroup missing... using 'Default'");
        sGroup = "Default";
    }

    if (sFileName.isEmpty())
    {
        QString sMsg ( "GetFile - FileName missing." );

        LOG(VB_UPNP, LOG_ERR, sMsg);

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
            QString("GetImageFile - Unable to find %1.").arg(sFileName));

        return QFileInfo();
    }

    // ----------------------------------------------------------------------
    // check to see if the file (still) exists
    // ----------------------------------------------------------------------

    if ((nWidth == 0) && (nHeight == 0))
    {
        if (QFile::exists( sFullFileName ))
        {
            return QFileInfo( sFullFileName );
        }

        LOG(VB_UPNP, LOG_ERR,
            QString("GetImageFile - File Does not exist %1.").arg(sFullFileName));

        return QFileInfo();
    }

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

    QImage *pImage = new QImage( sFullFileName);

    if (!pImage)
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

    QUrl url(map.value(type).url);
    QString sFileName = url.path();

    return GetImageFile( sgroup, sFileName, nWidth, nHeight);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ArtworkInfoList* Content::GetRecordingArtworkList(
    int chanid, const QDateTime &recstarttsRaw)
{
    if (chanid <= 0 || !recstarttsRaw.isValid())
        throw( QString("Channel ID or StartTime appears invalid."));

    ProgramInfo pInfo(chanid, recstarttsRaw.toUTC());

    return GetProgramArtworkList(pInfo.GetInetRef(), pInfo.GetSeason());
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
        QFileInfo fi(sFullFileName);
        if (fi.suffix().toLower() == "jpg")
            return fi;
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

QFileInfo Content::GetPreviewImage(        int        nChanId,
                                     const QDateTime &recstarttsRaw,
                                           int        nWidth,
                                           int        nHeight,
                                           int        nSecsIn )
{
    if (!recstarttsRaw.isValid())
    {
        QString sMsg = QString("GetPreviewImage: bad start time '%1'")
            .arg(MythDate::toString(recstarttsRaw, MythDate::ISODate));

        LOG(VB_GENERAL, LOG_ERR, sMsg);

        throw sMsg;
    }

    QDateTime recstartts = recstarttsRaw.toUTC();

    // ----------------------------------------------------------------------
    // Read Recording From Database
    // ----------------------------------------------------------------------

    ProgramInfo pginfo( (uint)nChanId, recstartts);

    if (!pginfo.GetChanID())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetPreviewImage: No recording for '%1'")
            .arg(ProgramInfo::MakeUniqueKey(nChanId, recstartts)));
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

    QString sFileName = GetPlaybackURL(&pginfo);

    // ----------------------------------------------------------------------
    // check to see if default preview image is already created.
    // ----------------------------------------------------------------------

    QString sPreviewFileName;

    if (nSecsIn <= 0)
    {
        nSecsIn = -1;
        sPreviewFileName = sFileName + ".png";
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

    float fAspect = 0.0;

    QImage *pImage = new QImage(sPreviewFileName);

    if (!pImage)
        return QFileInfo();

    if (fAspect <= 0)
        fAspect = (float)(pImage->width()) / pImage->height();

    if (fAspect == 0)
    {
        delete pImage;
        return QFileInfo();
    }

    bool bDefaultPixmap = (nWidth == 0) && (nHeight == 0);

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

    QString sNewFileName;

    if (bDefaultPixmap)
        sNewFileName = sPreviewFileName;
    else
        sNewFileName = QString( "%1.%2.%3x%4.png" )
                          .arg( sFileName )
                          .arg( nSecsIn   )
                          .arg( nWidth    )
                          .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if scaled preview image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sNewFileName ))
    {
        delete pImage;
        return QFileInfo( sNewFileName );
    }

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

    delete pImage;

    return QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Content::GetRecording( int              nChanId,
                                 const QDateTime &recstarttsRaw )
{
    if (!recstarttsRaw.isValid())
        throw( "StartTime is invalid" );

    // ------------------------------------------------------------------
    // Read Recording From Database
    // ------------------------------------------------------------------

    QDateTime recstartts = recstarttsRaw.toUTC();

    ProgramInfo pginfo((uint)nChanId, recstartts);

    if (!pginfo.GetChanID())
    {
        LOG(VB_UPNP, LOG_ERR, QString("GetRecording - for '%1' failed")
            .arg(ProgramInfo::MakeUniqueKey(nChanId, recstartts)));

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
    QString sBasePath = gCoreContext->GetSetting( "MusicLocation", "");
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
            sFileName = QString( "%1/%2" )
                           .arg( sBasePath )
                           .arg( query.value(0).toString() );
        }
    }

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

/** \fn     Content::DeleteImage( const QString &sStorageGroup,
 *                                const QString &sFileName )
 *  \brief  Permanently deletes the given file from the disk.
 *  \param  sStorageGroup The storage group name where the image is located
 *  \param  sFileName The filename including the path that shall be deleted
 *  \return bool True if deletion was successful, otherwise false
 */
bool Content::DeleteFile( const QString &sStorageGroup,
                          const QString &sFileName )
{
    // Get the fileinfo object
    QFileInfo fileInfo = GetFile(sStorageGroup, sFileName);

    // Check if the file exists. Only then we can actually delete it.
    if (!fileInfo.isFile() && !QFile::exists( fileInfo.absoluteFilePath()))
    {
        LOG(VB_GENERAL, LOG_ERR, "DeleteFile - File does not exist.");
        return false;
    }
    return QFile::remove( fileInfo.absoluteFilePath() );
}

/** \fn     Content::RenameFile(const QString &sStorageGroup,
 *                              const QString &sFileName,
 *                              const QString &sNewFile)
 *  \brief  Renames the file to the new name.
 *  \param  sStorageGroup The storage group name where the image is located
 *  \param  sFileName The filename including the path that shall be renamed
 *  \param  sNewName  The new name of the file (only the name, no path)
 *  \return bool True if renaming was successful, otherwise false
 */
bool Content::RenameFile( const QString &sStorageGroup,
                          const QString &sFileName,
                          const QString &sNewName)
{
    QFileInfo fi = QFileInfo();
    fi = GetFile(sStorageGroup, sFileName);

    // Check if the file exists and is writable.
    // Only then we can actually delete it.
    if (!fi.isFile() && !QFile::exists(fi.absoluteFilePath()))
    {
        LOG(VB_GENERAL, LOG_ERR, "RenameFile - File does not exist.");
        return false;
    }

    // Check if the new filename has no path stuff specified
    if (sNewName.contains("/") || sNewName.contains("\\"))
    {
        LOG(VB_GENERAL, LOG_ERR, "RenameFile - New file must not contain a path.");
        return false;
    }
    
    // The newly renamed file must be in the same path as the original one.
    // So we need to check if a file of the new name exists and would be 
    // overwritten by the new filename. To to this get the path from the 
    // original file and append the new file name, Then check 
    // if it exists in the given storage group
    
    // Get the everthing until the last directory separator
    QString path = sFileName.left(sFileName.lastIndexOf("/"));
    // Append the new file name to the path
    QString newFileName = path.append("/").append(sNewName);
    
    QFileInfo nfi = QFileInfo();
    nfi = GetFile(sStorageGroup, newFileName);
    
    // Check if the target file is already present.
    // If is there then abort, overwriting is not supported
    if (nfi.isFile() || QFile::exists(nfi.absoluteFilePath()))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("RenameFile - New file %1 would overwrite "
                    "existing one, not renaming.").arg(sFileName));
        return false;
    }

    // All checks have been passed, rename the file
    QFile file; 
    file.setFileName(fi.fileName());
    QDir::setCurrent(fi.absolutePath());
    return file.rename(sNewName);
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
                       nMaxSegments, 10, 32000, nSampleRate);

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
    int              nChanId,
    const QDateTime &recstarttsRaw,
    int              nMaxSegments,
    int              nWidth,
    int              nHeight,
    int              nBitrate,
    int              nAudioBitrate,
    int              nSampleRate )
{
    if (!recstarttsRaw.isValid())
        throw( "StartTime is invalid" );

    // ------------------------------------------------------------------
    // Read Recording From Database
    // ------------------------------------------------------------------

    QDateTime recstartts = recstarttsRaw.toUTC();

    ProgramInfo pginfo((uint)nChanId, recstartts);

    if (!pginfo.GetChanID())
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("AddRecordingLiveStream - for %1, %2 failed")
            .arg(ProgramInfo::MakeUniqueKey(nChanId, recstartts)));
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
                                    .arg( recstartts.toString() ));
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
