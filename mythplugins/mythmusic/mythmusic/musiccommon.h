#ifndef MUSICCOMMON_H_
#define MUSICCOMMON_H_

// qt
#include <QKeyEvent>
#include <QObject>

// mythtv
#include <audiooutput.h>
#include <mythscreentype.h>

// mythmusic
#include "metadata.h"
#include "playlist.h"
#include "musicplayer.h"

class Output;
class Decoder;
class QTimer;
class MythUIProgressBar;
class MythUIImage;
class MythUIText;
class MythUIStateType;
class MythUIButton;
class MythUIVideo;

class MPUBLIC MusicCommon : public MythScreenType
{
    Q_OBJECT

  protected:

    MusicCommon(MythScreenStack *parent, const QString &name);
    ~MusicCommon(void);

    bool CreateCommon(void);

    void switchView(int view);

    virtual void customEvent(QEvent *e);
    bool keyPressEvent(QKeyEvent *e);

  protected slots:

    void play(void);
    void stop(void);
    void pause(void);
    void previous(void);
    void next(void);
    void seekforward(void);
    void seekback(void);
    void seek(int);
    void stopAll(void);
    void setShuffleMode(MusicPlayer::ShuffleMode mode);
    void toggleShuffle(void);
    void increaseRating(void);
    void decreaseRating(void);
    void setRepeatMode(MusicPlayer::RepeatMode mode);
    void toggleRepeat(void);
    void editPlaylist(void);
    void nextAuto(void);

    void showViewMenu(void);
    void showPlaylistMenu(void);
    void showExitMenu(void);

    void playlistItemClicked(MythUIButtonListItem *item);
    void playlistItemSelected(MythUIButtonListItem *item);

    void metadataChanged(void);

  protected:

    QString getTimeString(int exTime, int maxTime);
    void    updateProgressBar(void);
    void    updateAlbumArtImage(Metadata *mdata);
    void    setTrackOnLCD(Metadata *mdata);
    void    editTrackInfo(Metadata *mdata);
    void    updateTrackInfo(Metadata *mdata);
    void    showTrackInfo(Metadata *mdata);
    void    updateUIPlaylist(void);
    void    updatePlaylistStats(void);

    void changeVolume(bool up);
    void changeSpeed(bool up);
    void toggleMute(void);
    void toggleUpmix(void);
    void showVolume(void);
    void updateVolume(uint volume, bool muted);
    void showSpeed(bool show);

    void cycleVisualizer(void);
    void resetVisualiserTimer(void);

    // visualiser stuff
    MainVisual            *m_mainvisual;
    bool                   m_fullscreenBlank;
    bool                   m_cycleVisualizer;
    bool                   m_randomVisualizer;

    QStringList            m_visualModes;
    unsigned int           m_currentVisual;
    int                    m_visualModeDelay;
    QTimer                *m_visualModeTimer;

    bool                   m_moveTrackMode;
    bool                   m_movingTrack;

    bool                   m_controlVolume;

    int                    m_currentTime;
    int                    m_maxTime;

    int                    m_playlistTrackCount;
    int                    m_playlistPlayedTime;
    int                    m_playlistMaxTime;

    int                    m_currentTrack;

    MythUIText            *m_titleText;
    MythUIText            *m_artistText;
    MythUIText            *m_albumText;
    MythUIText            *m_timeText;
    MythUIText            *m_infoText;
    MythUIText            *m_visualText;

    MythUIStateType       *m_shuffleState;
    MythUIStateType       *m_repeatState;

    MythUIStateType       *m_movingTracksState;

    MythUIStateType       *m_ratingState;

    MythUIProgressBar     *m_trackProgress;
    MythUIText            *m_trackProgressText;
    MythUIText            *m_trackSpeed;

    MythUIStateType       *m_muteState;
    MythUIText            *m_volumeText;

    MythUIProgressBar     *m_playlistProgress;
    MythUIText            *m_playlistProgressText;
    MythUIText            *m_playlistLengthText;

    MythUIButton          *m_prevButton;
    MythUIButton          *m_rewButton;
    MythUIButton          *m_pauseButton;
    MythUIButton          *m_playButton;
    MythUIButton          *m_stopButton;
    MythUIButton          *m_ffButton;
    MythUIButton          *m_nextButton;

    MythUIImage           *m_coverartImage;

    MythUIButtonList      *m_currentPlaylist;

    MythUIVideo           *m_visualizerVideo;
};

class MPUBLIC MythMusicVolumeDialog : public MythScreenType
{
    Q_OBJECT
  public:
    MythMusicVolumeDialog(MythScreenStack *parent, const char *name);
    ~MythMusicVolumeDialog(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);
    void customEvent(QEvent *event);

  protected:
    void increaseVolume(void);
    void decreaseVolume(void);
    void toggleMute(void);

    void updateDisplay(void);

    QTimer            *m_displayTimer;
    MythUIText        *m_messageText;
    MythUIText        *m_volText;
    MythUIStateType   *m_muteState;
    MythUIProgressBar *m_volProgress;
};

class MPUBLIC TrackInfoDialog : public MythScreenType
{
  Q_OBJECT
  public:
    TrackInfoDialog(MythScreenStack *parent, Metadata *mdata, const char *name);
    ~TrackInfoDialog(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

  protected:
    Metadata *m_metadata;
};

#endif
