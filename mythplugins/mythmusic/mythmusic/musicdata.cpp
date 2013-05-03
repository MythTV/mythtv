// qt
#include <QApplication>

// mythtv
#include <mythscreenstack.h>
#include <mythmainwindow.h>
#include <mythprogressdialog.h>
#include <musicmetadata.h>

// mythmusic
#include "musicdata.h"

// this is the global MusicData object shared thoughout MythMusic
MusicData  *gMusicData = NULL;

MusicData::MusicData(void)
{
    all_playlists = NULL;
    all_music = NULL;
    all_streams = NULL;
    initialized = false;
}

MusicData::~MusicData(void)
{
    if (all_playlists)
    {
        delete all_playlists;
        all_playlists = NULL;
    }

    if (all_music)
    {
        delete all_music;
        all_music = NULL;
    }

    if (all_streams)
    {
        delete all_streams;
        all_streams = NULL;
    }
}

/// reload music after a scan, rip or import
void MusicData::reloadMusic(void)
{
    if (!all_music || !all_playlists)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = QObject::tr("Rebuilding music tree");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");

    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = NULL;

    all_music->startLoading();
    while (!all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }

    if (busy)
        busy->Close();
}
