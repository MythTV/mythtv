#ifndef GUIDEGRID_H_
#define GUIDEGRID_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qdatetime.h>
#include <vector>

class QFont;
class ProgramInfo;
class TimeInfo;
class ChannelInfo;

using namespace std;

class GuideGrid : public QDialog
{
    Q_OBJECT
  public:
    GuideGrid(const QString &channel, QWidget *parent = 0, 
              const char *name = 0);
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

    void fillChannelInfos();

    void fillTimeInfos();

    ProgramInfo *getProgramInfo(unsigned int row, unsigned int col);
    void fillProgramInfos(void);

    QBrush getBGColor(const QString &category);
    
    QFont *m_font;
    QFont *m_largerFont;

    vector<ChannelInfo> m_channelInfos;
    TimeInfo *m_timeInfos[10];
    ProgramInfo *m_programInfos[10][10];

    QDateTime m_originalStartTime;
    QDateTime m_currentStartTime;
    unsigned int m_currentStartChannel;
    QString m_startChanStr;
    
    int m_currentRow;
    int m_currentCol;

    bool selectState;
    bool showInfo;

    int screenwidth, screenheight;
    float wmult, hmult;

    bool usetheme;
    QColor fgcolor;
    QColor bgcolor;
};

#define CHANNUM_MAX  128
#endif
