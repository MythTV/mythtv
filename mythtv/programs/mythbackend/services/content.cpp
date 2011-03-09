//////////////////////////////////////////////////////////////////////////////
// Program Name: content.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo* Content::GetFile( const QString &sStorageGroup, const QString &sFileName )
{
    QString sGroup = sStorageGroup;

    if (sGroup.isEmpty())
    {
        VERBOSE( VB_UPNP, "GetFile - StorageGroup missing... using 'Default'");
        sGroup = "Default";
    }

    if (sFileName.isEmpty())
    {
        QString sMsg ( "GetFile - FileName missing." );

        VERBOSE( VB_UPNP, sMsg );

        throw sMsg;
    }

    // ------------------------------------------------------------------
    // Search for the filename
    // ------------------------------------------------------------------

    StorageGroup storage( sGroup );
    QString sFullFileName = storage.FindRecordingFile( sFileName );

    if (sFullFileName.isEmpty())
    {
        VERBOSE( VB_UPNP, QString("GetFile - Unable to find %1.").arg(sFileName));

        return NULL;
    }

    // ----------------------------------------------------------------------
    // check to see if the file (still) exists
    // ----------------------------------------------------------------------

    if (QFile::exists( sFullFileName ))
    {
        return new QFileInfo( sFullFileName );
    }

    VERBOSE( VB_UPNP, QString("GetFile - File Does not exist %1.").arg(sFullFileName));

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::StringList* Content::GetFileList( const QString &sStorageGroup )
{

    if (sStorageGroup.isEmpty())
    {
        QString sMsg( "GetFileList - StorageGroup missing.");
        VERBOSE( VB_UPNP, sMsg );

        throw sMsg;
    }

    QStringList      sStorageGroupDirs;
    QString          hostname    = gCoreContext->GetHostName();
    DTC::StringList *pStringList = new DTC::StringList();

    if (StorageGroup::FindDirs(sStorageGroup, hostname, &sStorageGroupDirs))
    {
        QStringList::iterator it = sStorageGroupDirs.begin();

        for ( ; it != sStorageGroupDirs.end(); ++it)
        {
            QDir dir(*it);

            dir.setFilter(QDir::Files);
            dir.setSorting(QDir::Name | QDir::IgnoreCase);

            QFileInfoList fileList = dir.entryInfoList();
            QFileInfoList::iterator fit = fileList.begin();

            for ( ; fit != fileList.end(); ++fit )
            {
                /*                                
                if (bShowLinks)
                {
                    stream
                        << "<a href='/Myth/GetFile?StorageGroup="
                        << sStorageGroup << "&FileName=" << (*fit).fileName()
                        << "'>" << (*fit).fileName() << "</a><br>\n";
                }
                else
                */
                pStringList->Values().append( (*fit).fileName() );
            }
        }
    }

    return pStringList;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo* Content::GetVideoArt( int nId )
{
    VERBOSE(VB_UPNP, QString("GetVideoArt ID = %1").arg(nId));

    // ----------------------------------------------------------------------
    // Read Video poster file path from database
    // ----------------------------------------------------------------------

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT coverfile FROM videometadata WHERE intid = :ITEMID");
    query.bindValue(":ITEMID", nId);

    if (!query.exec())
        MythDB::DBError("GetVideoArt ", query);

    if (!query.next())
        return NULL;

    QString sFileName = query.value(0).toString();

    // ----------------------------------------------------------------------
    // check to see if albumart image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sFileName ))
        return new QFileInfo( sFileName );

    // ----------------------------------------------------------------------
    // Not there? Perhaps we need to look in a storage group?
    // ----------------------------------------------------------------------

    return GetFile( "Coverart", sFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo* Content::GetAlbumArt( int nId, int nWidth, int nHeight )
{
    QString sFullFileName;

    // ----------------------------------------------------------------------
    // Read AlbumArt file path from database
    // ----------------------------------------------------------------------

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT CONCAT_WS('/', music_directories.path, "
                  "music_albumart.filename) FROM music_albumart "
                  "LEFT JOIN music_directories ON "
                  "music_directories.directory_id=music_albumart.directory_id "
                  "WHERE music_albumart.albumart_id = :ARTID;");

    query.bindValue(":ARTID", nId );

    if (!query.exec())
        MythDB::DBError("Select ArtId", query);

    QString sMusicBasePath = gCoreContext->GetSetting("MusicLocation", "");

    if (query.next())
    {
        sFullFileName = QString( "%1/%2" )
                        .arg( sMusicBasePath )
                        .arg( query.value(0).toString() );
    }

    if (!QFile::exists( sFullFileName ))
        return NULL;

    if ((nWidth == 0) && (nHeight == 0))
        return new QFileInfo( sFullFileName );

    QString sNewFileName = QString( "%1.%2x%3.png" )
                              .arg( sFullFileName )
                              .arg( nWidth    )
                              .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if albumart image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sNewFileName ))
        return new QFileInfo( sNewFileName );

    // ----------------------------------------------------------------------
    // Must generate Albumart Image, Generate Image and save.
    // ----------------------------------------------------------------------

    float fAspect = 0.0;

    QImage *pImage = new QImage( sFullFileName);

    if (!pImage)
        return NULL;

    if (fAspect <= 0)
           fAspect = (float)(pImage->width()) / pImage->height();

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    QByteArray fname = sNewFileName.toAscii();
    img.save( fname.constData(), "PNG" );

    delete pImage;

    return new QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo* Content::GetPreviewImage(       int        nChanId,
                                     const QDateTime &dtStartTime,
                                           int        nWidth,    
                                           int        nHeight,   
                                           int        nSecsIn )
{
    if (!dtStartTime.isValid())
    {
        QString sMsg = QString("GetPreviewImage: bad start time '%1'")
                          .arg( dtStartTime.toString() );

        VERBOSE(VB_IMPORTANT, sMsg);

        throw sMsg;
    }

    // ----------------------------------------------------------------------
    // Read Recording From Database
    // ----------------------------------------------------------------------

    ProgramInfo pginfo( (uint)nChanId, dtStartTime);

    if (!pginfo.GetChanID())
    {
        VERBOSE(VB_IMPORTANT, QString( "GetPreviewImage: no recording for start time '%1'" )
                                 .arg( dtStartTime.toString() ));
        return NULL;
    }

    if ( pginfo.GetHostname() != gCoreContext->GetHostName())
    {
        QString sMsg = QString("GetPreviewImage: Wrong Host '%1' request from '%2'")
                          .arg( gCoreContext->GetHostName())
                          .arg( pginfo.GetHostname() );

        VERBOSE(VB_IMPORTANT, sMsg);

        throw sMsg;
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
        if (!pginfo.IsLocal() && sFileName.left(1) == "/")
            pginfo.SetPathname(sFileName);

        if (!pginfo.IsLocal())
            return NULL;

        PreviewGenerator *previewgen = new PreviewGenerator( &pginfo, 
                                                             QString(), 
                                                             PreviewGenerator::kLocal);
        previewgen->SetPreviewTimeAsSeconds( nSecsIn          );
        previewgen->SetOutputFilename      ( sPreviewFileName );

        bool ok = previewgen->Run();

        previewgen->deleteLater();

        if (!ok)
            return NULL;
    }

    float fAspect = 0.0;

    QImage *pImage = new QImage(sPreviewFileName);

    if (!pImage)
        return NULL;

    if (fAspect <= 0)
        fAspect = (float)(pImage->width()) / pImage->height();

    if (fAspect == 0)
    {
        delete pImage;
        return NULL;
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
        return new QFileInfo( sNewFileName );
    }

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    QByteArray fname = sNewFileName.toAscii();
    img.save( fname.constData(), "PNG" );

    makeFileAccessible(fname.constData());

    delete pImage;

    return new QFileInfo( sNewFileName );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo* Content::GetRecording( int              nChanId,
                                  const QDateTime &dtStartTime )
{
    if (!dtStartTime.isValid())
        throw( "StartTime is invalid" );

    // ------------------------------------------------------------------
    // Read Recording From Database
    // ------------------------------------------------------------------

    ProgramInfo pginfo( (uint)nChanId, dtStartTime );

    if (!pginfo.GetChanID())
    {
        VERBOSE( VB_UPNP, QString( "GetRecording - for %1, %2 "
                                   "failed" )
                                    .arg( nChanId )
                                    .arg( dtStartTime.toString() ));
        return NULL;
    }

    if ( pginfo.GetHostname() != gCoreContext->GetHostName())
    {
        // We only handle requests for local resources

        QString sMsg = QString( "GetRecording - To access this "
                                   "recording, send request to %1." )
                          .arg( pginfo.GetHostname() );

        VERBOSE( VB_UPNP, sMsg );
        
        throw sMsg;
    }

    QString sFileName( GetPlaybackURL(&pginfo) );

    // ----------------------------------------------------------------------
    // check to see if the file exists
    // ----------------------------------------------------------------------

    if (QFile::exists( sFileName ))
        return new QFileInfo( sFileName );

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo* Content::GetMusic( int nId )
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
            return NULL;
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
        return new QFileInfo( sFileName );

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo* Content::GetVideo( int nId )
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
            return NULL;
        }

        if (query.next())
            sFileName = query.value(0).toString();
    }

    if (sFileName.isEmpty())
        return NULL;

    if (!QFile::exists( sFileName ))
        return GetFile( "Videos", sFileName );

    return new QFileInfo( sFileName );
}

