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
#include "globalsettings.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

#ifdef ENABLE_LIRC
#include "lirc_client.h"

int lfd;
struct lirc_config *config;
#endif

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

MythContext *gContext;
void runMenu(QString, QSqlDatabase *, QString);
void VideoCallback(void *, QString &);
void SearchDir(QSqlDatabase *, QString &);
void BuildFileList(QString &, VideoLoadedMap &);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator translator( 0 );
    
    gContext = new MythContext(MYTH_BINARY_VERSION);

#ifdef ENABLE_LIRC
    lfd=lirc_init("mythvideo",1);
    lirc_readconfig(NULL,&config,NULL);
#endif

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    gContext->LoadQtConfig();

    translator.load(PREFIX + QString("/share/mythtv/i18n/mythvideo_") + 
                    QString(gContext->GetSetting("Locale")) + QString(".qm"),
                    ".");
    a.installTranslator(&translator);

    QString themename = gContext->GetSetting("Theme");
    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        db->close();
        delete gContext;
        exit(0);
    }

    QString startdir = gContext->GetSetting("VideoStartupDir", "/share/Movies/dvd");

    runMenu(themedir, db, startdir);

    db->close();

    delete gContext;

    return 0;
}

void runMenu(QString themedir, QSqlDatabase *db, QString startdir)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(),
                                      "videomenu.xml");

    GenericData data;

    data.db = db;
    data.startdir = startdir;

    diag->setCallback(VideoCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        gContext->LCDswitchToTime();
        diag->Show();
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

        VideoManager *manage = new VideoManager(mdata->db);
        manage->Show();
        manage->exec();
    }
    else if (sel == "browser")
    {
        VideoBrowser *browse = new VideoBrowser(mdata->db);
        browse->Show();
        browse->exec();
    }
    if (sel == "settings_general")
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

    BuildFileList(directory, video_files);

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
            QString title = name.right(name.length() - name.findRev("/") - 1);
            title.replace(QRegExp("_"), " ");
            title = title.left(title.findRev("."));
            title.replace(QRegExp("\\."), " ");
            cout << "Replace .: " << title << endl;
            title = title.left(title.find("["));
            title = title.left(title.find("("));

            myNewFile = new Metadata(name, "No Cover", title, 1895, "00000000", 
                                     "Uknown", "None", 0.0, "NR", 0, 0, 1);
            myNewFile->dumpToDatabase(db);
            if (myNewFile)
                delete myNewFile;
        }
        if (*iter == kDatabase)
        {
            QString name(iter.key());
            name.replace(quote_regex, "\"\"");
 
            QString querystr = QString("DELETE FROM videometadata WHERE "
                                       "filename=\"%1\"").arg(name);
            query.exec(querystr);
        }

        file_checking->setProgress(++counter);
    }
    file_checking->Close();
    delete file_checking;
}

void BuildFileList(QString &directory, VideoLoadedMap &video_files)
{
    QDir d(directory);

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
        if (fi->isDir())
            BuildFileList(filename, video_files);
        else
            video_files[filename] = kFileSystem;
    }
}
