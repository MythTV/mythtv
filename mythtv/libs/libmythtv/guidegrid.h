#ifndef GUIDEGRID_H_
#define GUIDEGRID_H_

#include <qwidget.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qdatetime.h>

#include <vector>

using namespace std;

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
};

class GuideGrid : public QWidget
{
    Q_OBJECT
  public:
    GuideGrid(QWidget *parent = 0, const char *name = 0);

  protected slots:
    void scrollLeft();
    void scrollRight();
    void scrollDown();
    void scrollUp();

  protected:
    void paintEvent(QPaintEvent *);

  private:
    void paintChannels(QPainter *p);
    void paintTimes(QPainter *p);
    void paintPrograms(QPainter *p);

    QRect channelRect() const;
    QRect timeRect() const;
    QRect programRect() const;

    ChannelInfo *getChannelInfo(int channum);
    void fillChannelInfos();

    void fillTimeInfos();

    ProgramInfo *getProgramInfo(unsigned int row, unsigned int col);
    void fillProgramInfos(void);

    QFont *m_font;
    vector<ChannelInfo *> m_channelInfos;
    vector<TimeInfo *> m_timeInfos;

    QDateTime m_currentStartTime;
    unsigned int m_currentStartChannel;
    unsigned int m_currentEndChannel;

    ProgramInfo *m_programInfos[10][10];
    int m_programRows;
};

#define CHANNUM_MAX  128
#endif
