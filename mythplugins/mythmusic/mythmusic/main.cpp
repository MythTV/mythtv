#include <qdir.h>
#include <iostream>
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
#include <mythtv/settings.h>

Settings *globalsettings;

char theprefix[] = PREFIX;

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

Decoder *getDecoder(QString &filename)
{
    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);
    return decoder;
}

void CheckFile(QString &filename)
{
    Decoder *decoder = getDecoder(filename);

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

void SearchDir(QString &directory)
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
            SearchDir(filename);
        else
            CheckFile(filename);
    }
}

void startPlayback(QSqlDatabase *db, QValueList<Metadata> *playlist)
{
    PlaybackBox *pbb = new PlaybackBox(db, playlist);
    pbb->Show();

    pbb->exec();
    pbb->stop();

    qApp->processEvents();

    delete pbb;
}

void startDatabaseTree(QSqlDatabase *db, QString &paths, 
                       QValueList<Metadata> *playlist)
{
    DatabaseBox dbbox(db, paths, playlist);
    dbbox.Show();

    dbbox.exec();
}

void startRipper(QSqlDatabase *db)
{
    Ripper rip(db);
    rip.Show();
    rip.exec();
}

struct MusicData
{
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
        startDatabaseTree(mdata->db, mdata->paths, mdata->playlist);
    else if (sel == "music_play")
        startPlayback(mdata->db, mdata->playlist);
    else if (sel == "music_rip")
    {
        startRipper(mdata->db);
        SearchDir(mdata->startdir);
    }
    else if (sel == "music_setup")
        ;
}

void runMenu(QString themedir, QSqlDatabase *db, 
             QString paths, QValueList<Metadata> &playlist, QString startdir)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), theprefix,
                                      "musicmenu.xml");
       
    MusicData data;
   
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

    globalsettings = new Settings();

    globalsettings->LoadSettingsFiles("mythmusic-settings.txt", theprefix);
    globalsettings->LoadSettingsFiles("theme.txt", theprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", theprefix);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    db->setDatabaseName(globalsettings->GetSetting("DBName"));
    db->setUserName(globalsettings->GetSetting("DBUserName"));
    db->setPassword(globalsettings->GetSetting("DBPassword"));
    db->setHostName(globalsettings->GetSetting("DBHostName"));

    if (!db->open())
    {
        printf("couldn't open db\n");
        return -1;
    }

    CheckFreeDBServerFile();

    QString startdir = globalsettings->GetSetting("MusicLocation");

    if (startdir != "")
        SearchDir(startdir);

    QString paths = globalsettings->GetSetting("TreeLevels");
    QValueList<Metadata> playlist;

    QString themename = globalsettings->GetSetting("Theme");

    QString themedir = findThemeDir(themename, theprefix);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    LoadDefaultPlaylist(db, playlist);
    runMenu(themedir, db, paths, playlist, startdir);
    SaveDefaultPlaylist(db, playlist);

    db->close();

    delete globalsettings;

    return 0;
}
