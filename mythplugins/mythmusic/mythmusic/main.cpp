#include <qdir.h>
#include <iostream>
#include <map>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include <cdaudio.h>

#include "decoder.h"
#include "metadata.h"
#include "maddecoder.h"
#include "vorbisdecoder.h"
#include "databasebox.h"
#include "playbackbox.h"
#include "cdrip.h"
#include "playlist.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

void CheckFreeDBServerFile(void)
{
    char filename[1024];
    if (getenv("HOME") == NULL)
        return;

    sprintf(filename, "%s/.cdserverrc", getenv("HOME"));

    QFile file(filename);

    if (!file.exists())
    {
        struct cddb_conf cddbconf;
        struct cddb_serverlist list;
        struct cddb_host proxy_host;

        cddbconf.conf_access = CDDB_ACCESS_REMOTE;
        list.list_len = 1;
        strncpy(list.list_host[0].host_server.server_name, 
                "www.freedb.org", 256);
        strncpy(list.list_host[0].host_addressing,
                "cgi-bin/cddb.cgi", 256);
        list.list_host[0].host_server.server_port = 80;
        list.list_host[0].host_protocol = CDDB_MODE_HTTP;

        cddb_write_serverlist(cddbconf, list, proxy_host.host_server);
    }
}

Decoder *getDecoder(MythContext *context, const QString &filename)
{
    Decoder *decoder = Decoder::create(context, filename, NULL, NULL, true);
    return decoder;
}

void CheckFile(MythContext *context, const QString &filename)
{
    Decoder *decoder = getDecoder(context, filename);

    if (decoder)
    {
        cout << "Checking: " << filename << endl;
        QSqlDatabase *db = QSqlDatabase::database();

        Metadata *data = decoder->getMetadata(db);
        if (data)
            delete data;
 
        delete decoder;
    }
}

typedef map<QString, bool> MusicLoadedMap;

void BuildFileList(MythContext *context, QString &directory,
                   MusicLoadedMap &music_files)
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
            BuildFileList(context, filename, music_files);
        else
            music_files[filename] = false;
    }
}

void SearchDir(MythContext *context, QString &directory)
{
    MusicLoadedMap music_files;
    MusicLoadedMap db_files;
    MusicLoadedMap::iterator iter;

    BuildFileList(context, directory, music_files);

    QSqlQuery query;

    query.exec("SELECT filename FROM musicmetadata;");
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString name = query.value(0).toString();
            if (name != QString::null)
            {
                db_files[name] = false;
                if ((iter = music_files.find(name)) != music_files.end())
                {
                    (*iter).second = true;
                }
            }
        }
    }

    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if ((*iter).second == false)
        {
            CheckFile(context, (*iter).first);
        }
    }
}

void startPlayback(MythContext *context, QSqlDatabase *db, 
                   QValueList<Metadata> *playlist)
{
    PlaybackBox *pbb = new PlaybackBox(context, db, playlist);
    pbb->Show();

    pbb->exec();
    pbb->stop();

    qApp->processEvents();

    delete pbb;
}

void startDatabaseTree(MythContext *context, QSqlDatabase *db, QString &paths, 
                       QValueList<Metadata> *playlist)
{
    DatabaseBox dbbox(context, db, paths, playlist);
    dbbox.Show();

    dbbox.exec();
}

void startRipper(MythContext *context, QSqlDatabase *db)
{
    Ripper rip(context, db);
    rip.Show();
    rip.exec();
}

struct MusicData
{
    MythContext *context;
    QString paths;
    QSqlDatabase *db;
    QString startdir;
    QValueList<Metadata> *playlist;
};

void MusicCallback(void *data, QString &selection)
{
    MusicData *mdata = (MusicData *)data;

    QString sel = selection.lower();

    if (sel == "music_create_playlist")
        startDatabaseTree(mdata->context, mdata->db, mdata->paths, 
                          mdata->playlist);
    else if (sel == "music_play")
        startPlayback(mdata->context, mdata->db, mdata->playlist);
    else if (sel == "music_rip")
    {
        startRipper(mdata->context, mdata->db);
        SearchDir(mdata->context, mdata->startdir);
    }
    else if (sel == "music_setup")
        ;
}

void runMenu(MythContext *context, QString themedir, QSqlDatabase *db, 
             QString paths, QValueList<Metadata> &playlist, QString startdir)
{
    ThemedMenu *diag = new ThemedMenu(context, themedir.ascii(), 
                                      "musicmenu.xml");
       
    MusicData data;
   
    data.context = context;
    data.paths = paths;
    data.db = db;
    data.startdir = startdir;
    data.playlist = &playlist;

    diag->setCallback(MusicCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        diag->Show();
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }
}            

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext();
    context->LoadQtConfig();

    context->LoadSettingsFiles("mythmusic-settings.txt");

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    if (!context->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    CheckFreeDBServerFile();

    QString startdir = context->GetSetting("MusicLocation");

    if (startdir != "")
        SearchDir(context, startdir);

    QString paths = context->GetSetting("TreeLevels");
    QValueList<Metadata> playlist;

    QString themename = context->GetSetting("Theme");
    QString themedir = context->FindThemeDir(themename);

    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    LoadDefaultPlaylist(db, playlist);
    runMenu(context, themedir, db, paths, playlist, startdir);
    SaveDefaultPlaylist(db, playlist);

    db->close();

    delete context;

    return 0;
}
