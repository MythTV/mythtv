#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qwidget.h>
#include "libmyth/mythwidgets.h"

#include <pthread.h>

class QListViewItem;
class MyListView;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;
class MythContext;

class PlaybackBox : public MyDialog
{
    Q_OBJECT
  public:
    typedef enum { Play, Delete } BoxType;

    PlaybackBox(MythContext *context, BoxType ltype, QWidget *parent = 0, 
                const char *name = 0);
   ~PlaybackBox(void);
    
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

    QLabel *title;
    QLabel *subtitle;
    QLabel *description;
    QLabel *date;
    QLabel *chan;
    QLabel *pixlabel;

    MyListView *listview;

    QLabel *freespace;
    QProgressBar *progressbar;

    int descwidth;

    void killPlayer(void);
    void startPlayer(ProgramInfo *rec);

    QTimer *timer;
    NuppelVideoPlayer *nvp;
    RingBuffer *rbuffer;
    pthread_t decoder;
};

#endif
