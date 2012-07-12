#ifndef MUSICPLAYER_H_
#define MUSICPLAYER_H_

// mythtv
#include <mythdialogs.h>
#include <audiooutput.h>
#include <mythobservable.h>

// mythmusic
#include "metadata.h"
#include "decoderhandler.h"

class AudioOutput;
class MainVisual;
class Playlist;
class CDWatcherThread;

class MusicPlayerEvent : public MythEvent
{
    public:
        MusicPlayerEvent(Type t, int id) :
            MythEvent(t), TrackID(id), Volume(0), IsMuted(false) {}
        MusicPlayerEvent(Type t, uint vol, bool muted) :
            MythEvent(t), TrackID(0), Volume(vol), IsMuted(muted) {}
        ~MusicPlayerEvent() {}

        virtual MythEvent *clone(void) const { return new MusicPlayerEvent(*this); }

        // for track changed/added/deleted/metadata changed/playlist changed events
        int TrackID;

        // for volume changed event
        uint Volume;
        bool IsMuted;

        static Type TrackChangeEvent;
        static Type VolumeChangeEvent;
        static Type TrackAddedEvent;
        static Type TrackRemovedEvent;
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
     MusicPlayer(QObject *parent, const QString &dev);
    ~MusicPlayer(void);

    enum PlayMode
    {
      PLAYMODE_TRACKS = 0,
      PLAYMODE_RADIO,
    };

    void setPlayMode(PlayMode mode);
    PlayMode getPlayMode(void) { return m_playMode; }

    void playFile(const Metadata &meta);

    void addListener(QObject *listener);
    void removeListener(QObject *listener);

    void addVisual(MainVisual *visual);
    void removeVisual(MainVisual *visual);

    void setCDDevice(const QString &dev) { m_CDdevice = dev; }

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

    Decoder        *getDecoder(void) { return m_decoderHandler ? m_decoderHandler->getDecoder() : NULL; }
    DecoderHandler *getDecoderHandler(void) { return m_decoderHandler; }
    AudioOutput    *getOutput(void) { return m_output; }

    void         loadPlaylist(void);
    Playlist    *getPlaylist(void) { return m_currentPlaylist; }
    StreamList  *getStreamList(void) { return gMusicData->all_streams->getStreams(); }

    // these add and remove tracks from the active playlist
    void         removeTrack(int trackID);
    void         addTrack(int trackID, bool updateUI);

    void         moveTrackUpDown(bool moveUp, int whichTrack);

    QList<Metadata*> &getPlayedTracksList(void) { return m_playedList; }

    int          getCurrentTrackPos(void) { return m_currentTrack; }
    bool         setCurrentTrackPos(int pos);
    void         changeCurrentTrack(int trackNo);

    void         activePlaylistChanged(int trackID, bool deleted);
    void         playlistChanged(int playlistID);

    void         savePosition(void);
    void         restorePosition(void);
    void         setAllowRestorePos(bool allow) { m_allowRestorePos = allow; }
    void         seek(int pos);

    Metadata    *getCurrentMetadata(void);
    Metadata    *getNextMetadata(void);
    void         sendMetadataChangedEvent(int trackID);
    void         sendTrackStatsChangedEvent(int trackID);
    void         sendAlbumArtChangedEvent(int trackID);
    void         sendCDChangedEvent(void);

    void         toMap(QHash<QString, QString> &infoMap);

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

    ResumeMode  getResumeMode(void) { return m_resumeMode; }

    void getBufferStatus(int *bufferAvailable, int *bufferSize);

  protected:
    void customEvent(QEvent *event);

  private:
    void loadSettings(void);
    void stopDecoder(void);
    bool openOutputDevice(void);
    void updateLastplay(void);
    void updateVolatileMetadata(void);
    void sendVolumeChangedEvent(void);

    void setupDecoderHandler(void);
    void decoderHandlerReady(void);

    Playlist    *m_currentPlaylist;
    int          m_currentTrack;
    int          m_currentTime;

    Metadata    *m_currentMetadata;
    Metadata    *m_oneshotMetadata;

    AudioOutput    *m_output;
    DecoderHandler *m_decoderHandler;

    QSet<QObject*>  m_visualisers;

    QString      m_CDdevice;

    PlayMode     m_playMode;
    bool         m_isPlaying;
    bool         m_isAutoplay;
    bool         m_canShowPlayer;
    bool         m_autoShowPlayer;
    bool         m_wasPlaying;
    bool         m_updatedLastplay;
    bool         m_allowRestorePos;

    int          m_lastplayDelay;

    ShuffleMode  m_shuffleMode;
    RepeatMode   m_repeatMode;
    ResumeMode   m_resumeMode;

    float        m_playSpeed;

    // cd stuff
    CDWatcherThread *m_cdWatcher;

    // radio stuff
    QList<Metadata*>  m_playedList;
    int               m_lastTrackStart;
    int               m_bufferAvailable;
    int               m_bufferSize;
};

Q_DECLARE_METATYPE(MusicPlayer::RepeatMode);
Q_DECLARE_METATYPE(MusicPlayer::ShuffleMode);

// This global variable contains the MusicPlayer instance for the application
extern MPUBLIC MusicPlayer *gPlayer;


///////////////////////////////////////////////////////////////////////////////

class CDWatcherThread : public QThread
{
  public:

    CDWatcherThread(const QString &dev);
    virtual void run(void);
    bool    statusChanged(void) { return m_cdStatusChanged; }
    QMutex *getLock(void) { return &m_musicLock; }
    void    stop(void) { m_stopped = true; }

  private:

    bool    m_stopped;
    QString m_cdDevice;
    bool    m_cdStatusChanged;
    QMutex  m_musicLock;
};

#endif
