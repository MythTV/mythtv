#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qurloperator.h>
#include <qptrlist.h>
#include <qtoolbutton.h>
#include <qmutex.h>

#include "metadata.h"

class QLabel;
class QString;
class MQ3;
class Buffer;
class Decoder;
class Output;
class QIODevice;
class QSqlDatabase;
class QListView;
class QListViewItem;
class QSlider;
class ScrollLabel;
class MyButton;

class PlaybackBox : public QDialog
{
    Q_OBJECT
  public:
    PlaybackBox(QSqlDatabase *ldb, QValueList<Metadata> *playlist,
                QWidget *parent = 0, const char *name = 0);

    ~PlaybackBox(void);

    void Show(void);

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
    void changeSong();
    void stopAll();
    void toggleShuffle();
    void toggleRepeat();
    void editPlaylist();
    void nextAuto();

  private slots:
    void startseek();
    void doneseek();

  private:
    void setupListView(void);

    void setupPlaylist(bool toggle = false);

    QPixmap scalePixmap(const char **xpmdata);

    float wmult, hmult;

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

    QListView *playview;
    QPtrList<QListViewItem> listlist;

    QSlider *seekbar;

    MyButton *randomize;
    MyButton *repeat;

    bool shufflemode;
    bool repeatmode;

    bool isplaying;
};

#endif
