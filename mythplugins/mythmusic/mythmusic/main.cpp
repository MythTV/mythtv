#include <qdir.h>
#include <iostream>
#include <map>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qregexp.h>
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

MythContext *gContext;

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

Decoder *getDecoder(const QString &filename)
{
    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);
    return decoder;
}

void CheckFile(const QString &filename)
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

enum MusicFileLocation
{
    kFileSystem,
    kDatabase,
    kBoth
};

typedef map<QString, MusicFileLocation> MusicLoadedMap;

void BuildFileList(QString &directory, MusicLoadedMap &music_files)
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
            BuildFileList(filename, music_files);
        else
            music_files[filename] = kFileSystem;
    }
}

void SearchDir(QString &directory)
{
    MusicLoadedMap music_files;
    MusicLoadedMap::iterator iter;

    BuildFileList(directory, music_files);

    QSqlQuery query;

    query.exec("SELECT filename FROM musicmetadata;");
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString name = query.value(0).toString();
            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                    (*iter).second = kBoth;
                else
                    music_files[name] = kDatabase;
            }
        }
    }

    QRegExp quote_regex("\"");
    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if ((*iter).second == kFileSystem)
        {
            CheckFile((*iter).first);
        }
        else if ((*iter).second == kDatabase)
        {
            QString name((*iter).first);
            name.replace(quote_regex, "\"\"");

            QString querystr = QString("DELETE FROM musicmetadata WHERE "
                                       "filename=\"%1\"").arg(name);
            query.exec(querystr);
        }
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

void runMenu(QString themedir, QSqlDatabase *db, QString paths,
             QValueList<Metadata> &playlist, QString startdir)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), 
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
		gContext->LCDswitchToTime();
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
    srand(time(NULL));

    QApplication a(argc, argv);

    gContext = new MythContext();

    gContext->LoadSettingsFiles("mythmusic-settings.txt");

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

    CheckFreeDBServerFile();

    QString startdir = gContext->GetSetting("MusicLocation");

    if (startdir != "")
        SearchDir(startdir);

    QString paths = gContext->GetSetting("TreeLevels");
    QValueList<Metadata> playlist;

    QString themename = gContext->GetSetting("Theme");
    QString themedir = gContext->FindThemeDir(themename);

    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    QString lcd_host = gContext->GetSetting("LCDHost");
    QString lcd_port = gContext->GetSetting("LCDPort");
    int lcd_port_number = lcd_port.toInt();
    if (lcd_host.length() > 0 && lcd_port_number > 1024)
    {
        gContext->LCDconnectToHost(lcd_host, lcd_port_number);
    }

    LoadDefaultPlaylist(db, playlist);
    runMenu(themedir, db, paths, playlist, startdir);
    SaveDefaultPlaylist(db, playlist);

    db->close();

    delete gContext;

    return 0;
}
