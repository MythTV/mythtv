#ifndef PROGLIST_H_
#define PROGLIST_H_

#include <qdatetime.h>
#include "libmyth/uitypes.h"
#include "libmyth/xmlparse.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythdialogs.h"
#include "libmythtv/programinfo.h"

enum ProgListType {
    plUnknown = 0,
    plTitle = 1,
    plNewListings = 2,
    plTitleSearch = 3,
    plDescSearch = 4,
    plChannel = 5,
    plCategory = 6,
    plMovies = 7
};

class QSqlDatabase;

class ProgLister : public MythDialog
{
    Q_OBJECT

  public:
    ProgLister(ProgListType pltype, const QString &ltitle, QSqlDatabase *ldb, 
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
    void customEvent(QCustomEvent *e);
    void quickRecord(void);

  private:
    ProgListType type;
    QString title;
    QSqlDatabase *db;
    QDateTime startTime;
    QString timeFormat;

    int curItem;
    int itemCount;
    QPtrList<ProgramInfo> itemList;

    XMLParse *theme;
    QDomElement xmldata;

    QRect listRect;
    QRect infoRect;
    QRect fullRect;
    int listsize;

    bool allowEvents;
    bool allowUpdates;
    bool updateAll;
    bool refillAll;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);
    void fillItemList(void);
    void LoadWindow(QDomElement &);
};

#endif
