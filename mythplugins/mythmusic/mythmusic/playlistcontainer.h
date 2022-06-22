#ifndef PLAYLIST_CONTAINER_H
#define PLAYLIST_CONTAINER_H

// qt
#include <QCoreApplication>

// MythTV
#include <libmythbase/mthread.h>


static constexpr const char* DEFAULT_PLAYLIST_NAME     { "default_playlist_storage" };
static constexpr const char* DEFAULT_STREAMLIST_NAME   { "stream_playlist" };

class PlaylistContainer;
class Playlist;
class AllMusic;

class PlaylistLoadingThread : public MThread
{
  public:
    PlaylistLoadingThread(PlaylistContainer *parent_ptr,
                          AllMusic *all_music_ptr)
        : MThread("PlaylistLoading"), m_parent(parent_ptr),
          m_allMusic(all_music_ptr) {}

    void run() override; // MThread

  private:
    PlaylistContainer *m_parent   {nullptr};
    AllMusic          *m_allMusic {nullptr};
};

class PlaylistContainer
{
    Q_DECLARE_TR_FUNCTIONS(PlaylistContainer);

  public:
    explicit PlaylistContainer(AllMusic *all_music);
    PlaylistContainer(const PlaylistContainer &rhs) = delete;
   ~PlaylistContainer();
    PlaylistContainer& operator=(const PlaylistContainer &rhs) = delete;

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
    void            renamePlaylist(int index, const QString& new_name);

    bool            nameIsUnique(const QString& a_name, int which_id);

    void            clearActive();

    bool            doneLoading() const{return m_doneLoading;}

    bool            cleanOutThreads();

    void            FillIntelliWeights(int &rating, int &playcount,
                                       int &lastplay, int &random) const;
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

#endif // PLAYLIST_CONTAINER_H
