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
void BuildFileList(QString &, VideoLoadedMap &, QStringList &);

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
                    QString(gContext->GetSetting("Locale")) + QString(".qm"),
                    ".");
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
                    QString(gContext->GetSetting("Locale")) + QString(".qm"),
                    ".");
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
        gContext->LCDswitchToTime();
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

void VideoCallback(void *data, QString &selection)
{
    GenericData *mdata = (GenericData *)data;

    QString sel = selection.lower();

    if (sel == "manager")
    {
        SearchDir(mdata->db, mdata->startdir);

        VideoManager *manage = new VideoManager(mdata->db, 
                                                gContext->GetMainWindow(),
                                                "video manager");
        manage->exec();
        delete manage;
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
        GeneralSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "settings_player")
    {
        PlayerSettings settings;
        settings.exec(QSqlDatabase::database());
    }
}

void SearchDir(QSqlDatabase *db, QString &directory)
{
    VideoLoadedMap video_files;
    VideoLoadedMap::Iterator iter;

    QStringList imageExtensions = QImage::inputFormatList();
    BuildFileList(directory, video_files, imageExtensions);

    QSqlQuery query("SELECT filename FROM videometadata;", db);

    int counter = 0;

    MythProgressDialog *file_checking =
                         new MythProgressDialog("Searching for video files",
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
        new MythProgressDialog("Updating video database", video_files.size());

    Metadata *myNewFile = NULL;

    QRegExp quote_regex("\"");
    for (iter = video_files.begin(); iter != video_files.end(); iter++)
    {
        if (*iter == kFileSystem)
        {
            QString name(iter.key());
            name.replace(quote_regex, "\"\"");

            myNewFile = new Metadata(name, "No Cover", "", 1895, "00000000", 
                                     "Unknown", "None", 0.0, "NR", 0, 0, 1);
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

void BuildFileList(QString &directory, VideoLoadedMap &video_files,
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
        if (fi->fileName() == "." || fi->fileName() == ".." ||
            fi->fileName() == "Thumbs.db")
            continue;
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
