#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
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
#include "globalsettings.h"
#include "fileassoc.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>

enum VideoFileLocation
{
    kFileSystem,
    kDatabase,
    kBoth
};

typedef QMap <QString, VideoFileLocation> VideoLoadedMap;

struct GenericData
{
    QString paths;
    QSqlDatabase *db;
    QString startdir;
};

void runMenu(QString, const QString &, QSqlDatabase *, QString);
void VideoCallback(void *, QString &);
void SearchDir(QSqlDatabase *, QString &);
void BuildFileList(QSqlDatabase *, QString &, VideoLoadedMap &, QStringList &);

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythvideo", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;


    GeneralSettings general;
    general.load(QSqlDatabase::database());
    general.save(QSqlDatabase::database());
    PlayerSettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());

    return 0;
}

int mythplugin_run(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythvideo_") + 
                    QString(gContext->GetSetting("Language").lower()) + 
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    QString startdir = gContext->GetSetting("VideoStartupDir", 
                                            "/share/Movies/dvd");

    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "videomenu.xml", QSqlDatabase::database(), startdir);

    qApp->removeTranslator(&translator);

    return 0;
}

int mythplugin_config(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythvideo_") +
                    QString(gContext->GetSetting("Language").lower()) + 
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    QString startdir = gContext->GetSetting("VideoStartupDir",
                                            "/share/Movies/dvd");

    QString themedir = gContext->GetThemeDir();
    runMenu(themedir, "video_settings.xml", QSqlDatabase::database(), startdir);

    qApp->removeTranslator(&translator);

    return 0;
}

void runMenu(QString themedir, const QString &menuname, 
             QSqlDatabase *db, QString startdir)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), menuname,
                                      gContext->GetMainWindow(), "videomenu");

    GenericData data;

    data.db = db;
    data.startdir = startdir;

    diag->setCallback(VideoCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        gContext->GetLCDDevice()->switchToTime();
        diag->exec();
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
    if(password.length() < 1)
    {
        return true;
    }

    //
    //  See if we recently (and succesfully)
    //  asked for a password
    //
    
    if(last_time_stamp.length() < 1)
    {
        //
        //  Probably first time used
        //

        cerr << "main.o: Could not read password/pin time stamp. "
             << "This is only an issue if it happens repeatedly. " << endl;
    }
    else
    {
        QDateTime last_time = QDateTime::fromString(last_time_stamp, Qt::TextDate);
        if(last_time.secsTo(curr_time) < 120)
        {
            //
            //  Two minute window
            //
            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    
    //
    //  See if there is a password set
    //
    
    if(password.length() > 0)
    {
        bool ok = false;
        MythPasswordDialog *pwd = new MythPasswordDialog(QObject::tr("Parental Pin:"),
                                                         &ok,
                                                         password,
                                                         gContext->GetMainWindow());
        pwd->exec();
        delete pwd;
        if(ok)
        {
            //
            //  All is good
            //

            last_time_stamp = curr_time.toString(Qt::TextDate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }
    else
    {
        return true;
    }
    return false;
}

void VideoCallback(void *data, QString &selection)
{
    GenericData *mdata = (GenericData *)data;

    QString sel = selection.lower();

    if (sel == "manager")
    {
        if(checkParentPassword())
        {
            SearchDir(mdata->db, mdata->startdir);

            VideoManager *manage = new VideoManager(mdata->db, 
                                                    gContext->GetMainWindow(),
                                                    "video manager");
            manage->exec();
            delete manage;
        }
    }
    else if (sel == "browser")
    {
        VideoBrowser *browse = new VideoBrowser(mdata->db,
                                                gContext->GetMainWindow(),
                                                "video browser");
        browse->exec();
        delete browse;
    }
    else if (sel == "listing")
    {
        VideoTree *tree = new VideoTree(gContext->GetMainWindow(),
                                        mdata->db, "videotree", "video-");
        tree->exec();
        delete tree;
    }
    else if (sel == "settings_general")
    {
        //
        //  If we are doing aggressive 
        //  Parental Control, then junior
        //  is going to have to try harder
        //  than that!
        //
        
        if(gContext->GetNumSetting("VideoAggressivePC", 0))
        {
            if(checkParentPassword())
            {
                GeneralSettings settings;
                settings.exec(QSqlDatabase::database());
            }
        }
        else
        {
            GeneralSettings settings;
            settings.exec(QSqlDatabase::database());
        }
    }
    else if (sel == "settings_player")
    {
        PlayerSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "settings_associations")
    {
        FileAssocDialog fa(mdata->db,
                           gContext->GetMainWindow(),
                           "file_associations",
                           "video-",
                           "fa dialog");
        
        fa.exec();
    }
}

void SearchDir(QSqlDatabase *db, QString &directory)
{
    VideoLoadedMap video_files;
    VideoLoadedMap::Iterator iter;

    QStringList imageExtensions = QImage::inputFormatList();
    BuildFileList(db, directory, video_files, imageExtensions);

    QSqlQuery query("SELECT filename FROM videometadata;", db);

    int counter = 0;

    MythProgressDialog *file_checking =
               new MythProgressDialog(QObject::tr("Searching for video files"),
                                      query.numRowsAffected());

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString name = query.value(0).toString();
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
            name.replace(quote_regex, "\"\"");

            myNewFile = new Metadata(name, QObject::tr("No Cover"), "", 
                                     1895, "00000000", QObject::tr("Unknown"), 
                                     QObject::tr("None"), 0.0, 
                                     QObject::tr("NR"), 0, 0, 1);
            myNewFile->guessTitle();
            myNewFile->dumpToDatabase(db);
            if (myNewFile)
                delete myNewFile;
        }
        if (*iter == kDatabase)
        {
            QString name(iter.key());
            name.replace(quote_regex, "\"\"");
 
            QString querystr;
            querystr.sprintf("DELETE FROM videometadata WHERE "
                                       "filename=\"%s\"", name.ascii());
            query.exec(querystr);
        }

        file_checking->setProgress(++counter);
    }
    file_checking->Close();
    delete file_checking;
}

bool IgnoreExtension(QSqlDatabase *db, QString extension)
{
    
    QString q_string = QString("SELECT f_ignore FROM videotypes WHERE extension = \"%1\" ;")
                              .arg(extension);
    QSqlQuery a_query(q_string, db);
    if(a_query.isActive() && a_query.numRowsAffected() > 0)
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

void BuildFileList(QSqlDatabase *db,
                   QString &directory, 
                   VideoLoadedMap &video_files,
                   QStringList &imageExtensions)
{
    QDir d(directory);

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
            if(IgnoreExtension(db, fi->extension(false)))
            {
                continue;
            }
        }
        
        QString filename = fi->absFilePath();
        if (fi->isDir())
            BuildFileList(db, filename, video_files, imageExtensions);
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

