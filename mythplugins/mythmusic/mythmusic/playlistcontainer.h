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
    PlaylistContainer(AllMusic *all_music, const QString &host_name);
   ~PlaylistContainer();

    void            load();
    void            describeYourself(void) const;    // debugging

    Playlist*       getActive(void) { return active_playlist; }
    Playlist*       getPlaylist(int id);

    void            setActiveWidget(PlaylistTitle *widget);
    PlaylistTitle*  getActiveWidget(void) { return active_widget; }

    GenericTree*    writeTree(GenericTree *tree_to_write_to);
    void            clearCDList();
    void            addCDTrack(int x);
    void            removeCDTrack(int x);
    bool            checkCDTrack(int x);
    void            save();

    void            createNewPlaylist(QString name);
    void            copyNewPlaylist(QString name);
    void            copyToActive(int index);

    void            showRelevantPlaylists(TreeCheckItem *alllist);
    void            refreshRelevantPlaylists(TreeCheckItem *alllist);

    QString         getPlaylistName(int index, bool &reference);

    void            postLoad();

    void            deletePlaylist(int index);
    void            renamePlaylist(int index, QString new_name);

    void            popBackPlaylist();
    bool            pendingWriteback();
    void            setPending(int x){pending_writeback_index = x;}
    int             getPending(){return pending_writeback_index;}

    bool            nameIsUnique(QString a_name, int which_id);

    void            clearActive();

    bool            doneLoading(){return done_loading;}

    bool            cleanOutThreads();

    void            FillIntelliWeights(int &rating, int &playcount,
                                       int &lastplay, int &random);
  private:  
    Playlist            *active_playlist;
    Playlist            *backup_playlist;
    list<int>            cd_playlist;
    list<Playlist*>     *all_other_playlists;
    AllMusic            *all_available_music;
    PlaylistTitle       *active_widget;
    int                 pending_writeback_index;
    
    PlaylistLoadingThread  *playlists_loader;
    bool                    done_loading;
    QString                 my_host;

    int RatingWeight;
    int PlayCountWeight;
    int LastPlayWeight;
    int RandomWeight;
};

#endif // _PLAYLIST_CONTAINER_H_
