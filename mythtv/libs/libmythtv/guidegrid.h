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
class QSqlDatabase;
class TV;
class QTimer;
class QWidget;

#define MAX_DISPLAY_CHANS 8
#define MAX_DISPLAY_TIMES 30

// Use this function to instantiate a guidegrid instance.
QString RunProgramGuide(QString startchannel, bool thread = false, 
                        TV *player = NULL);


class GuideGrid : public MythDialog
{
    Q_OBJECT
  public:
    GuideGrid(const QString &channel, TV *player = NULL,
              QWidget *parent = 0, const char *name = 0);
   ~GuideGrid();

    QString getLastChannel(void);

  signals:
    void killTheApp();

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
    void displayInfo();

  protected:
    void paintEvent(QPaintEvent *);

  private slots:
    void timeout();

  private:
    void updateBackground(void);
    void paintDate(QPainter *);
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

    int rectProgLeft;
    int rectProgTop;
    int rectProgWidth;
    int rectProgHeight;
    int rectInfoLeft;
    int rectInfoTop;
    int rectInfoWidth;
    int rectInfoHeight;
    int rectDateLeft;
    int rectDateTop;
    int rectDateWidth;
    int rectDateHeight;
    int rectChanLeft;
    int rectChanTop;
    int rectChanWidth;
    int rectChanHeight;
    int rectTimeLeft;
    int rectTimeTop;
    int rectTimeWidth;
    int rectTimeHeight;
    int rectCurILeft;
    int rectCurITop;
    int rectCurIWidth;
    int rectCurIHeight;
    int rectVideoLeft;
    int rectVideoTop;
    int rectVideoWidth;
    int rectVideoHeight;

    int gridfilltype;
    int scrolltype;

    QRect fullRect() const;
    QRect dateRect() const;
    QRect channelRect() const;
    QRect timeRect() const;
    QRect programRect() const;
    QRect infoRect() const;
    QRect curInfoRect() const;
    QRect videoRect() const;

    void fillChannelInfos(int &maxchannel, bool gotostartchannel = true);

    void fillTimeInfos();

    void fillProgramInfos(void);
    void fillProgramRowInfos(unsigned int row);

    void setStartChannel(int newStartChannel);

    void createProgramLabel(int, int);

    QString getDateLabel(ProgramInfo *pginfo);

    vector<ChannelInfo> m_channelInfos;
    TimeInfo *m_timeInfos[MAX_DISPLAY_TIMES];
    QPtrList<ProgramInfo> *m_programs[MAX_DISPLAY_CHANS];
    ProgramInfo *m_programInfos[MAX_DISPLAY_CHANS][MAX_DISPLAY_TIMES];

    QDateTime m_originalStartTime;
    QDateTime m_currentStartTime;
    QDateTime m_currentEndTime;
    unsigned int m_currentStartChannel;
    QString m_startChanStr;
    
    int m_currentRow;
    int m_currentCol;
    int showProgramBar;

    QLabel *forvideo;

    bool showInfo;
    bool selectState;
    bool showFavorites;
    bool displaychannum;

    bool showtitle;
    bool usetheme;

    int startChannel;
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

    QSqlDatabase *m_db;
};

#endif
