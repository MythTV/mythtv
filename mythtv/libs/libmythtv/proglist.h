#ifndef PROGLIST_H_
#define PROGLIST_H_

#include <qdatetime.h>
#include "libmyth/uitypes.h"
#include "libmyth/xmlparse.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythdialogs.h"
#include "libmythtv/programinfo.h"

class QSqlDatabase;

class ProgLister : public MythDialog
{
    Q_OBJECT

  public:
    ProgLister(const QString &ltitle, QSqlDatabase *ldb, 
	       MythMainWindow *parent, const char *name = 0);
    ~ProgLister();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void select();
    void edit();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void quickRecord(void);

  private:
    QString title;
    QSqlDatabase *db;
    QDateTime curTime;
    QString timeFormat;

    QPtrList<ProgramInfo> progList;
    int curProg;

    XMLParse *theme;
    QDomElement xmldata;
    bool pageDowner;
    int inList;
    int inData;
    int listCount;
    int dataCount;
    QRect listRect;
    QRect infoRect;
    QRect fullRect;
    int listsize;
    bool allowKeys;
    bool doingSel;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);
    void fillProgList(void);
    void LoadWindow(QDomElement &);
};

#endif
