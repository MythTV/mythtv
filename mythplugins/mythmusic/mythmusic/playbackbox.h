#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>
#include <mythtv/audiooutput.h>

#include "mainvisual.h"
#include "metadata.h"
#include "playlist.h"
#include "editmetadata.h"
#include "databasebox.h"

class Output;
class Decoder;

class PlaybackBoxMusic : public MythThemedDialog
{
    Q_OBJECT

  public:

    //
    // Now featuring a themed constructor ... YAH!!!
    //
    typedef QValueVector<int> IntVector;
    
    PlaybackBoxMusic(MythMainWindow *parent, QString window_name,
                QString theme_filename, PlaylistsContainer *the_playlists,
                AllMusic *the_music, const char *name = 0);

    ~PlaybackBoxMusic(void);

    void closeEvent(QCloseEvent *);
    void customEvent(QCustomEvent *);
    void showEvent(QShowEvent *);
    void keyPressEvent(QKeyEvent *e);
    void constructPlaylistTree();

    bool onMediaEvent(MythMediaDevice *pDev);

  public slots:

    void play();
    void pause();
    void stop();
    void stopDecoder();
    void previous();
    void next();
    void seekforward();
    void seekback();
    void seek(int);
    void stopAll();
    void setShuffleMode(unsigned int mode);
    void toggleShuffle();
    void increaseRating();
    void decreaseRating();
    void setRepeatMode(unsigned int mode);
    void toggleRepeat();
    void editPlaylist();
    void nextAuto();
    void showEditMetadataDialog();
    void checkForPlaylists();
    void handleTreeListSignals(int, IntVector*);
    void visEnable();
    void bannerDisable();
    void changeVolume(bool up_or_down);
    void toggleMute();
    void resetTimer();
    void hideVolume(){showVolume(false);}
    void showVolume(bool on_or_off);
    void wipeTrackInfo();
    void toggleFullBlankVisualizer();
    void end();

    void occasionallyCheckCD();

    // popup menu
    void showMenu();
    void closePlaylistPopup();
    void allTracks();
    void byArtist();
    void byAlbum();
    void byGenre();
    void byYear();    
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
    void CycleVisualizer(void);
    void updatePlaylistFromCD(void);
    void setTrackOnLCD(Metadata *mdata);
    void updateTrackInfo(Metadata *mdata);
    void openOutputDevice(void);
    void postUpdate();
    void playFirstTrack();
    void bannerEnable(Metadata *mdata);
    void bannerToggle(Metadata *mdata);

    QIODevice *input;
    AudioOutput *output;
    Decoder *decoder;

    QString playfile;
    QString statusString;
    QString curSmartPlaylistCategory;
    QString curSmartPlaylistName;
    
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
      MAX_SHUFFLE_MODES 
    };

    bool listAsShuffled;
    int outputBufferSize;
    int currentTime, maxTime;

    Metadata *curMeta;


    unsigned int shufflemode;
    unsigned int repeatmode;

    bool isplaying;
    bool lcd_volume_visible;

    bool menufilters;

    ReadCDThread *cd_reader_thread;
    QTimer *cd_watcher;
    bool cd_checking_flag;
    bool scan_for_cd;

    MainVisual *mainvisual;

    QString visual_mode;
    int visual_mode_delay;
    QTimer *visual_mode_timer;
    QTimer *lcd_update_timer;
    QTimer *banner_timer;
    int visualizer_status;

    bool showrating;
    bool vis_is_big;
    bool tree_is_done;
    bool first_playlist_check;
    
    AllMusic *all_music;
    PlaylistsContainer *all_playlists;

    QTimer  *waiting_for_playlists_timer;
    QTimer  *volume_display_timer;

    GenericTree *playlist_tree;
    
    bool cycle_visualizer;
    bool show_whole_tree;
    bool keyboard_accelerators;
    bool volume_control;

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
    
    UIRepeatedImageType   *ratings_image;
    UIBlackHoleType       *visual_blackhole;

    UIStatusBarType       *volume_status;

    UIPushButtonType      *prev_button;
    UIPushButtonType      *rew_button;
    UIPushButtonType      *pause_button;
    UIPushButtonType      *play_button;
    UIPushButtonType      *stop_button;
    UIPushButtonType      *ff_button;
    UIPushButtonType      *next_button;

    UITextButtonType      *shuffle_button;
    UITextButtonType      *repeat_button;
    UITextButtonType      *pledit_button;
    UITextButtonType      *vis_button;

    MythProgressDialog    *progress;
    enum { kProgressNone, kProgressMusic } progress_type;
};


#endif
