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
#include "themedmenu.h"
#include "menubox.h"
#include "cdrip.h"
#include "settings.h"

Settings *settings;

char theprefix[] = "/usr/local";

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

bool runMenu(bool usetheme, QString themedir, QSqlDatabase *db, 
             QString paths, QValueList<Metadata> &playlist, QString startdir)
{
    if (usetheme)
    {
        ThemedMenu *diag = new ThemedMenu(themedir.ascii(), "music.menu");
        
        if (diag->foundTheme())
        {
            diag->Show();
            diag->exec();
            
            QString sel = diag->getSelection().lower();

            bool retval = true;

            if (sel == "music_create_playlist")
                startDatabaseTree(db, paths, &playlist);
            else if (sel == "music_play")
                startPlayback(db, &playlist);
            else if (sel == "music_rip")
            {
                startRipper(db);
                SearchDir(startdir); 
            }
            else if (sel == "music_setup")
                ;
            else
                retval = false;

            delete diag;

            return retval;
        }
    }

    MenuBox diag("MythMusic");

    diag.AddButton("Make a Playlist");
    diag.AddButton("Play Music");
    diag.AddButton("Import a CD");

    diag.Show();
       
    int result = diag.exec();

    bool retval = true;
      
    switch (result)
    {   
        case 1: startDatabaseTree(db, paths, &playlist); break;
        case 2: startPlayback(db, &playlist); break;
        case 3: { startRipper(db); SearchDir(startdir); } break;
        default: break;
    }
    if (result == 0)
        retval = false;

    return retval;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    settings = new Settings();

    settings->LoadSettingsFiles("mythmusic-settings.txt", theprefix);
    settings->LoadSettingsFiles("theme.txt", theprefix);
    settings->LoadSettingsFiles("mysql.txt", theprefix);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    db->setDatabaseName("mythconverg");
    db->setUserName("mythtv");
    db->setPassword("mythtv");
    db->setHostName("localhost");

    if (!db->open())
    {
        printf("couldn't open db\n");
        return -1;
    }

    CheckFreeDBServerFile();

    QString startdir = settings->GetSetting("MusicLocation");

    if (startdir != "")
        SearchDir(startdir);

    QString paths = settings->GetSetting("TreeLevels");
    QValueList<Metadata> playlist;

    QString themename = settings->GetSetting("Theme");

    QString themedir = findThemeDir(themename, theprefix);
    bool usetheme = true;
    if (themedir == "")
        usetheme = false;

    while (1)
    {
        if (!runMenu(usetheme, themedir, db, paths, playlist, startdir))
            break;
    }

    db->close();

    delete settings;

    return 0;
}
