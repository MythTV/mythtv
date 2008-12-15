// -*- Mode: c++ -*-
#ifndef GUIDEGRID_H_
#define GUIDEGRID_H_

#include <QString>
#include <QDateTime>
#include <QLayout>
#include <QEvent>
#include <QPaintEvent>
#include <QPixmap>
#include <QKeyEvent>
#include <QDomElement>
#include <vector>

#include "mythwidgets.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"
#include "programlist.h"
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

class JumpToChannel;
class MPUBLIC JumpToChannelListener
{
  public:
    virtual void GoTo(int start, int cur_row) = 0;
    virtual void SetJumpToChannel(JumpToChannel *ptr) = 0;
    virtual int  FindChannel(uint chanid, const QString &channum,
                             bool exact = true) const = 0;
};

class MPUBLIC JumpToChannel : public QObject
{
    Q_OBJECT

  public:
    JumpToChannel(
        JumpToChannelListener *parent, const QString &start_entry,
        int start_chan_idx, int cur_chan_idx, uint rows_disp);

    bool ProcessEntry(const QStringList &actions, const QKeyEvent *e);

    QString GetEntry(void) const { return entry; }

  public slots:
    virtual void deleteLater(void);

  private:
    ~JumpToChannel() {}
    void Update(void);

  private:
    JumpToChannelListener *listener;
    QString  entry;
    int      previous_start_channel_index;
    int      previous_current_channel_index;
    uint     rows_displayed;
    QTimer  *timer;
};

class MPUBLIC GuideGrid : public MythDialog, public JumpToChannelListener
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

    virtual void GoTo(int start, int cur_row);
    virtual void SetJumpToChannel(JumpToChannel *ptr);

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

    void customEvent(QEvent *e);

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
    int  FindChannel(uint chanid, const QString &channum,
                     bool exact = true) const;

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

    QMutex         jumpToChannelLock;
    JumpToChannel *jumpToChannel;
    bool           jumpToChannelEnabled;
    bool           jumpToChannelHasRect;
};

#endif
