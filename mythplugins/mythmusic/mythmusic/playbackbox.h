#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

// qt
#include <QKeyEvent>
#include <Q3ValueVector>

// mythtv
#include <mythwidgets.h>
#include <dialogbox.h>
#include <audiooutput.h>

// mythmusic
#include "mainvisual.h"
#include "metadata.h"
#include "playlist.h"
#include "editmetadata.h"
#include "databasebox.h"
#include "musicplayer.h"

class Output;
class Decoder;
class QTimer;

class PlaybackBoxMusic : public MythThemedDialog
{
    Q_OBJECT

  public:

    typedef Q3ValueVector<int> IntVector;

    PlaybackBoxMusic(MythMainWindow *parent, QString window_name,
                     QString theme_filename,
                     const QString &cd_device, const char *name = 0);

    ~PlaybackBoxMusic(void);

    void customEvent(QEvent *);
    void keyPressEvent(QKeyEvent *e);
    void constructPlaylistTree();

    bool onMediaEvent(MythMediaDevice *pDev);

  public slots:

    void play();
    void stop();
    void pause();
    void previous();
    void next();
    void seekforward();
    void seekback();
    void seek(int);
    void stopAll();
    void setShuffleMode(MusicPlayer::ShuffleMode mode);
    void toggleShuffle();
    void increaseRating();
    void decreaseRating();
    void setRepeatMode(MusicPlayer::RepeatMode mode);
    void toggleRepeat();
    void editPlaylist();
    void nextAuto();
    void showEditMetadataDialog();
    void checkForPlaylists();
    void handleTreeListSignals(int, IntVector*);
    void visEnable();
    void bannerDisable();
    void changeVolume(bool up_or_down);
    void changeSpeed(bool up_or_down);
    void toggleMute();
    void toggleUpmix();
    void resetTimer();
    void hideVolume(){showVolume(false);}
    void showVolume(bool on_or_off);
    void showSpeed(bool on_or_off);
    void showProgressBar();
    void wipeTrackInfo();
    void toggleFullBlankVisualizer();
    void end();
    void resetScrollCount();
    void showAlbumArtImage(Metadata *mdata);
    void wipeAlbumArt();

    void handlePush(QString buttonname);

    void occasionallyCheckCD();

    // popup menu
    void showMenu();
    void closePlaylistPopup();
    void allTracks();
    void byArtist();
    void byAlbum();
    void byGenre();
    void byYear();
    void byTitle();
    void fromCD();
    void showSmartPlaylistDialog();
    void showSearchDialog();
    bool getInsertPLOptions(InsertPLOption &insertOption,
                            PlayPLOption &playOption, bool &bRemoveDups);

  signals:

    void dummy();   // debugging

  private:

    void wireUpTheme();
    void updatePlaylistFromQuickPlaylist(QString whereClause);
    void updatePlaylistFromSmartPlaylist();
    void doUpdatePlaylist(QString whereClause);
    void startVisualizer(void);
    void stopVisualizer(void);
    void CycleVisualizer(void);
    void updatePlaylistFromCD(void);
    void setTrackOnLCD(Metadata *mdata);
    void updateTrackInfo(Metadata *mdata);
    void postUpdate();
    void playFirstTrack();
    void bannerEnable(QString text, int millis);
    void bannerEnable(Metadata *mdata, bool fullScreen = false);
    void bannerToggle(Metadata *mdata);
    void savePosition(uint position);
    void restorePosition(const QString &position);
    void pushButton(UIPushButtonType *button);
    QString getTimeString(int exTime, int maxTime);

    QString playfile;
    QString statusString;
    QString curSmartPlaylistCategory;
    QString curSmartPlaylistName;

    bool listAsShuffled;
    int outputBufferSize;
    int currentTime, maxTime;
    int scrollCount;
    bool scrollingDown;

    Metadata *curMeta;

    unsigned int resumemode;

    bool menufilters;

    ReadCDThread *cd_reader_thread;
    QTimer *cd_watcher;
    bool cd_checking_flag;
    bool scan_for_cd;
    QString m_CDdevice;

    MainVisual *mainvisual;

    bool fullscreen_blank; 
    QStringList visual_modes; 
    unsigned int current_visual;
    int visual_mode_delay;
    QTimer *visual_mode_timer;
    QTimer *lcd_update_timer;
    QTimer *speed_scroll_timer;
    int visualizer_status;

    bool showrating;
    bool vis_is_big;
    bool tree_is_done;
    bool first_playlist_check;

    QTimer  *waiting_for_playlists_timer;
    QTimer  *volume_display_timer;

    bool cycle_visualizer;
    bool random_visualizer;
    bool show_album_art;
    bool show_whole_tree;
    bool keyboard_accelerators;
    bool volume_control;

    QString exit_action;

    MythPopupBox *playlist_popup;

    //
    //  Theme-related "widgets"
    //

    UIManagedTreeListType *music_tree_list;

    UITextType            *title_text;
    UITextType            *artist_text;
    UITextType            *album_text;
    UITextType            *time_text;
    UITextType            *info_text;
    UITextType            *current_visualization_text;

    UITextType            *shuffle_state_text;
    UITextType            *repeat_state_text;

    UIRepeatedImageType   *ratings_image;
    UIBlackHoleType       *visual_blackhole;

    UIStatusBarType       *volume_status;
    UIStatusBarType       *progress_bar;
    UITextType            *speed_status;

    UIPushButtonType      *prev_button;
    UIPushButtonType      *rew_button;
    UIPushButtonType      *pause_button;
    UIPushButtonType      *play_button;
    UIPushButtonType      *stop_button;
    UIPushButtonType      *ff_button;
    UIPushButtonType      *next_button;

    UIPushButtonType      *m_pushedButton;

    UIImageType           *albumart_image;

    UITextButtonType      *shuffle_button;
    UITextButtonType      *repeat_button;
    UITextButtonType      *pledit_button;
    UITextButtonType      *vis_button;

    MythProgressDialog    *progress;
    enum { kProgressNone, kProgressMusic } progress_type;
};


#endif
