#ifndef PLAYBACKBOX_H_
#define PLAYBACKBOX_H_

#include <qdatetime.h>
#include <qdom.h>
#include "libmyth/mythwidgets.h"
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

    PlaybackBox(BoxType ltype, QWidget *parent = 0, const char *name = 0);
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

    void doDelete();
    void noDelete();

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

    bool skipUpdate;
    bool noUpdate;
    bool pageDowner;
    ProgramInfo *curitem;
    ProgramInfo *delitem;

//    bool LoadTheme(void);
    void LoadWindow(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;
/*
    QString getFirstText(QDomElement &);
    void parseFont(QDomElement &);
    fontProp *GetFont(const QString &);
    void normalizeRect(QRect *);
    QPoint parsePoint(QString);
    QRect parseRect(QString);
*/
    void parsePopup(QDomElement &);
    void parseContainer(QDomElement &);
/*
    LayerSet *GetSet(const QString &text);
    void parseListArea(LayerSet *, QDomElement &);
    void parseTextArea(LayerSet *, QDomElement &);
    void parseStatusBar(LayerSet *, QDomElement &);
    void parseImage(LayerSet *, QDomElement &);

    QMap<QString, fontProp> fontMap;
    QMap<QString, LayerSet*> layerMap;
    vector<LayerSet *> *allTypes;
*/
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

    QRect fullRect() const;
    QRect usageRect() const;
    QRect infoRect() const;
    QRect listRect() const;
    QRect videoRect() const;

    MythPopupBox *popup;
    QPixmap myBackground;

    QPixmap *containerPixmap;
    QPixmap *fillerPixmap;
  
    QPixmap *bgTransBackup;

    int rectTopLeft;
    int rectTopTop;
    int rectTopWidth;
    int rectTopHeight;
    int rectListLeft;
    int rectListTop;
    int rectListWidth;
    int rectListHeight;
    int rectInfoLeft;
    int rectInfoTop;
    int rectInfoWidth;
    int rectInfoHeight;
    int rectUsageLeft;
    int rectUsageTop;
    int rectUsageWidth;
    int rectUsageHeight;
    int rectVideoLeft;
    int rectVideoTop;
    int rectVideoWidth;
    int rectVideoHeight;

    int listsize;
    int titleitems;

    QColor popupForeground;
    QColor popupBackground;
    QColor popupHighlight;
};

#endif
