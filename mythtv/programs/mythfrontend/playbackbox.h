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
  
  protected slots:
    void timeout(void);

    void cursorLeft();
    void cursorRight();
    void cursorDown(bool page = false, bool newview = false);
    void cursorUp(bool page = false, bool newview = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void selected();
    void playSelected();
    void stopSelected();
    void deleteSelected();
    void expireSelected();
    void showActionsSelected();

    void doPlay();

    void askStop();
    void doStop();
    void noStop();

    void askDelete();
    void doDelete();
    void doDeleteForgetHistory();
    void noDelete();

    void doAutoExpire();
    void noAutoExpire();

    void doCancel();

    void exitWin();

    void setUpdateFreeSpace() { updateFreeSpace = true; }

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    bool FillList(void);
    void UpdateProgressBar(void);

    QString cutDown(QString, QFont *, int);
    QPixmap getPixmap(ProgramInfo *);
    QPainter backup;
    void play(ProgramInfo *);
    void stop(ProgramInfo *);
    void remove(ProgramInfo *);
    void expire(ProgramInfo *);
    void showActions(ProgramInfo *);

    bool skipUpdate;
    bool pageDowner;
    bool connected;
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

    bool killPlayer(void);
    void killPlayerSafe(void);
    void startPlayer(ProgramInfo *rec);

    void doRemove(ProgramInfo *, bool forgetHistory);
    void promptEndOfRecording(ProgramInfo *);
    typedef enum {
         EndOfRecording,DeleteRecording,AutoExpireRecording,StopRecording
    } deletePopupType;
    void showDeletePopup(ProgramInfo *, deletePopupType);
    void showActionPopup(ProgramInfo *program);
    void initPopup(MythPopupBox *popup, ProgramInfo *program, 
                   QString message, QString message2);
    void cancelPopup();

    bool fileExists(ProgramInfo *pginfo);

    void showIconHelp();

    QTimer *timer;
    NuppelVideoPlayer *nvp;
    RingBuffer *rbuffer;
    pthread_t decoder;

    typedef enum 
    { kStarting, kPlaying, kKilling, kKilled, kStopping, kStopped, 
      kChanging } playerStateType;

    playerStateType state;

    typedef enum
    { kNvpToPlay, kNvpToStop, kDone } killStateType;
     
    killStateType killState;
    QTime killTimeout;

    QTime nvpTimeout;    
    QTime waitToStartPreviewTimer;
    bool waitToStart;
 
    QDateTime lastUpdateTime;
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

    bool expectingPopup;

    bool updateFreeSpace;
    QTimer *freeSpaceTimer;
    int freeSpaceTotal;
    int freeSpaceUsed;
};

#endif
