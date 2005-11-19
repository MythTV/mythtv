#ifndef VIEWSCHEDULEDIFF_H_
#define VIEWSCHEDULEDIFF_H_

#include <qvector.h>
#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"

struct ProgramStruct {
        ProgramInfo *before;
        ProgramInfo *after;
};

class ViewScheduleDiff : public MythDialog
{
    Q_OBJECT
  public:
    ViewScheduleDiff(MythMainWindow *parent, const char *name, QString altTbl,
                     int recordid = -1, QString ltitle = "");
    ~ViewScheduleDiff();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void edit(void);
    void upcoming(void);
    void details(void);
    void statusDialog(void);

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void FillList(void);
    void setShowAll(bool all);

    void updateBackground(void);
    void updateList(QPainter *);
    void updateShowLevel(QPainter *);
    void updateInfo(QPainter *);
    void updateRecStatus(QPainter *);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    QPixmap myBackground;

    bool conflictBool;
    QString dateformat;
    QString timeformat;
    QString channelFormat;

    QRect listRect;
    QRect infoRect;
    QRect showLevelRect;
    QRect recStatusRect;
    QRect fullRect;

    int listsize;

    bool inEvent;
    bool inFill;
    bool needFill;

    unsigned int listPos;

    ProgramInfo *CurrentProgram(void);

    ProgramList recListBefore;
    ProgramList recListAfter;

    QString altTable;
    QString m_title;

    QValueVector<struct ProgramStruct> recList;

    int recordid; ///< recordid that differs from master (-1 = assume all)

    int programCnt;
};

#endif
