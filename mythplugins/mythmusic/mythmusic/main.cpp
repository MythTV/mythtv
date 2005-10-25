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
#include "globalsettings.h"
#include "dbcheck.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/mythmedia.h>
#include <mythtv/mythdbcon.h>

void CheckFreeDBServerFile(void)
{
    char filename[1024];
    if (getenv("HOME") == NULL)
    {
        cerr << "main.o: You don't have a HOME environment variable. CD lookup will almost certainly not work." << endl;
        return;
    }
    sprintf(filename, "%s/.cdserverrc", getenv("HOME"));

    QFile file(filename);

    if (!file.exists())
    {
        struct cddb_conf cddbconf;
        struct cddb_serverlist list;
        struct cddb_host proxy_host;

        memset(&cddbconf, 0, sizeof(cddbconf));

        cddbconf.conf_access = CDDB_ACCESS_REMOTE;
        list.list_len = 1;
        strncpy(list.list_host[0].host_server.server_name,
                "freedb.freedb.org", 256);
        strncpy(list.list_host[0].host_addressing, "~cddb/cddb.cgi", 256);
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
        Metadata *data = decoder->getMetadata();
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

typedef QMap <QString, MusicFileLocation> MusicLoadedMap;

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

void SavePending(int pending)
{
    //  Temporary Hack until mythmusic
    //  has a proper settings/setup

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM settings "
                  "WHERE value = :LASTPUSH "
                  "AND hostname = :HOST ;");
    query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
    query.bindValue(":HOST", gContext->GetHostName());

    if (query.exec() && query.size() == 0)
    {
        //  first run from this host / recent version
        query.prepare("INSERT INTO settings (value,data,hostname) VALUES "
                         "(:LASTPUSH, :DATA, :HOST );");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":DATA", pending);
        query.bindValue(":HOST", gContext->GetHostName());
       
        query.exec(); 
    }
    else if (query.size() == 1)
    {
        //  ah, just right
        query.prepare("UPDATE settings SET data = :DATA WHERE "
                         "WHERE value = :LASTPUSH "
                         "AND hostname = :HOST ;");
        query.bindValue(":DATA", pending);
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":HOST", gContext->GetHostName());

        query.exec();
    }                       
    else
    {
        //  correct thor's diabolical plot to 
        //  consume all table space

        query.prepare("DELETE FROM settings WHERE "
                         "WHERE value = :LASTPUSH "
                         "AND hostname = :HOST ;");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":HOST", gContext->GetHostName());
        query.exec(); 
        
        query.prepare("INSERT INTO settings (value,data,hostname) VALUES "
                         "(:LASTPUSH, :DATA, :HOST );");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":DATA", pending);
        query.bindValue(":HOST", gContext->GetHostName());

        query.exec();
    }
}

void SearchDir(QString &directory)
{
    MusicLoadedMap music_files;
    MusicLoadedMap::Iterator iter;

    BuildFileList(directory, music_files);

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT filename FROM musicmetadata "
                    "WHERE filename NOT LIKE ('%://%');");

    int counter = 0;

    MythProgressDialog *file_checking;
    file_checking = new MythProgressDialog(QObject::tr("Searching for music files"),
                                           query.numRowsAffected());

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString name = directory + QString::fromUtf8(query.value(0).toString());
            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                    music_files.remove(iter);
                else
                    music_files[name] = kDatabase;
            }
            file_checking->setProgress(++counter);
        }
    }

    file_checking->Close();
    delete file_checking;

    file_checking = new MythProgressDialog(QObject::tr("Updating music database"), 
                                           music_files.size());

    QRegExp quote_regex("\"");
    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if (*iter == kFileSystem)
        {
            CheckFile(iter.key());
        }
        else if (*iter == kDatabase)
        {
            QString name(iter.key());
            name.remove(0, directory.length());

            query.prepare("DELETE FROM musicmetadata WHERE "
                          "filename = :NAME ;");
            query.bindValue(":NAME", name.utf8());
            query.exec();
        }

        file_checking->setProgress(++counter);
    }
    file_checking->Close();
    delete file_checking;
}

void startPlayback(PlaylistsContainer *all_playlists, AllMusic *all_music)
{
    PlaybackBoxMusic *pbb = new PlaybackBoxMusic(gContext->GetMainWindow(),
                                                 "music_play", "music-", 
                                                 all_playlists, all_music,
                                                 "music_playback");
    qApp->unlock();
    pbb->exec();
    qApp->lock();

    pbb->stop();

    qApp->processEvents();

    delete pbb;
}

void startDatabaseTree(PlaylistsContainer *all_playlists, AllMusic *all_music)
{
    DatabaseBox dbbox(all_playlists, all_music, gContext->GetMainWindow(),
                      "music_select", "music-", "music database");
    qApp->unlock();
    dbbox.exec();
    qApp->lock();
}

bool startRipper(void)
{
    Ripper rip(gContext->GetMainWindow(), "cd ripper");

    qApp->unlock();
    rip.exec();
    qApp->lock();

    if (rip.somethingWasRipped())
      return true;
    
    return false;
}

struct MusicData
{
    QString paths;
    QString startdir;
    PlaylistsContainer *all_playlists;
    AllMusic *all_music;
};

void MusicCallback(void *data, QString &selection)
{
    MusicData *mdata = (MusicData *)data;

    QString sel = selection.lower();

    if (sel == "music_create_playlist")
        startDatabaseTree(mdata->all_playlists, mdata->all_music);
    else if (sel == "music_play")
        startPlayback(mdata->all_playlists, mdata->all_music);
    else if (sel == "music_rip")
    {
        if (startRipper())
        {
            // If startRipper returns true, then new data should be present

            //  Reconcile with the database
            SearchDir(mdata->startdir);
            //  Tell the metadata to reset itself
            mdata->all_music->resync();
            mdata->all_playlists->postLoad();
        }
    }
    else if (sel == "settings_scan")
    {
        if ("" != mdata->startdir)
        {
            SearchDir(mdata->startdir);
            mdata->all_music->resync();
            mdata->all_playlists->postLoad();
        }
    }
    else if (sel == "music_set_general")
    {
        MusicGeneralSettings settings;
        settings.exec();
    }
    else if (sel == "music_set_player")
    {
        MusicPlayerSettings settings;
        settings.exec();
    }
    else if (sel == "music_set_ripper")
    {
        MusicRipperSettings settings;
        settings.exec();
    }
}

void runMenu(MusicData *mdata, QString which_menu)
{
    QString themedir = gContext->GetThemeDir();
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu, 
                                      gContext->GetMainWindow(), "music menu");

    diag->setCallback(MusicCallback, mdata);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (class LCD * lcd = LCD::Get())
            lcd->switchToTime();
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runMusicPlayback(void);
void runMusicSelection(void);
void runRipCD(void);


void handleMedia(MythMediaDevice *) 
{
    mythplugin_run();
}

void setupKeys(void)
{
    REG_JUMP("Play music",             "", "", runMusicPlayback);
    REG_JUMP("Select music playlists", "", "", runMusicSelection);
    REG_JUMP("Rip CD",                 "", "", runRipCD);

    REG_KEY("Music", "DELETE",     "Delete track from playlist", "D");
    REG_KEY("Music", "NEXTTRACK",  "Move to the next track",     ">,.,Z,End");
    REG_KEY("Music", "PREVTRACK",  "Move to the previous track", ",,<,Q,Home");
    REG_KEY("Music", "FFWD",       "Fast forward",               "PgDown");
    REG_KEY("Music", "RWND",       "Rewind",                     "PgUp");
    REG_KEY("Music", "PAUSE",      "Pause/Start playback",       "P");
    REG_KEY("Music", "STOP",       "Stop playback",              "O");
    REG_KEY("Music", "VOLUMEDOWN", "Volume down",                "[,{,F10");
    REG_KEY("Music", "VOLUMEUP",   "Volume up",                  "],},F11");
    REG_KEY("Music", "MUTE",       "Mute",                       "|,\\,F9");
    REG_KEY("Music", "CYCLEVIS",   "Cycle visualizer mode",      "6");
    REG_KEY("Music", "BLANKSCR",   "Blank screen",               "5");
    REG_KEY("Music", "THMBUP",     "Increase rating",            "9");
    REG_KEY("Music", "THMBDOWN",   "Decrease rating",            "7");
    REG_KEY("Music", "REFRESH",    "Refresh music tree",         "8");
    REG_KEY("Music", "FILTER",     "Filter All My Music",        "F");
    REG_KEY("Music", "JUMPA",      "Jump to the letter A",       "Ctrl+A");
    REG_KEY("Music", "JUMPB",      "Jump to the letter B",       "Ctrl+B");
    REG_KEY("Music", "JUMPC",      "Jump to the letter C",       "Ctrl+C");
    REG_KEY("Music", "JUMPD",      "Jump to the letter D",       "Ctrl+D");
    REG_KEY("Music", "JUMPE",      "Jump to the letter E",       "Ctrl+E");
    REG_KEY("Music", "JUMPF",      "Jump to the letter F",       "Ctrl+F");
    REG_KEY("Music", "JUMPG",      "Jump to the letter G",       "Ctrl+G");
    REG_KEY("Music", "JUMPH",      "Jump to the letter H",       "Ctrl+H");
    REG_KEY("Music", "JUMPI",      "Jump to the letter I",       "Ctrl+I");
    REG_KEY("Music", "JUMPJ",      "Jump to the letter J",       "Ctrl+J");
    REG_KEY("Music", "JUMPK",      "Jump to the letter K",       "Ctrl+K");
    REG_KEY("Music", "JUMPL",      "Jump to the letter L",       "Ctrl+L");
    REG_KEY("Music", "JUMPM",      "Jump to the letter M",       "Ctrl+M");
    REG_KEY("Music", "JUMPN",      "Jump to the letter N",       "Ctrl+N");
    REG_KEY("Music", "JUMPO",      "Jump to the letter O",       "Ctrl+O");
    REG_KEY("Music", "JUMPP",      "Jump to the letter P",       "Ctrl+P");
    REG_KEY("Music", "JUMPQ",      "Jump to the letter Q",       "Ctrl+Q");
    REG_KEY("Music", "JUMPR",      "Jump to the letter R",       "Ctrl+R");
    REG_KEY("Music", "JUMPS",      "Jump to the letter S",       "Ctrl+S");
    REG_KEY("Music", "JUMPT",      "Jump to the letter T",       "Ctrl+T");
    REG_KEY("Music", "JUMPU",      "Jump to the letter U",       "Ctrl+U");
    REG_KEY("Music", "JUMPV",      "Jump to the letter V",       "Ctrl+V");
    REG_KEY("Music", "JUMPW",      "Jump to the letter W",       "Ctrl+W");
    REG_KEY("Music", "JUMPX",      "Jump to the letter X",       "Ctrl+X");
    REG_KEY("Music", "JUMPY",      "Jump to the letter Y",       "Ctrl+Y");
    REG_KEY("Music", "JUMPZ",      "Jump to the letter Z",       "Ctrl+Z");

    REG_MEDIA_HANDLER("MythMusic Media Handler", "", "", handleMedia,
                      MEDIATYPE_AUDIO | MEDIATYPE_MIXED);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythmusic", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    UpgradeMusicDatabaseSchema();

    MusicGeneralSettings general;
    general.load();
    general.save();
    MusicPlayerSettings settings;
    settings.load();
    settings.save();
    MusicRipperSettings ripper;
    ripper.load();
    ripper.save();

    setupKeys();

    Decoder::SetLocationFormatUseTags();

    return 0;
}

static void preMusic(MusicData *mdata)
{
    srand(time(NULL));

    CheckFreeDBServerFile();


    MSqlQuery count_query(MSqlQuery::InitCon());
    count_query.exec("SELECT COUNT(*) FROM musicmetadata;");

    bool musicdata_exists = false;
    if (count_query.isActive())
    {
        if(count_query.next() && 
           0 != count_query.value(0).toInt())
        {
            musicdata_exists = true;
        }
    }

    //  Load all available info about songs (once!)
    QString startdir = gContext->GetSetting("MusicLocation");
    startdir = QDir::cleanDirPath(startdir);
    if (!startdir.endsWith("/"));
        startdir += "/";

    Metadata::SetStartdir(startdir);

    Decoder::SetLocationFormatUseTags();

    // Only search music files if a directory was specified & there
    // is no data in the database yet (first run).  Otherwise, user
    // can choose "Setup" option from the menu to force it.
    if (startdir != "" && !musicdata_exists)
        SearchDir(startdir);

    QString paths = gContext->GetSetting("TreeLevels");

    // Set the various track formatting modes
    Metadata::setArtistAndTrackFormats();
    
    AllMusic *all_music = new AllMusic(paths, startdir);

    //  Load all playlists into RAM (once!)
    PlaylistsContainer *all_playlists = new PlaylistsContainer(all_music, gContext->GetHostName());

    mdata->paths = paths;
    mdata->startdir = startdir;
    mdata->all_playlists = all_playlists;
    mdata->all_music = all_music;
}

static void postMusic(MusicData *mdata)
{
    // Automagically save all playlists and metadata (ratings) that have changed

    if (mdata->all_music->cleanOutThreads())
    {
        mdata->all_music->save();
    }

    if (mdata->all_playlists->cleanOutThreads())
    {
        mdata->all_playlists->save();
        int x = mdata->all_playlists->getPending();
        SavePending(x);
    }

    delete mdata->all_music;
    delete mdata->all_playlists;
}

int mythplugin_run(void)
{
    MusicData mdata;

    preMusic(&mdata);
    runMenu(&mdata, "musicmenu.xml");
    postMusic(&mdata);

    return 0;
}

int mythplugin_config(void)
{
    MusicData mdata;
    mdata.paths = gContext->GetSetting("TreeLevels");
    mdata.startdir = gContext->GetSetting("MusicLocation");
    mdata.startdir = QDir::cleanDirPath(mdata.startdir);
    
    if (!mdata.startdir.endsWith("/"))
        mdata.startdir += "/";

    Metadata::SetStartdir(mdata.startdir);

    Decoder::SetLocationFormatUseTags();

    runMenu(&mdata, "music_settings.xml");

    return 0;
}

void runMusicPlayback(void)
{
    MusicData mdata;

    preMusic(&mdata);
    startPlayback(mdata.all_playlists, mdata.all_music);
    postMusic(&mdata);
}



void runMusicSelection(void)
{
    MusicData mdata;

    preMusic(&mdata);
    startDatabaseTree(mdata.all_playlists, mdata.all_music);
    postMusic(&mdata);
}

void runRipCD(void)
{
    MusicData mdata;

    preMusic(&mdata);
    if (startRipper())
    {
        // if startRipper returns true, then new files should be present
        // so we should look for them.
        SearchDir(mdata.startdir);
        mdata.all_music->resync();
        mdata.all_playlists->postLoad();
    }
    postMusic(&mdata);
}
