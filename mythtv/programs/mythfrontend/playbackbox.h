#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdatetime.h>
#include "libmyth/mythwidgets.h"

#include <pthread.h>

class QListViewItem;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;

class PlaybackBox : public MythDialog
{
    Q_OBJECT
  public:
    typedef enum { Play, Delete } BoxType;

    PlaybackBox(BoxType ltype, QWidget *parent = 0, const char *name = 0);
   ~PlaybackBox(void);
   
    void customEvent(QCustomEvent *e);
 
  protected slots:
    void selected(QListViewItem *);
    void remove(QListViewItem *);
    void play(QListViewItem *);
    void changed(QListViewItem *);
    void timeout(void);

  private:
    QListViewItem *FillList(bool selectsomething);
    void UpdateProgressBar(void);

    BoxType type;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;
    QLabel *pixlabel;

    MythListView *listview;

    QLabel *freespace;
    QProgressBar *progressbar;

    int descwidth;
    int titlewidth;

    void killPlayer(void);
    void startPlayer(ProgramInfo *rec);

    void doRemove(QListViewItem *);
    void promptEndOfRecording(QListViewItem *);

    bool fileExists(const QString &pathname);

    QTimer *timer;
    NuppelVideoPlayer *nvp;
    RingBuffer *rbuffer;
    pthread_t decoder;

    QDateTime lastUpdateTime;
    bool ignoreevents;
    bool playbackPreview;
    bool generatePreviewPixmap;
    bool displayChanNum;
    QString dateformat;
    QString timeformat;
};

#endif
