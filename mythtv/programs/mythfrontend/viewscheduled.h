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
    ViewScheduled(QSqlDatabase *ldb, QWidget *parent = 0, const char *name = 0);
    ~ViewScheduled();

  signals:
    void killTheApp();

  protected slots:
    void edit();
    void selected();
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void exitWin();

  protected:
    void paintEvent(QPaintEvent *);

  private:
    void FillList(void);
    QMap<QString, ProgramInfo> conflictData;

    void handleConflicting(ProgramInfo *rec);
    void handleDuplicate(ProgramInfo *rec);
    void handleNotRecording(ProgramInfo *rec);
    void chooseConflictingProgram(ProgramInfo *rec);

    void grayOut(QPainter *);
    void updateBackground(void);
    void updateList(QPainter *);
    void updateConflict(QPainter *);
    void updateInfo(QPainter *);

    void resizeImage(QPixmap *, QString);
    void LoadWindow(QDomElement &);
    void parsePopup(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    ProgramInfo *curitem;

    QSqlDatabase *db;

    MythPopupBox *popup;
    QPixmap myBackground;
    QPixmap *bgTransBackup;

    bool conflictBool;
    bool graphicPopup;
    bool pageDowner;
    QString dateformat;
    QString timeformat;
    QString shortdateformat;
    bool displayChanNum;

    QAccel *accel;

    int inList;
    int inData;
    int listCount;
    int dataCount;

    QRect listRect;
    QRect infoRect;
    QRect conflictRect;
    QRect fullRect;

    int space_itemid;
    int enter_itemid;
    int return_itemid;
    int edit_itemid;

    int listsize;
    int titleitems;

    QColor popupForeground;
    QColor popupBackground;
    QColor popupHighlight;

    bool doingSel;
};

#endif
