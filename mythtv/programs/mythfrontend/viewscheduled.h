#ifndef VIEWSCHEDULED_H_
#define VIEWSCHEDULED_H_

#include <qdatetime.h>
#include <qdom.h>
#include "libmyth/mythwidgets.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "libmythtv/programinfo.h"

class QSqlDatabase;
class ProgramInfo;

class ViewScheduled : public MythDialog
{
    Q_OBJECT
  public:
    ViewScheduled(QSqlDatabase *ldb, QWidget *parent = 0, const char *name = 0);
    ~ViewScheduled();

  signals:
    void killTheApp();

  protected slots:
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
    QAccel *accel;

    int inList;
    int inData;
    int listCount;
    int dataCount;

    int rectListLeft;
    int rectListTop;
    int rectListWidth;
    int rectListHeight;
    int rectInfoLeft;
    int rectInfoTop;
    int rectInfoWidth;
    int rectInfoHeight;
    int rectConLeft;
    int rectConTop;
    int rectConWidth;
    int rectConHeight;

    int space_itemid;
    int enter_itemid;
    int return_itemid;

    int listsize;
    int titleitems;

    QColor popupForeground;
    QColor popupBackground;
    QColor popupHighlight;

    QRect listRect() const;
    QRect infoRect() const;
    QRect conflictRect() const;
    QRect fullRect() const;

    bool doingSel;
};

#endif
