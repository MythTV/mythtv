#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"

class QSqlDatabase;

class ViewScheduled : public MythDialog
{
    Q_OBJECT
  public:
    ViewScheduled(QSqlDatabase *ldb, MythMainWindow *parent, 
                  const char *name = 0);
    ~ViewScheduled();

  protected slots:
    void edit();
    void selected();
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

  private:
    void FillList(void);
    void setShowAll(bool all);

    int curRec;
    int recCount;
    QPtrList<ProgramInfo> recList;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateConflict(QPainter *);
    void updateShowLevel(QPainter *);
    void updateInfo(QPainter *);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    QSqlDatabase *db;

    QPixmap myBackground;

    bool conflictBool;
    QString dateformat;
    QString timeformat;
    bool displayChanNum;

    QRect listRect;
    QRect infoRect;
    QRect conflictRect;
    QRect showLevelRect;
    QRect fullRect;

    int listsize;

    bool allowEvents;
    bool allowUpdates;
    bool updateAll;
    bool refillAll;

    bool showAll;
};

#endif
