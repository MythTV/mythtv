#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include <qdialog.h>

#include <pthread.h>

class QSqlDatabase;
class QListViewItem;
class QLabel;
class TV;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;

class PlaybackBox : public QDialog
{
    Q_OBJECT
  public:
    PlaybackBox(QString prefix, TV *ltv, QSqlDatabase *ldb, 
                QWidget *parent = 0, const char *name = 0);
   ~PlaybackBox(void);
    
    void Show();
  
  protected slots:
    void selected(QListViewItem *);
    void changed(QListViewItem *);
    void timeout(void);

  private:
    QSqlDatabase *db;
    TV *tv;

    QString fileprefix;

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;
    QLabel *pixlabel;

    float wmult, hmult;

    void killPlayer(void);
    void startPlayer(ProgramInfo *rec);

    QTimer *timer;
    NuppelVideoPlayer *nvp;
    RingBuffer *rbuffer;
    pthread_t decoder;
};

#endif
