#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <unistd.h>
#include <qsocketnotifier.h>
#include <qtextcodec.h>
#include <qregexp.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#include "metadata.h"
#include "videomanager.h"
#include "videobrowser.h"
#include "videotree.h"
#include "videogallery.h"
#include "videofilter.h"
#include "globalsettings.h"
#include "fileassoc.h"
#include "dbcheck.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/lcddevice.h>
#include <mythtv/mythdbcon.h>

enum VideoFileLocation
{
    kFileSystem,
    kDatabase,
    kBoth
};

typedef QMap <QString, VideoFileLocation> VideoLoadedMap;

void runMenu(QString, const QString &);
void VideoCallback(void *, QString &);
void SearchDir(QString &);
void BuildFileList(QString &, VideoLoadedMap &, QStringList &);

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runVideoManager(void);
void runVideoBrowser(void);
void runVideoTree(void);
void runVideoGallery(void);
void runDefaultView(void);


void setupKeys(void)
{
    REG_JUMP("MythVideo", "The MythVideo default view", "", 
             runDefaultView);
             
    REG_JUMP("Video Manager", "The MythVideo video manager", "", 
             runVideoManager);
    REG_JUMP("Video Browser", "The MythVideo video browser", "", 
             runVideoBrowser);
    REG_JUMP("Video Listings", "The MythVideo video listings", "", 
             runVideoTree);
    REG_JUMP("Video Gallery", "The MythVideo video gallery", "",
             runVideoGallery);

    
    REG_KEY("Video","FILTER","Open video filter dialog","F");
    
    REG_KEY("Video","DELETE","Delete video","D");
    REG_KEY("Video","BROWSE","Change browsable in video manager","B");
    REG_KEY("Video","INCPARENT","Increase Parental Level","],},F11");
    REG_KEY("Video","DECPARENT","Decrease Parental Level","[,{,F10");
}


int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythvideo", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    UpgradeVideoDatabaseSchema();

    VideoGeneralSettings general;
    general.load();
    general.save();
    VideoPlayerSettings settings;
    settings.load();
    settings.save();

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "videomenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "video_settings.xml");

    return 0;
}

void runMenu(QString themedir, const QString &menuname)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), menuname,
                                      gContext->GetMainWindow(), "videomenu");

    diag->setCallback(VideoCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (class LCD * lcd = LCD::Get())
            lcd->switchToTime();

        qApp->unlock();
        diag->exec();
        qApp->lock();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

bool checkParentPassword()
{
    QDateTime curr_time = QDateTime::currentDateTime();
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    QString password = gContext->GetSetting("VideoAdminPassword");

    if (password.length() < 1)
        return true;

    // See if we recently (and succesfully) asked for a password
    
    if (last_time_stamp.length() < 1)
    {
        // Probably first time used

        cerr << "main.o: Could not read password/pin time stamp. "
             << "This is only an issue if it happens repeatedly. " << endl;
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp, 
                                                    Qt::TextDate);
        if (last_time.secsTo(curr_time) < 120)
        {
            // Two minute window
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    
    // See if there is a password set
    
    if (password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd = new MythPasswordDialog(QObject::tr("Parental Pin:"),
                                                         &ok,
                                                         password,
                                                         gContext->GetMainWindow());
        pwd->exec();
        delete pwd;

        if (ok)
        {
            // All is good
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    else
        return true;
    return false;
}

void runVideoManager(void)
{
    if (checkParentPassword())
    {
        QString startdir = gContext->GetSetting("VideoStartupDir",
                                                "/share/Movies/dvd");
        SearchDir(startdir);

        VideoManager *manage = new VideoManager(gContext->GetMainWindow(),
                                                "video manager");
        qApp->unlock();
        manage->exec();
        qApp->lock();
        delete manage;
    }
}

void runVideoBrowser(void)
{
    VideoBrowser *browse = new VideoBrowser(gContext->GetMainWindow(),
                                            "video browser");
    qApp->unlock();
    browse->exec();
    qApp->lock();
    delete browse;
}

void runVideoTree(void)
{
    VideoTree *tree = new VideoTree(gContext->GetMainWindow(),
                                    "videotree",
                                    "video-",
                                    "video tree"); 

    qApp->unlock();
    tree->exec();
    qApp->lock();
    delete tree;
}

void runDefaultView(void)
{
    int viewType = gContext->GetNumSetting("Default MythVideo View", VideoDialog::DLG_GALLERY);

    switch (viewType)
    {
        case VideoDialog::DLG_BROWSER:
            runVideoBrowser();
            break;
        case VideoDialog::DLG_GALLERY:
            runVideoGallery();
            break;
        case VideoDialog::DLG_TREE:
            runVideoTree();
            break;
        default:            
            runVideoGallery();
            break;
    };
    
}

void runVideoGallery(void)
{
    VideoGallery *gallery = new VideoGallery(gContext->GetMainWindow(),
                                             "video gallery");
    qApp->unlock();
    gallery->exec();
    qApp->lock();
    delete gallery;
}

void VideoCallback(void *data, QString &selection)
{
    (void)data;

    QString sel = selection.lower();

    if (sel == "manager")
        runVideoManager();
    else if (sel == "browser")
        runVideoBrowser();
    else if (sel == "listing")
        runVideoTree();
    else if (sel == "gallery")
        runVideoGallery();
    else if (sel == "settings_general")
    {
        //
        //  If we are doing aggressive 
        //  Parental Control, then junior
        //  is going to have to try harder
        //  than that!
        //
        
        if (gContext->GetNumSetting("VideoAggressivePC", 0))
        {
            if (checkParentPassword())
            {
                VideoGeneralSettings settings;
                settings.exec();
            }
        }
        else
        {
            VideoGeneralSettings settings;
            settings.exec();
        }
    }
    else if (sel == "settings_player")
    {
        VideoPlayerSettings settings;
        settings.exec();
    }
    else if (sel == "settings_associations")
    {
        FileAssocDialog fa(gContext->GetMainWindow(),
                           "file_associations",
                           "video-",
                           "fa dialog");
        
        fa.exec();
    }
}

void SearchDir(QString &directory)
{
    VideoLoadedMap video_files;
    VideoLoadedMap::Iterator iter;

    QStringList imageExtensions = QImage::inputFormatList();
    BuildFileList(directory, video_files, imageExtensions);

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT filename FROM videometadata;");

    int counter = 0;

    MythProgressDialog *file_checking =
               new MythProgressDialog(QObject::tr("Searching for video files"),
                                      query.numRowsAffected());

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString name = QString::fromUtf8(query.value(0).toString());
            if (name != QString::null)
            {
                if ((iter = video_files.find(name)) != video_files.end())
                    video_files.remove(iter);
                else
                    video_files[name] = kDatabase;
            }
            file_checking->setProgress(++counter);
        }
    }

    file_checking->Close();
    delete file_checking;

    file_checking =
        new MythProgressDialog(QObject::tr("Updating video database"), 
                               video_files.size());

    Metadata *myNewFile = NULL;

    QRegExp quote_regex("\"");
    for (iter = video_files.begin(); iter != video_files.end(); iter++)
    {
        if (*iter == kFileSystem)
        {
            QString name(iter.key());

            myNewFile = new Metadata(name, QObject::tr("No Cover"), "", 
                                     1895, "00000000", QObject::tr("Unknown"), 
                                     QObject::tr("None"), 0.0, 
                                     QObject::tr("NR"), 0, 0, 1);
            myNewFile->guessTitle();
            myNewFile->dumpToDatabase();
            if (myNewFile)
                delete myNewFile;
        }
        if (*iter == kDatabase)
        {
            QString name(iter.key());

            query.prepare("DELETE FROM videometadata WHERE "
                          "filename = :FILE ;");
            query.bindValue(":FILE", name);
            query.exec();
        }

        file_checking->setProgress(++counter);
    }
    file_checking->Close();
    delete file_checking;
}

bool IgnoreExtension(QString extension)
{
    MSqlQuery a_query(MSqlQuery::InitCon());
    a_query.prepare("SELECT f_ignore FROM videotypes WHERE extension = :EXT");
    a_query.bindValue(":EXT", extension);

    if(a_query.exec() && a_query.isActive() && a_query.size() > 0)
    {
        //
        //  This extension is a recognized
        //  file type (in the videotypes
        //  database). Return true only if
        //  ignore explicitly set.
        //
        a_query.next();
        return a_query.value(0).toBool();
    }
    
    //
    //  Otherwise, ignore this file only
    //  if the user has a setting to
    //  ignore unknown file types.
    //
    
    return !gContext->GetNumSetting("VideoListUnknownFileTypes", 1);
}

void BuildFileList(QString &directory, 
                   VideoLoadedMap &video_files,
                   QStringList &imageExtensions)
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
            if(IgnoreExtension(fi->extension(false)))
            {
                continue;
            }
        }
        
        QString filename = fi->absFilePath();
        if (fi->isDir())
            BuildFileList(filename, video_files, imageExtensions);
        else
        {
            r.setPattern("^" + fi->extension() + "$");
            r.setCaseSensitive(false);
            QStringList result = imageExtensions.grep(r);

            if (result.isEmpty())
                video_files[filename] = kFileSystem;
        }
    }
}

