#ifndef PLAYLIST_H_
#define PLAYLIST_H_

// c++
#include <utility>

// qt
#include <QList>
#include <QMap>

// MythTV
#include <libmythmetadata/musicmetadata.h>

// mythmusic
#include "musicplayer.h"
#include "playlistcontainer.h"

class PlaylistContainer;
class Playlist;
class MythSystemLegacy;

enum InsertPLOption : std::uint8_t
{
    PL_REPLACE = 1,
    PL_INSERTATBEGINNING,
    PL_INSERTATEND,
    PL_INSERTAFTERCURRENT
};

enum PlayPLOption : std::uint8_t
{
    PL_FIRST = 1,
    PL_FIRSTNEW,
    PL_CURRENT
};

struct PlaylistOptions
{
    InsertPLOption insertPLOption;
    PlayPLOption playPLOption;
};

using SongList = QList<MusicMetadata::IdType>;

class Playlist : public QObject
{
    Q_OBJECT

  public:
    Playlist(void);
    ~Playlist() override;

    void setParent(PlaylistContainer *myparent) { m_parent = myparent; }

    void loadPlaylist(const QString& a_name, const QString& a_host);
    void loadPlaylistByID(int id, const QString& a_host);

    void savePlaylist(const QString& a_name, const QString& a_host);

    void shuffleTracks(MusicPlayer::ShuffleMode mode);

    void describeYourself(void) const; //  debugging

    void fillSongsFromSonglist(const QString& songList);
    int fillSonglistFromQuery(const QString& whereClause,
                              bool removeDuplicates = false,
                              InsertPLOption insertOption = PL_REPLACE,
                              int currentTrackID = 0);
    int fillSonglistFromSmartPlaylist(const QString& category, const QString& name,
                                      bool removeDuplicates = false,
                                      InsertPLOption insertOption = PL_REPLACE,
                                      int currentTrackID = 0);
    int fillSonglistFromList(const QList<int> &songList,
                             bool removeDuplicates,
                             InsertPLOption insertOption,
                             int currentTrackID);
    QString toRawSonglist(bool shuffled = false, bool tracksOnly = false);


    MusicMetadata* getSongAt(int pos) const;

    int getTrackCount(void) { return m_songs.count(); }

    void moveTrackUpDown(bool flag, int where_its_at);

    bool checkTrack(MusicMetadata::IdType trackID) const;

    void addTrack(MusicMetadata::IdType trackID, bool update_display);

    int getTrackPosition(MusicMetadata::IdType trackID) { return m_shuffledSongs.indexOf(trackID); }

    void removeTrack(MusicMetadata::IdType trackID);
    void removeAllTracks(void);
    void removeAllCDTracks(void);

    void copyTracks(Playlist *to_ptr, bool update_display);

    bool hasChanged(void) const { return m_changed; }
    void changed(void);

    /// whether any changes should be saved to the DB
    void disableSaves(void) { m_doSave = false; }
    void enableSaves(void) { m_doSave = true; }
    bool doSaves(void) const { return m_doSave; }

    QString getName(void) { return m_name; } 
    void    setName(const QString& a_name) { m_name = a_name; }

    bool isActivePlaylist(void) { return m_name == DEFAULT_PLAYLIST_NAME; }

    int  getID(void) const { return m_playlistid; }
    void setID(int x) { m_playlistid = x; }

    void getStats(uint *trackCount, std::chrono::seconds *totalLength,
                  uint currentTrack = 0, std::chrono::seconds *playedLength = nullptr) const;

    void resync(void);

#ifdef CD_WRTITING_FIXED
    void computeSize(double &size_in_MB, double &size_in_sec);
    int CreateCDMP3(void);
    int CreateCDAudio(void);

  private slots:
    void mkisofsData(int fd);
    void cdrecordData(int fd);
    void processExit(uint retval);
    void processExit();
#endif

  private:
    MusicMetadata* getRawSongAt(int pos) const;
    static QString removeItemsFromList(const QString &remove_list, const QString &source_list);

    int                   m_playlistid  {0};
    QString               m_name;

    SongList              m_songs;
    SongList              m_shuffledSongs;

    PlaylistContainer    *m_parent      {nullptr};
    bool                  m_changed     {false};
    bool                  m_doSave      {true};
#ifdef CD_WRTITING_FIXED
    MythProgressDialog   *m_progress    {nullptr};
    MythSystemLegacy     *m_proc        {nullptr};
    uint                  m_procExitVal {0};
#endif
};

#endif
