// qt
#include <QApplication>

// mythtv
#include <mythscreenstack.h>
#include <mythmainwindow.h>
#include <mythprogressdialog.h>
#include <musicmetadata.h>
#include <musicfilescanner.h>
#include <musicutils.h>
#include <mthreadpool.h>

// mythmusic
#include "musicdata.h"
#include "musicplayer.h"

#include <unistd.h> // for usleep()

// this is the global MusicData object shared thoughout MythMusic
MusicData  *gMusicData = nullptr;


///////////////////////////////////////////////////////////////////////////////


MusicData::~MusicData(void)
{
    if (m_all_playlists)
    {
        delete m_all_playlists;
        m_all_playlists = nullptr;
    }

    if (m_all_music)
    {
        delete m_all_music;
        m_all_music = nullptr;
    }

    if (m_all_streams)
    {
        delete m_all_streams;
        m_all_streams = nullptr;
    }
}

void MusicData::scanMusic (void)
{
    QStringList strList("SCAN_MUSIC");
    auto *thread = new SendStringListThread(strList);
    MThreadPool::globalInstance()->start(thread, "Send SCAN_MUSIC");

    LOG(VB_GENERAL, LOG_INFO, "Requested a music file scan");
}

/// reload music after a scan, rip or import
void MusicData::reloadMusic(void)
{
    if (!m_all_music || !m_all_playlists)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = tr("Rebuilding music tree");

    auto *busy = new MythUIBusyDialog(message, popupStack, "musicscanbusydialog");

    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = nullptr;

    // TODO make it so the player isn't interupted
    // for the moment stop playing and try to resume after reloading
    bool wasPlaying = false;
    if (gPlayer->isPlaying())
    {
        gPlayer->savePosition();
        gPlayer->stop(true);
        wasPlaying = true;
    }

    m_all_music->startLoading();
    while (!m_all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    m_all_playlists->resync();

    if (busy)
        busy->Close();

    if (wasPlaying)
        gPlayer->restorePosition();
}

void MusicData::loadMusic(void)
{
    // only do this once
    if (m_initialized)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = qApp->translate("(MythMusicMain)",
                                      "Loading Music. Please wait ...");

    auto *busy = new MythUIBusyDialog(message, popupStack, "musicscanbusydialog");
    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = nullptr;

    // Set the various track formatting modes
    MusicMetadata::setArtistAndTrackFormats();

    auto *all_music = new AllMusic();

    //  Load all playlists into RAM (once!)
    auto *all_playlists = new PlaylistContainer(all_music);

    gMusicData->m_all_music = all_music;
    gMusicData->m_all_streams = new AllStream();
    gMusicData->m_all_playlists = all_playlists;

    gMusicData->m_initialized = true;

    while (!gMusicData->m_all_playlists->doneLoading()
           || !gMusicData->m_all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    gPlayer->loadStreamPlaylist();
    gPlayer->loadPlaylist();

    if (busy)
        busy->Close();
}
