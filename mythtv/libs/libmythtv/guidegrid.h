#ifndef GUIDEGRID_H_
#define GUIDEGRID_H_

#include <qwidget.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qdatetime.h>

class QFont;
class ChannelInfo 
{
 public:
    ChannelInfo() { icon = NULL; }
   ~ChannelInfo() { if (icon) delete icon; }

    void LoadIcon() { icon = new QPixmap(iconpath); }

    QString callsign;
    QString iconpath;
    QString chanstr;

    QPixmap *icon;
    int channum;
};

class TimeInfo
{
  public:
    QString usertime;
    QString sqltime;
};

class ProgramInfo
{
  public:
    QString title;
    QString subtitle;
    QString description;
    QString category;
    QString starttime;
    QString endtime;
    QString channum;

    int spread;
    int startCol;
};

class GuideGrid : public QWidget
{
    Q_OBJECT
  public:
    GuideGrid(int channel, QWidget *parent = 0, const char *name = 0);

    int getLastChannel(void);

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

    void enter();
    void escape();

    void displayInfo();

  protected:
    void paintEvent(QPaintEvent *);

  private:
    void paintChannels(QPainter *p);
    void paintTimes(QPainter *p);
    void paintPrograms(QPainter *p);

    QRect fullRect() const;
    QRect channelRect() const;
    QRect timeRect() const;
    QRect programRect() const;

    ChannelInfo *getChannelInfo(int channum);
    void fillChannelInfos();

    void fillTimeInfos();

    ProgramInfo *getProgramInfo(unsigned int row, unsigned int col);
    void fillProgramInfos(void);

    QFont *m_font;
    QFont *m_largerFont;

    ChannelInfo *m_channelInfos[10];
    TimeInfo *m_timeInfos[10];
    ProgramInfo *m_programInfos[10][10];

    QDateTime m_originalStartTime;
    QDateTime m_currentStartTime;
    unsigned int m_currentStartChannel;
    unsigned int m_currentEndChannel;

    int m_currentRow;
    int m_currentCol;

    bool selectState;
    bool showInfo;
};

#define CHANNUM_MAX  128
#endif
