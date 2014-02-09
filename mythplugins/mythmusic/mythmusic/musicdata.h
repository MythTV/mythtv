#ifndef MUSICDATA_H_
#define MUSICDATA_H_


// qt
#include <QRunnable>

// myth
#include <mythexp.h>
#include <mythcorecontext.h>

// mythmusic
#include "playlistcontainer.h"

class PlaylistContainer;
class AllMusic;
class AllStream;

/// send a message to the master BE without blocking the UI thread
class SendStringListThread : public QRunnable
{
  public:
    SendStringListThread(const QStringList &strList)
    {
        m_strList = strList;
    }

    void run()
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

    MusicData();
    ~MusicData();

    void scanMusic(void);
    void loadMusic(void);

  public slots:
    void reloadMusic(void);

  public:
    PlaylistContainer  *all_playlists;
    AllMusic           *all_music;
    AllStream          *all_streams;
    bool                initialized;
};

// This global variable contains the MusicData instance for the application
extern MPUBLIC MusicData *gMusicData;

#endif
