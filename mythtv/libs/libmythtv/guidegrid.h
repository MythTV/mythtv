// -*- Mode: c++ -*-
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
#include "channelutil.h"

using namespace std;

class ProgramInfo;
class TimeInfo;
class TV;
class QTimer;
class QWidget;

#define MAX_DISPLAY_CHANS 12
#define MAX_DISPLAY_TIMES 30

typedef vector<PixmapChannel>   pix_chan_list_t;
typedef vector<pix_chan_list_t> pix_chan_list_list_t;

class MPUBLIC GuideGrid : public MythDialog
{
    Q_OBJECT

  public:
    // Use this function to instantiate a guidegrid instance.
    static DBChanList Run(uint           startChanId,
                          const QString &startChanNum,
                          bool           thread = false,
                          TV            *player = NULL,
                          bool           allowsecondaryepg = true);

    DBChanList GetSelection(void) const;

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
    void volumeUpdate(bool);
    void toggleMute();

    void quickRecord();
    void editRecording();
    void editScheduled();
    void customEdit();
    void remove();
    void upcoming();
    void details();

    void customEvent(QCustomEvent *e);

  protected:
    GuideGrid(MythMainWindow *parent,
              uint chanid = 0, QString channum = "",
              TV *player = NULL, bool allowsecondaryepg = true,
              const char *name = "GuideGrid");
   ~GuideGrid();

    void paintEvent(QPaintEvent *);

  private slots:
    void timeCheckTimeout(void);
    void repaintVideoTimeout(void);
    void jumpToChannelTimeout(void);

  private:
    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);

    void updateBackground(void);
    void paintDate(QPainter *);
    void paintJumpToChannel(QPainter *);
    bool paintChannels(QPainter *);
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

    void fillChannelInfos(bool gotostartchannel = true);

    void fillTimeInfos();

    void fillProgramInfos(void);
    void fillProgramRowInfos(unsigned int row);

    void setStartChannel(int newStartChannel);

    void createProgramLabel(int, int);

    PixmapChannel       *GetChannelInfo(uint chan_idx, int sel = -1);
    const PixmapChannel *GetChannelInfo(uint chan_idx, int sel = -1) const;
    uint                 GetChannelCount(void) const;
    int                  GetStartChannelOffset(int row = -1) const;

    ProgramList GetProgramList(uint chanid) const;
    uint GetAlternateChannelIndex(uint chan_idx, bool with_same_channum) const;

  private:
    pix_chan_list_list_t m_channelInfos;
    QMap<uint,uint>      m_channelInfoIdx;

    TimeInfo *m_timeInfos[MAX_DISPLAY_TIMES];
    ProgramList *m_programs[MAX_DISPLAY_CHANS];
    ProgramInfo *m_programInfos[MAX_DISPLAY_CHANS][MAX_DISPLAY_TIMES];
    ProgramList m_recList;

    QDateTime m_originalStartTime;
    QDateTime m_currentStartTime;
    QDateTime m_currentEndTime;
    uint      m_currentStartChannel;
    uint      startChanID;
    QString   startChanNum;
    
    int m_currentRow;
    int m_currentCol;

    bool selectState;
    bool showFavorites;
    bool sortReverse;
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

    QTimer *timeCheck;
    QTimer *videoRepaintTimer;

    bool keyDown;

    void jumpToChannelResetAndHide();
    void jumpToChannelCancel();
    void jumpToChannelCommit();
    void jumpToChannelShowSelection();
    void jumpToChannelDeleteLastDigit();
    void jumpToChannelDigitPress(int);
    bool jumpToChannelGetInputDigit(QStringList & actions, int & digit);
    int jumpToChannel;
    int jumpToChannelPreviousStartChannel;
    int jumpToChannelPreviousRow;
    bool jumpToChannelEnabled;
    bool jumpToChannelActive;
    bool jumpToChannelHasRect;
    QTimer *jumpToChannelTimer;
};

#endif
