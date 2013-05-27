// POSIX headers
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

// Qt headers
#include <QDir>
#include <QApplication>
#include <QRegExp>

// MythTV headers
#include <mythcontext.h>
#include <mythplugin.h>
#include <mythmediamonitor.h>
#include <mythdbcon.h>
#include <mythdb.h>
#include <mythpluginapi.h>
#include <mythversion.h>
#include <myththemedmenu.h>
#include <compat.h>
#include <mythuihelper.h>
#include <mythprogressdialog.h>
#include <lcddevice.h>
#include <musicmetadata.h>
#include <musicutils.h>

// MythMusic headers
#include "musicdata.h"
#include "decoder.h"
#include "cddecoder.h"
#include "playlisteditorview.h"
#include "playlistview.h"
#include "streamview.h"
#include "playlistcontainer.h"
#include "dbcheck.h"
#include "filescanner.h"
#include "musicplayer.h"
#include "config.h"
#include "mainvisual.h"
#include "generalsettings.h"
#include "playersettings.h"
#include "visualizationsettings.h"
#include "importsettings.h"
#include "ratingsettings.h"
#include "importmusic.h"

#ifdef HAVE_CDIO
#include "cdrip.h"
#endif

/**
 * \brief Work out the best CD drive to use at this time
 */
static QString chooseCD(void)
{
    if (gCDdevice.length())
        return gCDdevice;

#ifdef Q_OS_MAC
    return MediaMonitor::GetMountPath(MediaMonitor::defaultCDdevice());
#endif

    return MediaMonitor::defaultCDdevice();
}

static void loadMusic()
{
    // only do this once
    if (gMusicData->initialized)
        return;

    MSqlQuery count_query(MSqlQuery::InitCon());

    bool musicdata_exists = false;
    if (count_query.exec("SELECT COUNT(*) FROM music_songs;"))
    {
        if(count_query.next() &&
            0 != count_query.value(0).toInt())
        {
            musicdata_exists = true;
        }
    }

    QString musicDir = getMusicDirectory();

    // Only search music files if a directory was specified & there
    // is no data in the database yet (first run).  Otherwise, user
    // can choose "Setup" option from the menu to force it.
    if (!musicDir.isEmpty() && !musicdata_exists)
    {
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(musicDir);
        delete fscan;
    }

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = QObject::tr("Loading Music. Please wait ...");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");
    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = NULL;

    // Set the various track formatting modes
    MusicMetadata::setArtistAndTrackFormats();

    AllMusic *all_music = new AllMusic();

    //  Load all playlists into RAM (once!)
    PlaylistContainer *all_playlists = new PlaylistContainer(all_music);

    gMusicData->all_music = all_music;
    gMusicData->all_streams = new AllStream();
    gMusicData->all_playlists = all_playlists;

    gMusicData->initialized = true;

    while (!gMusicData->all_playlists->doneLoading() || !gMusicData->all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    gPlayer->loadStreamPlaylist();
    gPlayer->loadPlaylist();

    if (busy)
        busy->Close();

}

static void startPlayback(void)
{
    loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaylistView *view = new PlaylistView(mainStack);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startStreamPlayback(void)
{
    loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    StreamView *view = new StreamView(mainStack);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startDatabaseTree(void)
{
    loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    QString lastView = gCoreContext->GetSetting("MusicPlaylistEditorView", "tree");
    PlaylistEditorView *view = new PlaylistEditorView(mainStack, lastView);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startRipper(void)
{
    loadMusic();

#if defined HAVE_CDIO
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Ripper *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
    {
        mainStack->AddScreen(rip);
        QObject::connect(rip, SIGNAL(ripFinished()),
                     gMusicData, SLOT(reloadMusic()),
                     Qt::QueuedConnection);
    }
    else
        delete rip;
#endif
}

static void runScan(void)
{
    // if we don't have a valid start dir warn the user and give up
    if (getMusicDirectory().isEmpty())
    {
        ShowOkPopup(QObject::tr("You need to tell me where to find your music on the "
                                "'General Settings' page of MythMusic's settings pages."));
       return;
    }

    if (!QFile::exists(getMusicDirectory()))
    {
        ShowOkPopup(QObject::tr("Can't find your music directory. Have you set it correctly on the "
                                "'General Settings' page of MythMusic's settings pages?"));
       return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Scanning '%1' for music files").arg(getMusicDirectory()));

    FileScanner *fscan = new FileScanner();
    QString musicDir = getMusicDirectory();
    fscan->SearchDir(musicDir);

    // save anything that may have changed
    if (gMusicData->all_music && gMusicData->all_music->cleanOutThreads())
        gMusicData->all_music->save();

    if (gMusicData->all_playlists && gMusicData->all_playlists->cleanOutThreads())
    {
        gMusicData->all_playlists->save();
    }

    // force a complete reload of the tracks and playlists
    gPlayer->stop(true);
    delete gMusicData;

    gMusicData = new MusicData;
    loadMusic();

    delete fscan;
}

static void startImport(void)
{
    loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ImportMusicDialog *import = new ImportMusicDialog(mainStack);

    if (import->Create())
    {
        mainStack->AddScreen(import);
        QObject::connect(import, SIGNAL(importFinished()),
                gMusicData, SLOT(reloadMusic()),
                Qt::QueuedConnection);
    }
    else
        delete import;
}

static void MusicCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.toLower();
    if (sel == "music_create_playlist")
        startDatabaseTree();
    else if (sel == "music_play")
        startPlayback();
    else if (sel == "stream_play")
        startStreamPlayback();
    else if (sel == "music_rip")
    {
        startRipper();
    }
    else if (sel == "music_import")
    {
        startImport();
    }
    else if (sel == "settings_scan")
    {
        runScan();
    }
    else if (sel == "settings_general")
     {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        GeneralSettings *gs = new GeneralSettings(mainStack, "general settings");

        if (gs->Create())
            mainStack->AddScreen(gs);
        else
            delete gs;
    }
    else if (sel == "settings_player")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        PlayerSettings *ps = new PlayerSettings(mainStack, "player settings");

        if (ps->Create())
            mainStack->AddScreen(ps);
        else
            delete ps;
    }
    else if (sel == "settings_rating")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        RatingSettings *rs = new RatingSettings(mainStack, "rating settings");

        if (rs->Create())
            mainStack->AddScreen(rs);
        else
            delete rs;
    }
    else if (sel == "settings_visualization")
    {

       MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
       VisualizationSettings *vs = new VisualizationSettings(mainStack, "visualization settings");

       if (vs->Create())
           mainStack->AddScreen(vs);
        else
            delete vs;
    }
    else if (sel == "settings_import")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        ImportSettings *is = new ImportSettings(mainStack, "import settings");

        if (is->Create())
            mainStack->AddScreen(is);
        else
            delete is;
    }
}

static int runMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "music menu");

    diag->setCallback(MusicCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (LCD *lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
        return 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete diag;
        return -1;
    }
}

static void runMusicPlayback(void)
{
    GetMythUI()->AddCurrentLocation("playmusic");
    startPlayback();
    GetMythUI()->RemoveCurrentLocation();
}

static void runMusicStreamPlayback(void)
{
    GetMythUI()->AddCurrentLocation("streammusic");
    startStreamPlayback();
    GetMythUI()->RemoveCurrentLocation();
}

static void runMusicSelection(void)
{
    GetMythUI()->AddCurrentLocation("musicplaylists");
    startDatabaseTree();
    GetMythUI()->RemoveCurrentLocation();
}

static void runRipCD(void)
{
    loadMusic();

#if defined HAVE_CDIO
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Ripper *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
        mainStack->AddScreen(rip);
    else
        delete rip;

    QObject::connect(rip, SIGNAL(ripFinished()),
                     gMusicData, SLOT(reloadMusic()),
                     Qt::QueuedConnection);
#endif
}

static void showMiniPlayer(void)
{
    if (!gMusicData->all_music)
        return;

    // only show the miniplayer if there isn't already a client attached
    if (!gPlayer->hasClient())
        gPlayer->showMiniPlayer();
}

static void handleMedia(MythMediaDevice *cd)
{
    if (!cd)
        return;

    // Note that we should deal with other disks that may contain music.
    // e.g. MEDIATYPE_MMUSIC or MEDIATYPE_MIXED
    LOG(VB_MEDIA, LOG_NOTICE, QString("Ignoring changed media event of type: %1")
        .arg(MythMediaDevice::MediaTypeString(cd->getMediaType())));
}

static void handleCDMedia(MythMediaDevice *cd)
{
#ifdef HAVE_CDIO

    if (!cd)
        return;

    LOG(VB_MEDIA, LOG_NOTICE, "Got a media changed event");

    QString newDevice;

    // save the device if valid
    if (cd->isUsable())
    {
#ifdef Q_OS_MAC
        newDevice = cd->getMountPath();
#else
        newDevice = cd->getDevicePath();
#endif

        gCDdevice = newDevice;
        LOG(VB_MEDIA, LOG_INFO, "MythMusic: Storing CD device " + gCDdevice);
    }
    else
    {
        LOG(VB_MEDIA, LOG_INFO, "Device is not usable clearing cd data");

        // device is not usable so remove any existing CD tracks
        if (gMusicData->all_music)
        {
            gMusicData->all_music->clearCDData();
            gMusicData->all_playlists->getActive()->removeAllCDTracks();
        }

        gPlayer->activePlaylistChanged(-1, true);
        gPlayer->sendCDChangedEvent();

        return;
    }

    if (!gMusicData->initialized)
        loadMusic();

    // remove any existing CD tracks
    if (gMusicData->all_music)
    {
        gMusicData->all_music->clearCDData();
        gMusicData->all_playlists->getActive()->removeAllCDTracks();
    }

    // find any new cd tracks
    CdDecoder *decoder = new CdDecoder("cda", NULL, NULL);
    decoder->setDevice(newDevice);

    int tracks = decoder->getNumTracks();
    bool setTitle = false;

    for (int trackNo = 1; trackNo <= tracks; trackNo++)
    {
        MusicMetadata *track = decoder->getMetadata(trackNo);
        if (track)
        {
            gMusicData->all_music->addCDTrack(*track);

            if (!setTitle)
            {

                QString parenttitle = " ";
                if (track->FormatArtist().length() > 0)
                {
                    parenttitle += track->FormatArtist();
                    parenttitle += " ~ ";
                }

                if (track->Album().length() > 0)
                    parenttitle += track->Album();
                else
                {
                    parenttitle = " " + QObject::tr("Unknown");
                    LOG(VB_GENERAL, LOG_INFO, "Couldn't find your "
                    " CD. It may not be in the freedb database.\n"
                    "    More likely, however, is that you need to delete\n"
                    "    ~/.cddb and ~/.cdserverrc and restart MythMusic.");
                }

                gMusicData->all_music->setCDTitle(parenttitle);
                setTitle = true;
            }

            delete track;
        }
    }

    gPlayer->sendCDChangedEvent();

    delete decoder;

    // if the AutoPlayCD setting is set we remove all the existing tracks
    // from the playlist and replace them with the new CD tracks found
    if (gCoreContext->GetNumSetting("AutoPlayCD", 0))
    {
        gMusicData->all_playlists->getActive()->removeAllTracks();

        QList<int> songList;

        for (int x = 1; x <= gMusicData->all_music->getCDTrackCount(); x++)
        {
            MusicMetadata *mdata = gMusicData->all_music->getCDMetadata(x);
            if (mdata)
                songList.append((mdata)->ID());
        }

        if (songList.count())
        {
            gMusicData->all_playlists->getActive()->fillSonglistFromList(
                    songList, true, PL_REPLACE, 0);
            gPlayer->setCurrentTrackPos(0);
        }
    }
    else
    {
        // don't show the music screen if AutoPlayCD is off
        return;
    }

    // if there is no music screen showing show the Playlist view
    if (!gPlayer->hasClient())
    {
        // make sure we start playing from the first track
        gCoreContext->SaveSetting("MusicBookmark", 0);
        gCoreContext->SaveSetting("MusicBookmarkPosition", 0);

        runMusicPlayback();
    }
#else
    LOG(VB_GENERAL, LOG_NOTICE, "MythMusic got a media changed event"
                                "but cdio support is not compiled in");
#endif
}

static void setupKeys(void)
{
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Play music"),
        "", "", runMusicPlayback);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Select music playlists"),
        "", "", runMusicSelection);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Play radio stream"),
        "", "", runMusicStreamPlayback);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Rip CD"),
        "", "", runRipCD);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Scan music"),
        "", "", runScan);
    REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "Show Music Miniplayer"),
        "", "", showMiniPlayer, false);

    REG_KEY("Music", "NEXTTRACK",  QT_TRANSLATE_NOOP("MythControls",
        "Move to the next track"),     ">,.,Z,End");
    REG_KEY("Music", "PREVTRACK",  QT_TRANSLATE_NOOP("MythControls",
        "Move to the previous track"), ",,<,Q,Home");
    REG_KEY("Music", "FFWD",       QT_TRANSLATE_NOOP("MythControls",
        "Fast forward"),               "PgDown");
    REG_KEY("Music", "RWND",       QT_TRANSLATE_NOOP("MythControls",
        "Rewind"),                     "PgUp");
    REG_KEY("Music", "PAUSE",      QT_TRANSLATE_NOOP("MythControls",
        "Pause/Start playback"),       "P");
    REG_KEY("Music", "PLAY",       QT_TRANSLATE_NOOP("MythControls",
        "Start playback"),             "");
    REG_KEY("Music", "STOP",       QT_TRANSLATE_NOOP("MythControls",
        "Stop playback"),              "O");
    REG_KEY("Music", "VOLUMEDOWN", QT_TRANSLATE_NOOP("MythControls",
        "Volume down"),       "[,{,F10,Volume Down");
    REG_KEY("Music", "VOLUMEUP",   QT_TRANSLATE_NOOP("MythControls",
        "Volume up"),         "],},F11,Volume Up");
    REG_KEY("Music", "MUTE",       QT_TRANSLATE_NOOP("MythControls",
        "Mute"),              "|,\\,F9,Volume Mute");
    REG_KEY("Music", "TOGGLEUPMIX",QT_TRANSLATE_NOOP("MythControls",
        "Toggle audio upmixer"),    "Ctrl+U");
    REG_KEY("Music", "CYCLEVIS",   QT_TRANSLATE_NOOP("MythControls",
        "Cycle visualizer mode"),      "6");
    REG_KEY("Music", "BLANKSCR",   QT_TRANSLATE_NOOP("MythControls",
        "Blank screen"),               "5");
    REG_KEY("Music", "THMBUP",     QT_TRANSLATE_NOOP("MythControls",
        "Increase rating"),            "9");
    REG_KEY("Music", "THMBDOWN",   QT_TRANSLATE_NOOP("MythControls",
        "Decrease rating"),            "7");
    REG_KEY("Music", "REFRESH",    QT_TRANSLATE_NOOP("MythControls",
        "Refresh music tree"),         "8");
    REG_KEY("Music", "SPEEDUP",    QT_TRANSLATE_NOOP("MythControls",
        "Increase Play Speed"),   "W");
    REG_KEY("Music", "SPEEDDOWN",  QT_TRANSLATE_NOOP("MythControls",
        "Decrease Play Speed"),   "X");
    REG_KEY("Music", "MARK",       QT_TRANSLATE_NOOP("MythControls",
        "Toggle track selection"), "T");
    REG_KEY("Music", "TOGGLESHUFFLE", QT_TRANSLATE_NOOP("MythControls",
        "Toggle shuffle mode"),    "");
    REG_KEY("Music", "TOGGLEREPEAT",  QT_TRANSLATE_NOOP("MythControls",
        "Toggle repeat mode"),     "");

    // switch to view key bindings
    REG_KEY("Music", "SWITCHTOPLAYLIST",              QT_TRANSLATE_NOOP("MythControls",
        "Switch to the current playlist view"), "");
    REG_KEY("Music", "SWITCHTOPLAYLISTEDITORTREE",    QT_TRANSLATE_NOOP("MythControls",
        "Switch to the playlist editor tree view"), "");
    REG_KEY("Music", "SWITCHTOPLAYLISTEDITORGALLERY", QT_TRANSLATE_NOOP("MythControls",
        "Switch to the playlist editor gallery view"), "");
    REG_KEY("Music", "SWITCHTOSEARCH",                QT_TRANSLATE_NOOP("MythControls",
        "Switch to the search view"), "");
    REG_KEY("Music", "SWITCHTOVISUALISER",            QT_TRANSLATE_NOOP("MythControls",
        "Switch to the fullscreen visualiser view"), "");
    REG_KEY("Music", "SWITCHTORADIO",                 QT_TRANSLATE_NOOP("MythControls",
        "Switch to the radio stream view"), "");

    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 1/2"), "", "", handleCDMedia,
        MEDIATYPE_AUDIO | MEDIATYPE_MIXED, QString::null);
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 2/2"), "", "", handleMedia,
        MEDIATYPE_MMUSIC, "mp3,mp2,ogg,oga,flac,wma,wav,ac3,"
                          "oma,omg,atp,ra,dts,aac,m4a,aa3,tta,"
                          "mka,aiff,swa,wv");
}

int mythplugin_init(const char *libversion)
{
    if (!gCoreContext->TestPluginVersion("mythmusic", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    bool upgraded = UpgradeMusicDatabaseSchema();
    gCoreContext->ActivateSettingsCache(true);

    if (!upgraded)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Couldn't upgrade music database schema, exiting.");
        return -1;
    }

    setupKeys();

    gPlayer = new MusicPlayer(NULL);
    gMusicData = new MusicData();

    return 0;
}


int mythplugin_run(void)
{
    return runMenu("musicmenu.xml");
}

int mythplugin_config(void)
{
    return runMenu("music_settings.xml");
}

void mythplugin_destroy(void)
{
    gPlayer->stop(true);

    // TODO these should be saved when they are changed
    // Automagically save all playlists and metadata (ratings) that have changed
    if (gMusicData->all_music && gMusicData->all_music->cleanOutThreads())
    {
        gMusicData->all_music->save();
    }

    if (gMusicData->all_playlists && gMusicData->all_playlists->cleanOutThreads())
    {
        gMusicData->all_playlists->save();
    }

    delete gPlayer;

    delete gMusicData;
}
