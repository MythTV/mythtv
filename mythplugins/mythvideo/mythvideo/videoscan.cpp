#include <qdir.h>
#include <qimage.h>
#include <iostream>
using namespace std;

#include "videoscan.h"
#include "metadata.h"
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>



VideoScanner::VideoScanner()
{
    m_RemoveAll = false;
    m_KeepAll = false;
    m_ListUnknown = gContext->GetNumSetting("VideoListUnknownFileTypes", 1);
}

void VideoScanner::doScan(const QString& videoDirs)
{
    QStringList imageExtensions = QImage::inputFormatList();
    int counter = 0;

    
    QStringList dirs = QStringList::split(":", videoDirs);
    QStringList::iterator iter;
        
    MythProgressDialog progressDlg(QObject::tr("Searching for video files"),
                                   dirs.size());
    
    for (iter = dirs.begin(); iter != dirs.end(); iter++)
    {
        buildFileList( *iter, imageExtensions );
        progressDlg.setProgress(++counter);
    }
    
    progressDlg.close();
    verifyFiles();
    updateDB();
}


void VideoScanner::promptForRemoval(const QString& filename)
{
    if (m_RemoveAll)
        Metadata::purgeByFilename(filename);
    
    if (m_KeepAll || m_RemoveAll)
        return;
    
    QStringList buttonText;
    buttonText += QObject::tr("No");
    buttonText += QObject::tr("No to all");
    buttonText += QObject::tr("Yes");
    buttonText += QObject::tr("Yes to all");
    
            
    int result = MythPopupBox::showButtonPopup(gContext->GetMainWindow(), QObject::tr("File Missing"),
                                               QString(QObject::tr("%1 appears to be missing.\nRemove it"  
                                                                   " from the database?")).arg(filename),
                                               buttonText, 1 );
    switch (result)
    {
        case 1:
            m_KeepAll = true;
            break;            
        case 2:
            Metadata::purgeByFilename(filename);
            break;
        case 3:
            m_RemoveAll = true;
            Metadata::purgeByFilename(filename);
            break;
    };                                               
    
}


void VideoScanner::updateDB()
{
    int counter = 0;
    MSqlQuery query(MSqlQuery::InitCon());
    MythProgressDialog progressDlg(QObject::tr("Updating video database"), 
                                   m_VideoFiles.size());
    VideoLoadedMap::Iterator iter;
    
    for (iter = m_VideoFiles.begin(); iter != m_VideoFiles.end(); iter++)
    {
        if (*iter == kFileSystem)
        {
            Metadata newFile(iter.key(), QObject::tr("No Cover"), "", 1895, 
                             "00000000", QObject::tr("Unknown"), 
                             QObject::tr("None"), 0.0, QObject::tr("NR"), 
                             0, 0, 1);
            
            newFile.guessTitle();
            newFile.dumpToDatabase();
        }
        
        if (*iter == kDatabase)
        {
            promptForRemoval( iter.key() );
        }

        progressDlg.setProgress(++counter);
    }
    
    progressDlg.Close();
}


void VideoScanner::verifyFiles()
{
    int counter = 0;
    VideoLoadedMap::Iterator iter;
    
    // Get the list of files from the database.                                   
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT filename FROM videometadata;");

    MythProgressDialog progressDlg(QObject::tr("Verifying video files"),
                                   query.numRowsAffected());
    
    
    // For every file we know about, check to see if it still exists.
    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString name = QString::fromUtf8(query.value(0).toString());
            if (name != QString::null)
            {
                
                if ((iter = m_VideoFiles.find(name)) != m_VideoFiles.end())
                {
                    // If it's both on disk and in the database we're done with it.
                    m_VideoFiles.remove(iter);
                } 
                else
                {
                    // If it's only in the database mark it as such for removal later
                    m_VideoFiles[name] = kDatabase;
                }
            }
            progressDlg.setProgress(++counter);
        }
    }

    progressDlg.Close();
}
 


void VideoScanner::buildFileList(const QString &directory, 
                                 const QStringList &imageExtensions)
{
    QDir d(directory);

    d.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase );

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;
    QRegExp r;
    
    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || 
            fi->fileName() == ".." ||
            fi->fileName() == "Thumbs.db")
        {
            continue;
        }
        
        if(!fi->isDir())
        {
            if(ignoreExtension(fi->extension(false)))
            {
                continue;
            }
        }
        
        QString filename = fi->absFilePath();
        if (fi->isDir())
            buildFileList(filename, imageExtensions);
        else
        {
            r.setPattern("^" + fi->extension() + "$");
            r.setCaseSensitive(false);
            QStringList result = imageExtensions.grep(r);

            if (result.isEmpty())
                m_VideoFiles[filename] = kFileSystem;
        }
    }
}


bool VideoScanner::ignoreExtension(const QString& extension) const
{
    MSqlQuery a_query(MSqlQuery::InitCon());
    a_query.prepare("SELECT f_ignore FROM videotypes WHERE extension = :EXT");
    a_query.bindValue(":EXT", extension);

    if(a_query.exec() && a_query.isActive() && a_query.size() > 0)
    {
        //  This extension is a recognized file type (in the videotypes
        //  database). Return true only if ignore explicitly set.
        //
        a_query.next();
        return a_query.value(0).toBool();
    }
    
    //
    //  Otherwise, ignore this file only if the user has a setting to
    //  ignore unknown file types.
    //
    return !m_ListUnknown;
}
