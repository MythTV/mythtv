#ifndef PLAYLIST_H_
#define PLAYLIST_H_

#include <qvaluelist.h>
#include <qlistview.h>
#include <qptrlist.h>
#include <qsqldatabase.h>
#include <qthread.h>

#include "metadata.h"
#include "treecheckitem.h"
#include <mythtv/uitypes.h>

class PlaylistsContainer;

class Track
{

    //  Why isn't this class just Metadata?
    //  Because soon it will need to be another
    //  playlist (playlists within playlists)
    //  a network connection, a radio station, etc.

  public:
    
    Track(int x, AllMusic *all_music_ptr);
    void postLoad(PlaylistsContainer *grandparent);
    void setParent(Playlist *parent_ptr);
    void setValue(int x){index_value = x;}
    void putYourselfOnTheListView(QListViewItem *a_listviewitem);
    void putYourselfOnTheListView(QListViewItem *a_listviewitem, QListViewItem *current_last_item);
    int  getValue(){return index_value;}
    void deleteYourWidget();
    void deleteYourself();
    void moveUpDown(bool flag);
    PlaylistTrack* getWidget(){return my_widget;}
    bool badReference(){return bad_reference;}
    void setCDFlag(bool yes_or_no){cd_flag = yes_or_no;}
    bool getCDFlag(){return cd_flag;}
  private:
    
    int           index_value;
    PlaylistTrack *my_widget;
    AllMusic      *all_available_music;
    QString       label;
    Playlist      *parent;
    bool          bad_reference;
    bool          cd_flag;
};

class Playlist
{
  public:
    
    Playlist(AllMusic *all_music_ptr);
    ~Playlist();
    Playlist& operator=(const Playlist& rhs);

    void setParent(PlaylistsContainer *myparent){parent=myparent;}
    void postLoad();
    void loadPlaylist(QString a_name, QSqlDatabase *a_db, QString a_host);
    void loadPlaylistByID(int id, QSqlDatabase *a_db, QString a_host);
    void savePlaylist(QString a_name, QSqlDatabase *a_db);
    void saveNewPlaylist(QSqlDatabase *a_db, QString a_host);
    void putYourselfOnTheListView(QListViewItem *a_parent);
    void writeMetadata(QPtrList<Metadata> *list_to_write_to);
    int writeTree(GenericTree *tree_to_write_to, int a_counter);
    void describeYourself(); //  debugging
    void fillSongsFromSonglist();
    void fillSonglistFromSongs();
    void moveTrackUpDown(bool flag, Track *the_track);
    bool checkTrack(int a_track_id);
    void addTrack(int the_track_id, bool update_display, bool cd_flag);
    void removeTrack(int the_track_id, bool cd_flag);
    void removeAllTracks();
    void copyTracks(Playlist *to_ptr, bool update_display);
    bool hasChanged(){return changed;}
    void Changed(){changed = true;}
    QString getName(){return name;} 
    void    setName(QString a_name){name = a_name;}
    int     getID(){return playlistid;}
    int     getFirstTrackID();
    void    setID(int x){playlistid = x;}
    bool    containsReference(int to_check, int depth);
    void    ripOutAllCDTracksNow();

  private:

    int                 playlistid;
    QString             name;
    QString             raw_songlist;
    QPtrList<Track>     songs;
    AllMusic            *all_available_music;
    PlaylistsContainer  *parent;
    bool                changed;
};

class PlaylistLoadingThread : public QThread
{
  public:
  
    PlaylistLoadingThread(PlaylistsContainer *parent_ptr,
                          AllMusic *all_music_ptr);
    virtual void run();
    
  private:
  
    PlaylistsContainer* parent;
    AllMusic*           all_music;
};


class PlaylistsContainer
{
  public:
  
    PlaylistsContainer(QSqlDatabase *db_ptr, AllMusic *all_music);

    void            load();
    void            describeYourself();    // debugging
    Playlist*       getActive(){return active_playlist;}
    Playlist*       getPlaylist(int id);
    void            setActiveWidget(PlaylistTitle *widget);
    PlaylistTitle*  getActiveWidget(){return active_widget;}
    void            writeActive(QPtrList<Metadata> *list_to_write_to);
    void            writeTree(GenericTree *tree_to_write_to);
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
    void            setHost(QString a_name){my_host = a_name;}

  private:
  
    Playlist            *active_playlist;
    Playlist            *backup_playlist;
    QValueList<int>     cd_playlist;
    QPtrList<Playlist>  *all_other_playlists;
    QSqlDatabase        *db;   
    AllMusic            *all_available_music;
    PlaylistTitle       *active_widget;
    int                 pending_writeback_index;
    
    PlaylistLoadingThread  *playlists_loader;
    bool                    done_loading;
    QString                 my_host;
};




#endif
