#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qurloperator.h>
#include <qptrlist.h>
#include <qtoolbutton.h>
#include <qtimer.h>
#include <qmutex.h>

#include <mythtv/mythwidgets.h>

#include "metadata.h"

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
class ScrollLabel;
class MyToolButton;
class MainVisual;

class PlaybackBox : public MythDialog
{
    Q_OBJECT
  public:
    PlaybackBox(QSqlDatabase *ldb, QValueList<Metadata> *playlist,
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
    void toggleShuffle();
    void toggleRepeat();
    void editPlaylist();
    void nextAuto();
    void visEnable();
    void resetTimer();
    void restartTimer();
    void jumpToItem(QListViewItem *curItem);
    void jumpToItem();   
 
  private slots:
    void startseek();
    void doneseek();

  private:
    void setupListView(void);

    void setupPlaylist(void);

    QPixmap scalePixmap(const char **xpmdata);

    QIODevice *input;
    Output *output;
    Decoder *decoder;

    QString playfile;
    QString statusString, timeString, infoString;

    bool firstShow, remainingTime, seeking;
    int outputBufferSize;
    int currentTime, maxTime;

    QSqlDatabase *db;

    QValueList<Metadata> *plist;
    QValueList<int> playlistorder;
    QMutex listlock;

    int playlistindex;
    int shuffleindex;
    Metadata curMeta;

    QLabel *timelabel;
    ScrollLabel *titlelabel;

    MythListView *playview;
    QPtrList<QListViewItem> listlist;

    QSlider *seekbar;

    MythToolButton *randomize;
    MythToolButton *repeat;
    MythToolButton *pledit;
    MythToolButton *vis;
    MythToolButton *pauseb;

    bool shufflemode;
    bool repeatmode;

    bool isplaying;

    MainVisual *mainvisual;

    QString visual_mode;
    int	visual_mode_delay;
    QTimer *visual_mode_timer;
    QTimer *lcd_update_timer;
    QTimer *playlist_timer;
    bool visualizer_is_active;
};

#endif
