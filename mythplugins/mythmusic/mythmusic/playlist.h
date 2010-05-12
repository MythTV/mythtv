#ifndef PLAYLIST_H_
#define PLAYLIST_H_

#include <vector>
#include <list>
using namespace std;

#include <QList>

#include "metadata.h"
#include "treecheckitem.h"
#include <uitypes.h>
#include <uilistbtntype.h>

class PlaylistContainer;


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

typedef enum TrackTypes
{
    kTrackCD,
    kTrackPlaylist,
    kTrackSong,
    kTrackUnknown,
} TrackType;

class Track
{
    //  Why isn't this class just Metadata?
    //  Because soon it will need to be another
    //  playlist (playlists within playlists)
    //  a network connection, a radio station, etc.

  public: 
    Track(int x, AllMusic *all_music_ptr);

    void postLoad(PlaylistContainer *grandparent);

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

    TrackType GetTrackType(void) const;

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

    //Playlist& operator=(const Playlist& rhs);

    void setParent(PlaylistContainer *myparent) { parent = myparent; }

    void postLoad(void);

    void loadPlaylist(QString a_name, QString a_host);
    void loadPlaylistByID(int id, QString a_host);

    void savePlaylist(QString a_name, QString a_host);

    void putYourselfOnTheListView(UIListGenericTree *a_parent);

    int writeTree(GenericTree *tree_to_write_to, int a_counter);

    void describeYourself(void) const; //  debugging

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

    QList<Track*> getSongs(void) { return songs; }
    Track* getSongAt(int pos);

    void moveTrackUpDown(bool flag, int where_its_at);
    void moveTrackUpDown(bool flag, Track *the_track);

    bool checkTrack(int a_track_id, bool cd_flag) const;

    void addTrack(int the_track_id, bool update_display, bool cd_flag);

    void removeTrack(int the_track_id, bool cd_flag);
    void removeAllTracks(void);
    void removeAllWidgets(void);
    
    void copyTracks(Playlist *to_ptr, bool update_display) const;

    bool hasChanged(void) { return changed; }
    void Changed(void) { changed = true; }

    QString getName(void) { return name; } 
    void    setName(QString a_name) { name = a_name; }

    int     getID(void) { return playlistid; }
    int     getFirstTrackID(void) const;
    void    setID(int x) { playlistid = x; }

    bool    containsReference(int to_check, int depth);
    void    ripOutAllCDTracksNow();

    void    getStats(int *trackCount, int *totalLength, int currentTrack = 0, 
                     int *playedLength = NULL) const;
    
    void computeSize(double &size_in_MB, double &size_in_sec);
    int CreateCDMP3(void);
    int CreateCDAudio(void);

  private:
    QString             removeDuplicateTracks(const QString &new_songlist);
    int                 playlistid;
    QString             name;
    QString             raw_songlist;
    typedef QList<Track*> SongList;
    SongList            songs;
    AllMusic           *all_available_music;
    PlaylistContainer  *parent;
    bool                changed;
};

#endif
