#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qurloperator.h>
#include <qptrlist.h>
#include <qtoolbutton.h>
#include <qtimer.h>
#include <qmutex.h>
#include <qvaluevector.h>

#include <mythtv/mythwidgets.h>

#include "metadata.h"
#include "playlist.h"

class QLabel;
class QString;
class MQ3;
class Buffer;
class Decoder;
class Output;
class QIODevice;
class QSqlDatabase;
class QListViewItem;
class QSlider;
class QAction;
class ScrollLabel;
class MyToolButton;
class MainVisual;

class PlaybackBox : public MythDialog
{
    Q_OBJECT
  public:
    PlaybackBox(PlaylistsContainer *the_playlists,
                AllMusic *the_music,
                QWidget *parent = 0, const char *name = 0);

    ~PlaybackBox(void);

    void closeEvent(QCloseEvent *);
    void customEvent(QCustomEvent *);
    void showEvent(QShowEvent *);

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
    void setTimeMode(unsigned int mode);
    void toggleTime();
    void increaseRating();
    void decreaseRating();
    void setRepeatMode(unsigned int mode);
    void toggleRepeat();
    void editPlaylist();
    void togglePlaylistView();
    void nextAuto();
    void visEnable();
    void CycleVisualizer();
    void resetTimer();
    void restartTimer();
    void jumpToItem(QListViewItem *curItem);
    void jumpToItem();   
    void keyPressFromVisual(QKeyEvent *e);
    void checkForPlaylists();

  private slots:
    void startseek();
    void doneseek();

  private:
    void setupListView(void);

    double computeIntelligentWeight(Metadata *mdata, double currentDateTime);
    void setupPlaylist(void);
    void sortListAsShuffled(void);

    QPixmap scalePixmap(const char **xpmdata);

    QIODevice *input;
    Output *output;
    Decoder *decoder;

    QString playfile;
    QString statusString, timeString, infoString;
    QString shuffleString, repeatString, ratingString;

    enum TimeDisplayMode
    { TRACK_ELAPSED = 0, 
      TRACK_REMAIN,
      PLIST_ELAPSED, 
      PLIST_REMAIN,
      MAX_TIME_DISPLAY_MODES 
    };
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
      MAX_SHUFFLE_MODES 
    };

    unsigned int timeMode;
    int plTime, plElapsed;

    bool firstShow, seeking, listAsShuffled;
    int outputBufferSize;
    int currentTime, maxTime;

    QPtrList<Metadata> *plist;
    QValueVector<int> playlistorder;
    QMutex listlock;

    int playlistindex;
    int shuffleindex;
    Metadata *curMeta;
    Metadata dummy_data;

    QLabel *shufflelabel;
    QLabel *repeatlabel;
    QLabel *timelabel;
    QLabel *infolabel;
    QLabel *ratinglabel;
    ScrollLabel *titlelabel;

    MythListView *playview;
    QPtrList<QListViewItem> listlist;

    QSlider *seekbar;

    MythToolButton *randomize, *repeat, *pledit, *vis, *pauseb, *prevb,
                   *prevfileb, *stopb, *nextb, *nextfileb, *rateup, *ratedn, 
                   *playb;

    QAction *prevfileAction, *prevAction, *pauseAction, *playAction,
            *stopAction, *nextAction, *nextfileAction, *rateupAction,
            *ratednAction, *shuffleAction, *repeatAction, *pleditAction,
            *visAction, *timeDisplaySelect, *playlistViewAction;

    unsigned int shufflemode;
    unsigned int repeatmode;

    bool isplaying;

    MainVisual *mainvisual;

    QString visual_mode;
    int visual_mode_delay;
    QTimer *visual_mode_timer;
    QTimer *lcd_update_timer;
    QTimer *playlist_timer;
    bool visualizer_is_active;
    bool keyboard_accelerator_flag;

    bool showrating;
    
    AllMusic *all_music;
    PlaylistsContainer *all_playlists;

    QTimer  *waiting_for_playlists_timer;
    int      wait_counter;    

    bool cycle_visualizer;
};

#endif
