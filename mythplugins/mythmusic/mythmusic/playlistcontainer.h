#ifndef _PLAYLIST_CONTAINER_H_
#define _PLAYLIST_CONTAINER_H_

#include "mthread.h"

#include "playlist.h"

class PlaylistLoadingThread : public MThread
{
  public:
    PlaylistLoadingThread(PlaylistContainer *parent_ptr,
                          AllMusic *all_music_ptr);
    virtual void run();

  private:
    PlaylistContainer *parent;
    AllMusic          *all_music;
};

class PlaylistContainer
{
  public:
    PlaylistContainer(AllMusic *all_music);
   ~PlaylistContainer();

    void            load();
    void            describeYourself(void) const;    // debugging

    Playlist*       getActive(void) { return m_activePlaylist; }
    Playlist*       getPlaylist(int id);
    Playlist*       getPlaylist(const QString &name);
    Playlist*       getStreamPlaylist(void) { return m_streamPlaylist; }

    void            clearCDList();
    void            addCDTrack(int x);
    void            removeCDTrack(int x);
    bool            checkCDTrack(int x);
    void            save();

    void            createNewPlaylist(QString name);
    void            copyNewPlaylist(QString name);
    void            copyToActive(int index);

    QString         getPlaylistName(int index, bool &reference);

    void            deletePlaylist(int index);
    void            renamePlaylist(int index, QString new_name);

    bool            nameIsUnique(QString a_name, int which_id);

    void            clearActive();

    bool            doneLoading(){return m_doneLoading;}

    bool            cleanOutThreads();

    void            FillIntelliWeights(int &rating, int &playcount,
                                       int &lastplay, int &random);
    QList<Playlist*> *getPlaylists(void) { return m_allPlaylists; }
    QStringList       getPlaylistNames(void);

  private:  
    Playlist            *m_activePlaylist;
    Playlist            *m_streamPlaylist;
    QList<int>           m_cdPlaylist;
    QList<Playlist*>    *m_allPlaylists;
    AllMusic            *m_allMusic;

    PlaylistLoadingThread  *m_playlistsLoader;
    bool                    m_doneLoading;
    QString                 m_myHost;

    int m_ratingWeight;
    int m_playCountWeight;
    int m_lastPlayWeight;
    int m_randomWeight;
};

#endif // _PLAYLIST_CONTAINER_H_
