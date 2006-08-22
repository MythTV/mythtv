#ifndef PLAYLIST_H_
#define PLAYLIST_H_

#include <qvaluelist.h>
#include <qlistview.h>
#include <qptrlist.h>
#include <qthread.h>

#include "metadata.h"
#include "treecheckitem.h"
#include <mythtv/uitypes.h>
#include <mythtv/uilistbtntype.h>

class PlaylistsContainer;


enum InsertPLOption
{
    PL_REPLACE = 1,
    PL_INSERTATBEGINNING,
    PL_INSERTATEND,
    PL_INSERTAFTERCURRENT,
    PL_FILTERONLY
};

enum PlayPLOption
{
    PL_FIRST = 1,
    PL_FIRSTNEW,
    PL_CURRENT
};

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

    void putYourselfOnTheListView(UIListGenericTree *a_listviewitem);

    int  getValue(){return index_value;}

    void deleteYourWidget();
    void deleteYourself();
    void moveUpDown(bool flag);

    PlaylistTrack* getWidget(void) { return my_widget; }
    bool badReference(void) { return bad_reference; }
    void setCDFlag(bool yes_or_no) { cd_flag = yes_or_no; }
    bool getCDFlag(void) { return cd_flag; }

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

    void setParent(PlaylistsContainer *myparent) { parent = myparent; }

    void postLoad(void);

    void loadPlaylist(QString a_name, QString a_host);
    void loadPlaylistByID(int id, QString a_host);

    void savePlaylist(QString a_name, QString a_host);

    void putYourselfOnTheListView(UIListGenericTree *a_parent);

    int writeTree(GenericTree *tree_to_write_to, int a_counter);

    void describeYourself(void); //  debugging

    void fillSongsFromCD();
    void fillSongsFromSonglist(bool filter);
    void fillSongsFromSonglist(QString songList, bool filter);
    void fillSonglistFromSongs();
    void fillSonglistFromQuery(QString whereClause, 
                               bool removeDuplicates = false,
                               InsertPLOption insertOption = PL_REPLACE,
                               int currentTrackID = 0);
    void fillSonglistFromSmartPlaylist(QString category, QString name,
                                       bool removeDuplicates = false,
                                       InsertPLOption insertOption = PL_REPLACE,
                                       int currentTrackID = 0);
    QString  getSonglist(void) { return raw_songlist; }

    void moveTrackUpDown(bool flag, Track *the_track);

    bool checkTrack(int a_track_id, bool cd_flag);

    void addTrack(int the_track_id, bool update_display, bool cd_flag);

    void removeTrack(int the_track_id, bool cd_flag);
    void removeAllTracks(void);
    void removeAllWidgets(void);
    
    void copyTracks(Playlist *to_ptr, bool update_display);

    bool hasChanged(void) { return changed; }
    void Changed(void) { changed = true; }

    QString getName(void) { return name; } 
    void    setName(QString a_name) { name = a_name; }

    int     getID(void) { return playlistid; }
    int     getFirstTrackID(void);
    void    setID(int x) { playlistid = x; }

    bool    containsReference(int to_check, int depth);
    void    ripOutAllCDTracksNow();

    void computeSize(double &size_in_MB, double &size_in_sec);
    int CreateCDMP3(void);
    int CreateCDAudio(void);

  private:
    QString             removeDuplicateTracks(const QString &new_songlist);
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
    PlaylistsContainer(AllMusic *all_music, QString host_name);
   ~PlaylistsContainer();

    void            load();
    void            describeYourself();    // debugging

    Playlist*       getActive(void) { return active_playlist; }
    Playlist*       getPlaylist(int id);

    void            setActiveWidget(PlaylistTitle *widget);
    PlaylistTitle*  getActiveWidget(void) { return active_widget; }

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

    void            FillIntelliWeights(int &rating, int &playcount,
                                       int &lastplay, int &random);
  private:  
    Playlist            *active_playlist;
    Playlist            *backup_playlist;
    QValueList<int>     cd_playlist;
    QPtrList<Playlist>  *all_other_playlists;
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

#endif
