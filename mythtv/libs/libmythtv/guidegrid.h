#ifndef GUIDEGRID_H_
#define GUIDEGRID_H_

#include <qstring.h>
#include <qdatetime.h>
#include <qptrlist.h>
#include <qlayout.h>
#include <vector>

#include "libmyth/mythwidgets.h"
#include <qdom.h>
#include "uitypes.h"
#include "xmlparse.h"
#include "libmythtv/programinfo.h"

using namespace std;

class ProgramInfo;
class TimeInfo;
class ChannelInfo;
class TV;
class QTimer;
class QWidget;

#define MAX_DISPLAY_CHANS 12
#define MAX_DISPLAY_TIMES 30

// Use this function to instantiate a guidegrid instance.
QString RunProgramGuide(QString startchannel, bool thread = false, 
                        TV *player = NULL, bool allowsecondaryepg = true);


class GuideGrid : public MythDialog
{
    Q_OBJECT
  public:
    GuideGrid(MythMainWindow *parent, const QString &channel, TV *player = NULL,
              bool allowsecondaryepg = true, const char *name = 0);
   ~GuideGrid();

    QString getLastChannel(void);

  protected slots:
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();

    void scrollLeft();
    void scrollRight();
    void scrollDown();
    void scrollUp();

    void dayLeft();
    void dayRight();
    void pageLeft();
    void pageRight();
    void pageDown();
    void pageUp();
    void toggleGuideListing();
    void toggleChannelFavorite();
    void generateListings();

    void enter();
    void escape();

    void showProgFinder();
    void channelUpdate();

    void quickRecord();
    void editRecording();
    void editScheduled();
    void upcoming();
    void details();

    void customEvent(QCustomEvent *e);

  protected:
    void paintEvent(QPaintEvent *);

  private slots:
    void timeout();
    void jumpToChannelTimeout();

  private:
    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);

    void updateBackground(void);
    void paintDate(QPainter *);
    void paintJumpToChannel(QPainter *);
    void paintChannels(QPainter *);
    void paintTimes(QPainter *);
    void paintPrograms(QPainter *);
    void paintCurrentInfo(QPainter *);
    void paintInfo(QPainter *);
 
    void resizeImage(QPixmap *, QString);
    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    int m_context;

    bool selectChangesChannel;
    int selectRecThreshold;
    
    int gridfilltype;
    int scrolltype;

    QRect fullRect;
    QRect dateRect;
    QRect jumpToChannelRect;
    QRect channelRect;
    QRect timeRect;
    QRect programRect;
    QRect infoRect;
    QRect curInfoRect;
    QRect videoRect;

    void fillChannelInfos(int &maxchannel, bool gotostartchannel = true);

    void fillTimeInfos();

    void fillProgramInfos(void);
    void fillProgramRowInfos(unsigned int row);

    void setStartChannel(int newStartChannel);

    void createProgramLabel(int, int);

    vector<ChannelInfo> m_channelInfos;
    TimeInfo *m_timeInfos[MAX_DISPLAY_TIMES];
    ProgramList *m_programs[MAX_DISPLAY_CHANS];
    ProgramInfo *m_programInfos[MAX_DISPLAY_CHANS][MAX_DISPLAY_TIMES];
    ProgramList m_recList;

    QDateTime m_originalStartTime;
    QDateTime m_currentStartTime;
    QDateTime m_currentEndTime;
    unsigned int m_currentStartChannel;
    QString m_startChanStr;
    
    int m_currentRow;
    int m_currentCol;

    bool selectState;
    bool showFavorites;
    QString channelFormat;

    int desiredDisplayChans;
    int DISPLAY_CHANS;
    int DISPLAY_TIMES;

    QDateTime firstTime;
    QDateTime lastTime;

    TV *m_player;

    QString channelOrdering;
    QString dateformat;
    QString timeformat;
    QString unknownTitle;
    QString unknownCategory;
    QString currentTimeColor;

    QTimer *timeCheck;

    bool keyDown;

    void jumpToChannelResetAndHide();
    void jumpToChannelCancel();
    void jumpToChannelCommit();
    void jumpToChannelShowSelection();
    void jumpToChannelDeleteLastDigit();
    void jumpToChannelDigitPress(int);
    int jumpToChannel;
    int jumpToChannelPreviousStartChannel;
    int jumpToChannelPreviousRow;
    bool jumpToChannelEnabled;
    bool jumpToChannelActive;
    bool jumpToChannelHasRect;
    QTimer *jumpToChannelTimer;
};

#endif
