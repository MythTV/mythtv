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

class MusicPlayerEvent : public MythEvent
{
    public:
        MusicPlayerEvent(Type t, int id) :
        MythEvent(t), TrackID(id), Volume(0), IsMuted(false) {}
        MusicPlayerEvent(Type t, uint vol, bool muted) :
        MythEvent(t), TrackID(0), Volume(vol), IsMuted(muted) {}
        ~MusicPlayerEvent() {}

        virtual MythEvent *clone(void) const { return new MusicPlayerEvent(*this); }

        // for track changed/added/deleted/metadata changed events
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
};

class MusicPlayer : public QObject, public MythObservable
{
    Q_OBJECT

  public:
     MusicPlayer(QObject *parent, const QString &dev);

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

    GenericTree *constructPlaylist(void);
    GenericTree *getPlaylistTree() { return m_playlistTree; }
    void         setCurrentNode(GenericTree *node);
    GenericTree *getCurrentNode(void) { return m_currentNode; }

    void         loadPlaylist(void);
    Playlist    *getPlaylist(void) { return m_currentPlaylist; }

    int          getCurrentTrackPos(void) { return m_currentTrack; }
    void         setCurrentTrackPos(int pos);

    void         playlistChanged(int trackID, bool deleted);

    QString      getRouteToCurrent(void);

    void         savePosition(void);
    void         restorePosition(const QString &position); //TODO remove
    void         restorePosition(int position);
    void         seek(int pos);

    Metadata    *getCurrentMetadata(void);
    Metadata    *getDisplayMetadata(void) { return &m_displayMetadata; }
    void         refreshMetadata(void);
    void         sendMetadataChangedEvent(int trackID);

    void         toMap(QHash<QString, QString> &infoMap);

    void showMiniPlayer(void);

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
    void        setShuffleMode(ShuffleMode mode) { m_shuffleMode = mode; }
    ShuffleMode toggleShuffleMode(void);

    ResumeMode  getResumeMode(void) { return m_resumeMode; }

  protected:
    ~MusicPlayer(void);
    void customEvent(QEvent *event);

  private:
    void stopDecoder(void);
    void openOutputDevice(void);
    QString getFilenameFromID(int id);
    void updateLastplay(void);
    void sendVolumeChangedEvent(void);

    void setupDecoderHandler(void);
    void decoderHandlerReady(void);
    void decoderHandlerInfo(const QString&, const QString&);
    void decoderHandlerOperationStart(const QString &);
    void decoderHandlerOperationStop();

    Playlist    *m_currentPlaylist;
    int          m_currentTrack;

    GenericTree *m_playlistTree;

    GenericTree *m_currentNode;
    Metadata    *m_currentMetadata;
    int          m_currentTime;

    Metadata     m_displayMetadata;

    AudioOutput    *m_output;
    DecoderHandler *m_decoderHandler;

    QSet<QObject*>  m_visualisers;

    QString      m_CDdevice;

    bool         m_isPlaying;
    bool         m_isAutoplay;
    bool         m_canShowPlayer;
    bool         m_autoShowPlayer;
    bool         m_wasPlaying;
    bool         m_updatedLastplay;

    int          m_lastplayDelay;

    ShuffleMode  m_shuffleMode;
    RepeatMode   m_repeatMode;
    ResumeMode   m_resumeMode;

    float        m_playSpeed;
};

// This global variable contains the MusicPlayer instance for the application
extern MPUBLIC MusicPlayer *gPlayer;

#endif
