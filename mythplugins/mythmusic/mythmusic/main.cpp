// POSIX headers
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

// Qt headers
#include <QDir>
#include <QApplication>
#include <QRegExp>
#include <QScopedPointer>

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
#include <musicfilescanner.h>

// MythMusic headers
#include "musicdata.h"
#include "decoder.h"
#include "cddecoder.h"
#include "playlisteditorview.h"
#include "playlistview.h"
#include "streamview.h"
#include "playlistcontainer.h"
#include "dbcheck.h"
#include "musicplayer.h"
#include "config.h"
#include "mainvisual.h"
#include "generalsettings.h"
#include "playersettings.h"
#include "visualizationsettings.h"
#include "importsettings.h"
#include "ratingsettings.h"
#include "importmusic.h"
#include "metaio.h"

#ifdef HAVE_CDIO
#include "cdrip.h"
#endif

#if defined HAVE_CDIO
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
#endif

/// checks we have at least one music directory in the 'Music' storage group
static bool checkStorageGroup(void)
{
    // get a list of hosts with a directory defined for the 'Music' storage group
    QStringList hostList;
    MSqlQuery query(MSqlQuery::InitCon());
    QString sql = "SELECT DISTINCT hostname "
                  "FROM storagegroup "
                  "WHERE groupname = 'Music'";
    if (!query.exec(sql) || !query.isActive())
        MythDB::DBError("checkStorageGroup get host list", query);
    else
    {
        while(query.next())
        {
            hostList.append(query.value(0).toString());
        }
    }

    if (hostList.isEmpty())
    {
        ShowOkPopup(qApp->translate("(MythMusicMain)",
                                    "No directories found in the 'Music' storage group. "
                                    "Please run mythtv-setup on the backend machine to add one."));
       return false;
    }

    // get a list of hosts with a directory defined for the 'MusicArt' storage group
    hostList.clear();
    sql = "SELECT DISTINCT hostname "
                  "FROM storagegroup "
                  "WHERE groupname = 'MusicArt'";
    if (!query.exec(sql) || !query.isActive())
        MythDB::DBError("checkStorageGroup get host list", query);
    else
    {
        while(query.next())
        {
            hostList.append(query.value(0).toString());
        }
    }

    if (hostList.isEmpty())
    {
        ShowOkPopup(qApp->translate("(MythMusicMain)",
                                    "No directories found in the 'MusicArt' storage group. "
                                    "Please run mythtv-setup on the backend machine to add one."));
       return false;
    }

    return true;
}

/// checks we have some tracks available
static bool checkMusicAvailable(void)
{
    MSqlQuery count_query(MSqlQuery::InitCon());
    bool foundMusic = false;
    if (count_query.exec("SELECT COUNT(*) FROM music_songs;"))
    {
        if(count_query.next() &&
            0 != count_query.value(0).toInt())
        {
            foundMusic = true;
        }
    }

    if (!foundMusic)
    {
        ShowOkPopup(qApp->translate("(MythMusicMain)",
                                    "No music has been found.\n"
                                    "Please select 'Scan For New Music' "
                                    "to perform a scan for music."));
    }

    return foundMusic;
}

static void startPlayback(void)
{
    if (!checkStorageGroup() || !checkMusicAvailable())
        return;

    gMusicData->loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaylistView *view = new PlaylistView(mainStack, NULL);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startStreamPlayback(void)
{
    gMusicData->loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    StreamView *view = new StreamView(mainStack, NULL);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startDatabaseTree(void)
{
    if (!checkStorageGroup() || !checkMusicAvailable())
        return;

    gMusicData->loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    QString lastView = gCoreContext->GetSetting("MusicPlaylistEditorView", "tree");
    PlaylistEditorView *view = new PlaylistEditorView(mainStack, NULL, lastView);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startRipper(void)
{
#if defined HAVE_CDIO
    if (!checkStorageGroup())
        return;

    gMusicData->loadMusic();

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

#else
    ShowOkPopup(qApp->translate("(MythMusicMain)",
                                "MythMusic hasn't been built with libcdio "
                                "support so ripping CDs is not possible"));
#endif
}

static void runScan(void)
{
    if (!checkStorageGroup())
        return;

    LOG(VB_GENERAL, LOG_INFO, "Scanning for music files");

    gMusicData->scanMusic();
}

static void startImport(void)
{
    if (!checkStorageGroup())
        return;

    gMusicData->loadMusic();

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

// these point to the the mainmenu callback if found
static void (*m_callback)(void *, QString &) = NULL;
static void *m_callbackdata = NULL;

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
    else
    {
        // if we have found the mainmenu callback
        // pass the selection on to it
        if (m_callback && m_callbackdata)
            m_callback(m_callbackdata, selection);
    }
}

static int runMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    // find the 'mainmenu' MythThemedMenu so we can use the callback from it
    MythThemedMenu *mainMenu = NULL;
    QObject *parentObject = GetMythMainWindow()->GetMainStack()->GetTopScreen();

    while (parentObject)
    {
        MythThemedMenu *menu = dynamic_cast<MythThemedMenu *>(parentObject);

        if (menu && menu->objectName() == "mainmenu")
        {
            mainMenu = menu;
            break;
        }

        parentObject = parentObject->parent();
    }

    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "music menu");

    // save the callback from the main menu
    if (mainMenu)
        mainMenu->getCallback(&m_callback, &m_callbackdata);

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
    gMusicData->loadMusic();

#if defined HAVE_CDIO
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Ripper *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
        mainStack->AddScreen(rip);
    else
    {
        delete rip;
        return;
    }

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

static QStringList GetMusicFilter()
{
    QString filt = MetaIO::ValidFileExtensions;
    filt.replace(".", "*.");
    return filt.split('|');
}

static QStringList BuildFileList(const QString &dir, const QStringList &filters)
{
    QStringList ret;

    QDir d(dir);
    if (!d.exists())
        return ret;

    d.setNameFilters(filters);
    d.setFilter(QDir::Files       | QDir::AllDirs |
                QDir::NoSymLinks  | QDir::Readable |
                QDir::NoDotAndDotDot);
    d.setSorting(QDir::Name | QDir::DirsLast);

    QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
        return ret;

    for(QFileInfoList::const_iterator it = list.begin(); it != list.end(); ++it)
    {
        const QFileInfo &fi = *it;
        if (fi.isDir())
        {
            ret += BuildFileList(fi.absoluteFilePath(), filters);
            qApp->processEvents();
        }
        else
        {
            ret << fi.absoluteFilePath();
        }
    }
    return ret;
}

static void handleMedia(MythMediaDevice *cd)
{
    static QString s_mountPath;

    if (!cd)
        return;

    if (MEDIASTAT_MOUNTED != cd->getStatus())
    {
        if (s_mountPath != cd->getMountPath())
            return;

        LOG(VB_MEDIA, LOG_INFO, QString(
            "MythMusic: '%1' unmounted, clearing data").arg(cd->getVolumeID()));

        if (gPlayer->isPlaying() && gPlayer->getCurrentMetadata()
            && gPlayer->getCurrentMetadata()->isCDTrack())
        {
            // Now playing a track which is no longer available so stop playback
            gPlayer->stop(true);
        }

        // device is not usable so remove any existing CD tracks
        if (gMusicData->initialized)
        {
            gMusicData->all_music->clearCDData();
            gMusicData->all_playlists->getActive()->removeAllCDTracks();
        }

        gPlayer->activePlaylistChanged(-1, false);
        gPlayer->sendCDChangedEvent();

        return;
    }

    LOG(VB_MEDIA, LOG_NOTICE, QString("MythMusic: '%1' mounted on '%2'")
        .arg(cd->getVolumeID()).arg(cd->getMountPath()) );

    s_mountPath.clear();

    // don't show the music screen if AutoPlayCD is off
    if (!gCoreContext->GetNumSetting("AutoPlayCD", 0))
        return;

    if (!gMusicData->initialized)
        gMusicData->loadMusic();

    // remove any existing CD tracks
    gMusicData->all_music->clearCDData();
    gMusicData->all_playlists->getActive()->removeAllCDTracks();

    gPlayer->sendCDChangedEvent();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = qApp->translate("(MythMusicMain)",
                                      "Searching for music files...");
    MythUIBusyDialog *busy = new MythUIBusyDialog( message, popupStack,
                                                    "musicscanbusydialog");
    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
    {
        delete busy;
        busy = NULL;
    }

    // Search for music files
    QStringList trackList = BuildFileList(cd->getMountPath(), GetMusicFilter());
    LOG(VB_MEDIA, LOG_INFO, QString("MythMusic: %1 music files found")
                                .arg(trackList.count()));

    if (busy)
        busy->Close();

    if (trackList.isEmpty())
        return;

    message = qApp->translate("(MythMusicMain)", "Loading music tracks");
    MythUIProgressDialog *progress = new MythUIProgressDialog( message,
                                        popupStack, "scalingprogressdialog");
    if (progress->Create())
    {
        popupStack->AddScreen(progress, false);
        progress->SetTotal(trackList.count());
    }
    else
    {
        delete progress;
        progress = NULL;
    }

    // Read track metadata and add to all_music
    int track = 0;
    for (QStringList::const_iterator it = trackList.begin();
            it != trackList.end(); ++it)
    {
        QScopedPointer<MusicMetadata> meta(MetaIO::readMetadata(*it));
        if (meta)
        {
            meta->setTrack(++track);
            gMusicData->all_music->addCDTrack(*meta);
        }
        if (progress)
        {
            progress->SetProgress(track);
            qApp->processEvents();
        }
    }
    LOG(VB_MEDIA, LOG_INFO, QString("MythMusic: %1 tracks scanned").arg(track));

    if (progress)
        progress->Close();

    // Remove all tracks from the playlist
    gMusicData->all_playlists->getActive()->removeAllTracks();

    // Create list of new tracks
    QList<int> songList;
    const int tracks = gMusicData->all_music->getCDTrackCount();
    for (track = 1; track <= tracks; track++)
    {
        MusicMetadata *mdata = gMusicData->all_music->getCDMetadata(track);
        if (mdata)
            songList.append(mdata->ID());
    }
    if (songList.isEmpty())
        return;

    s_mountPath = cd->getMountPath();

    // Add new tracks to playlist
    gMusicData->all_playlists->getActive()->fillSonglistFromList(
            songList, true, PL_REPLACE, 0);
    gPlayer->setCurrentTrackPos(0);

    // if there is no music screen showing then show the Playlist view
    if (!gPlayer->hasClient())
    {
        // make sure we start playing from the first track
        gCoreContext->SaveSetting("MusicBookmark", 0);
        gCoreContext->SaveSetting("MusicBookmarkPosition", 0);

        runMusicPlayback();
    }
}

#ifdef HAVE_CDIO
static void handleCDMedia(MythMediaDevice *cd)
{

    if (!cd)
        return;

    LOG(VB_MEDIA, LOG_NOTICE, "Got a CD media changed event");

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

        if (gPlayer->isPlaying() && gPlayer->getCurrentMetadata()
            && gPlayer->getCurrentMetadata()->isCDTrack())
        {
            // we was playing a cd track which is no longer available so stop playback
            // TODO should check the playing track is from the ejected drive if more than one is available
            gPlayer->stop(true);
        }

        // device is not usable so remove any existing CD tracks
        if (gMusicData->all_music)
        {
            gMusicData->all_music->clearCDData();
            gMusicData->all_playlists->getActive()->removeAllCDTracks();
        }

        gPlayer->activePlaylistChanged(-1, false);
        gPlayer->sendCDChangedEvent();

        return;
    }

    if (!gMusicData->initialized)
        gMusicData->loadMusic();

    // wait for the music and playlists to load
    while (!gMusicData->all_playlists->doneLoading() || !gMusicData->all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    // remove any existing CD tracks
    gMusicData->all_music->clearCDData();
    gMusicData->all_playlists->getActive()->removeAllCDTracks();

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
                    parenttitle = " " + qApp->translate("(MythMusicMain)", 
                                                        "Unknown");
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
}
#else
static void handleCDMedia(MythMediaDevice *)
{
    LOG(VB_GENERAL, LOG_NOTICE, "MythMusic got a media changed event"
                                "but cdio support is not compiled in");
}
#endif

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
    REG_KEY("Music", "TOGGLELAST",  QT_TRANSLATE_NOOP("MythControls",
        "Switch to previous radio stream"), "");

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
        "MythMusic Media Handler 1/2"), "", handleCDMedia,
        MEDIATYPE_AUDIO | MEDIATYPE_MIXED, QString::null);
    QString filt = MetaIO::ValidFileExtensions;
    filt.replace('|',',');
    filt.remove('.');
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 2/2"), "", handleMedia,
        MEDIATYPE_MMUSIC, filt);
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
