#ifndef _PLAYLIST_CONTAINER_H_
#define _PLAYLIST_CONTAINER_H_

// qt
#include <QCoreApplication>

// MythTV
#include "mthread.h"


#define DEFAULT_PLAYLIST_NAME     "default_playlist_storage"
#define DEFAULT_STREAMLIST_NAME   "stream_playlist"

class PlaylistContainer;
class Playlist;
class AllMusic;

class PlaylistLoadingThread : public MThread
{
  public:
    PlaylistLoadingThread(PlaylistContainer *parent_ptr,
                          AllMusic *all_music_ptr)
        : MThread("PlaylistLoading"), parent(parent_ptr),
          all_music(all_music_ptr) {}

    void run() override; // MThread

  private:
    PlaylistContainer *parent    {nullptr};
    AllMusic          *all_music {nullptr};
};

class PlaylistContainer
{
    Q_DECLARE_TR_FUNCTIONS(PlaylistContainer);

  public:
    explicit PlaylistContainer(AllMusic *all_music);
   ~PlaylistContainer();

    void            load();
    void            resync(void);
    void            describeYourself(void) const;    // debugging

    Playlist*       getActive(void) { return m_activePlaylist; }
    Playlist*       getPlaylist(int id);
    Playlist*       getPlaylist(const QString &name);
    Playlist*       getStreamPlaylist(void) { return m_streamPlaylist; }

    void            save();

    void            createNewPlaylist(const QString &name);
    void            copyNewPlaylist(const QString &name);
    void            copyToActive(int index);

    QString         getPlaylistName(int index, bool &reference);

    void            deletePlaylist(int kill_me);
    void            renamePlaylist(int index, QString new_name);

    bool            nameIsUnique(const QString& a_name, int which_id);

    void            clearActive();

    bool            doneLoading(){return m_doneLoading;}

    bool            cleanOutThreads();

    void            FillIntelliWeights(int &rating, int &playcount,
                                       int &lastplay, int &random);
    QList<Playlist*> *getPlaylists(void) { return m_allPlaylists; }
    QStringList       getPlaylistNames(void);

  private:
    Playlist               *m_activePlaylist  {nullptr};
    Playlist               *m_streamPlaylist  {nullptr};
    QList<Playlist*>       *m_allPlaylists    {nullptr};

    PlaylistLoadingThread  *m_playlistsLoader {nullptr};
    bool                    m_doneLoading     {false};
    QString                 m_myHost;

    int m_ratingWeight                        {2};
    int m_playCountWeight                     {2};
    int m_lastPlayWeight                      {2};
    int m_randomWeight                        {2};
};

#endif // _PLAYLIST_CONTAINER_H_
