#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"

#include <pthread.h>

class QListViewItem;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;

class LayerSet;

class PlaybackBox : public MythDialog
{
    Q_OBJECT
  public:
    typedef enum { Play, Delete } BoxType;

    PlaybackBox(BoxType ltype, MythMainWindow *parent, const char *name = 0);
   ~PlaybackBox(void);
   
    void customEvent(QCustomEvent *e);
  
  signals:
    void killTheApp();
 
  protected slots:
    void timeout(void);

    void cursorLeft();
    void cursorRight();
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void selected();
    void playSelected();
    void deleteSelected();
    void expireSelected();

    void doDelete();
    void noDelete();

    void doAutoExpire();
    void noAutoExpire();

    void exitWin();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void FillList(void);
    void UpdateProgressBar(void);

    QString cutDown(QString, QFont *, int);
    QPixmap getPixmap(ProgramInfo *);
    QPainter backup;
    void play(ProgramInfo *);
    void remove(ProgramInfo *);
    void expire(ProgramInfo *);

    bool skipUpdate;
    bool noUpdate;
    bool pageDowner;
    ProgramInfo *curitem;
    ProgramInfo *delitem;

    void LoadWindow(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void parsePopup(QDomElement &);
    void parseContainer(QDomElement &);

    int skipNum;
    int skipCnt;
    int listCount;
    bool inTitle;
    bool playingVideo;
    bool leftRight;
    int curTitle;
    int curShowing;
    QString *titleData;
    QMap<QString, QString> showList;
    QMap<QString, ProgramInfo> showData;
    QMap<QString, ProgramInfo> showDateData;

    BoxType type;

    void resizeImage(QPixmap *, QString);
    void killPlayer(void);
    void startPlayer(ProgramInfo *rec);

    void doRemove(ProgramInfo *);
    void promptEndOfRecording(ProgramInfo *);
    void showDeletePopup(int);

    bool fileExists(ProgramInfo *pginfo);

    QTimer *timer;
    NuppelVideoPlayer *nvp;
    RingBuffer *rbuffer;
    pthread_t decoder;

    QDateTime lastUpdateTime;
    bool ignoreevents;
    bool graphicPopup;
    bool playbackPreview;
    bool generatePreviewPixmap;
    bool displayChanNum;
    QString dateformat;
    QString timeformat;

    void grayOut(QPainter *);
    void updateBackground(void);
    void updateVideo(QPainter *);
    void updateShowTitles(QPainter *);
    void updateInfo(QPainter *);
    void updateUsage(QPainter *);
  
    QString showDateFormat;
    QString showTimeFormat;

    MythPopupBox *popup;
    QPixmap myBackground;

    QPixmap *containerPixmap;
    QPixmap *fillerPixmap;
  
    QPixmap *bgTransBackup;

    QRect fullRect;
    QRect listRect;
    QRect infoRect;
    QRect usageRect;
    QRect videoRect;

    int listsize;
    int titleitems;

    QColor popupForeground;
    QColor popupBackground;
    QColor popupHighlight;
};

#endif
