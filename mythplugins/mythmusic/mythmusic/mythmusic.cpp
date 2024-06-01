// C++ headers
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Qt headers
#include <QApplication>
#include <QDir>
#include <QScopedPointer>

// MythTV headers
#include <libmyth/mythcontext.h>
#include <libmythbase/compat.h>
#include <libmythbase/lcddevice.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythplugin.h>
#include <libmythbase/mythpluginapi.h>
#include <libmythbase/mythversion.h>
#include <libmythmetadata/metaio.h>
#include <libmythmetadata/musicfilescanner.h>
#include <libmythmetadata/musicmetadata.h>
#include <libmythmetadata/musicutils.h>
#include <libmythui/mediamonitor.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/myththemedmenu.h>
#include <libmythui/mythuihelper.h>

// MythMusic headers
#include "cddecoder.h"
#include "config.h"
#include "decoder.h"
#include "generalsettings.h"
#include "importmusic.h"
#include "importsettings.h"
#include "mainvisual.h"
#include "musicdata.h"
#include "musicdbcheck.h"
#include "musicplayer.h"
#include "playersettings.h"
#include "playlistcontainer.h"
#include "playlisteditorview.h"
#include "playlistview.h"
#include "ratingsettings.h"
#include "streamview.h"
#include "visualizationsettings.h"

#ifdef HAVE_CDIO
#include "cdrip.h"
#endif

#if defined HAVE_CDIO
/**
 * \brief Work out the best CD drive to use at this time
 */
static QString chooseCD(void)
{
    if (!gCDdevice.isEmpty())
        return gCDdevice;

#ifdef Q_OS_DARWIN
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
        ShowOkPopup(QCoreApplication::translate("(MythMusicMain)",
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
        ShowOkPopup(QCoreApplication::translate("(MythMusicMain)",
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
        ShowOkPopup(QCoreApplication::translate("(MythMusicMain)",
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

    auto *view = new PlaylistView(mainStack, nullptr);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startStreamPlayback(void)
{
    gMusicData->loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *view = new StreamView(mainStack, nullptr);

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
    auto *view = new PlaylistEditorView(mainStack, nullptr, lastView);

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

    auto *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
    {
        mainStack->AddScreen(rip);
        QObject::connect(rip, &Ripper::ripFinished,
                     gMusicData, &MusicData::reloadMusic,
                     Qt::QueuedConnection);
    }
    else
    {
        delete rip;
    }

#else
    ShowOkPopup(QCoreApplication::translate("(MythMusicMain)",
                                "MythMusic hasn't been built with libcdio "
                                "support so ripping CDs is not possible"));
#endif
}

static void runScan(void)
{
    if (!checkStorageGroup())
        return;

    LOG(VB_GENERAL, LOG_INFO, "Scanning for music files");

    MusicData::scanMusic();
}

static void startImport(void)
{
    if (!checkStorageGroup())
        return;

    gMusicData->loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *import = new ImportMusicDialog(mainStack);

    if (import->Create())
    {
        mainStack->AddScreen(import);
        QObject::connect(import, &ImportMusicDialog::importFinished,
                gMusicData, &MusicData::reloadMusic,
                Qt::QueuedConnection);
    }
    else
    {
        delete import;
    }
}

// these point to the the mainmenu callback if found
static void (*m_callback)(void *, QString &) = nullptr;
static void *m_callbackdata = nullptr;

static void MusicCallback([[maybe_unused]] void *data, QString &selection)
{
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
        auto *gs = new GeneralSettings(mainStack, "general settings");

        if (gs->Create())
            mainStack->AddScreen(gs);
        else
            delete gs;
    }
    else if (sel == "settings_player")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ps = new PlayerSettings(mainStack, "player settings");

        if (ps->Create())
            mainStack->AddScreen(ps);
        else
            delete ps;
    }
    else if (sel == "settings_rating")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *rs = new RatingSettings(mainStack, "rating settings");

        if (rs->Create())
            mainStack->AddScreen(rs);
        else
            delete rs;
    }
    else if (sel == "settings_visualization")
    {

       MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
       auto *vs = new VisualizationSettings(mainStack, "visualization settings");

       if (vs->Create())
           mainStack->AddScreen(vs);
       else
           delete vs;
    }
    else if (sel == "settings_import")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *is = new ImportSettings(mainStack, "import settings");

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

static int runMenu(const QString& which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    // find the 'mainmenu' MythThemedMenu so we can use the callback from it
    MythThemedMenu *mainMenu = nullptr;
    QObject *parentObject = GetMythMainWindow()->GetMainStack()->GetTopScreen();

    while (parentObject)
    {
        auto *menu = qobject_cast<MythThemedMenu *>(parentObject);

        if (menu && menu->objectName() == "mainmenu")
        {
            mainMenu = menu;
            break;
        }

        parentObject = parentObject->parent();
    }

    auto *diag = new MythThemedMenu(themedir, which_menu,
                                    GetMythMainWindow()->GetMainStack(),
                                    "music menu");

    // save the callback from the main menu
    if (mainMenu)
        mainMenu->getCallback(&m_callback, &m_callbackdata);

    diag->setCallback(MusicCallback, nullptr);
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
    LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
        .arg(which_menu, themedir));
    delete diag;
    return -1;
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

    auto *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
        mainStack->AddScreen(rip);
    else
    {
        delete rip;
        return;
    }

    QObject::connect(rip, &Ripper::ripFinished,
                     gMusicData, &MusicData::reloadMusic,
                     Qt::QueuedConnection);
#endif
}

static void showMiniPlayer(void)
{
    if (!gMusicData->m_all_music)
        return;

    // only show the miniplayer if there isn't already a client attached
    if (!gPlayer->hasClient())
        gPlayer->showMiniPlayer();
}

static QStringList GetMusicFilter()
{
    QString filt = MetaIO::kValidFileExtensions;
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

    for (const auto & fi : std::as_const(list))
    {
        if (fi.isDir())
        {
            ret += BuildFileList(fi.absoluteFilePath(), filters);
            QCoreApplication::processEvents();
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
        if (gMusicData->m_initialized)
        {
            gMusicData->m_all_music->clearCDData();
            gMusicData->m_all_playlists->getActive()->removeAllCDTracks();
        }

        gPlayer->activePlaylistChanged(-1, false);
        gPlayer->sendCDChangedEvent();

        return;
    }

    LOG(VB_MEDIA, LOG_NOTICE, QString("MythMusic: '%1' mounted on '%2'")
        .arg(cd->getVolumeID(), cd->getMountPath()) );

    s_mountPath.clear();

    // don't show the music screen if AutoPlayCD is off
    if (!gCoreContext->GetBoolSetting("AutoPlayCD", false))
        return;

    if (!gMusicData->m_initialized)
        gMusicData->loadMusic();

    // remove any existing CD tracks
    gMusicData->m_all_music->clearCDData();
    gMusicData->m_all_playlists->getActive()->removeAllCDTracks();

    gPlayer->sendCDChangedEvent();

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = QCoreApplication::translate("(MythMusicMain)",
                                      "Searching for music files...");
    auto *busy = new MythUIBusyDialog( message, popupStack, "musicscanbusydialog");
    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
    {
        delete busy;
        busy = nullptr;
    }

    // Search for music files
    QStringList trackList = BuildFileList(cd->getMountPath(), GetMusicFilter());
    LOG(VB_MEDIA, LOG_INFO, QString("MythMusic: %1 music files found")
                                .arg(trackList.count()));

    if (busy)
        busy->Close();

    if (trackList.isEmpty())
        return;

    message = QCoreApplication::translate("(MythMusicMain)", "Loading music tracks");
    auto *progress = new MythUIProgressDialog( message, popupStack,
                                               "scalingprogressdialog");
    if (progress->Create())
    {
        popupStack->AddScreen(progress, false);
        progress->SetTotal(trackList.count());
    }
    else
    {
        delete progress;
        progress = nullptr;
    }

    // Read track metadata and add to all_music
    int track = 0;
    for (const auto & file : std::as_const(trackList))
    {
        QScopedPointer<MusicMetadata> meta(MetaIO::readMetadata(file));
        if (meta)
        {
            meta->setTrack(++track);
            gMusicData->m_all_music->addCDTrack(*meta);
        }
        if (progress)
        {
            progress->SetProgress(track);
            QCoreApplication::processEvents();
        }
    }
    LOG(VB_MEDIA, LOG_INFO, QString("MythMusic: %1 tracks scanned").arg(track));

    if (progress)
        progress->Close();

    // Remove all tracks from the playlist
    gMusicData->m_all_playlists->getActive()->removeAllTracks();

    // Create list of new tracks
    QList<int> songList;
    const int tracks = gMusicData->m_all_music->getCDTrackCount();
    for (track = 1; track <= tracks; track++)
    {
        MusicMetadata *mdata = gMusicData->m_all_music->getCDMetadata(track);
        if (mdata)
            songList.append(mdata->ID());
    }
    if (songList.isEmpty())
        return;

    s_mountPath = cd->getMountPath();

    // Add new tracks to playlist
    gMusicData->m_all_playlists->getActive()->fillSonglistFromList(
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
#ifdef Q_OS_DARWIN
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
        if (gMusicData->m_all_music)
        {
            gMusicData->m_all_music->clearCDData();
            gMusicData->m_all_playlists->getActive()->removeAllCDTracks();
        }

        gPlayer->activePlaylistChanged(-1, false);
        gPlayer->sendCDChangedEvent();

        return;
    }

    if (!gMusicData->m_initialized)
        gMusicData->loadMusic();

    // wait for the music and playlists to load
    while (!gMusicData->m_all_playlists->doneLoading()
           || !gMusicData->m_all_music->doneLoading())
    {
        QCoreApplication::processEvents();
        usleep(50000);
    }

    // remove any existing CD tracks
    gMusicData->m_all_music->clearCDData();
    gMusicData->m_all_playlists->getActive()->removeAllCDTracks();

    // find any new cd tracks
    auto *decoder = new CdDecoder("cda", nullptr, nullptr);
    decoder->setDevice(newDevice);

    int tracks = decoder->getNumTracks();
    bool setTitle = false;

    for (int trackNo = 1; trackNo <= tracks; trackNo++)
    {
        MusicMetadata *track = decoder->getMetadata(trackNo);
        if (track)
        {
            gMusicData->m_all_music->addCDTrack(*track);

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
                    parenttitle = " " + QCoreApplication::translate("(MythMusicMain)",
                                                        "Unknown");
                    LOG(VB_GENERAL, LOG_INFO, "Couldn't find your "
                    " CD. It may not be in the freedb database.\n"
                    "    More likely, however, is that you need to delete\n"
                    "    ~/.cddb and ~/.cdserverrc and restart MythMusic.");
                }

                gMusicData->m_all_music->setCDTitle(parenttitle);
                setTitle = true;
            }

            delete track;
        }
    }

    gPlayer->sendCDChangedEvent();

    delete decoder;

    // if the AutoPlayCD setting is set we remove all the existing tracks
    // from the playlist and replace them with the new CD tracks found
    if (gCoreContext->GetBoolSetting("AutoPlayCD", false))
    {
        gMusicData->m_all_playlists->getActive()->removeAllTracks();

        QList<int> songList;

        for (int x = 1; x <= gMusicData->m_all_music->getCDTrackCount(); x++)
        {
            MusicMetadata *mdata = gMusicData->m_all_music->getCDMetadata(x);
            if (mdata)
                songList.append((mdata)->ID());
        }

        if (!songList.isEmpty())
        {
            gMusicData->m_all_playlists->getActive()->fillSonglistFromList(
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
static void handleCDMedia([[maybe_unused]] MythMediaDevice *cd)
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
        "Move to the next track"),     ">,.,Z,End,Media Next");
    REG_KEY("Music", "PREVTRACK",  QT_TRANSLATE_NOOP("MythControls",
        "Move to the previous track"), ",,<,Q,Home,Media Previous");
    REG_KEY("Music", "FFWD",       QT_TRANSLATE_NOOP("MythControls",
        "Fast forward"),               "PgDown,Ctrl+F,Media Fast Forward");
    REG_KEY("Music", "RWND",       QT_TRANSLATE_NOOP("MythControls",
        "Rewind"),                     "PgUp,Ctrl+B,Media Rewind");
    REG_KEY("Music", "PAUSE",      QT_TRANSLATE_NOOP("MythControls",
        "Pause/Start playback"),       "P,Media Play");
    REG_KEY("Music", "PLAY",       QT_TRANSLATE_NOOP("MythControls",
        "Start playback"),             "");
    REG_KEY("Music", "STOP",       QT_TRANSLATE_NOOP("MythControls",
        "Stop playback"),              "O,Media Stop");
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
        "Increase Play Speed"),   "W,3");
    REG_KEY("Music", "SPEEDDOWN",  QT_TRANSLATE_NOOP("MythControls",
        "Decrease Play Speed"),   "X,1");
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
        MEDIATYPE_AUDIO | MEDIATYPE_MIXED, QString());
    QString filt = MetaIO::kValidFileExtensions;
    filt.replace('|',',');
    filt.remove('.');
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 2/2"), "", handleMedia,
        MEDIATYPE_MMUSIC, filt);
}

int mythplugin_init(const char *libversion)
{
    if (!MythCoreContext::TestPluginVersion("mythmusic", libversion,
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

    gPlayer = new MusicPlayer(nullptr);
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
    if (gMusicData->m_all_music && gMusicData->m_all_music->cleanOutThreads())
    {
        gMusicData->m_all_music->save();
    }

    if (gMusicData->m_all_playlists && gMusicData->m_all_playlists->cleanOutThreads())
    {
        gMusicData->m_all_playlists->save();
    }

    delete gPlayer;

    delete gMusicData;
}
