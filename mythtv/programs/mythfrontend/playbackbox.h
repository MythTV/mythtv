#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdialog.h>

#include <pthread.h>

class QSqlDatabase;
class QListViewItem;
class MyListView;
class QLabel;
class QProgressBar;
class TV;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;
class MythContext;

class PlaybackBox : public QDialog
{
    Q_OBJECT
  public:
    typedef enum { Play, Delete } BoxType;

    PlaybackBox(MythContext *context, TV *ltv, QSqlDatabase *ldb, 
                BoxType ltype, QWidget *parent = 0, const char *name = 0);
   ~PlaybackBox(void);
    
    void Show();
  
  protected slots:
    void selected(QListViewItem *);
    void remove(QListViewItem *);
    void play(QListViewItem *);
    void changed(QListViewItem *);
    void timeout(void);

  private:
    void GetFreeSpaceStats(int &totalspace, int &usedspace); 
    void UpdateProgressBar(void);

    BoxType type;
    QSqlDatabase *db;
    TV *tv;

    QString fileprefix;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;
    QLabel *pixlabel;

    MyListView *listview;

    QLabel *freespace;
    QProgressBar *progressbar;

    float wmult, hmult;
    int descwidth;

    void killPlayer(void);
    void startPlayer(ProgramInfo *rec);

    QTimer *timer;
    NuppelVideoPlayer *nvp;
    RingBuffer *rbuffer;
    pthread_t decoder;

    MythContext *m_context;
};

#endif
