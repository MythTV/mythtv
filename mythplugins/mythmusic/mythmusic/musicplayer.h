#ifndef MUSICPLAYER_H_
#define MUSICPLAYER_H_

// mythtv
#include <audiooutput.h>
#include <mythobservable.h>

// libmythmetadata
#include <musicmetadata.h>

// mythmusic
#include "decoderhandler.h"

// how long to wait before updating the lastplay and playcount fields
#define LASTPLAY_DELAY 15

class AudioOutput;
class MainVisual;
class Playlist;

class MusicPlayerEvent : public MythEvent
{
    public:
        MusicPlayerEvent(Type type, int id) :
            MythEvent(type), m_trackID(id) {}
        MusicPlayerEvent(Type type, uint vol, bool muted) :
            MythEvent(type), m_trackID(0), m_volume(vol), m_isMuted(muted) {}
        ~MusicPlayerEvent() = default;

         MythEvent *clone(void) const override //  MythEvent
            { return new MusicPlayerEvent(*this); }

        // for track changed/added/deleted/metadata changed/playlist changed events
        int m_trackID;

        // for volume changed event
        uint m_volume {0};
        bool m_isMuted {false};

        static Type TrackChangeEvent;
        static Type VolumeChangeEvent;
        static Type TrackAddedEvent;
        static Type TrackRemovedEvent;
        static Type TrackUnavailableEvent;
        static Type AllTracksRemovedEvent;
        static Type MetadataChangedEvent;
        static Type TrackStatsChangedEvent;
        static Type AlbumArtChangedEvent;
        static Type CDChangedEvent;
        static Type PlaylistChangedEvent;
        static Type PlayedTracksChangedEvent;
};

class MusicPlayer : public QObject, public MythObservable
{
    Q_OBJECT

  public:
     explicit MusicPlayer(QObject *parent);
    ~MusicPlayer(void);

    enum PlayMode
    {
      PLAYMODE_TRACKSPLAYLIST = 0,
      PLAYMODE_TRACKSEDITOR,
      PLAYMODE_RADIO,
    };

    void setPlayMode(PlayMode mode);
    PlayMode getPlayMode(void) { return m_playMode; }

    void playFile(const MusicMetadata &mdata);

    void addListener(QObject *listener);
    void removeListener(QObject *listener);

    void addVisual(MainVisual *visual);
    void removeVisual(MainVisual *visual);

    void      toggleMute(void);
    MuteState getMuteState(void) const;
    bool      isMuted(void) const { return getMuteState() == kMuteAll; }

    void setVolume(int volume);
    void incVolume(void);
    void decVolume(void);
    uint getVolume(void) const;

    void  setSpeed(float speed);
    void  incSpeed();
    void  decSpeed();
    float getSpeed() { return m_playSpeed; }

    void play(void);
    void stop(bool stopAll = false);
    void pause(void);
    void next(void);
    void previous(void);

    void nextAuto(void);

    bool isPlaying(void) { return m_isPlaying; }
    bool isPaused(void) { return getOutput() ? getOutput()->IsPaused() : false; }
    bool isStopped(void) { return !(isPlaying() || isPaused()); }
    bool hasClient(void) { return hasListeners(); }

    /// This will allow/disallow the mini player showing on track changes
    void autoShowPlayer(bool autoShow) { m_autoShowPlayer = autoShow; }
    bool getAutoShowPlayer(void) { return m_autoShowPlayer; }

    /// This will allow/disallow the mini player showing even using its jumppoint
    void canShowPlayer(bool canShow) { m_canShowPlayer = canShow; }
    bool getCanShowPlayer(void) { return m_canShowPlayer; }

    Decoder        *getDecoder(void) { return m_decoderHandler ? m_decoderHandler->getDecoder() : nullptr; }
    DecoderHandler *getDecoderHandler(void) { return m_decoderHandler; }
    AudioOutput    *getOutput(void) { return m_output; }

    void         loadPlaylist(void);
    void         loadStreamPlaylist(void);
    Playlist    *getCurrentPlaylist(void);
    static StreamList *getStreamList(void);

    // these add and remove tracks from the active playlist
    void         removeTrack(int trackID);
    void         addTrack(int trackID, bool updateUI);

    void         moveTrackUpDown(bool moveUp, int whichTrack);

    QList<MusicMetadata*> &getPlayedTracksList(void) { return m_playedList; }

    int          getCurrentTrackPos(void) { return m_currentTrack; }
    bool         setCurrentTrackPos(int pos);
    void         changeCurrentTrack(int trackNo);

    int          getCurrentTrackTime(void) { return m_currentTime; }

    void         activePlaylistChanged(int trackID, bool deleted);
    void         playlistChanged(int playlistID);

    void         savePosition(void);
    void         restorePosition(void);
    void         setAllowRestorePos(bool allow) { m_allowRestorePos = allow; }
    void         seek(int pos);

    MusicMetadata *getCurrentMetadata(void);
    MusicMetadata *getNextMetadata(void);
    void         sendMetadataChangedEvent(int trackID);
    void         sendTrackStatsChangedEvent(int trackID);
    void         sendAlbumArtChangedEvent(int trackID);
    void         sendTrackUnavailableEvent(int trackID);
    void         sendCDChangedEvent(void);

    void         toMap(InfoMap &infoMap);

    void         showMiniPlayer(void);
    enum RepeatMode
    { REPEAT_OFF = 0,
      REPEAT_TRACK, 
      REPEAT_ALL, 
      MAX_REPEAT_MODES 
    };
    enum ShuffleMode
    { SHUFFLE_OFF = 0, 
      SHUFFLE_RANDOM, 
      SHUFFLE_INTELLIGENT,
      SHUFFLE_ALBUM,
      SHUFFLE_ARTIST,
      MAX_SHUFFLE_MODES 
    };

    enum ResumeMode
    { RESUME_OFF,
      RESUME_FIRST,
      RESUME_TRACK,
      RESUME_EXACT,
      MAX_RESUME_MODES
    };

    RepeatMode getRepeatMode(void) { return m_repeatMode; }
    void       setRepeatMode(RepeatMode mode) { m_repeatMode = mode; }
    RepeatMode toggleRepeatMode(void);

    ShuffleMode getShuffleMode(void) { return m_shuffleMode; }
    void        setShuffleMode(ShuffleMode mode);
    ShuffleMode toggleShuffleMode(void);

    ResumeMode  getResumeMode(void);

    void getBufferStatus(int *bufferAvailable, int *bufferSize);

  public slots:
    void StartPlayback(void);
    void StopPlayback(void);

  protected:
    void customEvent(QEvent *event) override; // QObject

  private:
    void loadSettings(void);
    void stopDecoder(void);
    bool openOutputDevice(void);
    void updateLastplay(void);
    void updateVolatileMetadata(void);
    void sendVolumeChangedEvent(void);
    int  getNotificationID(const QString &hostname);
    void sendNotification(int notificationID, const QString &title, const QString &author, const QString &desc);

    void setupDecoderHandler(void);
    void decoderHandlerReady(void);

    int          m_currentTrack {-1};
    int          m_currentTime {0};

    MusicMetadata  *m_oneshotMetadata {nullptr};

    AudioOutput    *m_output          {nullptr};
    DecoderHandler *m_decoderHandler  {nullptr};

    QSet<QObject*>  m_visualisers;

    PlayMode     m_playMode           {PLAYMODE_TRACKSPLAYLIST};
    bool         m_isPlaying          {false};
    bool         m_isAutoplay         {false};
    bool         m_canShowPlayer      {true};
    bool         m_autoShowPlayer     {true};
    bool         m_wasPlaying         {false};
    bool         m_updatedLastplay    {false};
    bool         m_allowRestorePos    {true};

    int          m_lastplayDelay      {LASTPLAY_DELAY};

    ShuffleMode  m_shuffleMode        {SHUFFLE_OFF};
    RepeatMode   m_repeatMode         {REPEAT_OFF};
    ResumeMode   m_resumeModePlayback {RESUME_EXACT};
    ResumeMode   m_resumeModeEditor   {RESUME_OFF};
    ResumeMode   m_resumeModeRadio    {RESUME_TRACK};

    float        m_playSpeed          {1.0F};

    // notification
    bool m_showScannerNotifications   {true};
    QMap<QString, int>  m_notificationMap;

    // radio stuff
    QList<MusicMetadata*>  m_playedList;
    int          m_lastTrackStart     {0};
    int          m_bufferAvailable    {0};
    int          m_bufferSize         {0};

    int          m_errorCount         {0};
};

Q_DECLARE_METATYPE(MusicPlayer::ResumeMode);
Q_DECLARE_METATYPE(MusicPlayer::RepeatMode);
Q_DECLARE_METATYPE(MusicPlayer::ShuffleMode);

// This global variable contains the MusicPlayer instance for the application
extern MPUBLIC MusicPlayer *gPlayer;

 // This stores the last MythMediaDevice that was detected:
extern MPUBLIC QString gCDdevice;

#endif
