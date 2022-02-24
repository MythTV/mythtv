#ifndef MUSICDATA_H_
#define MUSICDATA_H_

// C++
#include <utility>

// qt
#include <QRunnable>

// myth
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythpluginexport.h>

// mythmusic
#include "playlistcontainer.h"

class PlaylistContainer;
class AllMusic;
class AllStream;

/// send a message to the master BE without blocking the UI thread
class SendStringListThread : public QRunnable
{
  public:
    explicit SendStringListThread(QStringList strList)
        : m_strList(std::move(strList)) {}

    void run() override // QRunnable
    {
        gCoreContext->SendReceiveStringList(m_strList);
    }

  private:
    QStringList m_strList;
};

//----------------------------------------------------------------------------

class MusicData : public QObject
{
  Q_OBJECT

  public:

    MusicData() = default;
    ~MusicData() override;

    static void scanMusic(void);
    void loadMusic(void) const;

  public slots:
    void reloadMusic(void) const;

  public:
    PlaylistContainer  *m_all_playlists {nullptr};
    AllMusic           *m_all_music     {nullptr};
    AllStream          *m_all_streams   {nullptr};
    bool                m_initialized   {false};
};

// This global variable contains the MusicData instance for the application
extern MPLUGIN_PUBLIC MusicData *gMusicData;

#endif
